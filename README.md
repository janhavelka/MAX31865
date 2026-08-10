# MAX31865

A deterministic, status-first MAX31865 RTD-to-digital converter driver with a
framework-neutral C++ core and a guarded Arduino SPI adapter. The supported
repository targets are ESP32-S2 and ESP32-S3 with PlatformIO/Arduino; the core
can also be consumed by ESP-IDF applications through application-supplied
transport callbacks.

The library owns MAX31865 framing, register state, conversion sequencing,
fault handling, Callendar-Van Dusen conversion, and passive health accounting.
The application owns the SPI host, pins, backend lifetime, scheduling,
serialization, retries, recovery policy, logging, persistence, and machine
safety. The core creates no task, queue, ISR bridge, logger, or steady-state
heap allocation.

Target compilation and host tests are software evidence, not proof of the
actual board wiring, analog accuracy, cable-fault behavior, or long-duration
hardware reliability. Complete the documented HIL checks before release use.

## Release status

`1.0.0` is the planned first release. No project version has been published or
validated on real MAX31865 hardware. The current tree remains unreleased until
the required HIL matrix passes, its real-hardware results are accepted, and
publication is explicitly authorized.

## Start here

- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/api-overview.md">API overview</a>
- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/ARCHITECTURE.md">Architecture and ownership</a>
- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/ARDUINO.md">Arduino integration</a>
- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/CLI.md">Standalone diagnostic CLI</a>
- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/ESP_IDF.md">ESP-IDF callback integration</a>
- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/HARDWARE_BRINGUP.md">Hardware bringup</a>
- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/DATASHEET_AUDIT.md">Data-sheet audit</a>
- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/TESTING.md">Testing</a>
- <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/KNOWN_LIMITATIONS.md">Known limitations</a>

The primary device authority is Analog Devices' official
[MAX31865 product page](https://www.analog.com/en/products/max31865.html) and
[MAX31865 Rev. 3 data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31865.pdf).
The repository audit checked the official source again on 2026-08-07; Rev. 3
(7/15) remained the current functional data sheet.

## Minimal Arduino shape

```cpp
#include <Arduino.h>
#include <SPI.h>
#include "MAX31865/ArduinoBackend.h"
#include "MAX31865/MAX31865.h"

MAX31865ArduinoBackend backend;
MAX31865 rtd;

void setup() {
  MAX31865Pins pins{12, 13, 11, 10, MAX31865_PIN_UNUSED};
  SPI.begin(pins.sck, pins.miso, pins.mosi, MAX31865_PIN_UNUSED);

  const SPISettings settings(1000000U, MSBFIRST, SPI_MODE1);
  MAX31865Status status =
      max31865ArduinoBackendBind(backend, SPI, settings, 1000000U, pins);
  if (status.ok()) {
    status = max31865ArduinoConfigureControlPins(backend);
  }

  MAX31865BeginConfig config = max31865DefaultBeginConfig();
  config.transport = max31865ArduinoTransport(backend);
  config.initialDeviceConfig.wireMode = MAX31865WireMode::FourWire;
  config.initialDeviceConfig.filter = MAX31865Filter::Hz60;
  config.rtd.referenceResistorOhms = 400.0F; // Enter measured RREF.
  config.rtd.nominalResistanceOhms = 100.0F;
  config.rtd.inputFilterTimeConstantUs = 1000U;
  if (status.ok()) {
    status = rtd.begin(config);
  }
  if (!status.ok()) {
    // Map the explicit result to application policy here.
    return;
  }
}
```

`backend`, `SPI`, an optional bus-arbiter context, and every object reachable
through `MAX31865Transport::user` are borrowed and must outlive the driver
binding. Driver calls are synchronous, not ISR-safe, and not internally
thread-safe. An application-owned instance mutex protects a whole public call;
a shared-bus arbiter protects each SPI transaction. These solve different
ownership problems.

## Operating model

- `begin(config)` validates a complete binding and applies a verified desired
  register image. An optional application-selected power-ready delay runs
  before the bounded protocol deadline and adds to total begin wall time.
- `startContinuous(timeoutMs)` plus `tick(millis())` and `poll(sample)` provides
  cooperative continuous acquisition.
- `readOneShot(sample, timeoutMs)` performs one bounded bias-settle, conversion,
  read, and idle-state restore sequence. A readiness timeout deliberately
  leaves the one-shot armed so `readSingle()` or `stop()` can resolve it without
  accepting buffered stale data.
- `stop(timeoutMs)` proves normally-off conversion state. An armed one-shot is
  not cancellable in silicon, so stop waits for and discards a proven result.
  Normally this is bounded by the remaining 55/66 ms horizon; a D2 protected-
  input voltage fault can halt the ADC longer, in which case DeviceFault or a
  timeout leaves the conversion quarantined for later stop/recovery.
  `end()` and the destructor perform no callbacks and no device I/O.
- `MAX31865Status` is the complete result of every fallible operation.
  Validation and lifecycle rejections invoke no transport callback. On the few
  immediate-read APIs that accept timeout zero, zero means one no-wait attempt;
  `readSingle(..., 0)` checks readiness once and never sleeps.
  `health()` is a passive, zero-I/O snapshot with lifecycle, READY/DEGRADED/
  OFFLINE classification, failure streaks, sampling counters, transport
  failures, timeouts, and fault observations.
- `probe()` is read-only and health-neutral. It cannot prove silicon identity
  because the MAX31865 has no ID register. `recover()` is explicit and tracked;
  the device has no software-reset command or RESET pin.

When DRDY is configured, that active-low input is readiness-authoritative; a
high level is never replaced by an elapsed-time guess. After the maximum
horizon, a bounded FAULT_STATUS read detects D2 ADC halt. Without DRDY, the
driver uses documented maximum durations plus a 1 ms clock-quantization guard
and the same D2 check. Call `tick()`/a readiness API at least once per signed
32-bit millisecond half-range (about 24.9 days). Reading RTD registers 01h/02h
acknowledges DRDY high, so starts first flush an older RTD result.

## Examples

- `01_basic_bringup`: borrowed Arduino backend, begin, read-only probe, settings,
  and health.
- `02_continuous_sampling`: application-scheduled `tick()`/`poll()` acquisition.
- `03_one_shot`: bounded one-shot sampling with explicit statuses.
- `04_fault_diagnostics`: thresholds and manual/automatic fault-cycle rules.
- `05_rtd_configuration`: PT100/PT1000/custom scaling and CVD helpers.
- `06_diagnostic_cli`: standalone operator diagnostic console.

The first five examples share only `examples/common/BoardConfig.h` and
`ExampleStatus.h`. BoardConfig explicitly owns the selected physical wire mode,
notch filter, pins, and fitted-component values; it is example glue, not
installed library policy.

## Validation

```powershell
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
.\scripts\pio.cmd run -e ex_bringup_s2 -e ex_bringup_s3 `
  -e ex_continuous_s2 -e ex_continuous_s3 `
  -e ex_one_shot_s2 -e ex_one_shot_s3 `
  -e ex_fault_diagnostics_s2 -e ex_fault_diagnostics_s3 `
  -e ex_rtd_configuration_s2 -e ex_rtd_configuration_s3 `
  -e ex_diagnostic_s2 -e ex_diagnostic_s3
```

## Data-sheet-sensitive behavior

- SPI is MSB-first, mode 1 or mode 3, at no more than 5 MHz. The examples use
  mode 1 at 1 MHz.
- Bias settling before a new conversion is `ceil(10.5 * external RC) + 1 ms`.
  Maximum one-shot times are 55 ms at 60 Hz and 66 ms at 50 Hz; maximum later
  continuous periods are 18 ms and 21 ms respectively.
- A manual fault cycle waits through both internal 100 us step-1 phases, then
  at least five complete external RC time constants before step 2. Automatic
  fault detection is rejected when configured RC exceeds 100 us.
- Protected-input over/undervoltage (D2) halts ADC updates until the physical
  condition clears; the normal conversion maxima do not bound that interval.
- Threshold APIs accept a 15-bit code. Threshold-register D0 is documented
  don't-care and is masked during verification. Typed APIs avoid raw encoding.
- Default IEC 60751 conversion is explicitly bounded to -200 to +850 degrees C.
  Custom curves must be finite, strictly increasing over their configured
  domain, and representable by the selected RREF.
- In 3-/4-wire circuits a broken RTDIN+ lead can leave ADC+ floating and evade
  reliable detection. The data sheet recommends a 10 Mohm resistor from RTDIN+
  to BIAS when a full-scale indication is required for that fault.

See the <a href="https://github.com/janhavelka/MAX31865/blob/main/docs/DATASHEET_AUDIT.md">data-sheet audit</a> for the audited
register map, timings, side effects, and package-change notices.
