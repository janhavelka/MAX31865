# API Overview

## Wiring

- SPI: ESP32 SCK/MOSI/MISO to MAX31865 SCLK/SDI/SDO.
- CS: required, active low. The driver owns CS framing.
- DRDY: optional; pass `-1` when not wired. Without DRDY the driver uses
  datasheet conversion timing.
- RTD leads: select 2-wire, 3-wire, or 4-wire mode with `setWireMode()` or
  `MAX31865BeginConfig::wireMode`.
- Reference resistor: configure the board value with
  `MAX31865BeginConfig::referenceResistorOhms`.

The MAX31865 uses SPI mode 1 (`CPOL=0`, `CPHA=1`). The driver wraps transfers
in Arduino SPI transactions and serializes them with an internal ESP32 mutex.
`setSpiLockTimeoutMs()` changes the lock timeout used by register and
measurement operations.

## Initialization

New code should prefer the typed begin configuration:

```cpp
MAX31865 rtd;

MAX31865BeginConfig cfg{};
cfg.spi = &SPI;
cfg.pins = {12, 13, 11, 10, -1};
cfg.spiHz = 1000000U;
cfg.verifyProbe = true;
cfg.referenceResistorOhms = 400.0f;
cfg.rtdNominalOhms = 100.0f;
cfg.inputFilterTimeConstantUs = 1000U;
cfg.wireMode = MAX31865WireMode::FourWire;
cfg.filter = MAX31865Filter::Hz60;

if (!rtd.begin(cfg)) {
  Serial.println(rtd.lastErrorName());
}
```

`verifyProbe` runs a non-destructive writable-threshold readback test. The
MAX31865 has no chip-ID register, so a robust probe must verify that writable
register bits can be read, changed, and restored.

## Operating Model

The library is not a mandatory application task. It offers three read models:

- `readSingle(sample, timeoutMs)`: blocking one-shot read with VBIAS settle,
  conversion wait, fault-bit handling, and optional post-read bias shutdown.
- `readIfReady(sample)`: status-returning nonblocking read that does not pollute
  health counters when data is simply not ready.
- `startContinuous()` plus `available()`/`readSample(sample)`: continuous
  conversion with a one-sample cache.

The first sample after enabling continuous conversion uses the longer
single-conversion time. Later samples use the shorter continuous cadence for the
selected filter. Configuration and raw register writes should be performed
while continuous conversion is stopped; conflicting operations return `Busy`.

## Diagnostics

The driver exposes a lifecycle state machine:

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
- `probe()` for no-health-side-effect bus checks
- `recover()` for tracked config restore
- `lastError()`, `lastErrorName()`, and `max31865ErrorName()`
- `lastOperationStatus()` for status-style diagnostic reporting
- `lastOkMs()`, `lastErrorMs()`, `consecutiveFailures()`,
  `totalSuccess()`, and `totalFailures()`
- `health()` and `clearHealthCounters()`

`MAX31865Health` reports lifecycle state, READY/DEGRADED/OFFLINE health state,
last error, online flag, success/failure counters, conversion state,
cached-sample depth, read/kept/drop/overrun counters, SPI errors, SPI lock
timeouts, reference fault observations, conversion/fault-cycle timeouts, last
fault status, and last sample age. CRC/PGA/task fields are retained at zero for
shared sensor-dashboard compatibility.

`MAX31865Settings` is the decoded register snapshot used by the CLI:

- raw CONFIG register
- decoded VBIAS, auto-convert, one-shot, fault-cycle, wire, and filter state
- raw threshold ADC codes
- threshold resistance and Celsius conversions

## Conversion Helpers

- `MAX31865::codeToRatio(code)` converts the 15-bit ADC code to full-scale
  ratio.
- `codeToResistance(code)` converts ADC code to RTD resistance using the
  configured reference resistor.
- `resistanceToTemperature(ohms)` converts RTD resistance to Celsius using the
  configured Callendar-Van Dusen coefficients.
- `temperatureToResistance(celsius)` and `temperatureToCode(celsius)` convert
  engineering thresholds back to register-friendly values.

The default coefficients are IEC 60751 platinum RTD constants. Use
`setRtdParameters()` or `MAX31865BeginConfig::useCustomCoefficients` for custom
platinum curves. The driver enforces the documented reference-resistor operating
range of 350 ohm to 10 kohm and clamps SPI clock requests to the 5 MHz
datasheet maximum.

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

Faulted RTD samples are not returned as successful temperatures. `readSample()`
and `readSingle()` read the fault-status register, update health counters, and
return false with `FaultPresent` when the RTD fault bit is set.

Fault-detection cycles are only allowed while continuous conversion is stopped.
They leave the ADC in normally-off conversion mode, keep VBIAS on for stable
post-cycle diagnostics, and enforce the configured VBIAS settle delay.

## Register Diagnostics

Production application code should prefer typed methods, but bringup tools can
use:

- `readReg(addr, value)` and compact `readReg(addr)`
- `readRegs(start, out, len)`
- `writeReg(addr, value)`
- `writeRegVerify(addr, value, readBack)`
- `dumpRegisters(rows, max)`
- `resetRegisters()`
- `registerReadbackTest(readBack)`

`writeRegVerify()` masks self-clearing CONFIG bits before comparing readback.
`registerReadbackTest()` uses a threshold register and restores the original
value.
