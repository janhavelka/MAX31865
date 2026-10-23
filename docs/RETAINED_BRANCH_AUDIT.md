# Retained ESP-IDF port branch review

Review date: 2026-10-23. Reviewed current `main`:
`47357045f99c89ca1c7b75bd625d69db4956faab`.

## Decision

The port branch contains four genuinely unmerged commits, but its core
portability work is already provided by the newer main architecture. Its
PlatformIO wrapper is byte-identical to main. No important core fix was found
that should be cherry-picked from this branch.

One visible capability is absent: the branch has a native ESP-IDF adapter and
sample application, whereas main deliberately supplies a neutral component
with application-owned callbacks. That is an intentional, documented scope
difference. The old adapter cannot be restored unchanged: it ignores its
timeout argument, relies on automatic chip select, and targets a different
callback/API contract. If a maintained native IDF example is required, build
and qualify it against current main as a new scoped integration task.

## Snapshot and unique commits

The lead fetched `origin`; local main equalled upstream, its tree was clean,
and it was the newest remote tip by commit date. AGENTS and the complete CI
workflow were read before edits. No code, branch, or historical checkout was
changed for this review.

| Ref | Exact SHA | Main-only / ref-only commits |
| --- | --- | --- |
| `main` / `origin/main` | `47357045f99c89ca1c7b75bd625d69db4956faab` | Baseline |
| `origin/feature/idf-port` | `5263d9032b0185604696f777ef136558a8e3eb6e` | 5 / 4 |
| Merge base | `08c0e303078e33405a980057daf6b5a16f01c7eb` | Shared ancestor |

`git cherry main origin/feature/idf-port` marks all four commits `+`.
This review therefore compared their actual changes and the current final
behavior; ancestry and patch IDs alone cannot settle their value.

| Unique commit | Change | Disposition on current main |
| --- | --- | --- |
| `d0c2ba1f3ecf251cf1135cc2eaff8b52465e7cca` | Adds native ESP-IDF SPI path, root component metadata and example. | Component purpose exists; native backend/example is intentionally absent. See R1 and the incompatibilities below. |
| `57c8db7174e4ba27bda6d97a5c9ecd05aa50d49d` | Adds an IDF example source-contract checker and related guidance. | Superseded by current framework/component checks for the callback-only scope. There is no native example target to check on main. An actual future adapter needs its own build/test contract. |
| `7b4bcf85b7df45862a72ba98d78bc43251eb9a10` | Adds framework-neutral callbacks, a Core alias/header, separates Arduino code, adjusts tests and migration documents. | Semantic intent already satisfied by current neutral driver plus guarded Arduino backend. The old alias, constructor/begin assumptions, callback signatures and API migration guide are obsolete. |
| `5263d9032b0185604696f777ef136558a8e3eb6e` | Adds `scripts/pio.cmd`. | File is byte-identical on main, despite the commit's `+` patch classification. No wrapper change to recover. |

The latest substantive replacement on main is
`b95b1cf875b65424a48aa2e43e0aab755dd69acc`; later commit
`65f447555c9f02b02da7db85955862f0ebc0fc82` fixes documentation and whitespace CI.
Their full final contents, not just commit subjects, were used for this review.
The [historical port implementation notes](https://github.com/janhavelka/MAX31865/blob/5263d9032b0185604696f777ef136558a8e3eb6e/docs/IDF_PORT_IMPLEMENTATION.md)
remain inspectable at the pinned tip. They are not instructions for the current
public API.

## Semantic comparison

Line references in this section identify the reviewed main baseline unless
explicitly labelled historical.

| Port intent | Current evidence | Verdict |
| --- | --- | --- |
| Neutral SPI/GPIO/time/lock injection | `include/MAX31865/Transport.h` defines role-specific status-returning transfer, manual CS, optional DRDY and paired lock callbacks; `src/MAX31865.cpp:613` validates required hooks. | Implemented, with stronger explicit ownership than the old callback table. |
| Framework-independent core include | `include/MAX31865/MAX31865.h`, `Config.h`, `Status.h`, `Protocol.h`, and `Transport.h`; plain-host compile gate `tools/run_native_core_compile.py`; framework/header guards. | Implemented without needing the old Core alias. A new alias provides no missing state or behavior. |
| Arduino separation | `include/MAX31865/ArduinoBackend.h` and `src/platform/arduino/MAX31865ArduinoBackend.cpp`; dedicated fake-Arduino tests. | Implemented. Copying the older `src/MAX31865Arduino.cpp` would create a competing integration path. |
| Native component packaging | `CMakeLists.txt:1` registers only the core and protocol sources as C++17; `idf_component.yml:6` declares S2/S3 and SDK minimum; `tools/component_contract.py` and `check_framework_boundaries.py` enforce scope. | Present as static component integration. This is not a claim of a real native IDF build or board validation. |
| Clean installed consumer | `tools/package_manifest.txt` records exact payload; `tools/check_packaged_consumer.py` validates archive and installed native/Arduino consumers. | Stronger current packaging validation, but it does not build an IDF application. |
| Bounded transport and status propagation | `src/MAX31865.cpp:1040`, `:1097`, and `:1133` explicitly frame CS/transfer/cleanup and pass remaining operation budget. `test/transport_deadline_tests.cpp` and `tools/check_transport_contract.py` exercise behavior. | Current contract supersedes the old transfer-owned-CS design. |
| Native ESP-IDF backend/application | `docs/ESP_IDF.md:3`, `docs/KNOWN_LIMITATIONS.md`, and AGENTS expressly exclude a bundled backend/example. The framework guard rejects `examples/esp_idf` and native platform sources under the current policy. | Absent intentionally; conditional scope expansion R1, not a missing core bug fix. |
| Portable Windows PlatformIO entry point | `git diff origin/feature/idf-port main -- scripts/pio.cmd` is empty. | Already equivalent. |

The old adapter is not a ready-made implementation for R1:

- At the historical tip,
  `examples/esp_idf/basic/main/Max31865IdfSpi.cpp:32` accepts `timeoutMs` and
  immediately discards it before `spi_device_polling_transmit`. Thus that code
  does not establish the current finite per-operation callback contract.
  A successful source-contract scan is not timing evidence.
- Historical `examples/esp_idf/basic/main/main.cpp:45` assigns hardware
  `spics_io_num`. Current `MAX31865Transport` requires `setChipSelect` and
  explicitly separates transfer from CS; [ESP-IDF integration](ESP_IDF.md)
  requires disabling automatic CS. Mixing the two owners would be incorrect.
- The old DRDY hook returns a bare readiness boolean. Current `readPin` returns
  a Status plus electrical level so a GPIO failure is distinguishable from
  not-ready state. The historical callback signature and error mappings do not
  satisfy that interface.
- The old example initializes different begin fields and checks a bool result;
  current `MAX31865::begin` at `src/MAX31865.cpp:1619` returns Status and uses the
  current typed configuration/transport. Old migration instructions are not a
  reason to reintroduce removed compatibility APIs.
- Old lock/timeout behavior, GPIO initialization error handling, cleanup, and
  tick rounding require a fresh review under present ownership/deadline rules.
  The branch itself supplies no evidence qualifying those details on current
  ESP-IDF or current driver code.

## Remaining actionable work

### R1 - Conditional: a maintained native ESP-IDF integration

Owner: consuming firmware and library maintainer. Priority is medium only if
a product needs a repository-maintained adapter; otherwise retain callback-only
scope and retire the historical branch. Main currently states this boundary
accurately, so there is no hidden promise to ship that adapter.

Acceptance for scope expansion:

1. Define supported native SDK/targets and one real consumer. Keep native bus,
   GPIO, scheduler, and lock ownership outside the neutral core.
2. Implement the current callbacks with manual CS, full-duplex bounded frame
   transfer, role-correct error/detail mapping, explicit DRDY capability,
   modulo clocks, yielding sleep, and paired finite shared-bus arbitration.
   Prove callback duration and cleanup behavior, including contention and
   unsupported SDK timeout/cancellation limitations; do not discard budgets.
3. Add host adapter/fault tests and actual clean native IDF S2/S3 application
   builds for the selected SDK. Current component metadata checks and Arduino
   target builds do not substitute for these builds.
4. Deliberately update AGENTS, `docs/ESP_IDF.md`, the framework boundary checker,
   package payload/consumer checks, and CI to reflect the agreed new scope.
   Keep the core forbidden-framework checks intact; do not disable the guard
   just to make copied historical sources pass.
5. On real hardware, capture mode/clock/CS timing, DRDY and no-DRDY readiness,
   faults, deadline/cleanup behavior, RTD/reference-resistor accuracy, and
   shared-SPI load. Retain exact firmware/SDK/fixture evidence.

Even if only a native component smoke-build harness is needed, document that
smaller validation improvement honestly; it would not create a supported
native peripheral adapter.

### R2 - Required before hardware acceptance: run the HIL matrix

Owner: board/application integration. [Testing](TESTING.md) and
[hardware bringup](HARDWARE_BRINGUP.md) define PT100/PT1000, 2/3/4-wire,
50/60 Hz, one-shot/continuous, DRDY present/absent, safe induced faults,
shared-SPI contention, framing, recovery, and soak coverage. Current repository
documentation states no committed HIL evidence. This review did not access
hardware. The old port does not supply the missing current-firmware evidence.

Acceptance requires reviewed raw logs/captures and fixture identity for the
exact firmware, not merely a native test or parsed serial pass. The runner
explicitly leaves `hil_accepted` false pending review. Follow the existing
release/physical-acceptance policy before any release claim.

## Retain/retire and reproduction workflow

```powershell
git status --short --branch
git fetch origin
git rev-parse HEAD origin/main origin/feature/idf-port
git merge-base main origin/feature/idf-port
git rev-list --left-right --count main...origin/feature/idf-port
git log --format='%H %s' main..origin/feature/idf-port
git cherry main origin/feature/idf-port
git diff --stat main...origin/feature/idf-port
git diff origin/feature/idf-port main -- scripts/pio.cmd
git show origin/feature/idf-port:include/MAX31865/Transport.h
git show origin/feature/idf-port:examples/esp_idf/basic/main/Max31865IdfSpi.cpp
```

1. Repeat graph and semantic comparisons when either tip moves. Keep current
   implementation and this disposition on the existing main branch.
2. Decide R1 from an actual application requirement. If not needed, archive the
   unique port tip and retire its active branch after checking PR/consumer use.
   If needed, retain it temporarily as historical reference while implementing
   R1 against main; do not merge all four commits as a unit.
3. Before deleting the branch, preserve its exact SHA under a durable archive
   ref or verified Git bundle. This document's SHA is a locator, not a mechanism
   that prevents Git garbage collection. No refs were deleted by this review.
4. For implementation changes add meaningful focused regressions, use existing
   CI plus the new integration gate, and verify the exact final commit. Stage
   only owned changes, commit/push without force, and confirm upstream sync.
   The lead owns Git mutations for shared agent work.

## Checks performed and CI boundary

This documentation review passed `tools/check_framework_boundaries.py`,
`check_deleted_api.py`, `check_transport_contract.py`,
`check_version_metadata.py`, `check_header_hygiene.py`,
`python scripts/generate_version.py --check`, local Markdown target checks in
the changed files, and `git diff --check`. The transport contract checker
includes its existing behavioral host harness; this is not the full native
suite or hardware validation.

The full native suite, sanitizers, Doxygen, packaged-consumer build, all Arduino
target builds, and HIL were not rerun for this documentation-only change.
For future behavioral work use [Testing](TESTING.md) and
`.github/workflows/ci.yml`: strict native suite, address/undefined sanitizers,
all static gates, parser tests, Doxygen 1.9.8, twelve Arduino S2/S3 example
builds, and exact package/installed-consumer checks. PlatformIO is pinned to
6.1.19; Windows uses the existing `scripts/pio.cmd` installation. Current CI
has no native IDF target build. Major changes need green CI at their exact final
commit; this documentation chunk needs no extra manual dispatch, although a
normal main push can trigger configured CI.
