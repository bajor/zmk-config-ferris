#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/matrix.h>
#include <zmk/pm.h>

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
#include <zmk/usb.h>
#endif

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define DT_DRV_COMPAT cradio_stuck_key_wakeup

#define POSITION_WORD_BITS 32
#define POSITION_WORDS DIV_ROUND_UP(ZMK_KEYMAP_LEN, POSITION_WORD_BITS)

BUILD_ASSERT(CONFIG_CRADIO_STUCK_KEY_WAKEUP_INIT_PRIORITY > CONFIG_KSCAN_INIT_PRIORITY,
             "stuck-key wake device must initialize after kscan");

struct stuck_key_wakeup_config {
    const struct gpio_dt_spec *inputs;
    size_t input_count;
};

static uint32_t pressed_positions[POSITION_WORDS];
static size_t pressed_position_count;
K_MUTEX_DEFINE(pressed_positions_lock);

static void stuck_key_sleep_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(stuck_key_sleep_work, stuck_key_sleep_work_handler);

static bool usb_powered(void) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    return zmk_usb_is_powered();
#else
    return false;
#endif
}

static void schedule_stuck_key_sleep(void) {
    k_work_reschedule(&stuck_key_sleep_work,
                      K_MSEC(CONFIG_CRADIO_STUCK_KEY_SLEEP_TIMEOUT_MS));
}

static void cancel_stuck_key_sleep(void) { k_work_cancel_delayable(&stuck_key_sleep_work); }

static bool update_pressed_position(uint32_t position, bool pressed) {
    uint32_t *word = &pressed_positions[position / POSITION_WORD_BITS];
    uint32_t mask = BIT(position % POSITION_WORD_BITS);
    bool was_pressed = (*word & mask) != 0;

    if (was_pressed == pressed) {
        return false;
    }

    if (pressed) {
        *word |= mask;
        pressed_position_count++;
    } else {
        *word &= ~mask;
        pressed_position_count--;
    }

    return true;
}

static void stuck_key_sleep_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    bool should_sleep;
    bool should_reschedule = false;

    k_mutex_lock(&pressed_positions_lock, K_FOREVER);
    should_sleep = pressed_position_count > 0;
    if (should_sleep && usb_powered()) {
        should_sleep = false;
        should_reschedule = true;
    }
    k_mutex_unlock(&pressed_positions_lock);

    if (should_reschedule) {
        LOG_INF("USB power present; delaying stuck-key protected sleep");
        schedule_stuck_key_sleep();
        return;
    }

    if (!should_sleep) {
        return;
    }

    LOG_WRN("Entering stuck-key protected sleep");
    int err = zmk_pm_soft_off();
    if (err < 0) {
        LOG_ERR("Failed to enter stuck-key protected sleep: %d", err);
        schedule_stuck_key_sleep();
    }
}

static int stuck_key_position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (ev == NULL || ev->source != ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL ||
        ev->position >= ZMK_KEYMAP_LEN) {
        return 0;
    }

    k_mutex_lock(&pressed_positions_lock, K_FOREVER);
    bool changed = update_pressed_position(ev->position, ev->state);
    size_t pressed_count = pressed_position_count;
    k_mutex_unlock(&pressed_positions_lock);

    if (!changed) {
        return 0;
    }

    if (pressed_count > 0) {
        schedule_stuck_key_sleep();
    } else {
        cancel_stuck_key_sleep();
    }

    return 0;
}

ZMK_LISTENER(cradio_stuck_key_sleep, stuck_key_position_listener);
ZMK_SUBSCRIPTION(cradio_stuck_key_sleep, zmk_position_state_changed);

static int stuck_key_wakeup_init(const struct device *dev) {
    ARG_UNUSED(dev);

#if IS_ENABLED(CONFIG_PM_DEVICE)
    pm_device_init_suspended(dev);
#endif

    return 0;
}

#if IS_ENABLED(CONFIG_PM_DEVICE)

static int stuck_key_wakeup_resume(const struct device *dev) {
    const struct stuck_key_wakeup_config *config = dev->config;

    for (size_t i = 0; i < config->input_count; i++) {
        const struct gpio_dt_spec *gpio = &config->inputs[i];

        if (!device_is_ready(gpio->port)) {
            LOG_ERR("GPIO port %s is not ready", gpio->port->name);
            return -ENODEV;
        }

        int err = gpio_pin_configure_dt(gpio, GPIO_INPUT);
        if (err < 0) {
            LOG_ERR("Unable to configure wake input %u on %s: %d", gpio->pin, gpio->port->name,
                    err);
            return err;
        }

        int active = gpio_pin_get_dt(gpio);
        if (active < 0) {
            LOG_ERR("Unable to read wake input %u on %s: %d", gpio->pin, gpio->port->name, active);
            return active;
        }

        gpio_flags_t interrupt = active ? GPIO_INT_DISABLE : GPIO_INT_LEVEL_ACTIVE;
        err = gpio_pin_interrupt_configure_dt(gpio, interrupt);
        if (err < 0) {
            LOG_ERR("Unable to configure wake interrupt %u on %s: %d", gpio->pin,
                    gpio->port->name, err);
            return err;
        }
    }

    return 0;
}

static int stuck_key_wakeup_suspend(const struct device *dev) {
    const struct stuck_key_wakeup_config *config = dev->config;

    for (size_t i = 0; i < config->input_count; i++) {
        const struct gpio_dt_spec *gpio = &config->inputs[i];

        int err = gpio_pin_interrupt_configure_dt(gpio, GPIO_INT_DISABLE);
        if (err < 0) {
            LOG_ERR("Unable to disable wake interrupt %u on %s: %d", gpio->pin, gpio->port->name,
                    err);
            return err;
        }

        err = gpio_pin_configure_dt(gpio, GPIO_DISCONNECTED);
        if (err < 0) {
            LOG_ERR("Unable to disconnect wake input %u on %s: %d", gpio->pin, gpio->port->name,
                    err);
            return err;
        }
    }

    return 0;
}

static int stuck_key_wakeup_pm_action(const struct device *dev, enum pm_device_action action) {
    switch (action) {
    case PM_DEVICE_ACTION_RESUME:
        return stuck_key_wakeup_resume(dev);
    case PM_DEVICE_ACTION_SUSPEND:
        return stuck_key_wakeup_suspend(dev);
    default:
        return -ENOTSUP;
    }
}

#endif

#define STUCK_KEY_WAKEUP_INPUT(idx, n)                                                             \
    GPIO_DT_SPEC_GET_BY_IDX(DT_INST_PHANDLE(n, kscan), input_gpios, idx)

#define STUCK_KEY_WAKEUP_INST(n)                                                                   \
    BUILD_ASSERT(DT_NODE_HAS_PROP(DT_INST_PHANDLE(n, kscan), input_gpios),                         \
                 "cradio,stuck-key-wakeup requires a direct GPIO kscan with input-gpios");         \
                                                                                                   \
    static const struct gpio_dt_spec stuck_key_wakeup_inputs_##n[] = {                             \
        LISTIFY(DT_PROP_LEN(DT_INST_PHANDLE(n, kscan), input_gpios), STUCK_KEY_WAKEUP_INPUT, (, ), \
                n)};                                                                               \
                                                                                                   \
    static const struct stuck_key_wakeup_config stuck_key_wakeup_config_##n = {                     \
        .inputs = stuck_key_wakeup_inputs_##n,                                                      \
        .input_count = ARRAY_SIZE(stuck_key_wakeup_inputs_##n),                                     \
    };                                                                                             \
                                                                                                   \
    PM_DEVICE_DT_INST_DEFINE(n, stuck_key_wakeup_pm_action);                                       \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, stuck_key_wakeup_init, PM_DEVICE_DT_INST_GET(n), NULL,                \
                          &stuck_key_wakeup_config_##n, POST_KERNEL,                              \
                          CONFIG_CRADIO_STUCK_KEY_WAKEUP_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(STUCK_KEY_WAKEUP_INST)
