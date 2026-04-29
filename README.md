# MAX31865

Arduino/PlatformIO driver for the Maxim MAX31865 RTD-to-digital converter.

The driver exposes the complete documented register surface and typed helpers
for PT100/PT1000 workflows: VBIAS control, 2/3/4-wire configuration, 50/60 Hz
filter selection, one-shot and continuous conversion, raw ADC-code reads,
resistance and Callendar-Van Dusen temperature conversion, threshold registers,
fault-status decoding, and master-initiated fault detection.

## Basic Usage

The core library is transport-injected so tests and non-Arduino ports can use
the same driver. On Arduino, provide a callback that wraps one SPI transaction
with CS low/high framing. See `examples/01_basic_bringup_cli/main.cpp` for a
complete adapter.

```cpp
#include <MAX31865/MAX31865.h>

MAX31865::MAX31865 rtd;

MAX31865::Config cfg;
cfg.spiTransfer = mySpiTransfer;
cfg.spiUser = &mySpiContext;
cfg.referenceResistorOhms = 400.0f;
cfg.rtdNominalOhms = 100.0f;
cfg.wireMode = MAX31865::WireMode::THREE_WIRE;
cfg.filter = MAX31865::Filter::HZ_50;

if (rtd.begin(cfg).ok()) {
  MAX31865::Reading sample;
  if (rtd.readBlocking(sample).ok()) {
    Serial.println(sample.temperatureC, 3);
  }
}
```

## Notes

- SPI mode must use CPHA = 1. Mode 1 is the default recommendation.
- The chip has no device ID register. `probe()` performs a conservative
  configuration-register read and rejects an all-ones response.
- Threshold APIs use the 15-bit ADC-code value, not the shifted register word.
- One-shot reads use the datasheet settling rule of `10.5 * input RC + 1 ms`
  before starting conversion.
