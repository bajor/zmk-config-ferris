# Changelog

## v0.4.0 - 2026-06-23

### Fixed

- Stopped the left split central from continuing BLE split scans after all split peripherals disconnect while the keyboard is idle.

## v0.3.1 - 2026-06-21

### Fixed

- Shortened stuck-key protected sleep from 10 minutes to 60 seconds.
- Sampled `kscan0` GPIO inputs before protected soft-off so physical key state decides whether the board sleeps.
- Configured held wake inputs to wake on release, avoiding repeated wake loops while a case-held key remains pressed.
- Stopped indicator LED blink output while ZMK activity state is idle or sleep.
