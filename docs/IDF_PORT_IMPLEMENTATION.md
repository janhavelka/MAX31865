# MAX31865 ESP-IDF Port Implementation

Date: 2026-05-25

## Implemented

- Added `MAX31865TransportConfig` with callbacks for full SPI register-frame
  transfer, optional DRDY read, millisecond time, millisecond delay,
  microsecond delay, cooperative yield, and optional bounded lock/unlock.
- Moved the transport contract to `MAX31865/Transport.h` and added
  `MAX31865/Core.h` as the clean framework-neutral driver include path.
- `MAX31865/Core.h` exposes `MAX31865Core` as an alias over the transport-backed
  driver implementation, giving non-Arduino consumers a pure-core type name
  without duplicate protocol or health state.
- `begin(const MAX31865BeginConfig&)` is now the application-owned transport
  path. Arduino bus ownership uses the guarded
  `begin(SPIClass&, const MAX31865BeginConfig&)` compatibility overload.
- Register I/O now routes through the transport callback when supplied, while
  preserving the Arduino SPI fallback for existing sketches.
- Guarded Arduino compatibility overloads and fallback SPI/GPIO/timing/DRDY
  functions now live in `src/MAX31865Arduino.cpp`; the main source remains the
  transport-backed protocol/register/RTD/fault/health path.
- DRDY, timing, delay, and yield behavior are callback-owned when supplied.
- Transport builds now reject missing `nowMs`, `delayMs`, and `delayUs` hooks
  during `begin()` and bounded polling loops have a stalled-time
  guard so a non-advancing clock cannot spin forever.
- `MAX31865BeginConfig::spi` was removed so `Config.h` no longer needs Arduino
  SPI declarations, and `<SPI.h>` ambient include detection was removed.
- `MAX31865.h` no longer exposes FreeRTOS headers or `SemaphoreHandle_t`; the
  former private semaphore implementation has been removed from
  `src/MAX31865.cpp`.
- Arduino headers and SPI/GPIO fallback code are compile-guarded behind
  `MAX31865_HAS_ARDUINO_BACKEND`.
- Root ESP-IDF component metadata no longer requires `freertos`; FreeRTOS use
  remains isolated to the native ESP-IDF example adapter.
- Transport `transfer` and `lock` callback failures now preserve the callback
  status message and detail through `lastOperationStatus()`.
- Added root ESP-IDF component metadata and a basic ESP-IDF SPI example.
- Added `scripts/check_idf_example_contract.py` to statically verify that the
  IDF example uses native IDF SPI/GPIO/timing APIs and no Arduino compatibility
  facade.

## Remaining Blockers

- `MAX31865.h` is still a compatibility driver header and still declares
  Arduino overloads when `MAX31865_HAS_ARDUINO_BACKEND` is enabled. A true
  separate `MAX31865Core` class plus backend wrapper objects remains future
  work; the current release-candidate step uses an alias to avoid duplicate
  state.
- A standalone Arduino backend with explicit bounded locking has not been
  introduced. Applications that require bounded bus serialization should use
  the transport backend and provide `lock`/`unlock`.
- The ESP-IDF SPI adapter remains example-local. It should not be promoted to a
  reusable public adapter until ESP-IDF builds and hardware validation are run.
- The ESP-IDF example still needs to be built with a sourced ESP-IDF toolchain;
  `idf.py` was not available in this shell.
- Hardware validation is required for PT100/PT1000 wiring modes, DRDY behavior,
  and fault-cycle behavior.
- Native fake-transport tests should be expanded to cover ESP-IDF-style
  transfer failure mapping and DRDY callback readiness.
