# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Canonical public driver header `MAX31865/MAX31865.h`.
- Split public support headers `MAX31865/Config.h` and
  `MAX31865/Status.h`.
- Typed setup API: `MAX31865BeginConfig`.
- Driver supervision APIs: `MAX31865State`, `MAX31865DriverState`,
  `MAX31865Error`, `MAX31865Status`, `MAX31865Health`,
  `max31865StateName()`, `max31865DriverStateName()`, and
  `max31865ErrorName()`.
- Health counters for SPI errors, DRDY timeouts, read/kept/drop counts, last
  sample age, and success/failure tracking.
- Manual acquisition APIs: blocking `readSingle()`, nonblocking `poll()`, and
  one-sample cache access.
- Register helpers: `readReg()`, `readRegs()`, `writeReg()`,
  `writeRegVerify()`, and `dumpRegisters()`.
- Production diagnostics: `readIfReady()`, `available()`, `getSettings()`,
  `getSettingsStatus()`, `resetRegisters()`, `registerReadbackTest()`, SPI lock
  timeout controls, RTD coefficient configuration, and
  `MAX31865/CommandTable.h`.
- `isInitialized()`, `MAX31865Status::isOk()`, and `MAX31865Status::inProgress()` helpers.
- MAX31865 conversion helpers for raw ADC code, resistance, temperature, and
  threshold conversion.
- Fault latch decoding, clear, automatic fault cycle, and manual fault cycle
  helpers.
- CLI `faultdecode <raw>` for offline decoding of captured fault-status bytes.
- ADS1261-style bringup CLI, shared example helper framework, API smoke
  example, and PlatformIO environments `ex_bringup_s3`, `ex_bringup_s2`, and
  `ex_api_smoke_s3`.
- Native PlatformIO/Unity test scaffold and `native` validation environment.
- Minimal Doxygen configuration for public API documentation.
- Root `AGENTS.md` production guidelines for future driver work.
- `MAX31865TransportConfig` callback backend for application-owned SPI, DRDY,
  timing, delay, and yield hooks.
- `MAX31865/Transport.h` for the framework-neutral transport contract and
  `MAX31865/Core.h` as a clean core contract include path.
- `MAX31865Core` pure-core alias from `MAX31865/Core.h` for non-Arduino
  consumers using the transport-backed driver path without duplicate state.
- Optional paired backend lock/unlock callbacks in `MAX31865TransportConfig`.
- Migration notes for the begin/transport split.
- ESP-IDF component metadata and a basic `spi_master` example.
- `scripts/check_idf_example_contract.py` to guard the native ESP-IDF example
  against Arduino compatibility facades and missing IDF dependencies.
- `src/MAX31865Arduino.cpp` as the guarded Arduino compatibility implementation
  source.

### Changed
- Public API now follows the I2C library layout: nested canonical header,
  split config/status headers, global device class,
  device-prefixed types, typed begin config, health snapshot, and C-style name
  helpers.
- Reference documentation now separates compact RTD-converter notes from full PDF/application-note extractions under `docs/extracted-md/` and `docs/pdf-extracted-md/`.
- Configuration setters preserve cached state when register writes fail, probe
  uses writable-register readback/restore, and failed initialization releases
  allocated driver resources.
- Repeated or failed `begin()` now resets runtime state, resources, counters,
  cached samples, and settings readback so `isInitialized()` accurately reflects
  the active session.
- README and support docs now describe protocol ownership, repository layout,
  validation gates, and ESP-IDF portability expectations.
- Arduino SPI/GPIO fallback code is compile-guarded so the transport backend can
  build without Arduino headers.
- `MAX31865BeginConfig` is now framework-neutral and direct `begin(config)` is
  reserved for transport-backed callers with explicit timing hooks.
- Arduino typed begin now uses the guarded `begin(SPIClass&, config)`
  compatibility overload.
- Core/source path no longer owns a FreeRTOS semaphore; optional bounded bus
  serialization is now transport `lock`/`unlock` callback-owned.
- `lastOperationStatus()` now preserves transport callback status messages and
  details for transfer and lock failures.
- Arduino SPI/GPIO/timing fallback definitions moved out of the main protocol
  source; `src/MAX31865.cpp` remains the transport-backed core path and builds
  with `MAX31865_HAS_ARDUINO_BACKEND=0`.
- Doxygen now generates local ignored HTML under `docs/generated/`, matching
  repository documentation.
- `MAX31865.h` no longer includes Arduino/SPI framework headers transitively;
  Arduino sketches should rely on the normal framework prelude or include
  `<SPI.h>` before using `SPIClass` directly.
- `MAX31865/Core.h` now exposes the transport-backed driver entry point, not
  only support contracts. It rejects already-enabled Arduino backend mode so
  the pure-core include cannot accidentally grow compatibility overloads.

### Removed
- `MAX31865BeginConfig::spi`; Arduino callers should pass the SPI bus to
  `begin(SPI, config)` or use the compact positional overload.
- Old root-level public header layout.
- Root ESP-IDF component dependency on `freertos`; FreeRTOS remains only in
  platform/example code that directly calls its APIs.

## [0.1.2] - 2026-04-29

### Added
- Initial MAX31865 library implementation aligned with the unified
  ADS1261-style repository contract.

[Unreleased]: https://github.com/janhavelka/MAX31865/compare/v0.1.2...HEAD
[0.1.2]: https://github.com/janhavelka/MAX31865/releases/tag/v0.1.2
