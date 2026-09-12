# Testing

## Static and native gates

Install Doxygen and Graphviz (`dot`) before generating the documentation.
CI uses Doxygen 1.9.8 and treats documentation warnings as errors.

```bash
python scripts/generate_version.py --check
python tools/check_transport_contract.py
python tools/check_core_timing_guard.py
python tools/check_framework_boundaries.py
python tools/check_deleted_api.py
python tools/check_cli_contract.py
python tools/check_version_metadata.py
python tools/check_header_hygiene.py
python tools/check_firmware_integration_contract.py
python tools/test_run_max31865_hil_parser.py
python tools/run_native_suite.py
python tools/run_native_suite.py --sanitizer address
python tools/run_native_suite.py --sanitizer undefined
python tools/check_packaged_consumer.py
doxygen Doxyfile
git diff --check
```

The native suite uses an independent device-side model and scripted callback
failures. It covers protocol framing, register image behavior, transport
deadlines, DRDY/elapsed readiness, conversion math, threshold encoding, fault
cycles, public API contracts, and health accounting. Sanitizers are additional
host evidence, not target/HIL evidence.

`python tools/run_firmware_owner_fixture.py` compiles a consumer-neutral owner
around the real core and independent device model. Its
`FIRMWARE_OWNER_CALL_BUDGET_MS` is 20 ms, with a separate native scheduler
tolerance used only for the host contention assertion. The fixture proves DTO
publication only after valid data, whole-call instance serialization, the
instance-before-bus lock order, dedicated/shared host behavior, outer-deadline
clamping and post-return publication suppression, passive-health mapping, and
explicit-only recovery. It is an integration contract test, not target or HIL
evidence.

`tools/package_manifest.txt` is the immutable release payload. The packaged
consumer gate compares both the source tree and generated archive exactly to
that manifest, exercises malicious archive shapes before extraction, builds a
real native begin/configure/read/probe/end consumer, and compiles the copied
installed `06_diagnostic_cli` example for both ESP32-S2 and ESP32-S3. It does
not rely on repository-only include paths.

## Arduino target builds

On Windows use the repository wrapper so the existing PlatformIO Core is used
without an implicit install or upgrade:

```powershell
.\scripts\pio.cmd run -e ex_bringup_s2 -e ex_bringup_s3 `
  -e ex_continuous_s2 -e ex_continuous_s3 `
  -e ex_one_shot_s2 -e ex_one_shot_s3 `
  -e ex_fault_diagnostics_s2 -e ex_fault_diagnostics_s3 `
  -e ex_rtd_configuration_s2 -e ex_rtd_configuration_s3 `
  -e ex_diagnostic_s2 -e ex_diagnostic_s3
```

The exact PlatformIO and pioarduino platform versions are pinned in repository
metadata/CI. Builds prove header/source integration for ESP32-S2/S3, not actual
GPIO suitability or connected hardware behavior.

## Required HIL matrix

- ESP32-S2 and ESP32-S3 representative boards.
- PT100 and PT1000/reference-resistor fixtures, plus precision-resistor points.
- 2-, 3-, and 4-wire connections.
- 50 Hz and 60 Hz filters; one-shot and continuous modes.
- DRDY connected and omitted.
- Open/short/reference/voltage fault injection and threshold equality edges.
- Shared-SPI contention with the application arbiter.
- Power cycling, explicit recovery, timeout/cleanup, and long-duration soak.
- Logic-analyzer checks of SPI mode, CS framing, 5 MHz maximum, and fault-cycle
  timing.

Retain raw evidence and review it independently before claiming hardware
validation or release readiness.

## Serial HIL evidence runner

`tools/run_max31865_hil.py` drives the machine format of example 06 through
the documented `COMMAND_BEGIN`/`COMMAND_END` and `JOB_BEGIN`/`JOB_END`
boundaries. Its parser is tested offline in CI and rejects mismatched command
IDs, inconsistent status fields, incomplete jobs, missing typed evidence,
partial register dumps, and lines longer than its host-side bound.

Inspect the non-I/O plan first:

```bash
python tools/test_run_max31865_hil_parser.py
python tools/run_max31865_hil.py --dry-run
```

After flashing `ex_diagnostic_s2` or `ex_diagnostic_s3` with reviewed wiring,
a default live capture is read-only:

```powershell
python tools/run_max31865_hil.py --port COM6
```

Side effects require separate opt-ins. For example:

```powershell
python tools/run_max31865_hil.py --port COM6 `
  --include-sampling --sample-count 10 `
  --include-stress --stress-count 100 `
  --include-stream --stream-count 100 --stream-interval-ms 100
```

`--include-register-dump` acknowledges DRDY by reading the RTD registers.
`--include-fault-auto` is valid only for a fixture whose configured external
RC time constant is at most 100 us; `--include-fault-manual` runs the timed
two-step sequence. These flags do not inject a physical fault or validate a
fault fixture by themselves.

The runner writes raw serial, JSON, and Markdown under ignored `.pio/hil_logs/`.
Dry-run artifacts have verdict `DRY_RUN_ONLY`. A successful live serial parse
has verdict `SERIAL_CHECKS_PASSED_REVIEW_REQUIRED`, and every summary keeps
`hil_accepted` false. No runner invocation or hardware result is committed in
this repository, so the current repository contains no HIL evidence.
