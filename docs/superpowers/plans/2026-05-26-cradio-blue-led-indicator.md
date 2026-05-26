# Cradio Blue LED Indicator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a repo-local blue LED indicator that shows BLE status once at boot, then switches after 20 seconds to battery-only runtime blinking with thresholds at 40% and 20%.

**Architecture:** Vendor the existing LED indicator as a tracked local ZMK module under `local-modules/cradio-led-indicator`, remove the remote dependency from `config/west.yml`, and pass the local module path via `build.yaml` `cmake-args`. Update the vendored indicator code so startup only shows BLE status once, then a delayed runtime thread continuously checks battery level and emits either no blink, a slow warning pattern, or a faster critical pattern.

**Tech Stack:** ZMK, Zephyr Kconfig, Zephyr threads/message queues, GitHub Actions `build.yaml`, C, Markdown

---

## File Map

- Create: `local-modules/cradio-led-indicator/CMakeLists.txt`
- Create: `local-modules/cradio-led-indicator/Kconfig`
- Create: `local-modules/cradio-led-indicator/LICENSE`
- Create: `local-modules/cradio-led-indicator/leds.c`
- Create: `local-modules/cradio-led-indicator/zephyr/module.yml`
- Create: `docs/superpowers/plans/2026-05-26-cradio-blue-led-indicator.md`
- Modify: `build.yaml`
- Modify: `config/cradio.conf`
- Modify: `config/west.yml`
- Modify: `README.md`

### Task 1: Wire A Repo-Local Module

**Files:**
- Create: `local-modules/cradio-led-indicator/CMakeLists.txt`
- Create: `local-modules/cradio-led-indicator/Kconfig`
- Create: `local-modules/cradio-led-indicator/LICENSE`
- Create: `local-modules/cradio-led-indicator/zephyr/module.yml`
- Modify: `config/west.yml`
- Modify: `build.yaml`

- [ ] **Step 1: Run the failing build against the planned local module path**

Run:

```bash
west build -s zmk/app -d build/plan-red-left -p always -b nice_nano_v2 -- \
  -DSHIELD=cradio_left \
  -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/local-modules/cradio-led-indicator"
```

Expected: FAIL because `local-modules/cradio-led-indicator` does not exist yet.

- [ ] **Step 2: Create the tracked local module shell from the existing indicator module**

Create these files with the existing module metadata shape:

```cmake
# local-modules/cradio-led-indicator/CMakeLists.txt
target_sources_ifdef(CONFIG_INDICATOR_LED_WIDGET app PRIVATE leds.c)
```

```yaml
# local-modules/cradio-led-indicator/zephyr/module.yml
build:
  cmake: .
  kconfig: Kconfig
  settings:
    board_root: .
```

Copy the upstream MIT license text into `local-modules/cradio-led-indicator/LICENSE`.

- [ ] **Step 3: Remove the remote module dependency and point builds at the local module**

Apply these structural changes:

```yaml
# config/west.yml
manifest:
  defaults:
    revision: v0.3
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
  projects:
    - name: zmk
      remote: zmkfirmware
      import: app/west.yml
  self:
    path: config
```

```yaml
# build.yaml
include:
  - board: nice_nano_v2
    shield: cradio_left
    cmake-args: -DZMK_EXTRA_MODULES=$GITHUB_WORKSPACE/local-modules/cradio-led-indicator
  - board: nice_nano_v2
    shield: cradio_right
    cmake-args: -DZMK_EXTRA_MODULES=$GITHUB_WORKSPACE/local-modules/cradio-led-indicator
```

- [ ] **Step 4: Run the build again to verify the local module is discovered**

Run:

```bash
west build -s zmk/app -d build/plan-green-left -p always -b nice_nano_v2 -- \
  -DSHIELD=cradio_left \
  -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/local-modules/cradio-led-indicator"
```

Expected: The missing-path failure is gone. The build may still fail later because the runtime behavior code has not been implemented yet.

- [ ] **Step 5: Commit the wiring changes**

```bash
git add build.yaml config/west.yml local-modules/cradio-led-indicator
git commit -m "Vendor local Cradio LED indicator module"
```

### Task 2: Implement Boot BLE And Delayed Runtime Battery Tracking

**Files:**
- Modify: `local-modules/cradio-led-indicator/Kconfig`
- Modify: `local-modules/cradio-led-indicator/leds.c`
- Modify: `config/cradio.conf`

- [ ] **Step 1: Add the desired runtime configuration to `config/cradio.conf`**

Set the board config to the approved behavior:

```ini
CONFIG_INDICATOR_LED_WIDGET=y
CONFIG_INDICATOR_LED_SHOW_BATTERY_ON_BOOT=n
CONFIG_INDICATOR_LED_SHOW_BLE=y
CONFIG_INDICATOR_LED_SHOW_LAYER_CHANGE=n
CONFIG_INDICATOR_LED_RUNTIME_BATTERY_TRACKING=y
CONFIG_INDICATOR_LED_RUNTIME_BATTERY_START_DELAY_S=20
CONFIG_INDICATOR_LED_RUNTIME_BATTERY_LOW_THRESHOLD=40
CONFIG_INDICATOR_LED_RUNTIME_BATTERY_CRITICAL_THRESHOLD=20
```

- [ ] **Step 2: Run the build to verify the new runtime config is not supported yet**

Run:

```bash
west build -s zmk/app -d build/runtime-red-left -p always -b nice_nano_v2 -- \
  -DSHIELD=cradio_left \
  -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/local-modules/cradio-led-indicator"
```

Expected: FAIL or emit unsupported configuration evidence because the new runtime Kconfig symbols and logic do not exist yet.

- [ ] **Step 3: Add the runtime battery Kconfig surface**

Extend the vendored `Kconfig` with the runtime options:

```kconfig
config INDICATOR_LED_RUNTIME_BATTERY_TRACKING
    bool "Enable delayed runtime battery tracking"
    default y

config INDICATOR_LED_RUNTIME_BATTERY_START_DELAY_S
    int "Delay before runtime battery tracking starts"
    default 20

config INDICATOR_LED_RUNTIME_BATTERY_LOW_THRESHOLD
    int "Runtime low battery threshold percentage"
    default 40

config INDICATOR_LED_RUNTIME_BATTERY_CRITICAL_THRESHOLD
    int "Runtime critical battery threshold percentage"
    default 20
```

- [ ] **Step 4: Implement the delayed runtime battery loop in `leds.c`**

Add code shaped like this:

```c
static struct blink_item runtime_battery_blink_for_level(uint8_t battery_level) {
    if (battery_level == 0 || battery_level > CONFIG_INDICATOR_LED_RUNTIME_BATTERY_LOW_THRESHOLD) {
        return (struct blink_item){0};
    }

    if (battery_level <= CONFIG_INDICATOR_LED_RUNTIME_BATTERY_CRITICAL_THRESHOLD) {
        return BLINK_STRUCT(CONFIG_INDICATOR_LED_BATTERY_CRITICAL_PATTERN, 1);
    }

    return BLINK_STRUCT(CONFIG_INDICATOR_LED_BATTERY_LOW_PATTERN, 1);
}

extern void led_runtime_battery_thread(void *d0, void *d1, void *d2) {
    k_sleep(K_SECONDS(CONFIG_INDICATOR_LED_RUNTIME_BATTERY_START_DELAY_S));

    while (true) {
        uint8_t battery_level = zmk_battery_state_of_charge();
        struct blink_item blink = runtime_battery_blink_for_level(battery_level);

        if (blink.n_repeats > 0) {
            k_msgq_put(&led_msgq, &blink, K_NO_WAIT);
        }

        k_sleep(K_SECONDS(10));
    }
}
```

Also:

- keep `indicate_ble();` in the init thread
- remove startup battery indication from the init thread
- disable the old critical-change listener path unless it still serves the runtime design cleanly
- ensure runtime battery tracking is the only post-startup behavior

- [ ] **Step 5: Run the build to verify the runtime behavior compiles**

Run:

```bash
west build -s zmk/app -d build/runtime-green-left -p always -b nice_nano_v2 -- \
  -DSHIELD=cradio_left \
  -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/local-modules/cradio-led-indicator"
```

Expected: PASS.

- [ ] **Step 6: Inspect the generated config to verify the selected behavior**

Run:

```bash
rg -n "INDICATOR_LED_(SHOW_BATTERY_ON_BOOT|SHOW_BLE|RUNTIME_BATTERY)" \
  build/runtime-green-left/zephyr/.config
```

Expected:

```text
CONFIG_INDICATOR_LED_SHOW_BATTERY_ON_BOOT=n
CONFIG_INDICATOR_LED_SHOW_BLE=y
CONFIG_INDICATOR_LED_RUNTIME_BATTERY_TRACKING=y
CONFIG_INDICATOR_LED_RUNTIME_BATTERY_START_DELAY_S=20
CONFIG_INDICATOR_LED_RUNTIME_BATTERY_LOW_THRESHOLD=40
CONFIG_INDICATOR_LED_RUNTIME_BATTERY_CRITICAL_THRESHOLD=20
```

- [ ] **Step 7: Commit the runtime behavior changes**

```bash
git add config/cradio.conf local-modules/cradio-led-indicator/Kconfig local-modules/cradio-led-indicator/leds.c
git commit -m "Implement delayed runtime battery LED tracking"
```

### Task 3: Document The Shipped LED Behavior And Verify Both Halves

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update `README.md` with the final LED meanings**

Document these exact user-facing rules:

```md
## nice!nano blue LED indicator

On boot, the blue LED shows BLE status once:
- Connected profile `n`: `n` longer blinks
- Open or advertising profile `n`: `n` short blinks
- Disconnected profile `n`: `n` sparse blinks

After 20 seconds, the LED switches to battery-only tracking:
- `41%-100%`: no blinking
- `21%-40%`: slower repeating warning blink
- `0%-20%`: faster repeating warning blink
```

- [ ] **Step 2: Build both Cradio targets with the local module path**

Run:

```bash
west build -s zmk/app -d build/final-left -p always -b nice_nano_v2 -- \
  -DSHIELD=cradio_left \
  -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/local-modules/cradio-led-indicator"

west build -s zmk/app -d build/final-right -p always -b nice_nano_v2 -- \
  -DSHIELD=cradio_right \
  -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD/local-modules/cradio-led-indicator"
```

Expected: PASS for both halves.

- [ ] **Step 3: Run final diff hygiene checks**

Run:

```bash
git diff --check
git status --short
```

Expected: no whitespace errors and only intended tracked files changed.

- [ ] **Step 4: Commit the README update**

```bash
git add README.md
git commit -m "Document Cradio LED indicator behavior"
```

