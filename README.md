# zmk-config-ferris

## nice!nano blue LED battery indicator

This config uses the nice!nano v2 onboard blue user LED on GPIO `P0.15` as the `indicator-led` for `zmk-poor-mans-led-indicator`.

Battery indication behavior:
- On boot, `>= 80%` battery blinks the blue LED 2 slow times.
- On boot, `11%-20%` battery blinks the blue LED 4 medium times.
- On boot, `<= 10%` battery blinks the blue LED 6 fast times.
- On boot, `21%-79%` battery does not blink the blue LED.
- While running, battery level changes at `<= 10%` trigger a single quick blue blink.

BLE profile and layer-change blinks are disabled, so the blue LED is reserved for battery indication.
