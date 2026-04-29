# MAX31865 PDF Document Inventory

## Source Set

Only PDF files under `docs/` were used. Non-PDF files present in `docs/` (`max31865.c`, `max31865.h`, `max31865.ibs`) were intentionally excluded because this extraction is required to be based only on PDFs.

| File | Document title used in citations | Pages | Document type | Revision/date visible in PDF | Role in this extraction |
|---|---:|---:|---|---|---|
| `docs/MAX31865.pdf` | `MAX31865 RTD-to-Digital Converter` | 26 | Datasheet | `19-6478; Rev 3; 7/15` on p. 1; revision history lists Rev 3 dated `7/15` on p. 26 | Primary source for chip identity, pinout, electrical limits, timing, registers, SPI behavior, fault behavior, application circuits, ordering, and packages. [Source: MAX31865 RTD-to-Digital Converter, p. 1; MAX31865 RTD-to-Digital Converter, p. 26] |
| `docs/ds67-achieve-high-accuracy-temperature-measurement-in-your-precision-designs.pdf` | `Achieve High-Accuracy Temperature Measurement in Your Precision Designs` | 4 | Design solution/application note | `Design Solutions No. 67`, `Rev 0; September 2017` on p. 4 | Supplemental source for RTD background, MAX31865-specific measurement formula, 2-/3-/4-wire examples, cable resistance discussion, and system-level claims. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 4] |
| `docs/temperature-sensor-tutorial.pdf` | `Temperature Sensor Tutorial` | 10 | Tutorial | `Rev. 0; September 2017` on p. 10 | Supplemental source for RTD background, Callendar-Van Dusen coefficients, and MAX31865 two-/three-/four-wire configuration figure. [Source: Temperature Sensor Tutorial, p. 5; Temperature Sensor Tutorial, p. 7; Temperature Sensor Tutorial, p. 10] |
| `docs/maxrefdes42-iolink-rtd-temp-sensor-quick-start-guide-iq2-iolink-master-version.pdf` | `MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)` | 20 | Reference design quick-start guide | `Rev 0; 1/15` on p. 1; revision history lists `0`, `1/15`, `Initial release` on p. 20 | Supplemental, module-level source for MAXREFDES42 IO-Link index reads that expose MAX31865 RTD code and ambient temperature values. [Source: MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 1; MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 15; MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 16; MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 17; MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 18; MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 19; MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 20] |

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
| `07_initialization_reset_and_operational_notes.md` | Power-up/POR behavior; POR register states; startup and settling delays; single-conversion sequencing facts; fault-detection sequencing facts; decoupling; input-filter notes; RTDIN+ cable-fault workaround; temperature example table for PT100 with 400 Ω RREF. |
| `08_variant_differences_and_open_questions.md` | Ordering table; package code/outline/land-pattern table; revision history; variant/package differences; cross-document conflicts or ambiguities; mandatory known-unknowns/gaps list. |

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
| FIFO/buffer behavior | No FIFO or buffer behavior found in the PDFs; recorded as a known unknown/gap in `08_variant_differences_and_open_questions.md`. |
| Calibration/trimming/configuration | Configuration bits and RTD conversion formulas covered in `05_register_map.md` and `07_initialization_reset_and_operational_notes.md`; no chip trimming registers found. |
| Operating modes/power modes | Covered in `05_register_map.md`, `06_modes_interrupts_status_and_faults.md`, and `07_initialization_reset_and_operational_notes.md`. |
| Clocks/oscillator requirements | SPI clock requirements covered in `03_electrical_and_timing.md` and `04_protocol_commands_and_transactions.md`; no external oscillator requirement found. |
| Error conditions/fault diagnostics | Covered in `06_modes_interrupts_status_and_faults.md`. |
| State machines/sequencing rules | Fault-detection and one-shot sequencing covered in `06_modes_interrupts_status_and_faults.md` and `07_initialization_reset_and_operational_notes.md`. |
| Limitations/caveats/errata-like notes | Covered in `07_initialization_reset_and_operational_notes.md` and `08_variant_differences_and_open_questions.md`. |
| Formulas/conversion/scaling | Covered in `01_chip_overview.md`, `05_register_map.md`, and `07_initialization_reset_and_operational_notes.md`. |
| Application-note-only recommendations | Covered in `01_chip_overview.md`, `02_pinout_and_signals.md`, and `07_initialization_reset_and_operational_notes.md`. |

## Pass Review Notes

Pass 1 identified four PDFs, with `MAX31865.pdf` as the primary datasheet. Pass 2 extracted datasheet tables, register descriptions, timing, pin tables, and figures, then extracted only MAX31865-relevant facts from the supplementary documents. Pass 3 merged overlapping facts and marked supplementary additions. Pass 4 identified gaps such as absent CRC, absent FIFO, absent chip ID, and limited reserved-bit guidance. Pass 5 re-read targeted sections around pages 13-19 and 21-24 of the datasheet plus the MAXREFDES42 pages 15-18. Pass 6/7 audit findings are reflected in `08_variant_differences_and_open_questions.md`.

A second diligence pass re-read the datasheet figure/text map and the supplemental MAX31865-specific passages, then added missed details for ADC saturation behavior, block-diagram connections, exact typical-operating-characteristic plot axes/labels, DRDY operation figure sequencing, application-circuit pin ties, supplemental PT100 range/linearity statements, and fault-flowchart delay sequencing.
