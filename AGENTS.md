# AGENTS.md - MAX31865 Production Embedded Guidelines

## PlatformIO

Before editing, fetch remotes and fast-forward the newest intended working
branch to its upstream. Stop and report dirty, divergent, or conflicted state;
never overwrite work to force a sync.

On Windows, use `.\scripts\pio.cmd <arguments>`; it selects the current user's
VS Code-managed installation. Never install another PlatformIO Core. If the
wrapper cannot find it, stop and report the missing installation.

## Role and target

Build a production-grade MAX31865 RTD-to-digital converter library.

- Targets: ESP32-S2/ESP32-S3 Arduino examples through exact-pinned PlatformIO,
  and ESP-IDF consumers through a framework-neutral core component with
  application-supplied callbacks.
- Model: deterministic managed synchronous driver, explicit status and health,
  application-owned scheduling, serialization, and recovery policy.
- Goals: stable API contracts, portability, long-term reliability, and no
  hidden ownership or timing surprises.

These rules are binding.

## Repository model

```text
include/MAX31865/          Public API headers only
  MAX31865.h               Main synchronous driver API
  Config.h                 Device/RTD/begin configuration and defaults
  Status.h                 Error, status, sample, fault, and health types
  Transport.h              Framework-neutral SPI/GPIO/timing callbacks
  ArduinoBackend.h         Guarded Arduino adapter
  Protocol.h               Framework-neutral protocol helpers
  CommandTable.h           Audited register/bit/timing constants
  Version.h                Generated; never edit manually
src/
  MAX31865.cpp             Managed driver and sequencing
  MAX31865_Protocol.cpp    Protocol helper implementation
  platform/arduino/        Arduino-only SPI/GPIO/timing backend
examples/
  01_basic_bringup/
  02_continuous_sampling/
  03_one_shot/
  04_fault_diagnostics/
  05_rtd_configuration/
  06_diagnostic_cli/       Self-contained diagnostic console
  common/                  Example-only helpers for examples 01-05
test/                      Native model and strict host tests
tools/                     Static, package, and native validation gates
CMakeLists.txt             Core-only ESP-IDF component registration
idf_component.yml          Core-only ESP-IDF component metadata
```

- `examples/common/` is not library code.
- Public headers remain under `include/MAX31865/`; private code remains under
  `src/`.
- No board pin defaults, global bus, task, logging, retry, or safety policy in
  library code.
- Wiring and SPI ownership live in the application transport or guarded
  Arduino adapter.
- Keep the layout boring and predictable.

## Core engineering rules

- No unbounded wait or retry. Every blocking helper has one finite deadline.
- Managed synchronous lifecycle: `begin(config)`, configuration/register APIs,
  `startContinuous()`, `triggerSingleConversion()`, `stop()`, `poll()`,
  `readSample()`, `readSingle(timeoutMs)`, `recover()`, and `end()`.
- `tick(nowMs)` performs only zero-I/O elapsed-time bookkeeping.
- No driver-owned task, ISR bridge, ring buffer, scheduler, hidden RTOS
  dependency, or steady-state heap allocation.
- No logging in library code; examples may log.
- Public/core headers and core sources must not include Arduino, ESP-IDF, or
  FreeRTOS headers.
- Public APIs are not ISR-safe and instances are not internally thread-safe.
  Applications serialize each instance and every shared SPI bus.
- Fallible public APIs return `MAX31865Status`; do not add bool/error-side-
  channel compatibility APIs.
- Validation, precondition, `NoData`, and ordinary not-ready results are not
  transport/device health failures.

## SPI ownership and transport

- The core never owns or configures an SPI host.
- The driver owns MAX31865 chip framing, CS timing, register images,
  conversion/fault sequencing, sample freshness, and decoded device faults.
- SPI transfer, CS, optional DRDY, clocks, sleep, microsecond delay, and optional
  finite bus locking live behind `MAX31865Transport`.
- The callback table is copied; `user` and all reachable resources are borrowed
  and must outlive the binding.
- Bus lock callbacks are either both null or both present and receive the
  remaining whole-operation budget. They must use the same arbiter as every
  other client of that SPI host.
- SPI/CS/GPIO failures, lock timeouts, conversion timeouts, verification
  failures, and device faults update status and health through common paths.
- Native Arduino/ESP-IDF/FreeRTOS error details must be mapped to
  `MAX31865Error`, never exposed through the core API.

## Framework boundary

- Arduino types are allowed only in `ArduinoBackend.h`,
  `src/platform/arduino/`, and Arduino examples.
- The repository supplies no native ESP-IDF backend or example. ESP-IDF
  applications use the core-only component and implement application-owned
  callbacks for SPI, CS/DRDY, time, sleep, delay, and bus arbitration.
- `CMakeLists.txt` registers only `MAX31865.cpp` and
  `MAX31865_Protocol.cpp`; it must not require peripheral or RTOS components.
- Repository examples use Arduino, advance cooperative work from `loop()`, and
  build through exact-pinned PlatformIO environments.

## Status and health

```cpp
struct MAX31865Status {
    MAX31865Error code;
    const char* msg; // static storage only
    int32_t detail;
};
```

- No exceptions and no silent failure.
- `probe()` performs raw read-only register consistency checks and preserves
  health counters and last-operation status.
- `recover()` is explicit, tracked, and never restarts acquisition.
- Health is `UNINIT`, `READY`, `DEGRADED`, or `OFFLINE`. A tracked success
  clears the failure streak; tracked failures reach
  `MAX31865Health::offlineThreshold` before threshold-based `OFFLINE`.
- A lifecycle `Fault` is immediately `OFFLINE` because hardware/configuration
  authority is lost.
- Lifetime counters saturate intentionally.

## MAX31865 requirements

- Support measured/custom RREF and RTD R0, PT100/PT1000 profiles, and custom
  finite monotonic Callendar-Van Dusen coefficients/domains.
- Support 2-/3-/4-wire modes and 50/60 Hz filters.
- Support VBIAS, bounded one-shot and continuous conversions, authoritative
  stop semantics, wired DRDY, and bounded elapsed-time readiness without DRDY.
- Decode the 15-bit RTD code and every documented fault bit.
- Support raw/ohm/Celsius thresholds, explicit latch clear, and fresh automatic
  and correctly timed manual fault cycles.
- Keep settings reads acquisition-neutral; document that RTD register reads
  and full dumps acknowledge DRDY.
- Provide typed diagnostic register access without encouraging application
  bypass of managed configuration APIs.
- Track sample/frame/no-data/drop/overrun, transport, timeout, and fault
  observations with explicit validity.

## Evidence honesty

- Never claim hardware validation without dated repository evidence or
  user-supplied logs.
- Distinguish native tests, target compilation, static ESP-IDF component
  readiness, package-consumer checks, and actual board/HIL runs.
- Examples and the CLI are diagnostics, not machine-safety implementations.
- Calibration persistence, filtering, units, limits, retries, alarms, motion
  interlocks, and actuator policy belong above this library.

## Versioning and releases

`library.json` is the version source of truth; `Version.h` is generated.

- MAJOR: breaking API/configuration/enum changes.
- MINOR: backward-compatible features or error codes.
- PATCH: fixes, refactors, and documentation.

Release steps:

1. Update `library.json` and regenerate `Version.h`.
2. Keep prospective work under `CHANGELOG.md` `[Unreleased]`.
3. Update README, Doxygen, guides, examples, tests, and package manifest with
   behavior changes.
4. Run strict native/static/sanitizer/package/Doxygen gates and every
   ESP32-S2/S3 example build.
5. Complete the required HIL matrix, retain dated evidence, and obtain explicit
   acceptance of the hardware results.
6. Only then create a dated changelog section, commit/tag a release, and publish
   with explicit authorization.

## Naming conventions

- Members: `_camelCase`.
- Methods, locals, parameters, and config fields: `camelCase`.
- Preserve existing register-style constants and public enum mappings; prefer
  `static constexpr` for new C++ constants.
