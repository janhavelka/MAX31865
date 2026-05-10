# Initialization, Reset, and Operational Notes

## Power-On Reset and Default Register States

The electrical characteristics table lists the power-on reset voltage threshold as 2 V min and 2.27 V typ, with 120 mV power-on reset voltage hysteresis. [Source: MAX31865 RTD-to-Digital Converter, p. 3]

The datasheet register table provides these POR states. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

| Register | Address | POR state | Source |
|---|---:|---:|---|
| Configuration | 00h | 00h | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| RTD MSBs | 01h | 00h | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| RTD LSBs | 02h | 00h | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| High Fault Threshold MSB | 03h | FFh | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| High Fault Threshold LSB | 04h | FFh | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| Low Fault Threshold MSB | 05h | 00h | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| Low Fault Threshold LSB | 06h | 00h | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |
| Fault Status | 07h | 00h | [Source: MAX31865 RTD-to-Digital Converter, p. 13] |

No dedicated hardware reset pin is shown in the pin-configuration figure or pin-description table. [Source: MAX31865 RTD-to-Digital Converter, p. 7; MAX31865 RTD-to-Digital Converter, p. 8]

The SPI function table uses the mode label `Disable Reset` for CS high, with SCLK and SDI input disabled and SDO high impedance. The PDFs do not further define a separate serial reset command. [Source: MAX31865 RTD-to-Digital Converter, p. 17]

## Supply and Decoupling Notes

VDD and DVDD are both recommended at 3.0 V min, 3.3 V typ, and 3.6 V max. [Source: MAX31865 RTD-to-Digital Converter, p. 2]

The pin-description table states that DVDD should connect to a 3.3 V power supply and be bypassed to DGND with a 0.1 uF bypass capacitor. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

The pin-description table states that VDD should connect to a 3.3 V power supply and be bypassed to GND1 with a 0.1 uF bypass capacitor. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

The Applications Information section states that best results are achieved by decoupling VDD and DVDD with a 0.1 uF capacitor, using a high-quality ceramic surface-mount capacitor if possible. It states that surface-mount components minimize lead inductance and that ceramic capacitors tend to have adequate high-frequency response for decoupling applications. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

GND2 is analog ground and should connect to GND1; GND1 is analog ground and should connect to GND2. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

The TQFN exposed pad should connect to GND1. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

## VBIAS, Input Filter, and Settling Delays

If VBIAS is on as selected by the Configuration register, the RTD voltage is sampled when CS goes high and conversion begins. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

If VBIAS is off to reduce supply current between conversions, filter capacitors at the RTDIN inputs need to charge before an accurate conversion can be performed. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

Before initiating a one-shot conversion after enabling VBIAS from off, the datasheet states to enable VBIAS and wait at least 10.5 time constants of the input RC network plus an additional 1 ms. [Source: MAX31865 RTD-to-Digital Converter, p. 13]

The electrical-characteristics note for 15-bit settling states that a wait of at least 10.5 input-RC time constants is required and that maximum startup time is calculated with a 10 kOhm reference resistor and a 0.1 uF capacitor across the RTD inputs. [Source: MAX31865 RTD-to-Digital Converter, p. 4]

For operation in noisy environments, the datasheet states that a filter capacitor may be placed across the RTDIN+ and RTDIN- inputs. [Source: MAX31865 RTD-to-Digital Converter, p. 19]

After an overvoltage or undervoltage fault, after a fault-detection cycle, or after enabling VBIAS, the datasheet states to always allow settling time of the input filter before restarting the ADC. It recommends at least five time constants plus an additional 1 ms for protection devices to stabilize to achieve specified accuracy. [Source: MAX31865 RTD-to-Digital Converter, p. 19]

Settling-delay guidance differs by context: the one-shot/VBIAS text uses 10.5 time constants plus 1 ms, while the Applications Information text recommends five time constants plus 1 ms after faults, fault-detection cycles, or enabling VBIAS. This is recorded as an unresolved wording difference in `08_variant_differences_and_open_questions.md`. [Conflict: MAX31865 RTD-to-Digital Converter, p. 13 vs MAX31865 RTD-to-Digital Converter, p. 19]

The typical application circuits label CI as 10 nF for a 1 kOhm RTD and 100 nF for a 100 Ohm RTD. [Sources: MAX31865 RTD-to-Digital Converter, p. 1; MAX31865 RTD-to-Digital Converter, p. 24; Temperature Sensor Tutorial, p. 7]

## Conversion Timing Facts

Single conversion time is 52 ms typ / 55 ms max with the 60 Hz notch and 62.5 ms typ / 66 ms max with the 50 Hz notch. [Source: MAX31865 RTD-to-Digital Converter, p. 3]

Continuous conversion time is 16.7 ms typ / 17.6 ms max with the 60 Hz notch and 20 ms typ / 21 ms max with the 50 Hz notch. [Source: MAX31865 RTD-to-Digital Converter, p. 3]

The first conversion after enabling continuous conversion mode takes a time equal to the single conversion time for the selected notch frequency. [Source: MAX31865 RTD-to-Digital Converter, p. 4]

The automatic fault-detection cycle time is 550 us typ / 600 us max from CS high to cycle complete. [Source: MAX31865 RTD-to-Digital Converter, p. 3]

## Reference Resistor and Ratio Notes

The recommended operating conditions table allows RREF from 350 Ohm min to 10 kOhm max. [Source: MAX31865 RTD-to-Digital Converter, p. 2]

The datasheet states that a reference resistor equal to four times the RTD 0degC resistance is optimum for a platinum RTD; it gives 400 Ohm for PT100 and 4 kOhm for PT1000. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The design solution states that the MAX31865 does not require an exact current source, because RTD current is a byproduct of the RRTD/RREF ratio, and that precision is dependent on the discrete RREF. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 3]

The design solution states that system stability depends on the RREF/RTD resistor ratio and low-noise characteristics of the delta-sigma ADC. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 3]

## Cable Resistance Notes

The recommended operating conditions table allows RCABLE from 0 Ohm to 50 Ohm per lead. [Source: MAX31865 RTD-to-Digital Converter, p. 2]

For PT100, the datasheet states that 0.4 Ohm series resistance causes approximately 1degC error. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The design solution states that reasonable RCABLE values can be as high as 50 Ohm, and that a 2-wire RTD with 10 Ohm cable resistance creates approximately 25degC error across the entire PT100 temperature range. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 2]

The design solution states that 3-wire and 4-wire RTD hardware implementations significantly reduce these cable-resistance errors. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 2]

## RTDIN+ Broken-Cable Workaround

In 3-wire and 4-wire RTD configurations, a broken or disconnected RTDIN+ cable leaves the ADC+ input unbiased, causing unpredictable ADC conversion results that can be influenced by PCB layout, external circuit noise, and ambient temperature. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

This cable-fault condition can go undetected depending on fault-threshold register values. The datasheet states that adding a 10 MOhm resistor from RTDIN+ to BIAS causes a broken or disconnected RTDIN+ lead to produce a full-scale RTD resistance measurement. [Source: MAX31865 RTD-to-Digital Converter, p. 21]

## RTD Resistance and Temperature Conversion

The datasheet provides this resistance equation:

```text
RRTD = (ADC Code x RREF) / 2^15
```

where `ADC Code` is the 15-bit ADC result from RTD Data registers 01h-02h and `RREF` is the resistance of the reference resistor. [Source: MAX31865 RTD-to-Digital Converter, p. 19; MAX31865 RTD-to-Digital Converter, p. 20]

The design solution gives the same formula as Equation 2. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 3]

The design solution states that PT100 resistance over temperature ranges from 18.52 Ohm at -200degC to 390 Ohm at +850degC. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 2]

The design solution states that the maximum RTD end-point linearity error from the Callendar-Van Dusen equation is approximately 4.34% for PT100 or PT1000. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 2]

For the range -100degC to +100degC, the datasheet provides this approximation:

```text
Temperature (degC) ~= (ADC code / 32) - 256
```

The datasheet states this approximation gives 0degC error at 0degC, -1.75degC error at -100degC, and -1.4degC error at +100degC, assuming an IEC751 RTD and RREF equal to four times the 0degC RTD resistance. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

For high precision, the datasheet states to use the Callendar-Van Dusen equation or a lookup table to correct predictable RTD nonlinearity. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

## PT100 with 400 Ohm RREF Temperature Example

The datasheet states that in the typical case of a PT100 RTD with a 400 Ohm high-precision, low-drift reference resistor, Table 9 gives temperature/resistance/ADC examples. [Source: MAX31865 RTD-to-Digital Converter, p. 20]

| Temperature (degC) | RTD resistance (Ohm) | RTD Data Reg (01h-02h) hex | ADC code (dec) | ADC code/32 - 256 (degC) | Source |
|---:|---:|---:|---:|---:|---|
| -200 | 18.52 | 0BDAh | 1517 | -208.59 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -175 | 29.22 | 12B4h | 2394 | -181.19 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -150 | 39.72 | 196Ch | 3254 | -154.31 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -125 | 50.06 | 200Ah | 4101 | -127.84 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -100 | 60.26 | 2690h | 4936 | -101.75 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -75 | 70.33 | 2D04h | 5762 | -75.94 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -50 | 80.31 | 3366h | 6579 | -50.41 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -40 | 84.27 | 35EEh | 6903 | -40.28 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -30 | 88.22 | 3876h | 7227 | -30.16 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -20 | 92.16 | 3AFCh | 7550 | -20.06 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| -10 | 96.09 | 3D7Eh | 7871 | -10.03 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 0 | 100.00 | 4000h | 8192 | 0.00 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 10 | 103.90 | 4280h | 8512 | 10.00 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 20 | 107.79 | 44FCh | 8830 | 19.94 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 30 | 111.67 | 4778h | 9148 | 29.88 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 40 | 115.54 | 49F2h | 9465 | 39.78 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 50 | 119.40 | 4C6Ah | 9781 | 49.66 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 60 | 123.24 | 4EE0h | 10096 | 59.50 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 70 | 127.08 | 5154h | 10410 | 69.31 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 80 | 130.90 | 53C6h | 10723 | 79.09 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 90 | 134.71 | 5636h | 11035 | 88.84 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 100 | 138.51 | 58A4h | 11346 | 98.56 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 110 | 142.29 | 5B12h | 11657 | 108.28 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 120 | 146.07 | 5D7Ch | 11966 | 117.94 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 130 | 149.83 | 5FE4h | 12274 | 127.56 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 140 | 153.58 | 624Ch | 12582 | 137.19 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 150 | 157.33 | 64B0h | 12888 | 146.75 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 160 | 161.05 | 6714h | 13194 | 156.31 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 170 | 164.77 | 6974h | 13498 | 165.81 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 180 | 168.48 | 6BD4h | 13802 | 175.31 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 190 | 172.17 | 6E30h | 14104 | 184.75 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 200 | 175.86 | 708Ch | 14406 | 194.19 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 225 | 185.01 | 7668h | 15156 | 217.63 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 250 | 194.10 | 7C3Ah | 15901 | 240.91 | [Source: MAX31865 RTD-to-Digital Converter, p. 20] |
| 275 | 203.11 | 81FEh | 16639 | 263.97 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 300 | 212.05 | 87B6h | 17371 | 286.84 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 325 | 220.92 | 8D64h | 18098 | 309.56 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 350 | 229.72 | 9304h | 18818 | 332.06 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 375 | 238.44 | 989Ah | 19533 | 354.41 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 400 | 247.09 | 9E24h | 20242 | 376.56 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 425 | 255.67 | A3A2h | 20945 | 398.53 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 450 | 264.18 | A914h | 21642 | 420.31 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 475 | 272.61 | AE7Ah | 22333 | 441.91 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 500 | 280.98 | B3D4h | 23018 | 463.31 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 525 | 289.27 | B922h | 23697 | 484.53 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |
| 550 | 297.49 | BE64h | 24370 | 505.56 | [Source: MAX31865 RTD-to-Digital Converter, p. 21] |

## Operational Gaps

The provided PDFs do not document a required full initialization sequence beyond the individual configuration, delay, and settling facts listed above. [Sources: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 19]

The provided PDFs do not document a nonvolatile configuration store, calibration register, factory trim register, device ID register, or software reset command. [Sources: MAX31865 RTD-to-Digital Converter, p. 13; MAX31865 RTD-to-Digital Converter, p. 14; MAX31865 RTD-to-Digital Converter, p. 15; MAX31865 RTD-to-Digital Converter, p. 16; MAX31865 RTD-to-Digital Converter, p. 17]
