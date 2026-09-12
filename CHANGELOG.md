# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

`1.0.0` is the planned first release. No project version has been published or
validated on real MAX31865 hardware. These changes remain unreleased until the
required HIL matrix passes, its real-hardware results are accepted, and
publication is explicitly authorized.

### Fixed

- Documentation CI installs Graphviz for the generated diagrams and selects
  the root README by path, so nested README files do not become duplicate
  main pages in Doxygen 1.9.8. Warning checks remain enabled.
- Removed an extra trailing blank line in `Protocol.h` that failed the
  committed-tree whitespace check.

### Added

- Framework-neutral, noncopyable synchronous `MAX31865` driver with borrowed
  `MAX31865Transport` callbacks and zero-I/O construction, destruction, and
  `end()`.
- Guarded Arduino SPI/GPIO/timing adapter with caller-owned SPI initialization,
  wiring, settings, lifetime, and optional shared-bus arbitration.
- Complete status-first lifecycle, desired/observed configuration model,
  passive health snapshot, saturating transport/sample/fault counters, explicit
  `probe()` and manual `recover()`.
- Typed 2-/3-/4-wire, 50/60 Hz, VBIAS, one-shot, continuous, DRDY/elapsed-ready,
  raw-register diagnostic, threshold, and automatic/manual fault-cycle APIs.
- PT100, PT1000, and custom RTD/RREF support with bounded IEC 60751 and custom
  monotonic Callendar-Van Dusen conversion helpers.
- Audited command table and standalone protocol codec covering all eight
  registers, defined-bit verification masks, 15-bit RTD/threshold encoding, and
  documented timing limits.
- Six focused Arduino examples, including a self-contained table-driven
  diagnostic CLI with fixed storage, exact confirmation, stable machine
  records, cooperative finite jobs, and transport counters.
- Independent native device model, scripted transport/failure injection, strict
  public API/protocol/deadline/conversion/fault/CVD/CLI tests, sanitizer runners,
  and event-transcript overflow guards.
- Exact-pinned ESP32-S2/S3 PlatformIO matrix, core-only ESP-IDF component
  metadata, deterministic version generation, strict header/framework checks,
  warning-clean Doxygen, immutable package manifest, archive-safety tests, and
  clean installed-consumer builds.
- Architecture, Arduino, ESP-IDF callback, CLI, bringup, testing, limitations,
  repository-contract, and official-data-sheet audit documentation.

### Changed

- SPI host configuration, pins, task ownership, serialization, retry policy,
  logging, persistence, and machine safety are application responsibilities;
  the core owns only bounded chip-level transactions and state.
- Every Arduino example now selects its physical wire mode and 50/60 Hz notch
  explicitly in application-owned configuration; CLI wiring evidence records
  both selections and the selected SPI clock.
- Every fallible public operation returns `MAX31865Status`; validation and
  ordinary not-ready results remain health-neutral while transport/device
  failures use one common tracked path.
- Fresh-result handling now distinguishes buffered data from a newly armed
  conversion, uses wired DRDY exclusively when present, and phase-locks the
  no-DRDY continuous cadence.
- Split and composite one-shot reads restore the configured idle VBIAS image
  before reporting `Ready`; ambiguous CS/transfer outcomes require bounded
  quarantine and explicit recovery.
- Diagnostic settings reads avoid RTD registers so they do not acknowledge
  DRDY; full register dumps explicitly document that side effect.

### Fixed

- Managed SPI sessions now enforce the whole-operation deadline before and
  after lock, CS, transfer, timing, and unlock boundaries; an already-uncertain
  CS is driven high while the shared bus is still owned even when lock
  acquisition consumes the remaining deadline.
- Transport callback results now preserve only documented role statuses,
  replace null messages with static fallbacks, retain unexpected original
  codes in `detail`, and distinguish action-free `InvalidState` from a failed
  required CS deassertion.
- Rejected lifecycle/argument requests no longer sample the transport clock,
  health timestamps use the captured operation start, and timeout zero means
  one immediate no-wait attempt. `readSingle(0)` checks readiness once and
  returns health-neutral `NoData` without sleeping when no result is ready.
- Manual fault detection waits both internal step-1 phases plus at least five
  external RC time constants, resolves interrupted manual cycles, and never
  reports `Ready` while FORCE- may remain open.
- Automatic fault detection rejects external RC constants above 100 us and all
  diagnostic cycles clear stale latched faults before reporting fresh results.
- Threshold LSB D0 is treated as data-sheet-defined don't-care during
  verification.
- Starting a conversion first acknowledges stale RTD data and proves wired
  DRDY returned high, preventing an old low level from satisfying a new start.
- First continuous conversion uses the one-shot maximum; subsequent periods use
  the documented continuous cadence.
- Cleanup preserves the primary failure, performs only a finite zero-wait
  safety sequence after deadline expiry, commits caller outputs and sample
  timestamps only after a successful required restore, and retains timeout
  provenance inside nested/composite `RestoreFailed` results.
- Required sleeps are rechecked at their final callback boundary, preventing
  callback-owned waiting from starting after the remaining budget is consumed;
  late readiness/timestamp observations cannot publish a sample as successful.
- Callendar-Van Dusen conversion rejects values outside the configured domain,
  round-trips float endpoints and maximum ADC codes, and accepts valid custom
  curves with zero-valued polynomial terms.
- Read-only probe no longer writes threshold registers or changes health/last
  operation, avoiding false latched threshold faults during acquisition.

### Removed

- Direct Arduino, global SPI, FreeRTOS mutex, GPIO, and bus-initialization
  ownership from the core driver.
- Pre-release bool/error-side-channel APIs, legacy monolithic examples/CLI,
  dynamic `String` parsing, and reset terminology that implied nonexistent
  MAX31865 reset hardware or a software-reset command.
