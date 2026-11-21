# Prompt 01: native ESP-IDF SPI adapter, scope election and guards

**Status: NOT IMPLEMENTED.** Audit mapping: R1 transport, ownership and SDK
limits. Requires: none. Next: [02](02_NATIVE_APPLICATION_AND_CI.md). Revision
anchor: `8a0c24b3a295fb46676bcbf34da40652138c4cbb`; record the fetched `main`
HEAD at entry. Historical reference only:
`5263d9032b0185604696f777ef136558a8e3eb6e`. Apply the [index](README.md)
decisions and common rules.

## Outcome and scope

Deliver one example-owned adapter implementing the current
`MAX31865Transport` on ESP-IDF 6.0.1, proved by host tests on minimal SDK fakes
and by real `esp32s3`/`esp32s2` builds of a minimal native application that
uses it. Make the scope election durable in AGENTS, docs and the framework
guard. Prompt 02 turns the minimal application into the maintained CLI
application; do not add a competing example.

The owner elected native ESP-IDF support on 2026-11-16. This prompt authorizes
the edits below without another scope question. Do not merge, cherry-pick or
restore the historical port or its APIs (transfer-owned CS, bool DRDY result,
old begin fields or aliases).

Owned in 01: `examples/esp_idf/basic/**`, `test/fake_idf/**`, one adapter test
source and its runner, `tools/run_native_suite.py:14-27` (registration),
`tools/check_framework_boundaries.py`, `tools/check_packaged_consumer.py:37-47`
and `tools/package_manifest.txt` (payload only), `idf_component.yml:12`,
`CHANGELOG.md`, the status column of the index, and the documents marked 01
below. Forbidden: `src/**`, `include/MAX31865/**` (no IDF
backend header, no public API change), root `CMakeLists.txt`, the Arduino
backend, `examples/06_diagnostic_cli/**` (02 owns its one string fix) and
sibling repositories (read-only).

### Statements and guards this set changes

Line numbers match the commit that adds this revision (parent `8a0c24b`);
verify each at entry. Edit a guard so it enforces the new design; never delete
or loosen a check of shipped behaviour.

| Path:line | Current rule | Prompt |
| --- | --- | --- |
| `AGENTS.md:46-48`, `:72-84` | targets and layout list no native example | 01 |
| `AGENTS.md:135-137`, `:140-146` | "supplies no native ESP-IDF backend or example"; examples use Arduino and PlatformIO; 2026-11-16 planning note | 01 (replace the note with the actual scope) |
| `AGENTS.md:190-191` | evidence layers lack a native IDF build | 01 |
| `docs/ESP_IDF.md:3-7`, `:31-36`, `:43-52` | no bundled backend, no native build claim | 01; final commands 02 |
| `docs/KNOWN_LIMITATIONS.md:29-30` | no native backend/example | 01 |
| `docs/repository-contract.md:10-11`, `:17`, `:21-24` | Arduino-only examples; package scope | 01, 02 |
| `docs/RETAINED_BRANCH_AUDIT.md:66`, `:99-128`, `:190-191` | R1 conditional; no native CI build | 01 partial, 02 close |
| `README.md:3-7`, `:129-142`, `:144-169` | targets, examples, validation list | 02 |
| `docs/CLI.md:3-4`, `:17-18`, `:32-34` | Arduino-only CLI wording | 02 |
| `tools/check_framework_boundaries.py:15`, `:119`, `:125-131`, `:148-155` | IDF/FreeRTOS tokens rejected in every example; `examples/esp_idf` forbidden | 01 |
| `tools/check_packaged_consumer.py:37-47`, `:48-71`, `:731-742` | exact payload directories; exact manifest excludes; `examples/` token forbidden in the manifest | 01 payload, 02 excludes |
| `tools/check_cli_contract.py:142-151`, `:290-300` | engine and shell rules cover example 06 only | 02 |
| `tools/check_version_metadata.py:20-24`, `:211-216` | exact action set; `ubuntu-24.04` runners | 02 |
| `examples/06_diagnostic_cli/Max31865Cli.cpp:1507` | "Arduino transport setup is unavailable" | 02 |
| `idf_component.yml:12`, `:13-36` | `idf: ">=5.3"`; exclude list | 01 version, 02 excludes |

## Audit before editing

- Full `AGENTS.md`, `.github/workflows/ci.yml`, `docs/ESP_IDF.md`, R1 in
  `docs/RETAINED_BRANCH_AUDIT.md:99-128` and the index decisions.
- `include/MAX31865/Transport.h:25-120`, `Status.h`, `CommandTable.h:24,28`,
  `Config.h:54-61` (`MAX31865Pins`). In `src/MAX31865.cpp`:
  `deadlineRemainingMs` (`:825`), `openSession` (`:1002`), `transferInSession`
  (`:1078`), `closeSession` (`:1119`) and callback status normalization.
- Ownership and error mapping of the guarded Arduino backend
  (`include/MAX31865/ArduinoBackend.h`,
  `src/platform/arduino/MAX31865ArduinoBackend.cpp`); the host pattern
  `test/fake_arduino_backend/`, `test/arduino_backend_tests.cpp`,
  `tools/run_arduino_backend_tests.py:29-42`, `tools/native_compile.py`;
  `test/transport_deadline_tests.cpp`, `test/support/ScriptedMax31865Transport.*`.
- The guards in the table plus `tools/component_contract.py`,
  `check_transport_contract.py` and `check_deleted_api.py` (its code patterns
  reject members named `.cs`/`.drdy`, so name adapter fields differently).
- SDK sources in `C:\esp\v6.0.1\esp-idf` and `C:\esp\v6.1\esp-idf` (confirm
  `git describe --tags` prints `v6.0.1`/`v6.1`):
  `components/esp_driver_spi/src/gpspi/spi_master.c` (6.0.1: acquire `:1317`,
  polling start `:1381`, polling end `:1425-1456`, polling transmit
  `:1462-1471`, transmit `:1295-1306`; 6.1: `:1304`, `:1368`),
  `include/driver/spi_master.h:81` (`spics_io_num = -1` disables hardware CS),
  `include/driver/spi_common.h:73` (`SPI_DMA_DISABLED`), and
  `SOC_SPI_MAXIMUM_BUFFER_SIZE` (6.0.1: 72 bytes on S2,
  `components/soc/esp32s2/include/soc/soc_caps.h:267`; 64 on S3,
  `esp32s3/.../soc_caps.h:286`). A finite `spi_device_polling_end` returns
  `ESP_ERR_TIMEOUT` before transaction cleanup and bus release, and a finite
  `spi_device_get_trans_result` (`:1270-1292`) returns `ESP_ERR_TIMEOUT` with
  the transaction still in flight, so neither is a cancellable timeout.
- Sibling references, read-only; record path, HEAD and adopted/rejected
  choices: INA228 `examples/esp_idf/basic/main/Ina228IdfI2cTransport.{h,cpp}`
  (context, error mapping, cleanup retention); INA3221
  `examples/esp_idf/basic/components/INA3221/CMakeLists.txt` and SCD41
  `examples/idf/basic/components/SCD41/CMakeLists.txt:1-10` (fixed-name
  wrapper); OPT4001 `examples/esp_idf/basic/{CMakeLists.txt,main/CMakeLists.txt,sdkconfig.defaults}`
  (minimal project); ADS1261 for the shared SPI decision text.
- Historical `examples/esp_idf/basic/main/Max31865IdfSpi.cpp` and `main.cpp`
  only through `git show 5263d90:<path>`, as a list of what not to copy
  (`docs/RETAINED_BRANCH_AUDIT.md:69-91`).

## Implement

1. **Project.** `examples/esp_idf/basic/CMakeLists.txt` (project
   `max31865_basic`, no `EXTRA_COMPONENT_DIRS` pointing at the repository root);
   `components/MAX31865/CMakeLists.txt` registering exactly the root sources and
   include directory by paths relative to its own directory (SCD41/INA3221
   form); `main/CMakeLists.txt` with
   `REQUIRES MAX31865 esp_driver_spi esp_driver_gpio esp_timer freertos` plus only
   what the build proves necessary; `sdkconfig.defaults` with
   `CONFIG_FREERTOS_HZ=1000` and `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192`
   (Arduino loop-task size; IDF default 3584, 6.0.1
   `components/esp_system/Kconfig:232-234`; high-water mark NOT RUN). Root
   `CMakeLists.txt` stays byte-identical.
2. **Binding.** `main/Max31865IdfTransport.{h,cpp}` (name may differ; not the
   historical `Max31865IdfSpi`): a compact context borrowing the application's
   `spi_device_handle_t`, CS GPIO and optional DRDY GPIO, returning a
   `MAX31865Transport`. Bind takes the handle plus the same
   `spi_device_interface_config_t` given to `spi_bus_add_device` (or an adapter
   helper fills it) and validates, before any bus or GPIO action: mode 1 or 3;
   no `SPI_DEVICE_TXBIT_LSBFIRST`/`SPI_DEVICE_RXBIT_LSBFIRST`
   (`spi_master.h:36-37`); `spics_io_num == -1`; distinct valid pins; an
   actual SCLK (`spi_device_get_actual_freq`, kHz, `spi_master.c:652-658`) from
   100 kHz to 5 MHz; DRDY capability matching a configured input. Document any
   other device setup as unsupported. No framework type enters the core.
3. **Transfer (decision 6).** One exact full-duplex frame of
   1..`MAX_FRAME_BYTES` bytes through a terminal polling call
   (`spi_device_polling_transmit`, or start and end both with `portMAX_DELAY`).
   The application must initialize the host with `SPI_DMA_DISABLED` (non-DMA
   transfers use the caller's buffers, `spi_master.c:1194-1198`; 9 bytes fit
   the FIFO). Never lock, toggle CS,
   retry or recover; never report a lock or transfer timeout; initialize every
   RX byte on success. A zero budget follows `Transport.h:89-90`: one immediate
   attempt whose only time is the frame itself. The core's after-return
   deadline checks still report lateness. Leave `lockBus`/`unlockBus` null for
   the dedicated host. No queue/result waits, `spi_device_acquire_bus`,
   unsupported abort or private reset, background worker or spin-drain loop.
4. **Errors by role.** SPI to `SpiTransferFailed`, CS GPIO to
   `ChipSelectFailed`, DRDY to `GpioFailed`, invalid binding or arguments to
   action-free `InvalidState`; keep the `esp_err_t` in `detail` and use static
   messages. `readPin` commits the level only on Ok.
5. **Time.** `nowMs`/`nowUs` from `esp_timer_get_time()` modulo 2^32;
   `sleepMs` loops `vTaskDelay` (at least one tick; ms to ticks without
   overflow) until `nowMs` shows the full requested time, with a bounded
   attempt count of 8 (as ADS1261
   `src/platform/arduino/ADS1261ArduinoBackend.cpp:8-9,295-305`); `delayUs`
   through `esp_rom_delay_us`.
   No allocation, logging or task creation in callbacks; no rebinding of live
   resources.
6. **Minimal application** `main/main.cpp`: create the dedicated host and one
   device, preset CS high before enabling the output, configure optional DRDY
   input, bind, `begin`, read-only `probe`, one bounded `readOneShot`, print
   statuses, `end`, then remove the device, free the bus and reset the GPIOs in
   reverse order; unwind partial setup on every failure. It is a real consumer,
   not a stub.
7. **Framework guard.** In `check_framework_boundaries.py` allow ESP-IDF and
   FreeRTOS tokens only under `examples/esp_idf/` and reject Arduino tokens
   there (`:119` now allows Arduino in every example). Keep IDF/FreeRTOS
   rejected in `include/`, `src/` and every other example, including the shared
   engine in `06_diagnostic_cli`; keep `include/MAX31865/IdfBackend.h` and
   `src/platform/esp_idf` forbidden (`:148-152`) and the exact root CMake check
   (`:157-161`). Prove both rejections (an IDF include in `src/` or
   `06_diagnostic_cli`, an Arduino include in `examples/esp_idf/`) on a
   temporary copy outside the tree or with a committed test.
8. **Package coherence.** The PlatformIO archive carries every non-excluded
   file and must equal the manifest (`check_packaged_consumer.py:582-588`).
   Add the new files to `tools/package_manifest.txt` (sorted, exact) and
   `examples/esp_idf` to `INTENDED_DIRECTORIES` (`:37-47`).
9. **Version claim.** `idf_component.yml:12` to `idf: ">=6.0.1"` with a
   CHANGELOG `### Changed` compatibility bullet (older SDKs are not tested).
10. **Docs.** AGENTS scope (keep the core prohibitions `AGENTS.md:105-106`);
    a native section in `docs/ESP_IDF.md` with SDK revision, symbols and
    lines, the decision 6 text including the rule sentence, both residual
    risks (blocked owner task; non-yielding busy-wait at `spi_master.c:1433-1439`
    starving lower-priority tasks and IDLE), the supported device setup and
    the build commands; `KNOWN_LIMITATIONS.md`; `repository-contract.md`;
    the R1 row (01 delivered, 02 pending); CHANGELOG `[Unreleased]`; this
    prompt's status.

## Tests and acceptance

Add an adapter test (for example `test/idf_adapter_tests.cpp`) with minimal
fakes under `test/fake_idf/` for exactly the SDK headers the adapter includes,
and a runner that builds it for S2 and S3 definitions like
`run_arduino_backend_tests.py:29-42`, registered in `run_native_suite.py`. Do
not create a parallel runner framework. Cover:

- a valid frame with complete RX; 1, 9 and 10 bytes; null buffers; unbound or
  null context; invalid mode, clock, pins, hardware CS, duplicate roles, DRDY
  present/absent, each rejected before any SDK or GPIO call;
- only terminal polling calls: no queue/result call, no finite acquire, nothing
  outstanding after return; a fake transfer that runs longer than `timeoutMs`
  still returns the SDK's terminal result and no adapter timeout;
- a structural check in the runner (like `run_arduino_backend_tests.py:9-26`)
  that the adapter source calls `spi_device_polling_transmit` and none of
  `spi_device_queue_trans`, `spi_device_get_trans_result`,
  `spi_device_acquire_bus`, `malloc`, `heap_caps_malloc`, `ESP_LOG` or
  `xTaskCreate`;
- an actual SCLK below 100 kHz or above 5 MHz, LSB-first flags and a hardware
  CS pin, each rejected;
- SDK, CS assert/deassert and DRDY failures with role-correct status and
  detail;
- `sleepMs` with a coarse-tick fake clock, a stopped clock (bounded by the
  attempt count), a large value and wraparound; clock wraparound and delay
  bounds;
- repeated bind/use/end and failed setup at each step unwinding without leaks
  or double removal (the fake counts create/remove);
- the core tests `deassertInvalidStateIsATrackedFramingFailure`,
  `primaryAndDeassertFailureComposeRestoreFailure`,
  `zeroBudgetNestedCleanupDoesNotInventADeadline` and
  `moduloWraparoundDeadlinesRemainFinite` (`test/transport_deadline_tests.cpp`)
  stay green.

Assert statuses, call sequences and resource ownership, not message text.

Stop condition: if the 6.0.1 source contradicts the hardware-bound premise
(for example another wait in the polled path that the frame does not bound),
record the exact source line, leave this prompt and R1 NOT IMPLEMENTED, keep
useful tests and ship no timeout claim. A conforming finite-wait path may
replace the polled path only with source and test evidence.

## Verify

Record each as PASS/FAIL/NOT RUN/BLOCKED with tool versions:

```powershell
python scripts/generate_version.py --check   # and every other static check in ci.yml:29-38
python tools/run_native_suite.py
python tools/run_native_suite.py --sanitizer address    # where the local compiler supports it
python tools/run_native_suite.py --sanitizer undefined
python tools/check_packaged_consumer.py
doxygen Doxyfile                              # CI pins 1.9.8; record the local version
git diff --check
C:\esp\idf.cmd 6.0.1 -C examples\esp_idf\basic -B C:\esp\build\max31865-esp32s3-6.0.1 -D SDKCONFIG=C:\esp\build\max31865-esp32s3-6.0.1.sdkconfig set-target esp32s3 build
C:\esp\idf.cmd 6.0.1 -C examples\esp_idf\basic -B C:\esp\build\max31865-esp32s2-6.0.1 -D SDKCONFIG=C:\esp\build\max31865-esp32s2-6.0.1.sdkconfig set-target esp32s2 build
```

Before the package check, delete any `dependencies.lock` or
`managed_components/` the build left in `examples/esp_idf/basic`: Git ignores
them, but `check_packaged_consumer.py:472-481` scans the filesystem (whether
`pio package pack` also includes them is for execution to check). Run the
twelve Arduino builds (`.\scripts\pio.cmd run -e ...`, list in
`README.md:163-168`) if 01 changed any file they compile; otherwise record NOT
RUN with that reason. No remote CI in 01.

## Close out

Stage only reviewed owned files, commit, push and confirm sync (index common
rules). Hand off the base and final SHA, SDK revision, files, tests and each
evidence layer; physical NOT RUN. Mark this prompt IMPLEMENTED only when the
adapter tests and both target builds pass.
