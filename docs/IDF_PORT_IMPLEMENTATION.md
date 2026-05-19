# MAX31865 ESP-IDF Port Implementation

Date: 2026-05-19

## Implemented

- Added `MAX31865TransportConfig` with callbacks for full SPI register-frame
  transfer, optional DRDY read, millisecond time, millisecond delay,
  microsecond delay, and cooperative yield.
- `begin(const MAX31865BeginConfig&)` now accepts either the existing Arduino
  `SPIClass` path or the application-owned transport callback path.
- Register I/O now routes through the transport callback when supplied, while
  preserving the Arduino SPI fallback for existing sketches.
- DRDY, timing, delay, and yield behavior are callback-owned when supplied.
- Arduino headers and SPI/GPIO fallback code are compile-guarded behind
  `MAX31865_HAS_ARDUINO_BACKEND`.
- Added root ESP-IDF component metadata and a basic ESP-IDF SPI example.
- Added `scripts/check_idf_example_contract.py` to statically verify that the
  IDF example uses native IDF SPI/GPIO/timing APIs and no Arduino compatibility
  facade.

## Remaining Blockers

- The ESP-IDF example still needs to be built with a sourced ESP-IDF toolchain;
  `idf.py` was not available in this shell.
- Hardware validation is required for PT100/PT1000 wiring modes, DRDY behavior,
  and fault-cycle behavior.
- Native fake-transport tests should be expanded to cover ESP-IDF-style
  transfer failure mapping and DRDY callback readiness.
