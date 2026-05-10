# MAX31865 PDF Document Inventory

## Source Set

Only PDF files stored under `docs/source-pdfs/` were used. Non-PDF vendor/support
files under `docs/vendor-reference-code/` were intentionally excluded.

### Primary Datasheet

- File: `docs/source-pdfs/datasheets/max31865_datasheet_rev3_2015-07.pdf`
- Citation title: `MAX31865 RTD-to-Digital Converter`
- Pages: 26
- Revision/date: `19-6478; Rev 3; 7/15`
- Role: primary source for chip identity, pinout, electrical limits, timing,
  registers, SPI behavior, fault behavior, application circuits, ordering, and
  packages. [Source: MAX31865 RTD-to-Digital Converter, pp. 1, 26]

### Supplemental PDFs

| File | Title | Pages | Role |
|---|---|---:|---|
| `docs/source-pdfs/application-notes/maxim_design_solution_67_high_accuracy_temperature_measurement_rev0_2017-09.pdf` | `Achieve High-Accuracy Temperature Measurement in Your Precision Designs` | 4 | RTD background, MAX31865 measurement formula, wiring examples, cable-resistance discussion. |
| `docs/source-pdfs/application-notes/maxim_temperature_sensor_tutorial_rev0_2017-09.pdf` | `Temperature Sensor Tutorial` | 10 | RTD background, Callendar-Van Dusen coefficients, and MAX31865 wiring figure. |
| `docs/source-pdfs/application-notes/maxrefdes42_iolink_rtd_temp_sensor_quick_start_iq2_rev0_2015-01.pdf` | `MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide` | 20 | Module-level IO-Link index reads exposing MAX31865 RTD code and ambient temperature values. |

Supplemental citations are kept in the topic files where those facts are used.

## Output Navigation Index

| Output file | Exact topics covered |
|---|---|
| `00_document_inventory.md` | PDF inventory; source authority; excluded non-PDF files; generated-file navigation; extraction coverage checklist; primary/supplementary-source policy. |
| `01_chip_overview.md` | MAX31865 identity; applications; feature summary; functional blocks; supported sensor types; RTD conversion principle; Callendar-Van Dusen equation; RREF selection; temperature/resistance conversion rules; supplemental module-level context. |
| `02_pinout_and_signals.md` | TQFN and SSOP pin configurations; complete pin-description table; exposed-pad requirement; supply/ground pins; SPI pins; DRDY behavior at pin level; 2-/3-/4-wire application-circuit connections; passive components shown in figures; pin-name ambiguities. |
| `03_electrical_and_timing.md` | Absolute maximum ratings; package thermal characteristics; recommended DC operating conditions; DC electrical characteristics; AC SPI timing table; timing notes; timing diagrams; typical operating-characteristic figures and their conditions. |
| `04_protocol_commands_and_transactions.md` | SPI interface mode requirements; CPOL/CPHA behavior; read/write address-byte rules; register read/write address scheme; single-byte and multibyte transfer behavior; auto-increment/wrap behavior; invalid-address behavior; read-only write behavior; MAXREFDES42 IO-Link index transactions. |
| `05_register_map.md` | Full documented eight-register map; addresses; write addresses; POR states; access; configuration register bit fields and side effects; RTD data register format; fault threshold register format; fault status register bit meanings; documented reserved/don't-care fields. |
| `06_modes_interrupts_status_and_faults.md` | Conversion modes; VBIAS behavior; one-shot behavior; filter-select behavior; DRDY; fault-detection classes; automatic/manual fault-detection cycles; fault latching/clearing; overvoltage/undervoltage behavior; 2-/3-/4-wire fault decoding tables. |
| `07_initialization_reset_and_operational_notes.md` | Power-up/POR behavior; POR register states; startup and settling delays; single-conversion sequencing facts; fault-detection sequencing facts; decoupling; input-filter notes; RTDIN+ cable-fault workaround; temperature example table for PT100 with 400 Ohm RREF. |
| `08_variant_differences_and_open_questions.md` | Ordering table; package code/outline/land-pattern table; revision history; variant/package differences; cross-document conflicts or ambiguities; facts not found in the PDFs. |

## Source Authority Policy

The datasheet is treated as primary for chip-level electrical limits, timing, register behavior, SPI protocol, pin names, and ordering information because it directly specifies the MAX31865 device. The design solution, tutorial, and quick-start guide are treated as supplemental: they are included only where they add MAX31865-specific facts, examples, or module-level interface details. Conflicts or ambiguities are recorded instead of resolved by guesswork.

## Coverage Checklist

| Category | Extraction status |
|---|---|
| Chip/device identity and variants | Covered in `01_chip_overview.md` and `08_variant_differences_and_open_questions.md`. |
| Feature summary | Covered in `01_chip_overview.md`. |
| Ordering/package distinctions | Covered in `08_variant_differences_and_open_questions.md`. |
| Pinout and pin descriptions | Covered in `02_pinout_and_signals.md`. |
| Alternate/special pin functions | Covered in `02_pinout_and_signals.md`, especially FORCE2, DRDY, CS, EP, and protected RTD pins. |
| Supply requirements and operating conditions | Covered in `03_electrical_and_timing.md` and `07_initialization_reset_and_operational_notes.md`. |
| Absolute maximum ratings | Covered in `03_electrical_and_timing.md`. |
| DC and AC electrical characteristics | Covered in `03_electrical_and_timing.md`. |
| Timing requirements and diagrams | Covered in `03_electrical_and_timing.md` and `04_protocol_commands_and_transactions.md`. |
| Protocol/bus behavior | Covered in `04_protocol_commands_and_transactions.md`. |
| Commands/opcodes/transaction formats | Covered in `04_protocol_commands_and_transactions.md`; the PDFs document address-byte transactions rather than named opcodes. |
| Addressing schemes | Covered in `04_protocol_commands_and_transactions.md` and `05_register_map.md`. |
| Register map and reset values | Covered in `05_register_map.md`. |
| Bit fields and side effects | Covered in `05_register_map.md` and `06_modes_interrupts_status_and_faults.md`. |
| Default states and power-up behavior | Covered in `05_register_map.md` and `07_initialization_reset_and_operational_notes.md`. |
| Initialization requirements | Covered as documented timing/sequencing facts in `07_initialization_reset_and_operational_notes.md`. |
| Interrupt/status/event behavior | DRDY and latched fault status covered in `02_pinout_and_signals.md`, `05_register_map.md`, and `06_modes_interrupts_status_and_faults.md`; no interrupt controller beyond DRDY is documented in the PDFs. |
| FIFO/buffer behavior | No FIFO or host-readable sample buffer behavior found in the PDFs; the precise missing fact is recorded in `08_variant_differences_and_open_questions.md`. |
| Calibration/trimming/configuration | Configuration bits and RTD conversion formulas covered in `05_register_map.md` and `07_initialization_reset_and_operational_notes.md`; no chip trimming registers found. |
| Operating modes/power modes | Covered in `05_register_map.md`, `06_modes_interrupts_status_and_faults.md`, and `07_initialization_reset_and_operational_notes.md`. |
| Clocks/oscillator requirements | SPI clock requirements covered in `03_electrical_and_timing.md` and `04_protocol_commands_and_transactions.md`; no external oscillator requirement found. |
| Error conditions/fault diagnostics | Covered in `06_modes_interrupts_status_and_faults.md`. |
| State machines/sequencing rules | Fault-detection and one-shot sequencing covered in `06_modes_interrupts_status_and_faults.md` and `07_initialization_reset_and_operational_notes.md`. |
| Limitations/caveats/errata-like notes | Covered in `07_initialization_reset_and_operational_notes.md` and `08_variant_differences_and_open_questions.md`. |
| Formulas/conversion/scaling | Covered in `01_chip_overview.md`, `05_register_map.md`, and `07_initialization_reset_and_operational_notes.md`. |
| Application-note-only recommendations | Covered in `01_chip_overview.md`, `02_pinout_and_signals.md`, and `07_initialization_reset_and_operational_notes.md`. |

## Pass Review Notes

Pass 1 identified four PDFs, with `max31865_datasheet_rev3_2015-07.pdf` as the
primary datasheet. Later passes extracted the datasheet tables, register
descriptions, timing, pin tables, and figures, then added only MAX31865-relevant
supplemental facts. Targeted re-reads covered datasheet pages 13-19 and 21-24
plus MAXREFDES42 pages 15-18. Audit findings are reflected in
`08_variant_differences_and_open_questions.md`.

A second diligence pass re-read the datasheet figure/text map and the supplemental MAX31865-specific passages, then added missed details for ADC saturation behavior, block-diagram connections, exact typical-operating-characteristic plot axes/labels, DRDY operation figure sequencing, application-circuit pin ties, supplemental PT100 range/linearity statements, and fault-flowchart delay sequencing.
