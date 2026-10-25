# Prompt 01: implement a native adapter with truthful completion and ownership

**Status: NOT IMPLEMENTED.** Audit mapping: R1 transport, ownership and SDK
acceptance. Prerequisite: none. Follow with [02](02_NATIVE_APPLICATION_AND_CI.md).
Current source anchor: `9937c886d5617bc8deca1d7db148a3fcf286a377`.
Historical reference only: `5263d9032b0185604696f777ef136558a8e3eb6e`.

Implement this work, including regression tests and reviewable integration,
on the existing branch. This prompt deliberately expands the currently
documented scope to include a maintained native ESP-IDF **example-owned**
adapter. Update the narrow policy/guard exceptions needed for that scope;
do not ask for another scope decision or weaken the neutral core contract.
Do not merge/cherry-pick the old port or replace the newer driver.

## Read and audit before editing

- Full `AGENTS.md`, `.github/workflows/ci.yml`, `docs/RETAINED_BRANCH_AUDIT.md`
  R1 and `docs/ESP_IDF.md`.
- `include/MAX31865/Transport.h`, `Status.h`, `CommandTable.h`, `Config.h`;
  `MAX31865::openSession`, `transferInSession`, `closeSession`,
  `deadlineRemainingMs` and callback normalization in `src/MAX31865.cpp`.
- The guarded Arduino backend for existing ownership/error conventions,
  `test/transport_deadline_tests.cpp`, `test/arduino_backend_tests.cpp`,
  `test/support/ScriptedMax31865Transport.*` and `tools/run_native_suite.py`.
- `tools/check_framework_boundaries.py`, `component_contract.py`,
  `check_transport_contract.py`, `check_deleted_api.py`,
  `check_packaged_consumer.py`, `package_manifest.txt`, root CMake/manifest.
- Historical `examples/esp_idf/basic/main/Max31865IdfSpi.cpp` and `main.cpp`
  through `git show <historical-sha>:<path>`, without checking out old files.

The historical transfer ignores its budget, uses old callback signatures and
depends on automatic CS. Current transfer is
`MAX31865Status (*)(void *user, const uint8_t *tx, uint8_t *rx, size_t length,
uint32_t timeoutMs)`; manual `setChipSelect` belongs to the core session, and
optional `readPin` reports Status plus an electrical level. An adapter port
must satisfy those contracts rather than translating enough names to compile.

## SDK feasibility is part of implementation

Audit headers **and implementation** for each selected native SDK. IDF 5.5.5
is an inspected candidate, not an automatically supported SDK. Its
[SPI master source](https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_driver_spi/src/gpspi/spi_master.c)
rejects finite waits in `spi_device_polling_start` and `spi_device_acquire_bus`;
`spi_device_polling_transmit` uses unlimited waits. A polling-end timeout
returns before normal transaction cleanup. Queue/result waits do not establish
that an accepted transaction was cancelled. Record the relevant symbols and
selected SDK revision in the integration guide.

Choose the smallest supported transfer path for which finite acquisition,
terminal completion, RX/descriptor lifetime and release are demonstrable.
An application arbiter must include every client of the host; it must not
hide another unbounded SDK acquisition. Do not return a timeout while the SDK
can still reference caller buffers, clock more bytes, or require the core to
hold CS/lock beyond callback return. Do not claim cancellation from a wait
timeout, use an unsupported abort/private reset, spin indefinitely to drain,
or add a background worker just to conceal an unbounded call.

If the chosen SDK cannot provide a conforming peripheral path, expose and
test action-free rejection of that unsupported configuration and record the
precise remaining blocker. Continue useful component/test work, but leave
R1 and this prompt NOT IMPLEMENTED until a real supported transfer path is
proved. A smoke build or a deadline checked after return is not completion.
Do not silently relax the current synchronous transport to make the port pass.

## Implementation direction

1. Prefer a compact context and ordinary callbacks under
   `examples/esp_idf/common/`. The application owns bus/device creation,
   GPIO configuration, lock lifetime and destruction. Validate the binding
   before bus/GPIO action. Do not add bus handles or framework types to core.
2. Supply one exact full-duplex MSB-first frame, at most
   `max31865_cmd::MAX_FRAME_BYTES` (nine bytes). Require mode 1 or 3, clock at
   most `SPI_MAX_HZ`, hardware CS disabled, and sole manual CS control.
   Transfer must not lock, toggle CS, retry or recover internally.
3. Implement paired lock/unlock with one acquisition attempt using the supplied
   remaining budget. Zero means no callback-owned waiting. Preserve the same
   arbiter across all host users, avoid recursive locking, and release exactly
   once only after the transaction is terminal. Convert ticks without overflow
   or accidentally mapping zero/sub-tick budgets to an unlimited wait.
4. Map errors by role to `BusLockTimeout`, `BusLockFailed`,
   `SpiTransferFailed`, `ChipSelectFailed`, `GpioFailed` or action-free
   `InvalidState`, retaining useful native detail and static messages.
   A callback reporting successful transfer must initialize every RX byte.
5. Match DRDY capability to a validated input and commit output level only on
   success. Provide modulo clocks, yielding millisecond sleep and bounded
   protocol delays. No rebinding of live resources, ISR use, hidden allocation
   in callbacks, task creation or logging in the core.
6. Deliberately update AGENTS and `docs/ESP_IDF.md` for this example exception.
   Restrict IDF/FreeRTOS allowance in `check_framework_boundaries.py` to its
   actual example subtree. Keep framework prohibitions on every core source
   and public header, the guarded Arduino path, and the canonical root CMake.
   Reject Arduino source inclusion/facades in the new native example subtree.
7. Keep package validation coherent in this chunk: add only intended adapter
   files to `package_manifest.txt` and the package checker's allowed payload
   if shipped. Preserve docs/tool exclusions and installed native/Arduino
   consumers. Do not broadly allow all new files or remove exact manifest checks.

## Tests and acceptance

Add behavior tests around the actual adapter using minimal SDK fakes, following
the existing host backend pattern. Register them in the existing native suite;
do not create a parallel runner framework. Include:

- Valid frame/RX, nine-byte limit, invalid/null/unbound context, invalid
  mode/frequency/pins, duplicate roles and DRDY present/absent.
- Acquired/contended lock, zero/sub-tick/exhausted/wrapped budgets, and no
  physical transfer after failed acquisition or preflight rejection.
- Failed CS assertion/deassertion and transfer failures with partial effects;
  exactly one deassert/release path and correct original/cleanup diagnostics.
- In-flight timeout/cancellation limits: prove no descriptor/buffer access or
  clocks after callback return. A fake that always completes immediately is
  insufficient. Retain core `deassertInvalidStateIsATrackedFramingFailure`,
  `primaryAndDeassertFailureComposeRestoreFailure`,
  `zeroBudgetNestedCleanupDoesNotInventADeadline` and
  `moduloWraparoundDeadlinesRemainFinite` coverage.
- Repeated bind/use/end and failed initialization/cleanup without leaks or
  double removal. The core's `end()`/destructor remain zero-I/O; application
  cleanup happens only after the driver stops using borrowed resources.

Add the minimal native compilation fixture needed to compile the actual
adapter with the chosen SDK on S2/S3. SDK mocks alone cannot establish API
availability, clock configuration or lifetime correctness. Prompt 02 turns
that fixture into the maintained application/build matrix, rather than adding
a competing example.

## Validation, closeout and Git

Fetch/inspect the current branch/upstream, preserve unrelated work and follow
lead-owned Git mutations if using agents. Run the static commands listed in
the existing CI, `python tools/run_native_suite.py`, both sanitizer variants
on their supported platform, strict Doxygen, all twelve pinned Arduino example
builds and `python tools/check_packaged_consumer.py`; use `scripts/pio.cmd`
for direct PlatformIO commands on Windows. Add the actual adapter/native
fixture checks to existing CI and verify its exact final commit. Report any
unavailable check without calling it passed.

Update public/example contracts, `docs/ESP_IDF.md`, `docs/KNOWN_LIMITATIONS.md`,
`CHANGELOG.md` Unreleased, the R1 audit row and prompt status. Record exact SDK,
tests/builds and remaining limitations. Keep R2 hardware acceptance open.
Commit only owned changes, push without force, confirm upstream synchronization
and provide the final SHA and CI terminal results. No release or branch
deletion is part of this task.
