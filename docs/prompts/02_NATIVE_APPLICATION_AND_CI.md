# Prompt 02: native diagnostic application, package and CI checkpoint

**Status: NOT IMPLEMENTED.** Audit mapping: R1 application, CLI, package and
CI. Requires: [01](01_NATIVE_ADAPTER.md) complete, with adapter tests and both
6.0.1 target builds passing and the framework guard updated. Revision anchor:
`8a0c24b3a295fb46676bcbf34da40652138c4cbb`; record the fetched `main` HEAD and
01's final SHA at entry. Historical reference only:
`5263d9032b0185604696f777ef136558a8e3eb6e`. Apply the [index](README.md)
decisions and common rules. This is the set's only remote-CI checkpoint.

## Outcome and scope

Turn `examples/esp_idf/basic` into the maintained native diagnostic
application: one native owner, the adapter from 01, and full command parity
with `examples/06_diagnostic_cli` through the same framework-neutral engine.
Add the example contract checker, package integration, an S3/S2 CI job, the
documentation, and verify remote CI on the exact final commit. Inspect 01's
diff, tests and SDK evidence first; a status line is not proof.

Owned in 02: `examples/esp_idf/basic/**`, the one string at
`examples/06_diagnostic_cli/Max31865Cli.cpp:1507`, `test/` additions,
`tools/check_cli_contract.py`, `tools/check_idf_example_contract.py` and its
test, `tools/check_packaged_consumer.py`, `tools/package_manifest.txt`,
`tools/check_version_metadata.py`, `tools/check_framework_boundaries.py`
(export set only if needed), `idf_component.yml` excludes, `library.json` export
excludes only if needed, `.github/workflows/ci.yml`, README, AGENTS, `docs/*`,
CHANGELOG and this set's status. Forbidden: `src/**`, public headers, root
`CMakeLists.txt`, the Arduino backend, new PlatformIO environments, a second CLI
engine or command table, and sibling repositories.

## Audit before editing

- AGENTS, `ci.yml`, R1, `docs/ESP_IDF.md`, `docs/CLI.md`, `docs/TESTING.md`,
  `docs/repository-contract.md` and every file 01 changed.
- The engine and shell: `Max31865Cli.h:11-67` (`Platform`, `Context`, API),
  `Max31865Cli.cpp:10-24` (identity macros), `:72` (`COMMANDS`), `:796-811`
  (`version`), `:1507`, `:2058-2059` (`service()` drives `tick`);
  `Max31865CliShell.h:19-29` (needs only `available()`/`read()`), `:84-85`
  (32 bytes per poll, 192-byte buffer); Arduino owner `main.cpp:83-129`;
  `DiagnosticConfig.h:14-31` (example values; the file includes `Arduino.h`
  at `:3`, so native code must not include it); `test/diagnostic_cli_tests.cpp`,
  `tools/run_native_cli_tests.py`, `scripts/generate_version.py:108-152`
  (Arduino identity), `tools/run_max31865_hil.py:220-235` (required `version`
  fields).
- Guards: `check_cli_contract.py`, `check_packaged_consumer.py:37-71,714-768`,
  `check_version_metadata.py:20-24,206-224`, `check_framework_boundaries.py:68-91`,
  `package_manifest.txt`, `library.json:51-76`, `idf_component.yml:13-36`.
- Sibling references, read-only (record path, HEAD, adopted/rejected): OPT4001
  `examples/esp_idf/basic/main/main.cpp:1420-1450` (non-blocking console) and
  `tools/check_idf_example_contract.py` with its test
  `tools/test_idf_example_contract.py`; INA228
  `examples/esp_idf/basic/main/CMakeLists.txt:9-31` and
  `main/GenerateGitProvenance.cmake` (identity), `tools/check_idf_example_contract.py`
  (CLI and docs tokens), `examples/esp_idf/basic/README.md`; TMP1x2
  `examples/esp_idf/basic/sdkconfig.defaults.esp32s2`/`.esp32s3` and
  `main/CMakeLists.txt:5-8` (IDF 6 console component); TCA9548A
  `idf_component.yml:25-29` (example build-output excludes); OPT4001
  `.github/workflows/ci.yml:155-185` (IDF job shape).

## Implement

1. **Owner.** `app_main` is the single owner of the SPI host, device, adapter
   context, GPIOs, `TransportObserver`, driver, begin configuration and CLI
   context. Set up in order, check every return, unwind in reverse on failure;
   allocate SDK resources only during setup. Put board values in an
   example-local header mirroring `DiagnosticConfig.h:14-31` and 06
   `main.cpp:63-66` (SCK/MISO/MOSI/CS 12/13/11/10, DRDY unused, 1 MHz mode 1,
   250 ms, four-wire, 60 Hz, 400/100 ohm, 1000 us), documented as example
   values. Keep the owner logic testable on the host (for
   example `main/Max31865IdfApp.{h,cpp}` with a thin `app_main`).
2. **CLI parity.** Compile `examples/06_diagnostic_cli/Max31865Cli.cpp` from
   `main/CMakeLists.txt`; copy nothing. Reuse `BoundedSerialShell` through a
   small stdin stream adapter over non-blocking `getchar` (OPT4001 recipe:
   unbuffered stdio, `O_NONBLOCK`; refuse to start a blocking CLI). Supply
   `Platform{write, nowMs, freeHeapBytes}`, wrap the adapter with
   `observeTransport`, call `bind` and `start`, then loop: poll the shell,
   `processLine` or `processInputOverflow` plus `printPrompt`, `service()`, and
   yield at least one tick (`vTaskDelay(1)`) on every loop iteration. `tick`
   stays zero-I/O; no second
   acquisition engine, task, ISR or queue.
3. **Neutral string.** Change `Max31865Cli.cpp:1507` to framework-neutral
   wording and `docs/CLI.md:3-4,17-18,32-34` to cover both shells. Arduino
   behaviour is otherwise unchanged; add a native CLI test for the
   transport-unavailable `begin` path.
4. **Identity.** The example-only script `main/GenerateBuildIdentity.cmake`
   (INA228 pattern) generates a header in the build directory that defines the
   engine's macros (`Max31865Cli.cpp:10-24`), formatted like the Arduino
   identity (`scripts/generate_version.py:115-152`):
   `MAX31865_CLI_GIT_COMMIT` from `git rev-parse --short=12 HEAD`;
   `MAX31865_CLI_GIT_STATUS` `clean`/`dirty`/`unknown`; both `unknown` unless
   `git -C <repo root> rev-parse --show-toplevel` equals the repository root;
   `MAX31865_CLI_TARGET` maps `IDF_TARGET` to `esp32-s2`/`esp32-s3`;
   `MAX31865_CLI_FRAMEWORK` `esp-idf`; `MAX31865_CLI_BUILD_KIND`
   `diagnostic_cli`. It refreshes on every build and reaches only this build of
   the engine (for example a forced include on that one source). The library
   version still comes from `Version.h`; core and ordinary consumers need no
   Git or Python.
5. **Console defaults.** `sdkconfig.defaults` keeps `CONFIG_FREERTOS_HZ=1000`
   and `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` from 01 (Arduino loop-task size;
   IDF default 3584, `esp_system/Kconfig:232-234`; stack high-water mark NOT
   RUN); per-target
   `sdkconfig.defaults.esp32s3` (USB Serial/JTAG) and `.esp32s2` (USB CDC) as in
   TMP1x2. IDF 6 may need `esp_usb_cdc_rom_console` for S2; verify by build and
   keep the UART console for a target where the USB console cannot serve
   `getchar`. Console input itself is a hardware result (NOT RUN).
6. **Example README.** Wiring table, the `idf.cmd` build commands, flash and
   monitor, a first session (`help`, `version`, `wiring`, `probe`,
   `one confirm`, `health`), both residual risks, the safety notes, "no Arduino
   sources", and "a native build or contract check is not hardware validation".
   Packaged README links to `docs/` are absolute GitHub links (docs are
   excluded from packages).

## Checkers, package and CI

- `tools/check_idf_example_contract.py` plus
  `tools/test_check_idf_example_contract.py` with mutation cases (OPT4001
  precedent), checking: required files; wrapper sources and include directory equal the root
  component (`component_contract.py:10-19`) under the fixed name `MAX31865`; no
  `override_path` and no `EXTRA_COMPONENT_DIRS` at the repository root; main
  `REQUIRES` the library and its IDF components; the engine compiled from
  `06_diagnostic_cli`, with no second `COMMANDS` table under `examples/esp_idf`;
  no Arduino tokens; `CONFIG_FREERTOS_HZ=1000`; CI job tokens; README and
  `docs/ESP_IDF.md` tokens (decision 6 risk sentence, not-hardware-validation
  statement). Add both to the static step (`ci.yml:27-38`), `README.md:146-162`
  and `docs/TESTING.md`.
- `check_cli_contract.py`: keep every 06 rule and add the native shell: engine
  entry points, `observeTransport(`, an owned `MAX31865` instance,
  `BoundedSerialShell`, the 06 engine path in `main/CMakeLists.txt`, no Arduino
  tokens, and the existing `FORBIDDEN` heap/`String` patterns
  (`:142-151`); only the native owner may use the FreeRTOS include and delay.
- Package: add every native file to `package_manifest.txt`. Add exact
  example build-output entries to `idf_component.yml` excludes (TCA9548A form)
  and the same set to `REQUIRED_COMPONENT_EXCLUSIONS`
  (`check_packaged_consumer.py:48-71`); narrow its `examples/` token check
  (`:737`) so it still rejects any other `examples/` reference in the manifest
  or root CMake. Keep the manifest description and token rules
  (`check_framework_boundaries.py:163-175`). Decide by packing after an in-tree
  build (then remove that output) whether `library.json` export needs exact
  example entries; if so, update `REQUIRED_EXPORT_EXCLUSIONS`
  (`check_framework_boundaries.py:68-91`) to match.
- Native package consumer (decision 13): add an option to
  `check_packaged_consumer.py` (for example `--payload-dir <path>`) that writes
  the payload it extracted from the validated `pio package pack` archive to a
  caller-given short path not named `MAX31865`, such as
  `C:\esp\build\max31865-pkg\payload` (never `%TEMP%`); build
  `examples/esp_idf/basic` from there with `idf.cmd`. In CI the
  `packaged-consumers` job uploads that payload as an artifact and the ESP-IDF
  job downloads it to a folder under `$GITHUB_WORKSPACE` (for example
  `pkg/payload`): the action mounts only the workspace and takes `path`
  relative to it (`action.yml` at `v1.2.0`). Fail, never skip, if the payload
  is incomplete. Arduino environments never compile native sources
  (`platformio.ini:21-23`, `library.json` `srcDir`).
- CI: add a matrix job (`esp32s3`, `esp32s2`, `fail-fast: false`) that needs
  `packaged-consumers`. Keep `runs-on: ubuntu-24.04`, which
  `check_version_metadata.py:211-213` requires. Use
  `espressif/esp-idf-ci-action@v1.2.0`, a tag like the rest of this file:
  `v1` is a branch (`9d38657f…`, the commit OPT4001 pins), while tag `v1.2.0`
  (`e6f5c742…`) differs from it only in `README.md` (checked with `gh api`
  2026-11-16; re-verify and record). Build `path: examples/esp_idf/basic` and
  the downloaded payload's `examples/esp_idf/basic` with
  `esp_idf_version: v6.0.1`. The IDF job installs no PlatformIO, so the two
  `platformio==6.1.19` lines (`:217-218`) and the one package cache line
  (`:223-224`) stay exact. Use `actions/upload-artifact@v4` (`ea165f8d…`) and
  `actions/download-artifact@v4` (`d3f86a10…`), both verified tags, and add
  every new `uses:` value to `CI_ACTIONS` (`:20-24`). Write `uses:` lines
  without trailing comments: the regex at `:214` skips such lines, and no
  `uses:` line may escape the set check. Keep every existing job, pin and
  trigger.

## Tests and acceptance

Keep 01's adapter tests and the core deadline tests. Add host owner tests on
the SDK fakes: setup failure at each acquisition unwinds in reverse; transport
setup failure leaves the CLI usable and `begin` action-free; the stdin
adapter keeps `BoundedSerialShell`'s 32-byte poll and overflow paths; teardown
ends the driver before removing owned resources and never deletes borrowed
ones. CLI behaviour stays covered by `test/diagnostic_cli_tests.cpp`. Assert
statuses, output validity and ownership, not incidental strings.

## Document before the final commit

Update README (`:3-7`, `:129-142`, `:144-169`), AGENTS (final scope including
the native CLI, checker and CI job), `docs/ESP_IDF.md` (actual commands, SDK
evidence, both residual risks, `>=6.0.1`), `docs/CLI.md`, `docs/TESTING.md`,
`docs/repository-contract.md`, `docs/KNOWN_LIMITATIONS.md`, R1 (software closed
with local evidence; R2 open) and CHANGELOG `[Unreleased]`. README and
`docs/ESP_IDF.md` state (decision 14): "`library.json` lists `espidf` because
the package ships the ESP-IDF component and native example; PlatformIO
`framework = espidf` builds are not tested (only `idf.py` 6.0.1 for
esp32s3/esp32s2)." Set the index header and rows as the index common rules say
(decision 12) and both prompt status lines to EXECUTED, so the CI-verified
commit already carries them. CI run identifiers go only to the handoff.

## Verify locally

Record each under the six index layers (host tests; static/contract checks;
native ESP-IDF build; package/clean consumer; remote CI; physical hardware) as
PASS/FAIL/NOT RUN/BLOCKED with tool versions:

- all static checks in `ci.yml:29-38` plus the new checker and its test;
  `python tools/run_native_suite.py` and both sanitizer variants where the
  local compiler supports them; `doxygen Doxyfile`; `git diff --check`;
- all twelve Arduino builds through `.\scripts\pio.cmd` (`README.md:163-168`);
- `C:\esp\idf.cmd 6.0.1 -C examples\esp_idf\basic -B C:\esp\build\max31865-<target>-6.0.1 -D SDKCONFIG=C:\esp\build\max31865-<target>-6.0.1.sdkconfig set-target <target> build`
  for `esp32s3` and `esp32s2`; then delete in-tree
  `dependencies.lock`/`managed_components/`;
- `python tools/check_packaged_consumer.py` with the payload option set to
  `C:\esp\build\max31865-pkg\payload`, then
  `C:\esp\idf.cmd 6.0.1 -C C:\esp\build\max31865-pkg\payload\examples\esp_idf\basic -B C:\esp\build\max31865-pkg-<target>-6.0.1 -D SDKCONFIG=C:\esp\build\max31865-pkg-<target>-6.0.1.sdkconfig set-target <target> build`
  for both targets, before the checkpoint.

## Remote CI checkpoint and handoff

Stage only reviewed owned files, commit, push and confirm sync. Pushing to
`main` triggers `ci.yml` (`:3-10`): find the run for that exact SHA
(`gh run list --commit <sha>`), dispatch `workflow_dispatch` only if none
started, and wait for terminal results. Fix confirmed failures without
weakening checks and repeat on the new final commit; an earlier green run never
covers a later commit. Hand off the base and final SHA, the CI run ID with every
job and conclusion for that SHA, and each evidence layer; physical NOT RUN. No
tag, release, publication or branch deletion.
