#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/transport/central.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define SPLIT_IDLE_SCAN_GUARD_CHECK_INTERVAL_MS 1000

/* Intentional local access to ZMK's split central transport selector. */
extern const struct zmk_split_transport_central *active_transport;

static void split_idle_scan_guard_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(split_idle_scan_guard_work, split_idle_scan_guard_work_handler);

static enum zmk_activity_state current_activity_state = ZMK_ACTIVITY_ACTIVE;
static const struct zmk_split_transport_central *disabled_transport;

static int transport_status(const struct zmk_split_transport_central *transport,
                            struct zmk_split_transport_status *status) {
    if (transport == NULL || transport->api == NULL || transport->api->get_status == NULL) {
        return -ENODEV;
    }

    *status = transport->api->get_status();
    return 0;
}

static void schedule_idle_check(void) {
    if (current_activity_state == ZMK_ACTIVITY_ACTIVE || disabled_transport != NULL) {
        return;
    }

    k_work_reschedule(&split_idle_scan_guard_work,
                      K_MSEC(SPLIT_IDLE_SCAN_GUARD_CHECK_INTERVAL_MS));
}

static void disable_transport_if_idle_disconnected(void) {
    if (current_activity_state == ZMK_ACTIVITY_ACTIVE || disabled_transport != NULL) {
        return;
    }

    const struct zmk_split_transport_central *transport = active_transport;

    if (transport == NULL || transport->api == NULL || transport->api->set_enabled == NULL) {
        return;
    }

    struct zmk_split_transport_status status;
    int err = transport_status(transport, &status);
    if (err < 0 || !status.enabled) {
        return;
    }

    if (status.connections != ZMK_SPLIT_TRANSPORT_CONNECTIONS_STATUS_DISCONNECTED) {
        schedule_idle_check();
        return;
    }

    err = transport->api->set_enabled(false);
    if (err < 0) {
        LOG_WRN("Failed to stop idle split scanning: %d", err);
        schedule_idle_check();
        return;
    }

    disabled_transport = transport;
    LOG_DBG("Stopped idle split scanning after peripheral disconnect");
}

static void restore_transport_if_guard_disabled(void) {
    k_work_cancel_delayable(&split_idle_scan_guard_work);

    if (disabled_transport == NULL) {
        return;
    }

    const struct zmk_split_transport_central *transport = disabled_transport;

    if (transport->api == NULL || transport->api->set_enabled == NULL) {
        disabled_transport = NULL;
        return;
    }

    int err = transport->api->set_enabled(true);
    if (err < 0) {
        LOG_WRN("Failed to restore split scanning: %d", err);
        return;
    }

    disabled_transport = NULL;
    LOG_DBG("Restored split scanning after central activity");
}

static void split_idle_scan_guard_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    disable_transport_if_idle_disconnected();
}

static int split_idle_scan_guard_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *activity_ev = as_zmk_activity_state_changed(eh);

    if (activity_ev != NULL) {
        current_activity_state = activity_ev->state;

        if (current_activity_state == ZMK_ACTIVITY_ACTIVE) {
            restore_transport_if_guard_disabled();
        } else {
            disable_transport_if_idle_disconnected();
        }

        return 0;
    }

    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        disable_transport_if_idle_disconnected();
    }

    return 0;
}

ZMK_LISTENER(cradio_split_idle_scan_guard, split_idle_scan_guard_listener);
ZMK_SUBSCRIPTION(cradio_split_idle_scan_guard, zmk_activity_state_changed);
ZMK_SUBSCRIPTION(cradio_split_idle_scan_guard, zmk_peripheral_battery_state_changed);
