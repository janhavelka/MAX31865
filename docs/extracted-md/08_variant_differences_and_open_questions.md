# Variant Differences and Open Questions

## Ordering Information

`+` denotes a lead(Pb)-free/RoHS-compliant package. `T` denotes tape and reel. `*EP` denotes exposed pad. [Source: MAX31865 RTD-to-Digital Converter, p. 25]

| Part | Temperature range | Pin-package | Notes | Source |
|---|---|---|---|---|
| MAX31865AAP+ | -40degC to +125degC | 20 SSOP | Lead(Pb)-free/RoHS-compliant package. | [Source: MAX31865 RTD-to-Digital Converter, p. 25] |
| MAX31865AAP+T | -40degC to +125degC | 20 SSOP | Lead(Pb)-free/RoHS-compliant package; tape and reel. | [Source: MAX31865 RTD-to-Digital Converter, p. 25] |
| MAX31865ATP+ | -40degC to +125degC | 20 TQFN-EP | Lead(Pb)-free/RoHS-compliant package; exposed pad. | [Source: MAX31865 RTD-to-Digital Converter, p. 25] |
| MAX31865ATP+T | -40degC to +125degC | 20 TQFN-EP | Lead(Pb)-free/RoHS-compliant package; tape and reel; exposed pad. | [Source: MAX31865 RTD-to-Digital Converter, p. 25] |

## Package Information

For latest package outline information and land patterns, the datasheet directs readers to Maxim's packages page and notes that `+`, `#`, or `-` in the package code indicates RoHS status only; package drawings may show a different suffix character, but the drawing pertains to the package regardless of RoHS status. [Source: MAX31865 RTD-to-Digital Converter, p. 25]

| Package type | Package code | Outline no. | Land pattern no. | Source |
|---|---|---|---|---|
| 20 TQFN-EP | T2055+5 | 21-0140 | 90-0010 | [Source: MAX31865 RTD-to-Digital Converter, p. 25] |
| 20 SSOP | A20+1 | 21-0056 | 90-0094 | [Source: MAX31865 RTD-to-Digital Converter, p. 25] |

## Package/Variant Functional Differences Found

The PDFs identify package and shipping differences, but do not document different register maps, electrical behavior, protocol behavior, ADC behavior, or fault behavior among the listed part numbers. [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 15; MAX31865 RTD-to-Digital Converter, p. 16; MAX31865 RTD-to-Digital Converter, p. 17; MAX31865 RTD-to-Digital Converter, p. 25]

The TQFN package has an exposed pad that must connect to GND1; the pin-description table states the exposed pad applies to the TQFN package only. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

Pin numbers differ between TQFN and SSOP, as documented in the pin-description table and pin-configuration figure. [Source: MAX31865 RTD-to-Digital Converter, p. 7; MAX31865 RTD-to-Digital Converter, p. 8]

## Revision History

| Revision number | Revision date | Description | Pages changed | Source |
|---:|---|---|---|---|
| 0 | 10/12 | Initial release | - | [Source: MAX31865 RTD-to-Digital Converter, p. 26] |
| 1 | 1/15 | Revised Benefits and Features section | 1 | [Source: MAX31865 RTD-to-Digital Converter, p. 26] |
| 2 | 5/15 | Added the SSOP package to the datasheet and updated input voltage protection as +/-45 V. | 1, 2, 7, 10, 11, 24 | [Source: MAX31865 RTD-to-Digital Converter, p. 26] |
| 3 | 7/15 | Updated Table 6 | 16 | [Source: MAX31865 RTD-to-Digital Converter, p. 26] |

## Cross-Document Conflicts and Ambiguities

| Topic | Conflict or ambiguity | Source references |
|---|---|---|
| Threshold comparison operators | Register text uses inclusive high/low threshold comparisons; flowchart and decoding tables use strict greater-than/less-than wording. | [Conflict: MAX31865 datasheet, p. 16 vs pp. 11-12, 22-23] |
| Settling delay after enabling VBIAS | The one-shot section says that if VBIAS was off, enable VBIAS and wait at least 10.5 input-RC time constants plus 1 ms before initiating conversion. The Applications Information section recommends at least five time constants plus 1 ms after an overvoltage/undervoltage fault, after a fault-detection cycle, or after enabling VBIAS. | [Conflict: MAX31865 RTD-to-Digital Converter, p. 13 vs MAX31865 RTD-to-Digital Converter, p. 19] |
| DRDY high timing | The AC timing table defines `Address 01h or 02h Decoded to DRDY High` (`tDRDYH`) as 50 ns after RTD register read access. The DRDY section states that DRDY returns high when a read operation of the RTD Data Registers completes. | [Conflict: MAX31865 RTD-to-Digital Converter, p. 4 vs MAX31865 RTD-to-Digital Converter, p. 19] |
| SSOP pin 19 name | The SSOP pin-configuration figure labels pin 19 as `GND`, while the pin-description table identifies SSOP pin 19 as `GND1`. | [Conflict: MAX31865 RTD-to-Digital Converter, p. 7 vs MAX31865 RTD-to-Digital Converter, p. 8] |
| RTD alpha/coefficient wording in design solution | The design solution gives 0.00385 ohm/ohm/degC on p. 1 and 0.003925 ohm/ohm/degC on p. 2. The datasheet separately lists 0.00385 and 0.00392 alpha values for IEC 751 and SAMA. | [Conflict: Design Solution 67, pp. 1-2; Source: MAX31865 datasheet, p. 10] |
| Measurement range versus IC operating range | The design solution describes the sensed RTD range as -200 degC to +850 degC. The datasheet IC operating range is -40 degC to +125 degC, so driver notes must not treat RTD sensing range as IC ambient range. | [Conflict: Design Solution 67, pp. 3-4 vs MAX31865 datasheet, pp. 2, 10, 25] |
| MAXREFDES42 IO-Link index 261 scaling | In the MAXREFDES42 module context, index 261 is shifted by one bit and divided by two. This is not part of the bare MAX31865 SPI register map. | [Sources: MAXREFDES42 quick start, pp. 15-16; MAX31865 datasheet, p. 15] |
| Timing Note 8 units | Note 8 in the datasheet states measurements use `10 ms maximum rise and fall times`. This extraction preserves the PDF text and does not reinterpret it. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |

## Facts Not Documented in the PDFs

These items were not found in the provided PDFs and should not be invented by a later implementation agent.

| Missing or weakly specified item | What the PDFs do provide | Source references |
|---|---|---|
| Device ID or silicon revision register | The register map lists only Configuration, RTD data, threshold, and Fault Status registers. | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| Software reset command | No software reset command is defined in the serial interface or register sections. | [Sources: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 15; MAX31865 RTD-to-Digital Converter, p. 16; MAX31865 RTD-to-Digital Converter, p. 17; MAX31865 RTD-to-Digital Converter, p. 18] |
| Hardware reset pin | No hardware reset pin is present in pin configurations or pin-description table. | [Source: MAX31865 RTD-to-Digital Converter, p. 7; MAX31865 RTD-to-Digital Converter, p. 8] |
| General reserved-bit write policy | The PDFs mark some bits as `X` or don't care but do not provide a general rule to write reserved bits as 0, preserve them, or ignore readback values. | [Source: MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 16] |
| CRC/checksum/parity | The SPI transaction section describes address and data bytes only; no CRC, checksum, or parity is documented. | [Source: MAX31865 RTD-to-Digital Converter, p. 17; MAX31865 RTD-to-Digital Converter, p. 18] |
| FIFO or sample buffer | The register map contains RTD data registers but no FIFO or buffer registers. | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| Interrupt mask/enable register | DRDY and fault status are documented, but no interrupt mask or interrupt-enable register is listed. | [Sources: MAX31865 RTD-to-Digital Converter, p. 8; MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 19] |
| Busy bit or conversion-in-progress status bit | DRDY behavior and conversion times are documented, but no separate busy bit is listed in the register map. | [Sources: MAX31865 RTD-to-Digital Converter, p. 3; MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 19] |
| Exact RTD data update atomicity during multibyte reads | The datasheet documents DRDY operation and multibyte transfers, but does not explicitly state whether RTD MSB/LSB values are double-buffered or latched atomically during a read. | [Sources: MAX31865 RTD-to-Digital Converter, p. 17; MAX31865 RTD-to-Digital Converter, p. 18; MAX31865 RTD-to-Digital Converter, p. 19] |
| Default DRDY output state immediately after POR and before first conversion | DRDY low-on-new-data and high-after-read behavior is documented, but no explicit POR DRDY state is stated. | [Sources: MAX31865 RTD-to-Digital Converter, p. 8; MAX31865 RTD-to-Digital Converter, p. 19] |
| Threshold-selection procedure for specific RTD fault temperatures | Threshold registers and compare behavior are documented, but the PDFs do not prescribe threshold values for particular sensors or temperatures. | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| Fault clear completion timing | The Configuration register clear bit self-clears and clears Fault Status D[7:2], but no explicit timing for clear completion is provided. | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |
| Thermistor temperature conversion | The datasheet states thermistors may be used and that output data is sensor resistance/reference resistance; no thermistor-specific temperature equation or table is provided. | [Source: MAX31865 RTD-to-Digital Converter, p. 11] |
| Maximum external input filter value | The datasheet gives settling-time rules and says manual fault-detection timing should be used if the input-filter time constant exceeds 100 us, but no absolute maximum filter capacitance is specified. | [Sources: MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 19] |
| Nonvolatile memory or stored calibration | No nonvolatile configuration, EEPROM, trim, or calibration registers are listed. | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| Multi-device chip addressing | The SPI interface uses CS for selection and does not document address pins or in-band device addressing. | [Sources: MAX31865 RTD-to-Digital Converter, p. 7; MAX31865 RTD-to-Digital Converter, p. 8; MAX31865 RTD-to-Digital Converter, p. 16; MAX31865 RTD-to-Digital Converter, p. 17; MAX31865 RTD-to-Digital Converter, p. 18] |
| Broadcast addressing | No broadcast mechanism is documented. | [Source: MAX31865 RTD-to-Digital Converter, p. 16; MAX31865 RTD-to-Digital Converter, p. 17; MAX31865 RTD-to-Digital Converter, p. 18] |
| Endianness beyond SPI byte/bit order | The datasheet specifies MSB-first address/data transfers and register ordering but does not define host CPU endianness terminology. | [Sources: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 17] |
| NC pin electrical tolerance | The pin-description table says N.C. is `Do Not Connect`; no further electrical behavior is documented. | [Source: MAX31865 RTD-to-Digital Converter, p. 8] |
| Exact behavior when changing most configuration bits during active conversion | The datasheet explicitly says not to change notch frequency in auto conversion mode, but does not provide a general rule for every other configuration bit during active conversion. | [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 14] |
| Fault-status D1/D0 semantics | Fault Status D1 and D0 are marked `x`; no status meaning is defined. | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| Fault-threshold LSB D0 handling | Threshold-register LSB D0 is marked `X`; no additional write/read behavior is specified. | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |

## Unreadable or Non-Tabulated Source Areas

No relevant PDF pages used for extraction were unreadable after text extraction and visual inspection.

The typical operating-characteristic plots on datasheet page 6 were summarized by title, axes, visible labels, and conditions. Exact curve data points were not digitized because the PDF provides plots rather than tables of numeric points. [Source: MAX31865 RTD-to-Digital Converter, p. 6]

## Self-Audit Notes

All generated Markdown files were written from the PDFs listed in `00_document_inventory.md`. Non-PDF files in `docs/vendor-reference-code/` were not used as factual sources.

Every register, pin, timing value, fault bit, and conversion formula included in this extraction has a page citation. Open issues above are left unresolved where the PDFs do not provide a definitive resolution.
