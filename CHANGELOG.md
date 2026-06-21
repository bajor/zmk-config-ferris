# Changelog

## v0.3.1 - 2026-06-21

### Fixed

- Shortened stuck-key protected sleep from 10 minutes to 60 seconds.
- Sampled `kscan0` GPIO inputs before protected soft-off so physical key state decides whether the board sleeps.
- Configured held wake inputs to wake on release, avoiding repeated wake loops while a case-held key remains pressed.
