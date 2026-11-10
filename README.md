# zmk-config-ferris

## nice!nano blue LED battery indicator

This config uses the nice!nano v2 onboard blue user LED on GPIO `P0.15` as the `indicator-led` for a repo-local LED indicator module.

Each half shows its own battery level once when powered on. Each fast blink is 100 ms on followed by 100 ms off:

| Battery level | Blinks |
| --- | --- |
| 100% | 10 |
| 90–99% | 9 |
| 80–89% | 8 |
| 70–79% | 7 |
| 60–69% | 6 |
| 50–59% | 5 |
| 40–49% | 4 |
| 30–39% | 3 |
| 20–29% | 2 |
| 10–19% | 1 |
| 0–9% or unavailable | 0 |

At startup, a zero reading is retried for up to one second to allow the battery sample to become available. If it remains zero, no blinks are shown.

After that single sequence, the LED stays off. Periodic battery warnings are removed; Bluetooth and layer-change indications are disabled. Ordinary idle-to-active transitions do not repeat the sequence. A new boot, including a reset or wake from deep sleep that reboots ZMK, shows it again.

Idle and sleep states turn the LED off and discard pending blinks without replaying them on activity resume. This controls the blue user LED, not the hardware charging LED.
