# Prompt 02: ship the native application and clean consumer build gate

**Status: NOT IMPLEMENTED.** Audit mapping: R1 application/package/CI.
Prerequisite: [01](01_NATIVE_ADAPTER.md) has delivered and proved a conforming
native transfer path. Current source anchor:
`9937c886d5617bc8deca1d7db148a3fcf286a377`; historical reference:
`5263d9032b0185604696f777ef136558a8e3eb6e`.

Execute this implementation on the same branch as 01. Inspect its final diff,
tests and SDK evidence before proceeding; do not assume a status checkbox
proves bounded transfer cleanup. This prompt elects the maintained native
example scope, while the installed core remains neutral and application-owned.
Do not substitute an empty component build for the actual adapter/application.

## Source audit and failure mechanism

Read AGENTS, `.github/workflows/ci.yml`, R1 in the retained review,
`docs/ESP_IDF.md`, `docs/TESTING.md`, `docs/repository-contract.md`,
`tools/component_contract.py`, `check_framework_boundaries.py`,
`check_packaged_consumer.py`, `package_manifest.txt` and all files from 01.
Read current `MAX31865BeginConfig`, `MAX31865DeviceConfig`, transport and
Status declarations; use the current `MAX31865::begin`, `poll`, `readSample`,
`triggerSingleConversion`, `startContinuous`, `stop`, `recover` and `end`.
Use existing Arduino examples and `test/firmware_owner_fixture.cpp` as behavior
references, without compiling their Arduino glue into IDF.

At the source anchor root CMake already registers the neutral driver and
protocol sources, but no native SDK application is compiled in CI. The exact
package checker builds host/Arduino consumers, not native IDF. The historical
app uses automatic CS and obsolete begin/callback assumptions. Copying that
app or changing metadata alone would leave the current adapter unverified.

## Implement one small maintained application

1. Extend 01's native compile fixture under `examples/esp_idf/` into a real
   `app_main` bring-up example. Keep a single native application owner for
   SPI, adapter context, GPIO/DRDY, optional shared arbiter and driver. Allocate
   SDK resources during initialization only, check every setup return, and
   unwind partially created resources in reverse order. No default pin or
   scheduler ownership enters the library.
2. Use explicit board settings for CS/SCK/MISO/MOSI and optional DRDY, mode,
   frequency, wire/filter/RREF/R0 choices, finite operation deadlines and
   sleep cadence. Validate before asserting CS. Explain the mode/frequency
   and exclusive/shared-bus assumptions enforced by 01, including unsupported
   SDK cases, rather than inferring them from a successful driver begin.
3. Demonstrate configuration, a bounded one-shot sample and continuous
   `poll`/`readSample` ownership with explicit stop/recover. Retain honest
   validity and Status/fault output; do not print stale/uninitialized values
   as new temperatures or silently recover/retry a failed operation. `tick`
   remains zero-I/O elapsed-time bookkeeping; no second acquisition engine.
4. Keep the example finite or explicitly owner-looped with yielding cadence
   and an orderly stop path. On cancellation/failure, stop acquisition as the
   API allows, preserve cleanup errors, end the driver binding, then remove
   owned resources when safe. Never delete borrowed shared-host resources.
5. This is a diagnostic sample, not full Arduino CLI parity. Do not create a
   duplicate CLI framework. If commands are needed for the chosen sample,
   use a small fixed-buffer table and reject overlong/malformed input before
   hardware action; state the actual supported subset.

## Build and package integration

- Keep root CMake's two neutral sources and no peripheral/RTOS requirements.
  Put IDF SPI/GPIO/timer/RTOS dependencies on the native application component.
  Make the local component name stable independent of checkout directory;
  build once from a renamed checkout/package-consumer directory to prove it.
- Select explicit native SDK version(s) based on 01's source audit and builds.
  `idf_component.yml` currently says `>=5.3` for the neutral core; distinguish
  that core claim from the narrower adapter/example SDK support. Do not claim
  every allowed SDK was tested or copy the historical SDK requirement.
- Extend existing CI with real S2/S3 native builds for the supported matrix,
  preserving all current static/native/ASan/UBSan/Doxygen/12-Arduino/package
  jobs and their triggers. Reuse the minimal fixture instead of parallel
  redundant applications. Pin the SDK used and retain actionable build logs.
- Extend exact package/installed-consumer checks to compile the shipped native
  example against an extracted archive in a clean consumer. Add precise
  intended files to `package_manifest.txt` and payload validation. Check that
  native sources never get compiled by Arduino environments accidentally.
  Keep neutral headers consumable without either framework.
- `library.json` and the component manifest exclude `docs`; packaged README
  links must remain absolute GitHub links. Keep prompt/review docs out of
  the distributable and leave generated files to the version generator.

## Acceptance tests and commands

Preserve 01's callback/fault tests. Add owner-level tests for initialization
failure at each acquisition, successful sample delivery, no-data versus
fault/transport failure, DRDY absent, stopped acquisition, explicit recovery,
cleanup failure, and stop/cancellation during the supported owner sequence.
Assert status/output validity and resource ownership, not incidental strings.
Use the existing firmware fixture and test harness where they fit.

Run all existing gates in `.github/workflows/ci.yml`, including
`python tools/run_native_suite.py`,
`python tools/run_native_suite.py --sanitizer address`,
`python tools/run_native_suite.py --sanitizer undefined`,
`doxygen Doxyfile`, `python tools/check_packaged_consumer.py`, generated and
static checks, and all twelve pinned Arduino example builds. On Windows use
the existing `scripts/pio.cmd`. Build the actual native app for both targets
with `idf.py -C <native-app> -B <build-s2> set-target esp32s2 build` and
`idf.py -C <native-app> -B <build-s3> set-target esp32s3 build` using separate
clean build directories; record actual paths/SDK
commands in `docs/ESP_IDF.md` rather than leaving this placeholder in guides.

This is a substantive integration/build change: push completed owned changes,
verify CI for the exact final commit and fix confirmed failures without
weakening checks. Report local checks not available and their CI equivalents
honestly. No hardware run is implied by these tests.

Update AGENTS scope, README, `docs/ESP_IDF.md`, `docs/KNOWN_LIMITATIONS.md`, testing
and repository guides, Unreleased changelog, R1 reconciliation and this index
with final SDK/build/test evidence. R1 software closes only when the real
native consumer works under its documented contracts; R2 hardware acceptance
stays separate. Do not restore historical reports as current API guidance,
delete refs, publish, or tag a release. Stage only owned work, commit/push on
the existing branch without force and confirm upstream synchronization.
