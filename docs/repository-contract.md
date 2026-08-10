# Repository contract

## Layout

- `include/MAX31865/`: installed framework-neutral public API plus the guarded
  Arduino adapter header.
- `src/MAX31865.cpp`: managed synchronous state machine.
- `src/MAX31865_Protocol.cpp`: canonical protocol codec.
- `src/platform/arduino/`: Arduino-only borrowed transport adapter.
- `examples/01_basic_bringup` through `examples/06_diagnostic_cli`: focused
  Arduino consumers.
- `examples/common/`: example-only board/status helpers; not installed API.
- `test/`: native model, protocol, deadline, fault, conversion, API, Arduino
  backend, and consumer-neutral firmware-owner fixture tests.
- `tools/` and `scripts/`: reproducible validation/version helpers.
- `tools/package_manifest.txt`: frozen, sorted installed-release payload.
- `CMakeLists.txt` and `idf_component.yml`: core-only ESP-IDF component metadata.

`library.json` is the version source of truth. `Version.h` is generated and must
not be edited directly. Public includes must not depend on repository-relative
private headers. Package validation requires the source groups and archive to
match the manifest exactly, rejects unsafe archive member types/paths, and
builds clean native plus installed Arduino consumers, including the copied
self-contained diagnostic CLI on ESP32-S2/S3.

## Core contract

- caller-owned SPI and pins; no global bus or board defaults in library code;
- borrowed callback lifetime and explicit external serialization;
- bounded synchronous operations and no hidden retry/recovery;
- explicit `begin`, `tick`, `stop`, and zero-I/O `end` lifecycle;
- status-returning fallible APIs and zero-I/O passive health;
- desired/observed register-image verification;
- one-sample freshness, typed MAX31865 fault handling, and CVD conversion;
- no library logging, task, queue, ISR bridge, or steady-state allocation.

## Example contract

The first five examples are intentionally small and share fixed-buffer status
formatting. The diagnostic CLI is standalone. Example wiring is visibly
example-only. Every blocking driver call has an explicit finite timeout, every
status is checked, and no example introduces dynamic `String` processing.

## External owner contract

The application-owned integration fixture publishes a fixed, trivially
copyable `FirmwareRtdSample` rather than exposing the driver or transport to
downstream consumers. Its instance mutex, shared-SPI arbiter, outer deadlines,
publication decision, status mapping, and explicit recovery policy remain test
consumer code. Static validation prevents those `Firmware*` types or ownership
policies from leaking into the core.

## Change acceptance

A public behavior change updates Doxygen, README/guides, examples, native
tests, and CHANGELOG together. Register/timing changes cite the current official
data sheet. Release acceptance requires strict native gates, both sanitizer
runs, ESP32-S2/S3 builds for all examples, package-consumer checks, warning-clean
Doxygen, and reviewed HIL evidence appropriate to the behavior changed.
