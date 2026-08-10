# Contributing

Keep changes focused, deterministic, and covered at the same public boundary
they affect. Preserve caller ownership of SPI, pins, scheduling, and policy.

Before opening a pull request, run the static checks and native/sanitizer suites
listed in <a href="docs/TESTING.md">the testing guide</a>, build every ESP32-S2/S3 example,
run the packaged-consumer check, generate warning-clean Doxygen, and keep
`git diff --check` clean.

Public API or behavior changes require matching Doxygen, README/guides,
examples, tests, and CHANGELOG entries. Register, timing, fault, or electrical
claims must cite the current official Analog Devices document. Do not edit
`include/MAX31865/Version.h` directly; update `library.json` and run the version
generator.

Hardware-dependent changes need reviewed HIL evidence with board revision,
wiring, firmware commit, environment, raw logs/traces, result, date, and
reviewer. Target compilation alone is not HIL evidence.
