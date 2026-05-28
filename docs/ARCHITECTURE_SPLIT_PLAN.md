# MAX31865 Architecture Split Plan

This is a planning document plus staged cleanup log. It records historical and
remaining framework leaks and a staged split into a framework-neutral MAX31865
core plus Arduino and native ESP-IDF backends.

## Historical Leaks And Evidence

- The main public driver header used to require FreeRTOS through
  `freertos/FreeRTOS.h` and `freertos/semphr.h`.
- Arduino is conditionally included from the public driver header: `include/MAX31865/MAX31865.h:25-27` includes `<Arduino.h>` when `MAX31865_HAS_ARDUINO_BACKEND` is true.
- The compact Arduino begin overload is visible on the main public class: `include/MAX31865/MAX31865.h:63-69` declares `begin(SPIClass&, ...)`.
- The public class layout used to expose framework implementation details
  through private members in the public header by storing `SPIClass*` and
  `SemaphoreHandle_t`.
- `include/MAX31865/Config.h` used to derive `MAX31865_HAS_ARDUINO_BACKEND`
  from ambient `<SPI.h>` availability. Arduino backend selection now depends on
  `ARDUINO` or an explicit compile definition.
- `MAX31865BeginConfig` used to mix Arduino and transport configuration by
  storing `SPIClass* spi` beside `MAX31865TransportConfig transport`. The SPI
  object is now supplied only through guarded Arduino overloads.
- The core implementation used to create a FreeRTOS semaphore during `begin()`.
- The core implementation used to perform Arduino bus/pin setup when no
  transport was supplied. That setup now lives in
  `src/MAX31865Arduino.cpp`.
- Timing fallbacks used to call Arduino runtime APIs from the main source.
  Those fallback definitions now live in `src/MAX31865Arduino.cpp`.
- Native ESP-IDF support currently lives in the example, not the library backend surface: `examples/esp_idf/basic/main/Max31865IdfSpi.h:5-12` exposes IDF SPI/GPIO types and `examples/esp_idf/basic/main/Max31865IdfSpi.cpp:11-24` maps `esp_err_t` to `MAX31865Status`.
- The ESP-IDF component previously required `freertos` only to satisfy the
  private semaphore leak.

## Phase 4 Cleanup Stage - 2026-05-25

Implemented in this stage:

- Removed FreeRTOS includes, semaphore creation/deletion, and
  `xSemaphoreTake`/`xSemaphoreGive` from `src/MAX31865.cpp`.
- Removed the private `_spiMutex` storage from `MAX31865`. Internal bus
  serialization is now provided only by optional `MAX31865TransportConfig`
  `lock`/`unlock` callbacks. The guarded Arduino compatibility path uses
  `SPIClass::beginTransaction()`/`endTransaction()` but no library-owned
  FreeRTOS mutex.
- Removed the root component `freertos` requirement from `CMakeLists.txt`.
  Platform-specific examples/backends that call FreeRTOS APIs still declare
  `freertos` in their own component metadata.
- Added stored last-operation status detail so transport `transfer` and
  `lock` callback failures preserve their returned `msg` and `detail` through
  `lastOperationStatus()`, while the existing `bool` compatibility APIs remain.
- Extended `tools/check_core_timing_guard.py` to reject FreeRTOS/semaphore
  tokens in the core source path and root component metadata.

Remaining after this stage:

- The public class is still `MAX31865`; a separate `MAX31865Core` class and
  backend wrapper objects have not been introduced.
- `src/MAX31865.cpp` still contains guarded calls into the Arduino compatibility
  backend, but Arduino runtime API use has moved out of the main source. Full
  class-level split into `MAX31865Core` plus wrapper objects remains future
  work.
- A production Arduino backend with explicit bounded lock callbacks has not
  been introduced. Applications that need bounded bus serialization should use
  the transport backend and supply `lock`/`unlock`.

## Phase 5 Source Split Step - 2026-05-25

Implemented in this stage:

- Added `src/MAX31865Arduino.cpp` for guarded Arduino compatibility overloads
  and fallback SPI/GPIO/timing/DRDY functions.
- Left register protocol, RTD math, fault decode, conversion state, and health
  tracking in `src/MAX31865.cpp`, which now builds as the clean transport/core
  path with `MAX31865_HAS_ARDUINO_BACKEND=0`.
- Updated the root ESP-IDF component source list to include both source files;
  the Arduino source compiles to an empty translation unit when the backend is
  disabled.
- Pointed Doxygen output at the ignored `docs/generated/` directory that the
  repository documents, then regenerated local HTML.

Remaining after this stage:

- The public implementation class is still `MAX31865`; Phase 6 adds a
  `MAX31865Core` alias rather than a second wrapper class to avoid duplicate
  state.
- `MAX31865.h` still declares guarded Arduino overloads for source
  compatibility.
- The ESP-IDF adapter remains example-local rather than a promoted library
  backend.

## Phase 6 Pure-Core Facade Step - 2026-05-25

Implemented in this stage:

- `include/MAX31865/Core.h` now includes the transport-backed driver with
  `MAX31865_HAS_ARDUINO_BACKEND=0` and exposes `using MAX31865Core = MAX31865`.
- The pure-core type is intentionally an alias over the existing implementation
  rather than a wrapper. This gives non-Arduino consumers a clean driver type
  name without duplicate protocol/register/health state.
- Native tests now include `MAX31865/Core.h`, instantiate `MAX31865Core`, and
  statically assert that it adds no storage over `MAX31865`.
- `tools/check_core_timing_guard.py` now fails if `Core.h` stops exposing the
  transport-backed driver include or the `MAX31865Core` alias.
- The ESP-IDF SPI adapter remains example-local for this release-candidate
  step. It has not been promoted into a public backend because `idf.py` builds
  and hardware validation were not available in this workspace.

Remaining after this stage:

- A true separate `MAX31865Core` class plus `MAX31865` compatibility wrapper is
  still future work and should be done only if it removes remaining API debt
  without duplicating state.
- The guarded Arduino overload declarations still live in `MAX31865.h` for
  source compatibility.
- The ESP-IDF adapter is still example-local.

## Phase 3 Hardening Stage - 2026-05-25

Implemented in this stage:

- Added `include/MAX31865/Transport.h` as the framework-neutral transfer,
  DRDY, timing, yield, and optional lock/unlock callback contract.
- Added `include/MAX31865/Core.h` as the clean core contract include path. It
  includes only command, config, status, and transport headers and is guarded by
  static checks against Arduino, SPI, FreeRTOS, and ESP-IDF framework tokens.
- Removed `MAX31865BeginConfig::spi` and removed ambient `<SPI.h>`
  auto-detection from `Config.h`. `MAX31865BeginConfig` is now transport-first
  and direct `begin(config)` requires `transfer`, `nowMs`, `delayMs`, and
  `delayUs`.
- Kept Arduino compatibility behind `MAX31865_HAS_ARDUINO_BACKEND` with
  `begin(SPIClass&, const MAX31865BeginConfig&)` and the compact positional
  overload.
- Removed FreeRTOS includes and `SemaphoreHandle_t` from `MAX31865.h`; the
  semaphore-backed compatibility implementation still remained isolated in
  `src/MAX31865.cpp` until Phase 4 removed it.
- Fixed transport-backed `verifyProbe` by allowing `probe()` to operate when a
  transport backend is present and no Arduino SPI object exists.

Remaining after this stage:

- The public implementation class is still `MAX31865`; `MAX31865Core` is a
  pure-core alias exposed by `Core.h`, not a separate implementation class.
- `src/MAX31865.cpp` no longer contains Arduino runtime API definitions after
  the Phase 5 source split step, but it still contains guarded calls into the
  compatibility backend. Full source layout split into `src/core` and
  `src/platform/*` remains future work.
- Full source layout separation into core and backend directories remains
  future work.

## Proposed Core / Backend / API Layout

Target layout:

```text
include/MAX31865/
  Core.h                 - framework-neutral driver class and core config
  Transport.h            - framework-neutral SPI/GPIO/timing/lock callbacks
  Status.h
  Config.h               - core-only configuration and RTD math config
  CommandTable.h
  Version.h              - generated
  backends/
    ArduinoBackend.h     - ARDUINO-only SPI/GPIO/timing adapter
    IdfBackend.h         - ESP_PLATFORM-only SPI/GPIO/timing adapter
  MAX31865.h             - compatibility umbrella/wrapper only
src/
  core/
    MAX31865Core.cpp
  platform/arduino/
    MAX31865ArduinoBackend.cpp
  platform/esp_idf/
    MAX31865IdfBackend.cpp
```

Core responsibilities:

- Own MAX31865 register protocol, configuration validation, RTD resistance/temperature math, threshold conversion, fault decoding, sample cache, and health state.
- Depend only on core status/config/transport headers and standard integer/float headers.
- Receive all time, SPI, GPIO, DRDY, and locking behavior through a transport table.

Backend responsibilities:

- Arduino backend owns `SPIClass`, `SPISettings`, `/CS`, optional `/DRDY`, Arduino timing/yield, and any Arduino-specific setup.
- ESP-IDF backend owns caller-created `spi_device_handle_t`, optional GPIO setup, `esp_timer`, `esp_rom_delay_us`, FreeRTOS delays/yields, and `esp_err_t` mapping.
- Backends map every framework failure into `MAX31865Status` and never leak raw framework errors through core APIs.

Public API responsibilities:

- `Core.h` is the portable include and must compile without Arduino, ESP-IDF, or FreeRTOS headers.
- `MAX31865.h` may continue to serve Arduino sketches as a compatibility include if it only owns an Arduino backend and forwards to the core.
- ESP-IDF consumers should include the IDF backend header explicitly; examples should stop carrying a private backend once the library backend exists.

## Intentional Breaking Changes

- New fallible core APIs should return `MAX31865Status` rather than `bool`. Existing compact `bool` APIs can remain only in wrappers and must keep `lastOperationStatus()` accurate.
- `MAX31865BeginConfig` should remove `SPIClass*`; Arduino bus ownership moves to an Arduino backend config.
- Core config should not auto-detect Arduino by `__has_include(<SPI.h>)`. Backend selection should be explicit through `ARDUINO`, `ESP_PLATFORM`, or caller includes.
- The core class should not store `SemaphoreHandle_t`; transport/backends should provide bounded lock/unlock callbacks if bus serialization is needed.
- Core timing callbacks should be required for non-backend use. The core should not silently return `0` for time or no-op delays in native builds.
- The unguarded `begin(SPIClass&, ...)` declaration should move out of the portable core API and into an Arduino compatibility wrapper.

## Migration Path

- Add a new transport/backend API while leaving the current `MAX31865` class available for Arduino sketches.
- Move the example-local ESP-IDF adapter into `include/MAX31865/backends/IdfBackend.h` and `src/platform/esp_idf/`.
- For Arduino sketches, keep this migration shape:
  - old: `rtd.begin(SPI, sck, miso, mosi, cs, drdy, hz)`
  - transition: wrapper builds `MAX31865ArduinoBackend` and forwards to core
  - new: applications may explicitly create the backend and pass transport to the core
- For non-Arduino callers using `MAX31865BeginConfig::transport`, move to the new core config and provide explicit timing, lock, and transfer callbacks.
- Update `README.md`, `docs/IDF_PORT.md`, `docs/IDF_PORT_IMPLEMENTATION.md`, examples, and contract scripts in the same stage as the API move.

## Compatibility Wrappers Only If Clean

Compatibility wrappers are acceptable only when they:

- Include Arduino or ESP-IDF headers only from backend/wrapper headers.
- Own backend state privately and forward all protocol operations to the core.
- Preserve status/health counters and do not duplicate register protocol logic.
- Avoid `__has_include(<SPI.h>)` based backend selection for portable core builds.

If a wrapper requires FreeRTOS or Arduino types to appear in `Core.h`, do not add it. Treat that API as a major-version break and document the migration.

## Staged Sequence

1. Add compile/boundary tests that include core headers without Arduino, ESP-IDF, FreeRTOS, or test stubs.
2. Introduce `MAX31865Transport` with explicit transfer, CS/GPIO or frame-transfer, timing, yield, and bounded lock callbacks.
3. Partial: keep register protocol, RTD math, fault decode, and health state in
   the clean main source path; a future class rename can move it to
   `src/core/`.
4. Partial: move Arduino setup/timing into a guarded source file; a future
   backend object can move it under `src/platform/arduino/`.
5. Promote the example ESP-IDF adapter into the library backend and update the IDF example to use it.
6. Done: remove FreeRTOS semaphore ownership from the core class; use transport lock callbacks.
7. Update docs, examples, and CI scripts, then deprecate old wrapper APIs before the major release that removes them.

## Tests Required

- Header-boundary test for `Core.h`, `Config.h`, `Status.h`, `Transport.h`, and `CommandTable.h` with no Arduino/ESP-IDF include paths.
- Static boundary checks using `tools/check_core_timing_guard.py` and an updated framework-boundary check.
- Native fake-transport tests for register reads/writes, write-verify mismatch, SPI transfer failures, lock timeouts, conversion-not-ready behavior, fault decode, and health transitions.
- Math tests for ADC code ratio, resistance conversion, Callendar-Van Dusen conversion, custom coefficients, and threshold conversion.
- PlatformIO native tests currently represented by `test/test_max31865.cpp`.
- Host compile checks for `Core.h`, the backend-disabled core source path, and
  the guarded Arduino compatibility source with test stubs.
- Arduino PlatformIO builds for ESP32-S2 and ESP32-S3 examples.
- ESP-IDF example/component builds using the promoted backend.
- Contract test proving ESP-IDF examples use native APIs and do not include Arduino compatibility facades.

## Hardware Validation Sequence

1. ESP32-S3 Arduino with MAX31865 board and known PT100/PT1000 input: probe, register dump, one-shot read, resistance/temperature conversion, and version output.
2. ESP32-S2 Arduino: repeat smoke path and verify timing-sensitive one-shot behavior.
3. ESP32-S3 ESP-IDF: use native backend with caller-created SPI device; verify probe, register access, one-shot, continuous conversion, and DRDY/no-DRDY paths.
4. ESP32-S2 ESP-IDF: repeat IDF smoke path.
5. Validate 2-wire, 3-wire, and 4-wire modes; 50 Hz and 60 Hz filter settings; PT100, PT1000, and custom RTD coefficients.
6. Validate fault handling with open RTD, shorted RTD, threshold faults, REFIN high/low, RTDIN low, and over/under-voltage where the board allows.
7. Run read and conversion stress loops with transfer fault injection, lock timeout injection, and manual `recover()` checks.
8. Power-cycle and `end()` / `begin()` reuse tests to verify no stale backend or semaphore state remains.

## Generated Version.h Caveat

`include/MAX31865/Version.h` is generated from `library.json`; `include/MAX31865/Version.h:5-6` explicitly says not to edit it manually. Any architecture split release that changes config/API shape should update `library.json`, regenerate `Version.h`, and record the SemVer impact in `CHANGELOG.md`.
