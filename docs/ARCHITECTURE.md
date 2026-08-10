# Architecture and ownership

```text
application owner context
  -> optional guarded Arduino backend or custom platform adapter
  -> borrowed MAX31865Transport callbacks
  -> synchronous MAX31865 state machine
  -> canonical MAX31865 protocol codec
```

## Ownership boundary

The application owns physical wiring, SPI host/device setup, clock choice,
resource lifetime, driver-instance and shared-bus serialization, scheduling,
retries, recovery decisions, logging, telemetry, persistence, and safety
policy. The core owns one chip-level operation at a time: manual CS framing,
bounded transport sessions, register images, conversion/fault timing, sample
freshness, conversion math, lifecycle, and passive health.

Transport callbacks are borrowed and perform exactly one attempt. They must not
retry, recover, re-enter the same driver, or retain frame-buffer pointers after
return. The callback table is copied, while `user` and every reachable resource
must remain alive until `end()` or destruction. The core has no framework
headers, task, queue, logger, ISR bridge, or steady-state allocation.

## Serialization

Driver methods are not internally thread-safe. One owner context may call an
instance directly. Multiple contexts require an application-owned instance
mutex around each complete public call. A transport `lockBus`/`unlockBus` pair
protects one SPI transaction and must use the same arbiter as every other
client of that host. Arduino `beginTransaction()` changes settings; it is not a
cross-task mutex. The fixed lock order is instance ownership first, bus arbiter
inside the driver call.

Both bus callbacks must be null for a dedicated/single-owner bus, or both must
be present. Lock attempts receive the remaining whole-operation budget; zero
permits only an immediate attempt.

## Transaction and deadline model

Each public protocol operation has one modulo-2^32 millisecond deadline.
Internal configuration writes, CS setup/hold/inactive delays, sleeps, readiness
checks, and normal cleanup share that budget. After a mutation or conversion
frame may already have committed, an expired composite operation may make one
finite safety-cleanup sequence with timeout zero. That exception gives lock and
transfer callbacks no wait budget, permits only the required fixed frames and
CS timing, and permits no device sleep or retry. A manual fault cycle that still
needs its mandatory FORCE- settling time therefore remains explicitly `Fault`
until `recover(timeout)` receives a new caller-owned deadline; it is never
advanced early. `begin()` has one other explicit exception: its optional
`powerReadyDelayMs` sleep occurs before the default protocol deadline starts,
so total begin wall time includes both bounds. There are no hidden retries or
unbounded waits. On the few immediate-read APIs that accept zero, it is the
same no-wait sentinel rather than an elapsed-time deadline: lock and transfer
callbacks receive zero, no readiness sleep is allowed, and the one fixed frame
plus mandatory CS timing may still finish if the millisecond clock ticks. The
maximum frame is nine bytes. Every transaction owns the sequence:

1. acquire the optional shared-bus arbiter;
2. establish CS high and the data-sheet inactive interval;
3. assert CS, observe setup time, and transfer one contiguous frame;
4. observe hold time and deassert CS, committing any command on that edge;
5. release the shared bus exactly once.

A CS-control failure can put the lifecycle in `Fault` because command framing
is no longer authoritative. Callbacks map framework errors to
`MAX31865Error`. The driver preserves documented role-specific results and
health-neutral, action-free `InvalidState`, while normalizing any other
callback error to the appropriate bus-lock, chip-select, transfer, or GPIO
failure. A required deassertion after CS may already be active is the contextual
exception: rejecting that safety action with `InvalidState` cannot prove CS
high, so the driver reports and counts `ChipSelectFailed` while retaining the
original code in `detail`. The rejected callback itself is still treated as
action-free; the fault comes from the pre-existing framing uncertainty.

## Desired and observed state

The desired image is the complete application-requested persistent device
configuration. The observed image contains only register bytes proven by
successful readback. Configuration validates a scratch candidate before I/O,
writes in a fixed safe order, verifies defined bits, then commits desired and
observed state. An ambiguous partial mutation invalidates observed state and
requires explicit recovery.

MAX31865 lacks an ID register, RESET pin, and software-reset command. `probe()`
therefore performs read-only consistency checks and is health-neutral.
`recover()` establishes CS high, proves normally-off CONFIG, and reapplies the
stored desired image. It never restarts acquisition implicitly.

## Sampling freshness

One fresh-result barrier distinguishes an armed conversion from the device's
buffered previous RTD value. Reading RTD registers acknowledges DRDY high, so
start paths first flush RTD data and then arm a new conversion. With DRDY wired,
only the pin proves a new result; after the maximum horizon a bounded
FAULT_STATUS read can instead expose D2 ADC halt. Without DRDY, documented
maximum elapsed times plus a 1 ms clock-quantization guard and the same D2 check
are used. `tick()` only advances elapsed-time bookkeeping and never reads GPIO.
The modulo schedule must be serviced at least once per 2^31 ms half-range.

Continuous mode retains one latest-result opportunity rather than a queue.
Elapsed timing can detect likely overwritten periods and increments the overrun
counter; lossless acquisition and DRDY edge timestamps remain application
responsibilities.

## Status, output, and health boundaries

Public outputs are assembled in fixed local storage and committed only after
the operation has enough valid evidence. A faulted sample is the deliberate
exception: raw frame and decoded-fault fields may be committed with validity
flags while the terminal result is `DeviceFault` and data validity remains
clear.

One terminal operation result updates `lastOperationStatus()` and at most one
tracked success/failure. Lifecycle, argument, and timeout validation occurs
before the protocol operation is created; a rejected precondition invokes no
transport callback and ordinary `NoData` likewise does not count as a transport
failure. `lastOkMs` and `lastErrorMs` use the one operation-start timestamp
captured before the first protocol callback, not a second completion-time clock
read. Health counters and timestamps are saturating or explicitly
validity-qualified. `health()`, `state()`, and `end()` invoke no callbacks.

## Firmware boundary

A firmware acquisition owner should publish an application-local fixed-size
DTO, not a `MAX31865` object or a borrowed transport. It accepts engineering
data only after `status.ok()` and `MAX31865_SAMPLE_FLAG_DATA_VALID`; otherwise
it maps the explicit status and passive health to its own event policy.
Filtering, calibration persistence, stale-data acceptance, alarms, motion
interlocks, and actuator control remain outside this sensor library.

The compiled firmware-owner fixture in `test/firmware_owner_fixture.cpp`
demonstrates that boundary without becoming library code. It takes the
application instance mutex around each whole public call, then lets the driver
take the transport bus arbiter. It clamps each driver timeout to an outer request
deadline, rejects requests that cannot cover the driver's stored
`poll()` budget, and performs publication suppression if the outer deadline
expires after a successful driver return. Driver errors do not retry or recover
implicitly; recovery requires explicit recovery authorization from the
application. Dedicated-host and shared-host paths are exercised separately.
