# MAX31865 Documentation

This directory contains hand-written project documentation, source PDFs,
datasheet extraction notes, vendor reference material, and generated API
output.

## Guides

- [API overview](api-overview.md) - public API shape, wiring, diagnostics,
  conversion helpers, threshold configuration, and fault handling.
- [Repository contract](repository-contract.md) - repository layout, API/CLI
  requirements, validation gates, and portability rules.
- [ESP-IDF porting notes](esp-idf-porting.md) - planned migration path from
  Arduino/PlatformIO to an ESP-IDF component.
- [Vendor reference material](vendor-reference-code/README.md) - copied
  Maxim/Analog Devices reference files used for comparison.
- [Extracted source notes](extracted-md/00_document_inventory.md) - compact
  curated datasheet and application-note notes for later driver work.

## Directory Map

- `source-pdfs/datasheets/` - primary device datasheets.
- `source-pdfs/application-notes/` - supplementary Maxim/Analog Devices
  application notes and articles.
- `extracted-md/` - compact curated notes synthesized from the PDFs.
- `pdf-extracted-md/` - raw generated Markdown extracted from the PDFs.
- `vendor-reference-code/` - copied vendor-style reference files.
- `generated/` - Doxygen output.

## Generated Output

`generated/` is local Doxygen output from the root `Doxyfile` and is ignored by
Git. Do not edit generated files by hand; update the source headers or Markdown
pages and regenerate.

## Naming Rules

- Use lowercase, hyphen-separated Markdown filenames.
- Name each guide by its job, not by an internal acronym.
- Keep source PDFs under `source-pdfs/`.
- Keep compact notes under `extracted-md/` and raw PDF extracts under
  `pdf-extracted-md/`.
- Keep vendor reference material under `vendor-reference-code/`.
- Keep generated documentation under `generated/`.
