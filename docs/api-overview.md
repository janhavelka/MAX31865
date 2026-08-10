# API overview

## Public headers

- `MAX31865/MAX31865.h`: synchronous driver and complete public operations.
- `MAX31865/Config.h`: device, RTD, threshold, begin, settings, and probe types.
- `MAX31865/Status.h`: statuses, lifecycle, sample flags, faults, and health.
- `MAX31865/Transport.h`: framework-neutral borrowed callback contract.
- `MAX31865/ArduinoBackend.h`: guarded Arduino-only adapter.
- `MAX31865/Protocol.h`: canonical frame codecs for diagnostics and tests.
- `MAX31865/CommandTable.h`: audited register, bit, and timing constants.
- `MAX31865/Version.h`: generated stable semantic-version constants.

The core public path does not include Arduino, ESP-IDF, FreeRTOS, or SPI host
types. `ArduinoBackend.h` is visible only for Arduino builds.

## Construction and lifecycle

`MAX31865` is noncopyable and nonmovable. Construction performs no callback.
`begin(MAX31865BeginConfig)` copies a callback table and desired configuration,
but borrows the transport context. The driver does not own or initialize the
SPI host. `end()` and destruction are zero-I/O and do not attempt cleanup; call
bounded `stop()` explicitly while the transport is still valid when normally-off
hardware state is required.

An optional `powerReadyDelayMs` is a single begin-only pre-protocol sleep. It
runs before `defaultOperationTimeoutMs` starts, so total `begin()` wall time is
bounded by the configured power-ready delay plus the protocol deadline.
Every nonzero public operation timeout must be at most `INT32_MAX`
milliseconds; this keeps all deadline comparisons unambiguous across unsigned
millisecond-counter wraparound.

An armed one-shot cannot be cancelled by a later CONFIG write. `stop()` waits
for a proven result, discards freshness, and then proves the normally-off
image. The ordinary bound is the remaining documented 55/66 ms horizon, but a
protected-input D2 voltage condition halts ADC updates and can extend it. A
readiness timeout or DeviceFault leaves the conversion armed; `readSingle()`,
`stop()`, or explicit `recover()` can resolve it after the condition clears.

Stable lifecycle values are `Uninitialized`, `Ready`, `Converting`, and
`Fault`. Synchronous work does not expose transient states. `Fault` means device
selection or observed configuration is uncertain; `recover(timeoutMs)` is the
explicit tracked path that reapplies the stored desired image. The MAX31865 has
no device-ID register, reset pin, or software-reset command.

## Status and health

Every fallible method returns:

```cpp
struct MAX31865Status {
  MAX31865Error code;
  const char* msg; // Static storage only.
  int32_t detail;
};
```

Validation failures, `NoData`, and an ordinary high DRDY observation are not
transport failures. One public operation records at most one tracked success
or failure. `MAX31865Health` provides passive zero-I/O lifecycle and
READY/DEGRADED/OFFLINE state, desired/observed validity, saturating counters,
last-status/fault evidence, and explicitly valid timestamps.

`probe(out)` reads CONFIG, both thresholds, and FAULT_STATUS without writing and
without changing health accounting. It reports whether the writable image
matches the driver's desired image; it cannot authenticate device identity.

## Configuration

Use `max31865DefaultBeginConfig()` and replace every board-specific value:

- `transport`: complete borrowed callback table.
- `initialDeviceConfig`: wire mode, 50/60 Hz notch, idle VBIAS, raw thresholds.
- `rtd`: measured RREF, RTD R0, coefficients, fitted input RC, conversion domain.
- `powerReadyDelayMs`: optional begin-only delay outside the protocol deadline.
- `defaultOperationTimeoutMs`: deadline for no-timeout public methods.
- `offlineThreshold`: nonzero passive-health failure streak threshold.

The default RTD is IEC 60751 PT100 with 400 ohm RREF and a -200 to +850 degree C
domain. PT1000 typically uses a 4 kohm RREF. Custom positive R0 is supported;
the configured curve must be finite, strictly increasing in its domain, and fit
below the representable `32767/32768 * RREF` limit. The recommended RREF range
validated by the driver is 350 ohm to 10 kohm.

Typed configuration operations are Ready-only. A partial or ambiguous write
cannot silently commit a new desired image: observed state becomes uncertain,
the lifecycle enters `Fault`, and application policy explicitly chooses
`recover()` or `end()`.

Only three-wire compensation has a device CONFIG bit. Two-wire and four-wire
both encode that bit clear and differ primarily in physical routing; the driver
retains the requested two/four-wire enum in its desired local image because a
register read cannot distinguish them.

## Sampling

`startContinuous(timeoutMs)` clears stale faults, enables VBIAS, waits the
configured input-filter settling interval, reads RTD data to acknowledge a
possibly stale DRDY indication, and starts continuous conversion. The first
result uses the one-shot maximum; later periods use the continuous maximum.

`tick(nowMs)` performs only elapsed-time readiness/overrun bookkeeping and no
callback. Without DRDY it must be serviced at least once per signed modulo
half-range (less than 2^31 ms, about 24.9 days). `poll(out)` checks readiness
once and returns `NoData` without a transport-failure penalty when no fresh
result exists. `readSingle()` waits for an already armed conversion; with
`timeoutMs == 0` it performs exactly one readiness check, never sleeps, and
returns `NoData` if the result is not already available.
`readOneShot()` triggers, waits, reads, and restores the desired idle VBIAS
state under one whole-operation deadline.

With a DRDY capability, the active-low pin is readiness-authoritative. A high
level remains not ready after the maximum; the driver does not replace it with
an elapsed guess. At/after that horizon it may read FAULT_STATUS once to expose
a D2 ADC halt before a bounded wait otherwise returns `DrdyTimeout`. Without
DRDY, the documented maximum plus a 1 ms clock-quantization guard and a D2
status check form the readiness proof.

`readSample()` deliberately reads the buffered RTD registers immediately. It
can therefore return an older value while Ready and is rejected during an
armed one-shot. Use the fresh paths for acquisition.

A successful sample has `FRAME_VALID` and `DATA_VALID`. A faulted RTD frame may
commit frame and decoded-fault evidence while returning `DeviceFault`; its
temperature is not marked data-valid. Optional channel and ready-timestamp
metadata are supplied through `MAX31865ReadOptions`.

## Faults and thresholds

The typed API decodes high/low thresholds, REFIN high/low, RTDIN low, and
over/undervoltage. Fresh fault cycles clear old latches first. A manual cycle
waits 200 us for both internal step-1 phases, then at least `5 * external RC`,
issues step 2, and allows its two internal comparisons to finish. Automatic
fault detection is rejected before I/O when configured external RC is greater
than 100 us.

Thresholds can be set/read as 15-bit code, resistance, or temperature. Values
are encoded left-shifted in the two threshold registers. Threshold LSB D0 is
documented don't-care; readback verification compares only its defined D7:D1.
The register prose describes a high fault at or above the high threshold and a
low fault at or below the low threshold. Do not build a safety boundary around
a single threshold-code edge without hardware verification.

## Register diagnostics

Validated raw access is Ready-only and is intended for diagnostics, not normal
application control. CONFIG command bits must use typed operations.
`restoreWritableDefaults()` writes documented writable POR values and clears
faults; it is not a silicon reset.

Reading RTD addresses 01h/02h is the documented DRDY-high acknowledgement.
Consequently, `dumpRegisters()` consumes a pending readiness indication and is
Ready-only. `readConfiguration()` avoids that side effect by reading CONFIG and
threshold registers in separate transactions. `registerReadbackTest()` is
destructive to one threshold byte during the test and restores/verifies the
desired image before success.

## Conversion helpers

All helpers return a status and preserve output on failure:

- `codeToRatio()` and `codeToResistance()`
- `resistanceToCode()` and `resistanceToTemperature()`
- `temperatureToResistance()` and `temperatureToCode()`

The IEC 60751 forward function uses the C coefficient below 0 degrees C and the
A/B branch at and above 0 degrees C. Resistance inversion uses a fixed-count
bisection over the configured finite domain; it has no unbounded iteration.
