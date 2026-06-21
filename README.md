# zmk-config-ferris

## nice!nano blue LED battery indicator

This config uses the nice!nano v2 onboard blue user LED on GPIO `P0.15` as the `indicator-led` for a repo-local LED indicator module.

On boot, the blue LED shows BLE status once:
- Connected active profile `n`: `n` longer blinks.
- Open or advertising active profile `n`: `n` short blinks.
- Disconnected active profile `n`: `n` sparse blinks.
- On a peripheral half, connected shows one longer blink and disconnected shows sparse blinks during the startup indication.

After 20 seconds, the LED switches to battery-only tracking:
- `41%-100%`: no blinking.
- `21%-40%`: slower repeating warning blinks.
- `1%-20%`: faster repeating warning blinks.
- `0%`: treated as unknown, so the LED stays off until the next battery check.

The LED only blinks while ZMK activity state is active. Idle and sleep states turn the LED off and drop pending blink work.

Layer-change blinks are disabled, so after the startup BLE indication the blue LED is reserved for active-state battery warnings.
