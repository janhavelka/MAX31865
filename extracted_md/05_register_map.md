# Register Map

## Register Access Model

Communication is through eight 8-bit registers containing conversion, status, and configuration data. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

All programming is done by selecting the appropriate address of the desired register location. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

Registers use `0Xh` addresses for reads and `8Xh` addresses for writes, and data is read from or written to the registers MSB first. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

## Register Address and POR Table

| Register name | Width | Read address | Write address | POR state | Access | Purpose | Source |
|---|---:|---:|---:|---:|---|---|---|
| Configuration | 8 bits | 00h | 80h | 00h | R/W | Conversion mode, VBIAS enable, one-shot command, RTD wiring select, fault-detection cycle control, fault-status clear, 50/60 Hz filter select. | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| RTD MSBs | 8 bits | 01h | - | 00h | R | Upper byte of 15-bit RTD resistance ratio data. | [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs | 8 bits | 02h | - | 00h | R | Lower bits of RTD resistance ratio data plus fault bit at D0. | [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 15] |
| High Fault Threshold MSB | 8 bits | 03h | 83h | FFh | R/W | Upper byte of high RTD fault threshold. | [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 16] |
| High Fault Threshold LSB | 8 bits | 04h | 84h | FFh | R/W | Lower bits of high RTD fault threshold; D0 marked X in threshold format. | [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 16] |
| Low Fault Threshold MSB | 8 bits | 05h | 85h | 00h | R/W | Upper byte of low RTD fault threshold. | [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 16] |
| Low Fault Threshold LSB | 8 bits | 06h | 86h | 00h | R/W | Lower bits of low RTD fault threshold; D0 marked X in threshold format. | [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 16] |
| Fault Status | 8 bits | 07h | - | 00h | R | Latched detected fault bits. | [Source: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 16] |

Attempting to write to a read-only register results in no change to that register's contents. [Source: MAX31865 RTD-to-Digital Converter, p. 17]

## Configuration Register (00h Read, 80h Write)

The Configuration register selects conversion mode, enables/disables VBIAS, initiates one-shot conversions, selects 3-wire versus 2-/4-wire RTD connections, initiates a full fault-detection cycle, clears the Fault Status register, and selects filter notch frequencies. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

POR state is `00h`. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

| Bit(s) | Field | Reset from POR byte | Values and behavior | Side effects/notes | Source |
|---|---|---:|---|---|---|
| D7 | VBIAS | 0 | `1` = ON; `0` = OFF. | When no conversions are being performed, VBIAS may be disabled to reduce power dissipation. Write 1 before beginning a single one-shot conversion. When automatic continuous conversion mode is selected, VBIAS remains on continuously. | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| D6 | Conversion mode | 0 | `1` = Auto; `0` = Normally off. | Writing 1 selects automatic conversion mode with continuous conversions at a 50/60 Hz rate. Writing 0 exits automatic conversion mode and enters Normally Off mode; one-shot conversions may be initiated from Normally Off mode. | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| D5 | 1-shot | 0 | `1` = one-shot; auto-clear. | In Normally Off mode, writing 1 starts a single resistance conversion. Conversion triggers when CS goes high after writing 1; for multibyte writes it triggers when CS goes high at the transaction end. If VBIAS is off between conversions, input filter capacitors must charge before an accurate conversion. | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| D4 | 3-wire | 0 | `1` = 3-wire RTD; `0` = 2-wire or 4-wire. | In 3-wire mode, the voltage between FORCE+ and RTDIN+ is subtracted from `(RTDIN+ - RTDIN-)` to compensate IR errors from using one wire for FORCE- and RTDIN-. | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |
| D3:D2 | Fault Detection Cycle Control | 00b | See fault-detection cycle control table below. | If an external RTD input filter has a time constant greater than 100 µs, the fault-detection cycle timing should be controlled in manual mode. | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |
| D1 | Fault Status Clear | 0 | `1` = clear; auto-clear. | Write 1 while writing 0 to D5, D3, and D2 to return all fault status bits D[7:2] in the Fault Status register to 0. D1 self-clears to 0. Fault Status bit D2 and RTD LSB bit D0 may set again immediately if an over/undervoltage fault persists. | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |
| D0 | 50/60 Hz filter select | 0 | `1` = 50 Hz; `0` = 60 Hz. | Selects notch frequencies for the noise rejection filter. Write 0 to reject 60 Hz and harmonics; write 1 to reject 50 Hz and harmonics. The datasheet notes not to change notch frequency while in auto conversion mode. | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |

### Fault Detection Cycle Control Bits D3:D2

`X` is documented as don't care. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

| D3 | D2 | Configuration register write | Write action | Read meaning | Source |
|---:|---:|---|---|---|---|
| 0 | 0 | `XXXX00XXb` | No action | Fault detection finished | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |
| 0 | 1 | `100X010Xb` | Fault detection with automatic delay | Automatic fault detection still running | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |
| 1 | 0 | `100X100Xb` | Run fault detection with manual delay, cycle 1 | Manual cycle 1 still running; waiting for user to write 11 | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |
| 1 | 1 | `100X110Xb` | Finish fault detection with manual delay, cycle 2 | Manual cycle 2 still running | [Source: MAX31865 RTD-to-Digital Converter, p. 14] |

If 1 is written to D5 (1-shot) and D2 or D3 in a single write, both commands are ignored. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

If `100X110Xb` is set without first initiating the first manual step (`100X100Xb`), automatic fault detection mode is run instead. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

## RTD Resistance Registers (01h-02h)

The RTD MSBs and RTD LSBs registers contain RTD resistance data. The format is a 15-bit ratio of RTD resistance to reference resistance. D0 of the RTD LSBs register is a Fault bit indicating whether any RTD faults have been detected. [Source: MAX31865 RTD-to-Digital Converter, p. 15]

POR state for both registers is `00h`; both are read-only. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

| Register | Bit | Meaning | Bit weighting | Decimal value | Source |
|---|---|---|---:|---:|---|
| RTD MSBs (01h) | D7 | RTD resistance data MSB | 2^14 | 16384 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD MSBs (01h) | D6 | RTD resistance data | 2^13 | 8192 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD MSBs (01h) | D5 | RTD resistance data | 2^12 | 4096 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD MSBs (01h) | D4 | RTD resistance data | 2^11 | 2048 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD MSBs (01h) | D3 | RTD resistance data | 2^10 | 1024 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD MSBs (01h) | D2 | RTD resistance data | 2^9 | 512 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD MSBs (01h) | D1 | RTD resistance data | 2^8 | 256 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD MSBs (01h) | D0 | RTD resistance data | 2^7 | 128 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs (02h) | D7 | RTD resistance data | 2^6 | 64 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs (02h) | D6 | RTD resistance data | 2^5 | 32 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs (02h) | D5 | RTD resistance data | 2^4 | 16 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs (02h) | D4 | RTD resistance data | 2^3 | 8 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs (02h) | D3 | RTD resistance data | 2^2 | 4 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs (02h) | D2 | RTD resistance data | 2^1 | 2 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs (02h) | D1 | RTD resistance data LSB | 2^0 | 1 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| RTD LSBs (02h) | D0 | Fault | - | - | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |

### RTD Resistance-Data Relationship Examples

The datasheet notes that D0 (`Fault`) is assumed to be 0 for this table. [Source: MAX31865 RTD-to-Digital Converter, p. 15]

| RRTD/RREF | RTD MSBs binary | RTD LSBs binary | RTD MSBs hex | RTD LSBs hex | Decimal | Source |
|---:|---|---|---:|---:|---:|---|
| 0.025 | `0000 0110` | `0110 0110b` | 06h | 66h | 819 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| 0.125 | `0010 0000` | `0000 0000b` | 20h | 00h | 4096 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| 0.25 | `0100 0000` | `0000 0000b` | 40h | 00h | 8192 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| 0.50 | `1000 0000` | `0000 0000b` | 80h | 00h | 16,384 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| 0.75 | `1100 0000` | `0000 0000b` | C0h | 00h | 24,576 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |
| 0.999 | `1111 1111` | `1111 1110b` | FFh | FEh | 32,767 | [Source: MAX31865 RTD-to-Digital Converter, p. 15] |

## Fault Threshold Registers (03h-06h)

The High Fault Threshold and Low Fault Threshold registers select trip thresholds for RTD fault detection. RTD conversion results are compared with these registers to generate Fault Status register bits D7 and D6. [Source: MAX31865 RTD-to-Digital Converter, p. 16]

The RTD Data Registers, High Fault Threshold Registers, and Low Fault Threshold Registers all have the same format. [Source: MAX31865 RTD-to-Digital Converter, p. 16]

The body text states that RTD High is set if the RTD resistance register value is greater than or equal to the High Fault Threshold register value, and RTD Low is set if the RTD resistance value is less than or equal to the Low Fault Threshold register value. Other fault-flow/table wording uses `>` and `<`; this conflict is listed in `08_variant_differences_and_open_questions.md`. [Conflict: MAX31865 RTD-to-Digital Converter, p. 16 vs MAX31865 RTD-to-Digital Converter, p. 11; MAX31865 RTD-to-Digital Converter, p. 12; MAX31865 RTD-to-Digital Converter, p. 22; MAX31865 RTD-to-Digital Converter, p. 23]

POR value for the High Fault Threshold register is `FFFFh`; POR value for the Low Fault Threshold register is `0000h`. [Source: MAX31865 RTD-to-Digital Converter, p. 16]

| Register pair | Register | Bit | Meaning | Bit weighting | Decimal value | Source |
|---|---|---|---|---:|---:|---|
| High Fault Threshold | MSB (03h) | D7-D0 | Same 15-bit data format as RTD data, with D7 as MSB through D0 as 2^7. | 2^14 through 2^7 | 16384 through 128 | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| High Fault Threshold | LSB (04h) | D7-D1 | Same 15-bit data format as RTD data, with D1 as LSB. | 2^6 through 2^0 | 64 through 1 | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| High Fault Threshold | LSB (04h) | D0 | X / don't care in threshold table. | - | - | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| Low Fault Threshold | MSB (05h) | D7-D0 | Same 15-bit data format as RTD data, with D7 as MSB through D0 as 2^7. | 2^14 through 2^7 | 16384 through 128 | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| Low Fault Threshold | LSB (06h) | D7-D1 | Same 15-bit data format as RTD data, with D1 as LSB. | 2^6 through 2^0 | 64 through 1 | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| Low Fault Threshold | LSB (06h) | D0 | X / don't care in threshold table. | - | - | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |

## Fault Status Register (07h)

The Fault Status register latches detected fault bits. Writing 1 to the Fault Status Clear bit in the Configuration register returns all fault status bits to 0. [Source: MAX31865 RTD-to-Digital Converter, p. 16]

POR state is `00h`; the register is read-only. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

| Bit | Meaning | Condition text from register table | Additional documented behavior | Source |
|---|---|---|---|---|
| D7 | RTD High Threshold | RTD High Threshold | Set when RTD resistance register value is greater than or equal to the High Fault Threshold register value, per body text. | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| D6 | RTD Low Threshold | RTD Low Threshold | Set when RTD resistance value is less than or equal to the Low Fault Threshold register value, per body text. | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| D5 | REFIN- high fault | `REFIN- > 0.85 x VBIAS` | Checked during a master-initiated fault-detection cycle. | [Source: MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 16] |
| D4 | REFIN- low fault with FORCE- open | `REFIN- < 0.85 x VBIAS (FORCE- open)` | Checked during a master-initiated fault-detection cycle. | [Source: MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 16] |
| D3 | RTDIN- low fault with FORCE- open | `RTDIN- < 0.85 x VBIAS (FORCE- open)` | Checked during a master-initiated fault-detection cycle. | [Source: MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 16] |
| D2 | Overvoltage/undervoltage fault | Overvoltage/undervoltage fault | Set when an overvoltage or undervoltage condition is detected on protected inputs; ADC conversion updates halt until the fault is no longer detected. | [Source: MAX31865 RTD-to-Digital Converter, p. 11; MAX31865 RTD-to-Digital Converter, p. 16] |
| D1 | X | X / don't care | No status meaning is defined in the register table. | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |
| D0 | X | X / don't care | No status meaning is defined in the register table. | [Source: MAX31865 RTD-to-Digital Converter, p. 16] |

Fault Status bits are latched until the Fault Clear bit in the Configuration register is set; the datasheet states this allows intermittent faults to be captured. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

## Reserved/Don't-Care Handling Found in PDFs

The PDFs explicitly mark the fault-threshold LSB bit D0 as `X` / don't care in the threshold-register definition table. [Source: MAX31865 RTD-to-Digital Converter, p. 16]

The PDFs explicitly mark Fault Status register D1 and D0 as `x` in the fault-status definition table. [Source: MAX31865 RTD-to-Digital Converter, p. 16]

No general instruction was found in the provided PDFs requiring reserved bits to be written as 0, preserved, or ignored on readback; the register tables only identify specific `X` / don't-care fields. This gap is repeated in `08_variant_differences_and_open_questions.md`. [Sources: MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 16]
