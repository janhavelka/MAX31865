# MAX31865 Unification Standard

This repository follows the shared structure used by the sensor-driver
libraries while keeping MAX31865-specific diagnostics visible.

## Repository Contract

- Root-level PlatformIO library package:
  - `library.json`
  - `include/`
  - `src/`
- One canonical public RTD driver header: `include/MAX31865/MAX31865.h`.
- Support headers live under `include/MAX31865/`.
- Main operator example: `examples/01_basic_bringup_cli`.
- Public API compile/link smoke example: `examples/02_api_smoke`.
- Example-only helpers live under `examples/common/*`.
- Version metadata is generated from `library.json` by
  `scripts/generate_version.py`.

## Public API Contract

The driver API must expose:

- typed begin config structs
- lifecycle state and last-error reporting
- a health snapshot with conversion, sample, SPI, fault, and timing counters
- manual reads (`readSingle()`, `poll()`, `readIfReady()`)
- continuous conversion start/stop
- decoded settings snapshots (`MAX31865Settings`)
- register read/write helpers
- safe register reset and readback-test helpers
- conversion helpers for raw code, resistance, temperature, and thresholds
- MAX31865 fault decoding, clearing, and manual/automatic fault cycles
- RTD/reference/coefficient configuration helpers

The core driver owns the MAX31865 protocol. Application code may consume
samples and diagnostics, but should not issue raw register transactions
directly unless it is using the explicit register helper APIs for diagnostics.

## CLI Contract

The bringup CLI must expose these baseline operator commands:

- `help`
- `scan`
- `probe`
- `recover`
- `drv`
- `read`
- `cfg` or `settings`
- `verbose 0|1`
- `stress [N]`
- `health` or `state`
- `id`
- `comms`
- `reset`
- `regs`, `rreg`, `rregs`, `wreg`, and `wregv`
- `selftest`
- `stats`
- `sps [ms]`

MAX31865-specific register, threshold, fault-cycle, filter, wiring, bias, and
streaming commands are expected.

The health/state output must include driver state, last error, conversion
state, sample counters, SPI errors, lock timeouts, reference alarms, DRDY
timeouts, fault status, and last sample age. Result lines should use the shared
colored CLI helpers from `examples/common/CliStyle.h`.

## Quality Gates

- Build the bringup example for ESP32-S3 and ESP32-S2.
- Build the public API smoke example.
- Run `tools/check_cli_contract.py`.
- Run `tools/check_core_timing_guard.py`.
- Validate `library.json`.
- Keep `git diff --check` clean.

## Portability Rule

Arduino is the primary runtime today. ESP-IDF portability must be preserved by
keeping Arduino SPI/GPIO/timing calls concentrated in driver glue and by
leaving the public MAX31865 API stable. Portability-only work must not regress
current Arduino behavior.
