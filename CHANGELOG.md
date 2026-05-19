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
- ESP-IDF component metadata and a basic `spi_master` example.

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

### Removed
- Old root-level public header layout.

## [0.1.2] - 2026-04-29

### Added
- Initial MAX31865 library implementation aligned with the unified
  ADS1261-style repository contract.

[Unreleased]: https://github.com/janhavelka/MAX31865/compare/v0.1.2...HEAD
[0.1.2]: https://github.com/janhavelka/MAX31865/releases/tag/v0.1.2
