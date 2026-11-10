# Changelog

## v0.5.0 - 2026-11-10

### Changed

- Replaced periodic battery warnings and startup Bluetooth indications with one fast battery-level blink sequence per half at power-on: 10 blinks at 100%, 9 at 90–99%, down to 1 at 10–19% and none below 10% or when unavailable.
- Removed runtime battery LED tracking and its configuration settings. The LED stays off after the startup sequence and does not repeat on ordinary activity resume.
- Updated the README with blink counts, timing, and startup behavior.

## v0.4.1 - 2026-09-03

### Changed

- Changed sleep to 30 mins

## v0.4.0 - 2026-06-23

### Fixed

- Stopped the left split central from continuing BLE split scans after all split peripherals disconnect while the keyboard is idle.

## v0.3.1 - 2026-06-21

### Fixed

- Shortened stuck-key protected sleep from 10 minutes to 60 seconds.
- Sampled `kscan0` GPIO inputs before protected soft-off so physical key state decides whether the board sleeps.
- Configured held wake inputs to wake on release, avoiding repeated wake loops while a case-held key remains pressed.
- Stopped indicator LED blink output while ZMK activity state is idle or sleep.
