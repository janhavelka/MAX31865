# MAX31865

Arduino/PlatformIO driver for the Maxim MAX31865 RTD-to-digital converter on
ESP32-class MCUs.

The core driver owns SPI transactions, register access, RTD conversion timing,
fault handling, sample conversion, and low-level diagnostics. Application code
should use the public C++ API instead of issuing MAX31865 register transactions
directly.

## Start Here

- <a href="docs/api-overview.md">API overview</a>: public headers, wiring, quick start,
  diagnostics, conversion helpers, and fault handling.
- <a href="docs/repository-contract.md">Repository contract</a>: project layout, API
  expectations, CLI baseline, and quality gates.
- <a href="docs/IDF_PORT.md">ESP-IDF porting notes</a>: current component and
  transport-backend status.
- <a href="docs/IDF_PORT_IMPLEMENTATION.md">ESP-IDF implementation status</a>:
  completed work and remaining validation blockers.
- <a href="docs/extracted-md/00_document_inventory.md">Extracted source notes</a>: factual
  MAX31865 datasheet and application-note extraction for implementation work.
- <a href="docs/vendor-reference-code/README.md">Vendor reference material</a>: copied
  Maxim/Analog Devices reference files kept for comparison.

## Public Layout

- Main RTD header: `include/MAX31865/MAX31865.h`
- Configuration types: `include/MAX31865/Config.h`
- Status, sample, and health types: `include/MAX31865/Status.h`
- Register constants: `include/MAX31865/CommandTable.h`
- Sources: `src/`
- Bringup CLI: `examples/01_basic_bringup_cli`
- Public API smoke build: `examples/02_api_smoke`
- Hand-written docs: `docs/`
- Source PDFs: `docs/source-pdfs/`
- Compacted extracted notes: `docs/extracted-md/`; raw PDF extracts: `docs/pdf-extracted-md/`
- Vendor reference material: `docs/vendor-reference-code/`
- Generated Doxygen output: `docs/generated/`

Use this include:

```cpp
#include "MAX31865/MAX31865.h"
```

## Minimal Example

```cpp
#include "MAX31865/MAX31865.h"

MAX31865 rtd;

void setup() {
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
    return;
  }
}

void loop() {
  MAX31865Sample sample;
  if (rtd.readSingle(sample)) {
    Serial.println(sample.temperature_c, 3);
  }
}
```

The positional `begin(...)` overload remains available for compact examples.
New code should prefer `MAX31865BeginConfig`.

`MAX31865BeginConfig::transport` can be used instead of `SPIClass` when an
application owns SPI and GPIO setup, including ESP-IDF `spi_master` devices.

## Examples

`examples/01_basic_bringup_cli` is the service and bringup tool. It covers bus
probe, recovery, health/state output, register diagnostics, one-shot and
continuous conversion, threshold configuration, fault cycles, streaming, and
stress checks. It also includes `faultdecode <raw>` to decode a captured
fault-status byte without touching the SPI bus.

`examples/02_api_smoke` is intentionally small. It exists to compile and link
the public API surface in CI.

## Validation

CI builds:

- `ex_bringup_s3`
- `ex_bringup_s2`
- `ex_api_smoke_s3`
- `native` Unity tests (`pio test -e native`)
- ESP-IDF example: `examples/esp_idf/basic`

Validation scripts:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
pio test -e native
python -c "import json; json.load(open('library.json'))"
git diff --check
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Doxygen configuration is provided in `Doxyfile`. Generate local API docs with:

```bash
doxygen Doxyfile
```

## Device Notes

- SPI mode must use CPHA = 1. Mode 1 is the default recommendation.
- The chip has no device ID register. `probe()` performs a conservative
  configuration-register read and rejects an all-ones response.
- Threshold APIs use the 15-bit ADC-code value, not the shifted register word.
- One-shot reads use the datasheet settling rule of `10.5 * input RC + 1 ms`
  before starting conversion.

## Runtime Diagnostics

`isInitialized()` reports whether `begin()` completed successfully. Repeated or
failed `begin()` calls reset runtime resources, counters, cached samples, and
configuration readback state before retrying. `getSettingsStatus(settings)`
returns an explicit `MAX31865Status` while reading and decoding current device
settings. `MAX31865Status::isOk()` mirrors `ok()`, and
`MAX31865Status::inProgress()` is provided for family-style status handling
(always `false` for this synchronous driver).
