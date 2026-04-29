# MAX31865

Arduino/PlatformIO driver for the Maxim MAX31865 RTD-to-digital converter on
ESP32-class MCUs.

The core driver owns SPI transactions, register access, RTD conversion timing,
fault handling, sample conversion, and low-level diagnostics. Application code
should use the public C++ API instead of issuing MAX31865 register transactions
directly.

## Start Here

- [API overview](docs/api-overview.md): public headers, wiring, quick start,
  diagnostics, conversion helpers, and fault handling.
- [Repository contract](docs/repository-contract.md): project layout, API
  expectations, CLI baseline, and quality gates.
- [ESP-IDF porting notes](docs/esp-idf-porting.md): migration path from the
  current Arduino/PlatformIO runtime.
- [Extracted source notes](docs/extracted-md/00_document_inventory.md): factual
  MAX31865 datasheet and application-note extraction for implementation work.
- [Vendor reference material](docs/vendor-reference-code/README.md): copied
  Maxim/Analog Devices reference files kept for comparison.

## Public Layout

- Main RTD header: `include/MAX31865.h`
- Register constants: `include/MAX31865/max31865_driver.h`
- Sources: `src/`
- Bringup CLI: `examples/01_basic_bringup_cli`
- Public API smoke build: `examples/02_api_smoke`
- Hand-written docs: `docs/`
- Source PDFs: `docs/source-pdfs/`
- Extracted PDF notes: `docs/extracted-md/`
- Vendor reference material: `docs/vendor-reference-code/`
- Generated Doxygen output: `docs/generated/`

Use this include:

```cpp
#include <MAX31865.h>
```

## Minimal Example

```cpp
#include <MAX31865.h>

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

## Examples

`examples/01_basic_bringup_cli` is the service and bringup tool. It covers bus
probe, recovery, health/state output, register diagnostics, one-shot and
continuous conversion, threshold configuration, fault cycles, streaming, and
stress checks.

`examples/02_api_smoke` is intentionally small. It exists to compile and link
the public API surface in CI.

## Validation

CI builds:

- `ex_bringup_s3`
- `ex_bringup_s2`
- `ex_api_smoke_s3`

Validation scripts:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python -c "import json; json.load(open('library.json'))"
git diff --check
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
