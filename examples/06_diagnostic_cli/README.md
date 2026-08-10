# MAX31865 diagnostic CLI

This example is an application-owned, bounded diagnostic surface for real
hardware. It initializes the shared `SPI` object, binds the Arduino adapter,
and then gives the framework-neutral driver a borrowed transport. It neither
simulates readings nor creates a task, mutex, or heap-backed command object.

Enter `help` for the table-driven command catalog and `help <command>` for one
command. Hardware, driver-state, job-control, and local-configuration mutations
require a final `confirm` token.
`timeout [milliseconds]` changes the bounded default used by later CLI work;
`telemetry` reports uptime and optional free heap. `version` includes target,
framework, build kind, git metadata, and an explicit unvalidated-hardware
evidence marker, while `wiring` reports both selected and maximum SPI clocks.
The wiring record also captures the configured physical RTD wire mode and
50/60 Hz notch selection.
`stream` and `stress` are cooperative jobs: `loop()` advances at most one
sample attempt, while `job` reports progress and `cancel confirm` performs
bounded cleanup. A stream targets successful samples and also has a finite
timing-derived attempt ceiling. Full one-shot acquisition uses `one
[timeout_ms] confirm`. `format machine` selects stable `REC ...` records
suitable for a host/HIL harness. Every parsed command, including a rejected
one, is delimited by correlated `COMMAND_BEGIN`/`COMMAND_END` records, and jobs
add `JOB_BEGIN`/`JOB_END`.

`reg read addr [length]` provides bounded single or contiguous register
diagnostics. A successful read touching RTD address 01h or 02h explicitly warns
that it acknowledged DRDY; `reg dump` has the same documented side effect.

Before flashing, edit this example's local `DiagnosticConfig.h`, including its
wire mode and notch filter. The directory is self-contained apart from the
installed MAX31865 library and Arduino's SPI support; it does not depend on
`examples/common`. DRDY is optional; when connected it is the exclusive
readiness source. There is no elapsed-time fallback for a stuck or miswired
DRDY signal.
