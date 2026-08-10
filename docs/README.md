# MAX31865 documentation

## Integration guides

- [API overview](api-overview.md)
- [Architecture and ownership](ARCHITECTURE.md)
- [Arduino integration](ARDUINO.md)
- [Standalone diagnostic CLI](CLI.md)
- [ESP-IDF callback integration](ESP_IDF.md)
- [Hardware bringup](HARDWARE_BRINGUP.md)
- [Data-sheet audit](DATASHEET_AUDIT.md)
- [Testing](TESTING.md)
- [Known limitations](KNOWN_LIMITATIONS.md)
- [Repository contract](repository-contract.md)

The `extracted-md/` directory contains curated source notes. The
`pdf-extracted-md/` directory contains raw extraction output and is not a
normative API guide. Local source copies are retained under `source-pdfs/`, and
`vendor-reference-code/` is comparison material rather than production code.

The device authority is the official Analog Devices
[product page](https://www.analog.com/en/products/max31865.html) and
[Rev. 3 data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31865.pdf).
If a guide conflicts with the current data sheet, the data sheet and a reviewed
code/test update take precedence.

Doxygen output is generated locally under `docs/doxygen/` and must not be
edited by hand.
