# API Overview

## Wiring

- SPI: ESP32 SCK/MOSI/MISO to MAX31865 SCLK/SDI/SDO
- CS: required, active low
- DRDY: optional; pass `-1` when not wired
- RTD leads: configure 2-wire, 3-wire, or 4-wire mode with `setWireMode()`
- Reference resistor: set with `MAX31865BeginConfig::referenceResistorOhms`

The MAX31865 uses SPI mode 1 (`CPOL=0`, `CPHA=1`). The driver wraps transfers
in Arduino SPI transactions and owns CS framing.

## Operating Model

The library is not a mandatory application task. It offers three read models:

- `readSingle(sample, timeoutMs)`: blocking one-shot read.
- `poll(sample)`: nonblocking read when conversion is ready.
- `startContinuous()` plus `readSample(sample)`: continuous conversion with a
  one-sample cache.

Normal configuration and raw register writes should be performed while
continuous conversion is stopped. The driver reports `Busy` for changes that
would conflict with active conversion.

## Diagnostics

The driver exposes a small lifecycle state machine:

- `Uninitialized`
- `Ready`
- `Configuring`
- `Converting`
- `Recovering`
- `Fault`

Use these APIs for supervision and logs:

- `state()` and `max31865StateName()`
- `driverState()`, `healthState()`, `isOnline()`, and
  `max31865DriverStateName()`
- `probe()` for raw configuration-register checks
- `recover()` for tracked config restore
- `lastError()`, `lastErrorName()`, and `max31865ErrorName()`
- `lastOperationStatus()` for status-style diagnostic reporting
- `lastOkMs()`, `lastErrorMs()`, `consecutiveFailures()`,
  `totalSuccess()`, and `totalFailures()`
- `health()`
- `clearHealthCounters()`

`MAX31865Health` reports the detailed lifecycle state, unified
READY/DEGRADED/OFFLINE health state, last error, online flag, success/failure
counters, conversion state, cached-sample depth, read/kept/drop counters, SPI
errors, DRDY timeouts, last fault status, and last sample age.

## Conversion Helpers

- `MAX31865::codeToRatio(code)` converts the 15-bit ADC code to full-scale
  ratio.
- `codeToResistance(code)` converts ADC code to RTD resistance using the
  configured reference resistor.
- `resistanceToTemperature(ohms)` converts RTD resistance to Celsius using the
  Callendar-Van Dusen coefficients.
- `temperatureToResistance(celsius)` and `temperatureToCode(celsius)` convert
  thresholds back to register-friendly values.

The default coefficients are IEC 60751 platinum RTD constants. Set the nominal
RTD resistance and reference resistor in `MAX31865BeginConfig`.

## Fault Handling

The driver exposes both raw threshold registers and decoded fault bits:

- `setFaultThresholdsRaw(low, high)`
- `setFaultThresholdsResistance(lowOhms, highOhms)`
- `setFaultThresholdsTemperature(lowC, highC)`
- `readFaultStatus(status)`
- `clearFaults()`
- `runAutoFaultDetection(status)`
- `runManualFaultDetection(status, settleDelayUs)`

Threshold APIs accept the 15-bit RTD ADC code, not the shifted register word.
The helper writes the correct register encoding.
