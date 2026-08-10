# Standalone diagnostic CLI

`examples/06_diagnostic_cli` is an application-owned Arduino bringup and
diagnostic tool. It owns the shared `SPI` instance, backend, driver, begin
configuration, and transport observer, then passes borrowed pointers into a
fixed-storage command engine. It creates no task, mutex, queue, dynamic command
object, or hidden recovery policy.

Build and upload `ex_diagnostic_s2` or `ex_diagnostic_s3`, connect at 115200
baud, and enter `help`. Review
`examples/06_diagnostic_cli/DiagnosticConfig.h` before flashing; its pins,
physical wire mode, notch filter, RREF, RTD R0, and input RC are example values.
Example 06 is self-contained and does not include files from `examples/common`.

## Input and safety contract

The serial shell stores at most 191 characters, consumes at most 32 received
bytes per Arduino `loop()` pass, accepts at most 12 whitespace-separated tokens,
and discards an overlong line as one error. The command parser performs its own
bounded 192-byte scan and rejects non-printable input (horizontal tab is the
only accepted control character). Parser errors still emit a correlated command
boundary. It uses fixed arrays, not Arduino `String`. `service()` advances at
most one due cooperative job step per loop.

Every hardware mutation, local configuration mutation, destructive diagnostic,
counter clear, job start, or job cancel requires the exact final token
`confirm`. Missing or misplaced confirmation rejects the command before the
operation. Commands neither retry nor invoke `recover()` implicitly. `begin`,
`recover`, `stop`, waits, fault cycles, and register operations use finite
driver deadlines.

The CLI attempts one automatic `begin()` at startup only when Arduino transport
setup succeeded. A later `begin confirm`, `recover ... confirm`, or `end
confirm` is always an explicit operator action.

## Command catalog

| Command | Arguments | Purpose |
| --- | --- | --- |
| `help` | `[command]` | Print the complete catalog or one command contract. |
| `format` | `human\|machine` | Select interactive text/prompt or stable `REC` records. |
| `version` | | Print library/git/target/framework/build identity and the hardware-evidence disclaimer. |
| `wiring` | | Print application-owned pins, selected SPI clock, wire mode, notch, and limits. |
| `timeout` | `[milliseconds]` | Print or set the bounded default used by later CLI operations. |
| `telemetry` | | Print uptime and valid/optional free-heap telemetry. |
| `state` | | Print lifecycle and cooperative-job state. |
| `health` | | Print passive health, counters, validity, and last status. |
| `counters` | `show` or `clear confirm` | Print accounting; clearing is rejected while a job is active. |
| `begin` | `confirm` | Initialize from the stored complete begin configuration. |
| `end` | `confirm` | Perform zero-I/O unbind/local reset. |
| `recover` | `[timeout_ms] confirm` | Reapply and verify the desired image. |
| `probe` | | Run the health-neutral read-only image probe. |
| `settings` | | Read typed CONFIG and threshold settings. |
| `offline` | `threshold confirm` | Set the passive-health failure-streak threshold, 1..255. |
| `bias` | `on\|off confirm` | Set desired idle VBIAS. |
| `wire` | `2\|3\|4 confirm` | Set the desired lead-compensation mode. |
| `filter` | `50\|60 confirm` | Set the notch filter. |
| `rtd` | `get` or `set rref r0 tau_us min_c max_c confirm` | Read/change local scaling, RC timing, and CVD domain. |
| `cvd` | `set a b c confirm` | Replace local CVD coefficients. |
| `start` | `confirm` | Start continuous conversion after bounded settling. |
| `trigger` | `confirm` | Arm one split one-shot conversion. |
| `stop` | `confirm` | Stop conversion/job and restore desired idle VBIAS. |
| `ready` | | Check DRDY or elapsed maximum exactly once. |
| `poll` | | Consume one fresh armed result or return `NoData`. |
| `read` | | Read buffered RTD registers immediately. |
| `wait` | `[timeout_ms]` | Wait for and consume an already armed result. |
| `one` | `[timeout_ms] confirm` | Run one complete bounded one-shot. |
| `stream` | `count interval_ms confirm` | Cooperative continuous job targeting 1..100000 successful samples; interval 1..60000 ms. |
| `stress` | `count confirm` | Cooperative bounded one-shot job from Ready; count 1..100000. |
| `job` | | Print job kind, progress, outcomes, and elapsed time. |
| `cancel` | `confirm` | Cancel the active job and run bounded stop cleanup. |
| `faults` | | Read and decode latched fault status. |
| `fault-clear` | `confirm` | Clear latched fault status. |
| `fault-auto` | `confirm` | Run a fresh automatic cycle; rejected when RC > 100 us. |
| `fault-manual` | `confirm` | Run the fully timed two-step manual fault cycle. |
| `thresholds` | `get raw\|ohms\|c` or `set raw\|ohms\|c low high confirm` | Read/set typed inclusive low/high thresholds. |
| `reg` | `read addr [length]`, `dump`, `write addr value confirm`, `verify addr value confirm`, `test confirm`, or `defaults confirm` | Validated Ready-only register diagnostics; an explicit span is bounded within 00h..07h. |
| `convert` | `code n`, `resistance ohms`, or `temperature c` | Run local status-returning conversion helpers. |

Integer input accepts the command parser's C-style base notation where
applicable, so a register address/value may be entered as decimal or `0x...`.
Floating input must be finite. Timeouts are 1..`INT32_MAX` milliseconds.

## Cooperative jobs

`stream` performs `startContinuous()` once, then each due service step calls
`poll()`. Its target counts successful samples, not polls. `NoData` is counted
separately and is not treated as a transport failure. A deterministic attempt
ceiling prevents a stuck DRDY or repeated error from leaving a finite stream
job active forever. The ceiling is
`1 + ceil((single_ms + elapsed_guard_ms) / interval_ms) + (count - 1) *
(1 + ceil(continuous_ms / interval_ms))`; reaching it before the sample target
ends the job with `reason=attempt_limit`. `elapsed_guard_ms` is 1 when DRDY is
absent (covering the millisecond clock-quantization guard) and 0 when wired
DRDY provides the completion proof. `stress` validates Ready before
creating the job and runs at most one `readOneShot()` per service step. Both
keep attempt/success/failure/no-data counts, the attempt limit, and the first
  failure status. Completion, `cancel confirm`, or `stop confirm` runs bounded
  cleanup whenever the job owns continuous conversion or left the driver
  outside Ready (including an armed timed-out stress one-shot).

While a job is active, the command table permits only explicitly marked
inspection/control commands. Other device/configuration commands are rejected
with guidance to use `job` or `cancel confirm`. In particular, `poll` is gated
so an operator cannot consume a stream sample outside the job accounting. The
CLI is cooperative, but the application still must choose an interval
compatible with conversion time and serial-output load.

Each job snapshots the current `timeout` value in its `JOB_BEGIN
operation_timeout_ms=...` record. Later `timeout` commands affect only future
commands/jobs, never an active job's acquisition or cleanup deadline.

Stress/stream results are diagnostic observations, not proof of analog
accuracy, long-term reliability, or machine safety.

## Machine records

Every parsed nonempty command is enclosed by newline-delimited
`COMMAND_BEGIN id=... command=...` and `COMMAND_END id=...` records, in human
and machine modes. The matching end record carries terminal success, numeric
and named status, detail, and elapsed milliseconds. Parsing/confirmation/job
gate rejections also close the boundary, so a host can correlate completion
without waiting for a prompt.

`format machine` disables the prompt and emits typed payload observations with
the prefix `REC`. Each synchronous command boundary contains exactly one
`REC result` carrying that command's ID; cache-only and observation commands
emit an explicit success result even when their payload needs no driver call.
Asynchronous job-step failures can later add results using the ID of the command
that started the job. Records cover results, samples, decoded faults, readiness,
settings, health/counters, transport callback accounting, registers,
conversions, and correlated job start/progress/end data. `REC result` includes
numeric/name status, `detail`, operation, and its owning command ID. Register
read/readback/test observations and the register-read/register-dump DRDY
warnings are also `REC` records; only `COMMAND_*` and `JOB_*` boundary records
intentionally omit that prefix.
Cooperative job records retain the command ID which started that job even
though later steps run outside the original command boundary. Host automation
can correlate `JOB_BEGIN command_id=...` with the matching `JOB_END`; both
stream and stress also emit progress/payload records between those boundaries.
Hosts must treat unknown fields/record kinds as forward-compatible additions
and must still impose their own command and overall deadlines.

The repository's bounded host parser is `tools/run_max31865_hil.py`; its
offline contract tests are `tools/test_run_max31865_hil_parser.py`. The runner
sends ordinary CLI commands directly and correlates the exact command/job IDs.
It defaults to read-only observations, gates conversion, fault-cycle, stress,
stream, and DRDY-acknowledging dump commands behind explicit flags, and leaves
all generated evidence unaccepted pending human review. See
<a href="TESTING.md">Testing</a> for the command line and claim boundary.

Human and machine output expose explicit status; neither mode converts a failed
sample into valid temperature data. ANSI coloring is not required by the
machine contract.

## Diagnostic side effects

`reg read addr [length]` emits one named register record per returned byte. Any
successful single or span read containing RTD register 01h or 02h emits an
explicit warning because that read acknowledges DRDY high. `reg dump` reads the
same RTD registers and emits its own acknowledgement warning.
`reg test confirm` deliberately changes one threshold byte temporarily and
restores/verifies it. `reg defaults confirm` restores writable POR values and
clears faults; it is not a silicon reset. Raw CONFIG command bits are rejected
by the driver in favor of typed timed commands. Threshold D0 is don't-care and
defined-bit verification masks it.

Fault-cycle and cable-fault limitations in
<a href="HARDWARE_BRINGUP.md">the hardware guide</a> remain applicable to CLI
results. A decoded fault is evidence from the device, not a complete system
safety diagnosis.
