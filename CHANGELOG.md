# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Canonical public driver header `MAX31865.h`.
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
- MAX31865 conversion helpers for raw ADC code, resistance, temperature, and
  threshold conversion.
- Fault latch decoding, clear, automatic fault cycle, and manual fault cycle
  helpers.
- ADS1261-style bringup CLI, shared example helper framework, API smoke
  example, and PlatformIO environments `ex_bringup_s3`, `ex_bringup_s2`, and
  `ex_api_smoke_s3`.
- Minimal Doxygen configuration for public API documentation.

### Changed
- Public API now follows the ADS1261 sibling style: global device class,
  device-prefixed types, typed begin config, health snapshot, and C-style name
  helpers.
- README and support docs now describe protocol ownership, repository layout,
  validation gates, and ESP-IDF portability expectations.

### Removed
- Early native-only test scaffold and split namespaced public headers.

## [0.1.2] - 2026-04-29

### Added
- Initial MAX31865 library implementation aligned with the unified
  ADS1261-style repository contract.

[Unreleased]: https://github.com/janhavelka/MAX31865/compare/v0.1.2...HEAD
[0.1.2]: https://github.com/janhavelka/MAX31865/releases/tag/v0.1.2
