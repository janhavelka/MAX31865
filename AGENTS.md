# AGENTS.md - MAX31865 Production Embedded Guidelines

## Role and Target
You are a professional embedded software engineer building a production-grade MAX31865 RTD-to-digital converter library.

- Target: ESP32-S2 / ESP32-S3, Arduino framework, PlatformIO.
- Goals: deterministic behavior, long-term stability, clean API contracts, portability, and no surprises in the field.
- These rules are binding.

---

## Repository Model (Single Library)

```
include/                  - Public API headers only (Doxygen)
  MAX31865/
    MAX31865.h            - Main Arduino C++ driver API
    Config.h              - Configuration structs/enums and defaults
    Status.h              - Error/status, sample, and health types
    CommandTable.h        - Register addresses, bit masks, protocol constants
    Version.h             - Auto-generated (do not edit)
src/                      - Implementation (.cpp)
examples/
  01_*/
  02_*/
  common/                 - Example-only helpers and CLI glue
test/                     - Native/unit tests
platformio.ini
library.json
README.md
CHANGELOG.md
AGENTS.md
```

Rules:
- `examples/common/` is NOT part of the library. It simulates project glue and keeps examples self-contained.
- No board-specific pin defaults in library code; wiring must come from `MAX31865BeginConfig` or explicit begin arguments.
- Public headers stay under `include/`; do not expose private implementation headers from `src/`.
- Keep register/protocol constants in `MAX31865/CommandTable.h`.
- Examples demonstrate usage and may use `examples/common/BoardConfig.h`.
- Keep the layout boring and predictable.

---

## Core Engineering Rules (Mandatory)

- Deterministic: no unbounded loops/waits; all waits use deadlines or bounded timeouts.
- Lifecycle is explicit: `begin(...)`, `tick(uint32_t nowMs)`, `stop()`, and `end()`.
- Any blocking operation must document its maximum wait and timeout behavior.
- No heap allocation in steady state.
- No logging in library code; examples may log.
- No board-specific assumptions in library code.
- No macros for new constants unless required for compatibility or conditional compile. Prefer `static constexpr`.

---

## SPI Ownership and Transport

- The library MUST NOT hard-code a global SPI bus. The caller supplies `SPIClass*` / `SPIClass&`.
- The library may own MAX31865 chip-level framing, `/CS`, optional `/DRDY`, and register transactions after `begin()`.
- The library must not choose board pins internally; pins come from `MAX31865Pins` or explicit begin arguments.
- SPI bus locking must be bounded by `MAX31865_SPI_LOCK_TIMEOUT_MS` or caller-configured values.
- SPI transfer failures, lock timeouts, conversion timeouts, and decoded MAX31865 faults must update `lastError()` / `lastOperationStatus()` and health counters consistently.
- Do not leak raw ESP-IDF/FreeRTOS/SPI failure details through the public API without mapping them to `MAX31865Error` or `MAX31865Status`.

---

## Status / Error Handling (Mandatory)

Existing compact Arduino APIs may return `bool`, but failure must never be silent:

```cpp
typedef struct MAX31865Status {
    MAX31865Error code;
    const char* msg;   // static string only
    int32_t detail;
} MAX31865Status;
```

- New fallible APIs should return `MAX31865Status` when practical.
- Existing `bool` APIs must set `lastError()` / `lastOperationStatus()` on every failure.
- Do not use exceptions.
- Validation/precondition failures must not be counted as transport failures.

---

## MAX31865 Driver Requirements

- Support caller-supplied SPI clock and pins.
- Support PT100/PT1000 and custom RTD nominal/reference resistor values.
- Support 2-wire, 3-wire, and 4-wire RTD modes.
- Support 50 Hz and 60 Hz notch-filter selection.
- Support VBIAS control, one-shot conversion, continuous conversion, and stop behavior.
- Support conversion-ready detection from DRDY when wired or from bounded elapsed-time polling when DRDY is absent.
- Support raw 15-bit RTD ADC decoding and fault-bit handling.
- Support resistance and temperature conversion using IEC 60751 Callendar-Van Dusen coefficients, with custom coefficients when configured.
- Support high/low fault thresholds in raw code, ohms, and Celsius where exposed by the API.
- Support latched fault clearing and automatic/manual fault-detection cycles.
- Decode all documented fault bits: high threshold, low threshold, REFIN high/low, RTDIN low, and over/under-voltage.
- Provide register dump/read/write helpers for diagnostics without encouraging application code to bypass the typed API.
- Track sample counters, stale/not-ready behavior, dropped reads, overruns, fault counts, and timeout counts.

---

## Driver Architecture: Managed SPI Driver

The driver follows a managed synchronous SPI model:

- Public SPI operations are blocking and bounded.
- `tick()` may be used to service conversion-ready state and the one-sample cache.
- Health is tracked through common internal success/failure paths, not scattered ad hoc updates.
- Recovery is manual via `recover()`; the application controls retry strategy.

### Health State

```cpp
enum class MAX31865DriverState : uint8_t {
    UNINIT,
    READY,
    DEGRADED,
    OFFLINE
};
```

State transitions:
- `begin()` success -> READY
- Any tracked SPI/device failure in READY -> DEGRADED
- Success in DEGRADED/OFFLINE -> READY
- Failures reach `offlineThreshold()` -> OFFLINE
- `end()` -> UNINIT

### Health Tracking Rules

- `probe()` uses raw configuration-register checks and does not update health counters.
- `recover()` uses tracked operations and updates health.
- A simple "conversion not ready yet" result is not a transport failure.
- Do not count invalid arguments, invalid configuration, or invalid state as transport failures.
- Reset consecutive failures on tracked success.
- Keep lifetime success/failure counters saturating or wrapping intentionally; do not leave overflow behavior accidental.

---

## Versioning and Releases

Single source of truth: `library.json`. `Version.h` is auto-generated and must never be edited manually.

SemVer:
- MAJOR: breaking API/config/enum changes.
- MINOR: new backward-compatible features or error codes.
- PATCH: bug fixes, refactors, docs.

Release steps:
1. Update `library.json`.
2. Regenerate `Version.h`.
3. Update `CHANGELOG.md` using Added/Changed/Fixed/Removed.
4. Update `README.md` and examples if API or behavior changed.
5. Run tests and ESP32-S2/S3 builds.
6. Commit and tag: `Release vX.Y.Z`.

---

## Naming Conventions

- Member variables: `_camelCase`
- Methods/functions: `camelCase`
- Constants: `CAPS_CASE` for legacy macros, `static constexpr` names matching local namespace style for new constants
- Enum values: preserve existing public enum style
- Locals/params: `camelCase`
- Config fields: `camelCase`
