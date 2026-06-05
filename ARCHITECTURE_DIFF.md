# Architecture Diff

## Summary

Adds a per-half stuck-key protected sleep path that soft-offs only after the local pressed-key set remains unchanged for 10 minutes.

## Diagram(s)

```mermaid
flowchart TD
    A[Local position events] --> B[Pressed-key set tracker]
    B --> C{Any local key held?}
    C -->|No| D[Cancel protected timer]
    C -->|Yes| E[Reset 10-minute protected timer]
    E --> F{Timer fires with USB power?}
    F -->|Yes| E
    F -->|No| G[zmk_pm_soft_off]
    G --> H[cradio,stuck-key-wakeup device]
    H --> I[Read kscan0 GPIOs]
    I --> J[Enable wake on non-held keys]
    I --> K[Disable wake on held keys]
```

## Changes

### Added

- `cradio,stuck-key-wakeup`: soft-off wake device that reads `kscan0` GPIO state and only arms non-held keys.
- `CONFIG_CRADIO_STUCK_KEY_SLEEP`: module option that enables the protected timer and wake device source.
- `Makefile`: local `make test` target that builds both Ferris halves.

### Modified

- `config/cradio.conf`: normal idle sleep is 11 minutes, protected stuck-key sleep is 10 minutes, and ZMK soft-off support is enabled.
- `config/cradio.overlay`: registers the custom wake device under `zmk,soft-off-wakeup-sources`.
- `local-modules/cradio-led-indicator`: adds the pressed-key tracker, protected timer, USB guard, and devicetree binding.
