# MAX31865 native ESP-IDF implementation prompts

**Status: NOT IMPLEMENTED.** These prompts are plans, not evidence. Writing or
revising them changed no driver, adapter, example, package contract, checker
or CI.

Prepared 2026-10-25 from `main` `9937c886d5617bc8deca1d7db148a3fcf286a377`.
Revised 2026-11-16 at `main` `8a0c24b3a295fb46676bcbf34da40652138c4cbb`. The
five commits between them change only these prompts and links to them, agent
guidance, author metadata, the `CLAUDE.md` package exclusion (one entry in two
checker sets) and `.gitignore`; nothing under
`src/`, `include/`, `examples/`, `test/`, `scripts/` or `.github/`, and neither
root `CMakeLists.txt` nor `platformio.ini`, changed. The retained historical
port `origin/feature/idf-port` = `5263d9032b0185604696f777ef136558a8e3eb6e` is a
read-only reference. SHAs are evidence anchors, not reset instructions: at entry
fetch, record the actual HEAD and re-read every cited line.

## Scope election

The owner elected native ESP-IDF support on 2026-11-16: a maintained native
example with an example-owned SPI adapter and the existing diagnostic CLI.
Executing this set authorizes the AGENTS, README, docs, checker/guard, package
and CI edits listed in [01](01_NATIVE_ADAPTER.md). Do not ask for another scope
decision. Do not merge, cherry-pick or restore the historical port or its APIs;
build on the current core. The installed core stays framework-neutral and
callback-only; no public backend, bus manager, retry engine or Arduino
compatibility layer is added.

| Audit item | Prompt | Requires | Status / scope |
| --- | --- | --- | --- |
| R1 transport, ownership, SDK limits, guards | [01 Native adapter](01_NATIVE_ADAPTER.md) | none | NOT IMPLEMENTED. Example-owned adapter, SDK-fake tests, S3/S2 builds, scope election. Local checks only. |
| R1 application, CLI, package, CI | [02 Native application and CI](02_NATIVE_APPLICATION_AND_CI.md) | 01 complete | NOT IMPLEMENTED. Native diagnostic app with full CLI parity, contract checker, package and CI job, and the single remote-CI checkpoint. |
| R2 physical qualification | none | - | Separate hardware acceptance (`../HARDWARE_BRINGUP.md`, `../TESTING.md`). No board is available; physical evidence stays NOT RUN. |
| Historical neutral-core refactor, component, wrapper | none | - | Already equivalent on main ([retained review](../RETAINED_BRANCH_AUDIT.md)). |

## Fixed decisions (owner-delegated, 2026-11-16)

Numbers follow the owner's and lead's decision list shared with the sibling
sets: 1 is the scope election above, 11 and 12 are in the common rules below,
18 forbids literal placeholders, and 7, 8 and 15-17 concern other
repositories. A prompt reopens a decision only if a primary source contradicts
it; record that source and the consequence.

- **2 SDK.** Local builds and CI pin ESP-IDF 6.0.1 (`C:\esp\v6.0.1`, tag
  `v6.0.1`) for `esp32s3` and `esp32s2`. `idf_component.yml:12` changes from
  `">=5.3"` to `">=6.0.1"`, with a CHANGELOG compatibility note. No older-SDK
  claim. 6.1 (and the 5.5.5 inspected in 2026-10) is source evidence only.
- **3 Toolchain.** Only `C:\esp\idf.cmd 6.0.1 <idf.py args>` with
  `-B C:\esp\build\max31865-<target>-6.0.1` and
  `-D SDKCONFIG=C:\esp\build\max31865-<target>-6.0.1.sdkconfig`. Never install
  ESP-IDF globally, run Espressif's installer or set `IDF_PATH`/`IDF_TOOLS_PATH`
  in Windows settings (`../../../AGENTS.md:24-38`, `C:\esp\README.md`).
  `.gitignore:9-16` already ignores ESP-IDF output; do not re-add it.
- **4 Layout.** `examples/esp_idf/basic/` with `CMakeLists.txt`, `main/`,
  `sdkconfig.defaults*`, `README.md` and a fixed-name local wrapper component
  `components/MAX31865/` that registers the root sources, so root
  `CMakeLists.txt` stays byte-identical (`tools/component_contract.py:10-19`,
  `tools/check_framework_boundaries.py:157-161`). No `override_path`. Console:
  sdkconfig console plus non-blocking `getchar`. Identity: example-only
  CMake-generated header. Contract checker: `tools/check_idf_example_contract.py`.
  Guide: `docs/ESP_IDF.md`.
- **5 CLI.** Full command parity through the one framework-neutral engine
  `examples/06_diagnostic_cli/Max31865Cli.cpp`, used by both shells. Fix its
  Arduino-specific string. `tools/check_cli_contract.py` pins both shells.
- **6 SPI timeout semantics** (same text as ADS1261_ESP32):

  > ESP-IDF 6.0.1 `spi_device_acquire_bus` and `spi_device_polling_start`
  > accept only `portMAX_DELAY`
  > (`C:\esp\v6.0.1\esp-idf\components\esp_driver_spi\src\gpspi\spi_master.c:1317,1381`;
  > same in 6.1). The adapter therefore uses one dedicated SPI host/device owned
  > by the application, polling transactions, and frames bounded by the library
  > maxima stated below, and documents that the per-callback timeout budget is
  > honoured only by that hardware bound, not by a cancellable wait. Per the
  > repos' own rule ("if the selected primitive cannot honor that budget, the
  > callback must not claim that it can"), the adapter must not report a timeout
  > it cannot enforce; record the residual risk (a stuck bus blocks the owner
  > task) in the IDF doc. A shared bus with other devices is out of scope for
  > the example. If execution finds a conforming finite-wait path, it may use it
  > with evidence.

  MAX31865 maxima: `max31865_cmd::MAX_FRAME_BYTES` = 9 bytes (72 SCLK) and
  `max31865_cmd::SPI_MAX_HZ` = 5 MHz (`include/MAX31865/CommandTable.h:24,28`),
  mode 1 or 3, MSB-first. The adapter accepts an actual SCLK
  (`spi_device_get_actual_freq`, kHz) from 100 kHz to 5 MHz. Worst case per
  frame: 720 us at 100 kHz, 14.4 us at 5 MHz, plus SDK setup. Residual risk
  beyond the quoted one: `spi_device_polling_end` busy-waits without yielding
  (`spi_master.c:1433-1439`), so a stuck transfer also starves lower-priority
  tasks, including IDLE (task watchdog). A dedicated host leaves both lock
  callbacks null (`include/MAX31865/Transport.h:43-45`). This repository's form
  of the budget rule is `Transport.h:70,89-90`; prompt 01 records the remaining
  SDK evidence and adds the quoted rule sentence to `docs/ESP_IDF.md`.
- **9 CI.** Remote CI dispatch and waiting happen once, in prompt 02, on its
  exact final commit, as one batched major change (`AGENTS.md:8-19`). Prompt 01
  uses local checks only; pushes to `main` still trigger CI
  (`.github/workflows/ci.yml:3-10`), but 01 neither waits on nor claims them.
- **10 Evidence.** Report six layers separately as PASS/FAIL/NOT RUN/BLOCKED
  with command, tool/SDK version, commit and tree state: host tests;
  static/contract checks; native ESP-IDF build (S2/S3); package/clean consumer
  (including the native payload build); remote CI (prompt 02 only); physical
  hardware NOT RUN. A linked image proves linking only.
- **13 Packaged native consumer.** CI builds the example from the
  `pio package pack` archive that `tools/check_packaged_consumer.py` validated
  and extracted, handed to the ESP-IDF job as an artifact and placed in a
  folder under `$GITHUB_WORKSPACE` (the action mounts only the workspace) not
  named `MAX31865`; it fails, never skips, if the payload is
  incomplete. The same build runs locally before the checkpoint from a short
  caller-given path such as `C:\esp\build\max31865-pkg\payload`, never `%TEMP%`.
- **14 `espidf` in `library.json`.** It is already listed (`library.json:36-39`,
  pinned by `tools/check_framework_boundaries.py:182-184`). Prompt 02 states in
  README and `docs/ESP_IDF.md`: "`library.json` lists `espidf` because the
  package ships the ESP-IDF component and native example; PlatformIO
  `framework = espidf` builds are not tested (only `idf.py` 6.0.1 for
  esp32s3/esp32s2)."

## Execute one prompt

```text
Execute docs/prompts/<01_NATIVE_ADAPTER.md|02_NATIVE_APPLICATION_AND_CI.md>
completely. Follow AGENTS.md, the parent ..\AGENTS.md and docs/prompts/README.md
(decisions and common rules). Inspect git status, branch and upstream first,
fetch, preserve unrelated changes and work on main. Re-audit the named sibling
native examples read-only. Implement, test and document; run the prompt's
local checks with C:\esp\idf.cmd 6.0.1; commit only owned files, push and
verify upstream equality. Report PASS/FAIL/NOT RUN/BLOCKED per evidence layer.
```

## Common rules

- Work on `main` (`AGENTS.md:21-34`). Fetch and fast-forward only when safe;
  preserve unrelated or concurrent work; never stash, discard, force-push or
  rewrite history. With subagents the lead owns all Git mutations.
- Sync means: stage only reviewed owned files, commit, push, fetch again and
  confirm local `HEAD` equals `origin/main` with zero ahead/behind. Record the
  base SHA before committing and give the resulting SHA in the handoff, never
  inside the committed files. On a sync failure keep the local commit and
  report its SHA and the exact error.
- On Windows run PlatformIO only through `.\scripts\pio.cmd` (`AGENTS.md:36-40`).
- Inspect `.github/workflows/ci.yml` first and reuse its commands. Never weaken
  a check of shipped behaviour; edit a guard so it enforces the new design.
- Keep `docs/prompts/` out of distributables (`idf_component.yml:31`,
  `library.json:69`). Avoid legacy API names in new code and docs:
  `tools/check_deleted_api.py:85` scans `examples/`, `test/`, `tools/` and
  `docs/`.
- Execution records go to CHANGELOG `[Unreleased]`, the affected guides and the
  handoff, not to this index; here only the status lines change.
- After execution this folder stays in `docs/prompts/` as executed historical
  requirements (TUSB32x precedent); never delete or archive it. The last prompt
  sets this header to `**Status: EXECUTED (historical requirements, not open
  work); hardware NOT RUN.**` and every row to DONE; packages keep excluding
  `docs/prompts/`.
