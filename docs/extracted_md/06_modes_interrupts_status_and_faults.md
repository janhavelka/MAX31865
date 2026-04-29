# Modes, DRDY, Status, and Faults

## Conversion and Power Modes

### VBIAS Control

When no conversions are being performed, VBIAS may be disabled to reduce power dissipation. Write 1 to Configuration register D7 to enable VBIAS before a single one-shot conversion. When automatic continuous conversion mode is selected, VBIAS remains on continuously. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

### Automatic Conversion Mode

Writing 1 to Configuration register D6 selects automatic conversion mode, in which conversions occur continuously at a 50/60 Hz rate. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

The first conversion after enabling continuous conversion mode takes a time equal to the single conversion time for the selected notch frequency. [Source: MAX31865 RTD-to-Digital Converter, p. 4]

Temperature conversion time in continuous mode is 16.7 ms typ / 17.6 ms max with the 60 Hz notch, and 20 ms typ / 21 ms max with the 50 Hz notch. [Source: MAX31865 RTD-to-Digital Converter, p. 3]

### Normally Off Mode and One-Shot Conversion

Writing 0 to Configuration register D6 exits automatic conversion mode and enters `Normally Off` mode. One-shot conversions may be initiated from Normally Off mode. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

When conversion mode is `Normally Off`, writing 1 to D5 starts a single resistance conversion. The conversion is triggered when CS goes high after writing 1 to D5. If a multibyte write is performed, conversion is triggered when CS goes high at the end of the transaction. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

The 1-shot bit is self-clearing. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

Single conversion time is 52 ms typ / 55 ms max with the 60 Hz notch and 62.5 ms typ / 66 ms max with the 50 Hz notch. [Source: MAX31865 RTD-to-Digital Converter, p. 3]

### Filter Mode

Configuration register D0 selects notch frequencies for the noise rejection filter. Write 0 to reject 60 Hz and harmonics; write 1 to reject 50 Hz and harmonics. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

The datasheet notes not to change the notch frequency while in auto conversion mode. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

The ADC input noise is attenuated by a third-order digital sinc filter; noise from 50 Hz or 60 Hz power sources, including harmonics of the AC power fundamental, is attenuated by 82 dB. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

## DRDY Behavior

DRDY is an active-low, push-pull, data-ready output. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

DRDY goes low when a new conversion result is available in the RTD Data Registers. [Source: MAX31865 RTD-to-Digital Converter, p. 19]

When a read operation of the RTD Data Registers completes, DRDY returns high. [Source: MAX31865 RTD-to-Digital Converter, p. 19]

Figure 9, `DRDY Operation`, shows conversions labeled n, n+1, and n+2, DRDY asserting low when RTD register contents are available, and an SPI read using an RTD data address followed by RTD data on SDO before DRDY returns high. [Source: MAX31865 RTD-to-Digital Converter, p. 19]

The timing table specifies `tDRDYH`, `Address 01h or 02h Decoded to DRDY High`, as 50 ns after RTD register read access. This is recorded as a wording difference from the DRDY section's `read operation ... completes` phrasing in the open-questions file. [Source: MAX31865 RTD-to-Digital Converter, p. 4]

No additional interrupt-controller registers, interrupt masks, or interrupt-clear commands beyond DRDY and the fault-status clear behavior were found in the provided PDFs; the register map lists no interrupt-control register. [Sources: MAX31865 RTD-to-Digital Converter, p. 8; MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 15; MAX31865 RTD-to-Digital Converter, p. 16; MAX31865 RTD-to-Digital Converter, p. 19]

## RTD Data Fault Bit

D0 of the RTD LSBs register is a Fault bit indicating whether any RTD faults have been detected. [Source: MAX31865 RTD-to-Digital Converter, p. 15]

Fault-detection flowcharts show D0 of the RTD Data LSB register being set when overvoltage/undervoltage, master-initiated fault, or threshold fault paths set a corresponding Fault Status bit. [Source: MAX31865 RTD-to-Digital Converter, p. 12]

## Fault-Detection Classes

The MAX31865 detects faults in external RTD and 2-, 3-, or 4-wire cables. Some faults are detected on every conversion, and others are detected only when the master requests a fault-detection cycle. During a fault-detection cycle, the device can disconnect FORCE- from its GND2 return path using an internal analog switch. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

| Detection class | Conditions checked | Status bits set | Source |
|---|---|---|---|
| Always active | Overvoltage greater than VDD or undervoltage less than GND1 on FORCE+, FORCE2, RTDIN+, RTDIN-, or FORCE-. | Fault Status D2 and RTD Data LSB D0; ADC halts updates while the voltage fault is detected. | [Source: MAX31865 RTD-to-Digital Converter, p. 11; MAX31865 RTD-to-Digital Converter, p. 12] |
| Every ADC conversion | Greater-than-or-equal-to threshold high conversion result; less-than-or-equal-to threshold low conversion result. | Fault Status D7 for high threshold, D6 for low threshold, and RTD Data LSB D0. | [Sources: MAX31865 RTD-to-Digital Converter, p. 11; MAX31865 RTD-to-Digital Converter, p. 12; MAX31865 RTD-to-Digital Converter, p. 16] |
| Master-initiated fault-detection cycle | VREFIN- > 0.85 x VBIAS; VREFIN- < 0.85 x VBIAS when FORCE- switch is open; VRTDIN- < 0.85 x VBIAS when FORCE- switch is open. | Fault Status D5, D4, D3 respectively, and RTD Data LSB D0. | [Sources: MAX31865 RTD-to-Digital Converter, p. 11; MAX31865 RTD-to-Digital Converter, p. 12; MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 16] |

Threshold-comparison wording differs by section: body text says high/low threshold faults use `>=` and `<=`, while flowchart/fault-decoding wording uses `>` and `<`. The conflict is listed in `08_variant_differences_and_open_questions.md`. [Conflict: MAX31865 RTD-to-Digital Converter, p. 16 vs MAX31865 RTD-to-Digital Converter, p. 11; MAX31865 RTD-to-Digital Converter, p. 12; MAX31865 RTD-to-Digital Converter, p. 22; MAX31865 RTD-to-Digital Converter, p. 23]

## Input Protection and Overvoltage/Undervoltage Faults

FORCE+, FORCE2, FORCE-, RTDIN+, and RTDIN- are protected against input voltages up to ±45 V. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

Signals applied to those pins are gated by analog switches that open when the applied voltage is typically greater than VDD + 100 mV or less than GND1 - 400 mV. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

When a voltage fault occurs, the protection circuits may allow approximately 350 µA of current flow, and the datasheet states this current does not damage the MAX31865. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

When an overvoltage or undervoltage condition is detected, Fault Status register D2 is set and ADC conversion updates halt until the fault is no longer detected; conversions resume after the fault is no longer detected. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

## Fault Status Latching and Clear

The Fault Status register latches detected fault bits. Writing 1 to the Fault Status Clear bit in the Configuration register returns all fault status bits to 0. [Source: MAX31865 RTD-to-Digital Converter, p. 16]

Fault Status bits are latched until the Fault Clear bit in the Configuration register is set, which the datasheet states allows intermittent faults to be captured. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

To clear Fault Status bits, write 1 to Configuration register D1 while writing 0 to D5, D3, and D2. This returns Fault Status register bits D[7:2] to 0, and D1 self-clears. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

Fault Status D2 and RTD LSB D0 may set again immediately after resetting if an overvoltage/undervoltage fault persists. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

An open RTD element or a short across the RTD element is detected on every conversion based on resistance data. An open RTD element produces a full-scale reading, and a shorted RTD element produces a conversion result near zero. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

The high fault threshold registers set the threshold for open RTD element detection. If the conversion result is greater than or equal to the threshold value, the RTD High bit is set at the end of conversion. Open RTD element detection can also be performed on demand by testing for `VREFIN- > 0.85 x VBIAS`. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

The low fault threshold registers set the threshold for shorted RTD detection. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

## Master-Initiated Fault-Detection Cycle

The fault-detection cycle checks three comparisons and sets the associated Fault Status bits: REFIN- > 85% x VBIAS sets D5; REFIN- < 85% x VBIAS with FORCE- open sets D4; RTDIN- < 85% x VBIAS with FORCE- open sets D3. The datasheet note states all voltages are referenced to GND1. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

If external RTD interface circuitry includes an input filter with a time constant greater than 100 µs, the datasheet states the fault-detection cycle timing should be controlled in manual mode. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

### Automatic Fault-Detection Cycle

Writing `100X010Xb` to the Configuration register enters automatic fault detection. The ADC is then in `Normally Off` mode. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

The automatic fault-detection cycle inserts 100 µs delays before checking for faults, allowing the external input filter to settle. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

Fault Detect Cycle bits D[3:2] self-clear to `00b` on completion of the automatic fault-detection cycle. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

The electrical table lists automatic fault-detection cycle time as 550 µs typ / 600 µs max from CS high to cycle complete. [Source: MAX31865 RTD-to-Digital Converter, p. 3]

The automatic-mode flowchart shows this sequence: master writes `100X010Xb`; FORCE-input switch remains closed; 100 µs delay; test `VREFIN- > 0.85 x VBIAS`; 100 µs delay; open FORCE-input switch; 210 µs delay; test `VREFIN- < 0.85 x VBIAS`; 100 µs delay; test `RTDIN- < 0.85 x VBIAS`; close FORCE-input switch; set Configuration register to `100X000Xb`; end fault-detection cycle. [Source: MAX31865 RTD-to-Digital Converter, p. 12]

### Manual Fault-Detection Cycle

To enter manual fault detection, first ensure that VBIAS has been on for at least 5 time constants, then write `100X100Xb` to the Configuration register. The ADC is then in `Normally Off` mode. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

During manual cycle 1, the MAX31865 checks for faults while the FORCE- input switch is closed. When this check completes, the FORCE- input switch opens and Fault Detect Cycle bits D[3:2] remain set to `10b`. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

After manual cycle 1, wait at least 5 time constants, then write `100X110Xb` to the Configuration register. The MAX31865 checks for faults while the FORCE- input switch is open. When the check completes, the FORCE- input switch closes and Fault Detect Cycle bits D[3:2] self-clear to `00b`. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

The manual-mode flowchart shows this sequence: master writes `100X100Xb`; FORCE-input switch remains closed; 100 µs delay; test `VREFIN- > 0.85 x VBIAS`; 100 µs delay; open FORCE-input switch; wait for master write `100X110Xb`; 100 µs delay; test `VREFIN- < 0.85 x VBIAS`; 100 µs delay; test `RTDIN- < 0.85 x VBIAS`; close FORCE-input switch; set Configuration register to `100X000Xb`; end fault-detection cycle. [Source: MAX31865 RTD-to-Digital Converter, p. 12]

If 1 is written to D5 and D2 or D3 in a single Configuration-register write, both commands are ignored. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

If `100X110Xb` is set without prior initiation of manual cycle 1 using `100X100Xb`, automatic fault-detection mode is run instead. [Source: MAX31865 RTD-to-Digital Converter, p. 14]

## RTDIN+ Cable Fault Caveat

In 3-wire and 4-wire RTD connection configurations, a broken or disconnected RTDIN+ cable results in an unbiased ADC+ input into the MAX31865. The datasheet states that this causes unpredictable ADC conversion results influenced by PCB layout, external circuit noise, and ambient temperature. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

The datasheet states that this RTDIN+ cable fault condition can go undetected depending on the values set in the fault threshold registers. If this condition is of interest, it recommends adding a 10 MΩ resistor from RTDIN+ to BIAS so a broken or disconnected RTDIN+ lead produces a full-scale RTD resistance measurement. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

## Fault Decoding: 2-Wire Setups

Table applies when the Fault bit in the RTD Data LSB register is 1. [Source: MAX31865 RTD-to-Digital Converter, p. 22]

| Fault Status bit set | Possible cause | Condition detected | Resulting data | Source |
|---|---|---|---|---|
| D7 | Open RTD element | Measured resistance greater than High Fault Threshold value | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D6 | Shorted RTD element | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D6 | RTDIN+ shorted low | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D5 | Open RTD | VREFIN- > 0.85 x VBIAS | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D5 | RTDIN+ shorted high | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D5 | RTDIN- shorted high | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D4 | RTDIN- shorted low | VREFIN- < 0.85 x VBIAS (FORCE- open) | Appear to be valid | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D3 | RTDIN- shorted low | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Appear to be valid | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D3 | RTDIN+ shorted low | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D2 | Overvoltage or undervoltage fault | Any protected input voltage > VDD or < GND1 | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |

## Fault Decoding: 3-Wire Setups

Table applies when the Fault bit in the RTD Data LSB register is 1. [Source: MAX31865 RTD-to-Digital Converter, p. 22]

| Fault Status bit set | Possible cause | Condition detected | Resulting data | Source |
|---|---|---|---|---|
| D7 | Open RTD element | Measured resistance greater than High Fault Threshold value | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D7 | RTDIN+ shorted high and not connected to RTD | Measured resistance greater than High Fault Threshold value | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D7 | Force+ shorted high and connected to RTD | Measured resistance greater than High Fault Threshold value | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D6 | RTDIN+ shorted to RTDIN- | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D6 | RTDIN+ shorted low and not connected to RTD | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D6 | Force+ shorted low | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D5 | Open RTD element | VREFIN- > 0.85 x VBIAS | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D5 | Force+ shorted high and connected to RTD | VREFIN- > 0.85 x VBIAS | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D5 | Force+ unconnected | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D5 | Force+ shorted high and not connected to RTD | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D5 | RTDIN- shorted high | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D4 | RTDIN- shorted low | VREFIN- < 0.85 x VBIAS (FORCE- open) | Appear to be valid | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D3 | Force+ shorted low | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D3 | RTDIN+ shorted low and connected to RTD | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D3 | RTDIN- shorted low | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Appear to be valid | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |
| D2 | Overvoltage or undervoltage fault | Any protected input voltage > VDD or < GND1 | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 22] |

## Fault Decoding: 4-Wire Setups

Table applies when the Fault bit in the RTD Data LSB register is 1. [Source: MAX31865 RTD-to-Digital Converter, p. 23]

| Fault Status bit set | Possible cause | Condition detected | Resulting data | Source |
|---|---|---|---|---|
| D7 | Open RTD element | Measured resistance greater than High Fault Threshold value | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D7 | RTDIN+ shorted high and not connected to RTD | Measured resistance greater than High Fault Threshold value | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D7 | Force+ shorted high and connected to RTD | Measured resistance greater than High Fault Threshold value | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D6 | RTDIN+ shorted to RTDIN- | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D6 | RTDIN+ shorted low and not connected to RTD | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D6 | RTDIN- shorted high and not connected to RTD | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D6 | Force+ shorted low | Measured resistance less than Low Fault Threshold value | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D5 | Open RTD element | VREFIN- > 0.85 x VBIAS | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D5 | Force+ shorted high and connected to RTD | VREFIN- > 0.85 x VBIAS | Full scale | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D5 | Force- unconnected | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D5 | Force+ unconnected | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D5 | Force+ shorted high and not connected to RTD | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D5 | Force- shorted high and not connected to RTD | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D5 | Force- shorted high and connected to RTD | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D5 | Force- shorted low and not connected to RTD | VREFIN- > 0.85 x VBIAS | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D4 | Force- shorted low and connected to RTD | VREFIN- < 0.85 x VBIAS (FORCE- open) | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D4 | RTDIN- shorted low and connected to RTD | VREFIN- < 0.85 x VBIAS (FORCE- open) | Appear to be valid | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D3 | Force+ shorted low | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D3 | RTDIN+ shorted low and connected to RTD | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Near zero | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D3 | RTDIN- shorted low and connected to RTD | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Appear to be valid | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D3 | RTDIN- shorted low and not connected to RTD | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Appear to be valid | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D3 | Force- shorted low | VRTDIN- < 0.85 x VBIAS (FORCE- open) | Appear to be valid | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |
| D2 | Overvoltage or undervoltage fault | Any protected input voltage > VDD or < GND1 | Indeterminate | [Source: MAX31865 RTD-to-Digital Converter, p. 23] |

## Fault-Related Gaps

No fault FIFO, fault counter, timestamp, interrupt mask, or separate fault-clear register is documented in the provided PDFs. The only documented clear mechanism is Configuration register D1. [Sources: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 15; MAX31865 RTD-to-Digital Converter, p. 16; MAX31865 RTD-to-Digital Converter, p. 21]
