# Architecture Diff

## Summary

Enable the Cradio central half to fetch the peripheral half battery level and proxy it to BLE hosts.

## Diagram

```mermaid
flowchart LR
    Right[cradio_right peripheral] -->|Battery Level over split BLE| Left[cradio_left central]
    Left -->|Primary and proxied BAS levels| Host[BLE host]
```

## Changes

### Added

- `config/cradio_left.conf`: Enables peripheral battery fetching and proxying only on the central half.
