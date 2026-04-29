# Electrical and Timing Characteristics

## Absolute Maximum Ratings

The absolute maximum ratings are stress ratings only; the datasheet states that functional operation at these or any other conditions beyond the operational sections is not implied, and exposure to absolute maximum rating conditions for extended periods may affect device reliability. [Source: MAX31865 RTD-to-Digital Converter, p. 2]

| Rating | Value | Source |
|---|---:|---|
| Voltage range on VDD relative to GND1 | -0.3 V to +4.0 V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Voltage range on BIAS, REFIN+, REFIN-, ISENSOR | -0.3 V to (VDD + 0.3 V) | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Voltage range on FORCE+, FORCE2, FORCE-, RTDIN+, RTDIN- relative to GND1 | -50 V to +50 V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Voltage range on DVDD relative to DGND | -0.3 V to +4.0 V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Voltage range on all digital pins relative to DGND | -0.3 V to (VDVDD + 0.3 V) | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Continuous power dissipation, TQFN, TA = +70°C | 2758.6 mW, derate 34.5 mW/°C above +70°C | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Continuous power dissipation, SSOP, TA = +70°C | 952.4 mW, derate 11.9 mW/°C above +70°C | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| ESD protection, all pins, Human Body Model | ±2 kV | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Operating temperature range | -40°C to +125°C | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Junction temperature | +150°C | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Storage temperature range | -65°C to +150°C | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Soldering temperature, reflow | +260°C | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Lead temperature, soldering, 10 s | +300°C | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |

## Package Thermal Characteristics

Package thermal resistances were obtained using the JEDEC JESD51-7 method with a four-layer board. [Source: MAX31865 RTD-to-Digital Converter, p. 2]

| Package | Parameter | Value | Source |
|---|---|---:|---|
| TQFN | Junction-to-ambient thermal resistance, θJA | 29°C/W | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| TQFN | Junction-to-case thermal resistance, θJC | 2°C/W | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| SSOP | Junction-to-ambient thermal resistance, θJA | 84°C/W | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| SSOP | Junction-to-case thermal resistance, θJC | 32°C/W | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |

## Recommended DC Operating Conditions

Conditions for this table are `TA = -40°C to +125°C, unless otherwise noted`; the table references Notes 2 and 3. [Source: MAX31865 RTD-to-Digital Converter, p. 2]

| Parameter | Symbol | Conditions | Min | Typ | Max | Units | Source |
|---|---|---|---:|---:|---:|---|---|
| VDD | VDD | - | 3.0 | 3.3 | 3.6 | V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| DVDD | VDVDD | - | 3.0 | 3.3 | 3.6 | V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Input Logic 0 | VIL | CS, SDI, SCLK | -0.3 | - | 0.3 x VDVDD | V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Input Logic 1 | VIH | CS, SDI, SCLK | 0.7 x VDVDD | - | VDVDD + 0.3 | V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Analog voltages | - | FORCE+, FORCE2, FORCE-, RTDIN+, RTDIN-; normal conversion results | 0 | - | VBIAS | V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Reference resistor | RREF | - | 350 | - | 10k | Ω | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| Cable resistance | RCABLE | Per lead | 0 | - | 50 | Ω | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |

## DC Electrical Characteristics

Conditions for the electrical characteristics table are `3.0 V <= VDD <= 3.6 V`, `TA = -40°C to +125°C`, unless otherwise noted. Typical values are at `TA = +25°C`, `VDD = VDVDD = 3.3 V`. The table references Notes 2 and 3. [Sources: MAX31865 RTD-to-Digital Converter, p. 2; MAX31865 RTD-to-Digital Converter, p. 3]

The datasheet states that parametric min/max values shown in the Electrical Characteristics table are guaranteed and that other parametric values quoted in the datasheet are provided for guidance. [Source: MAX31865 RTD-to-Digital Converter, p. 26]

Where a value's min/typ/max column was not unambiguously separable from the extracted table text, the value is retained in the `Datasheet value` column without assigning an undocumented min/typ/max category.

| Parameter | Symbol | Conditions | Min | Typ | Max | Datasheet value | Units | Source |
|---|---|---|---:|---:|---:|---|---|---|
| ADC resolution | - | No missing codes | - | 15 | - | - | Bits | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| ADC full-scale input voltage, RTDIN+ - RTDIN- | - | - | - | - | - | REFIN+ - REFIN- | V | [Source: MAX31865 RTD-to-Digital Converter, p. 2] |
| ADC common-mode input range | - | - | 0 | - | VBIAS | - | V | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Input leakage current | - | RTDIN+, RTDIN-, 0°C to +70°C, on-state | - | - | 2 | - | nA | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Input leakage current | - | RTDIN+, RTDIN-, -40°C to +85°C, on-state | - | - | 5 | - | nA | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Input leakage current | - | RTDIN+, RTDIN-, -40°C to 100°C, on-state | - | - | 14 | - | nA | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Bias voltage | VBIAS | - | 1.95 | 2.00 | 2.06 | - | V | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Bias voltage output current | IOUT | - | 0.2 | - | 5.75 | - | mA | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Bias voltage load regulation | - | IOUT <= 5.75 mA | - | - | 30 | - | mV/mA | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Bias voltage startup time | - | Note 4 | - | - | 10 | - | ms | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| ADC full-scale error | - | - | - | - | - | ±1 | LSB | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| ADC integral nonlinearity | - | Differential input, endpoint fit, 0.3 x VBIAS <= VREF <= VBIAS | - | - | - | ±1 | LSB | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| ADC offset error | - | - | -3 | - | +3 | - | LSB | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Noise over Nyquist bandwidth | - | Input referred | - | 150 | - | - | µV RMS | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Common-mode rejection | - | - | - | 90 | - | - | dB | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| 50/60 Hz noise rejection | - | Fundamental and harmonics | - | 82 | - | - | dB | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Temperature conversion time | tCONV | Continuous conversion, 60 Hz notch | - | 16.7 | 17.6 | - | ms | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Temperature conversion time | tCONV | Single conversion, 60 Hz notch | - | 52 | 55 | - | ms | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Temperature conversion time | tCONV | Single conversion, 50 Hz notch | - | 62.5 | 66 | - | ms | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Temperature conversion time | tCONV | Continuous conversion, 50 Hz notch | - | 20 | 21 | - | ms | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Automatic fault detection cycle time | - | From CS high to cycle complete | - | 550 | 600 | - | µs | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Power-supply rejection | - | - | - | 1 | - | - | LSB/V | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Power-supply current | IDD Shutdown | Bias off, ADC off | - | 1.5 | 3 | - | mA | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Power-supply current | IDD | Bias on, active conversion | - | 2 | 3.5 | - | mA | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Power-on reset voltage threshold | - | - | 2 | 2.27 | - | - | V | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Power-on reset voltage hysteresis | - | - | - | 120 | - | - | mV | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Input capacitance | CIN | Logic inputs | - | 6 | - | - | pF | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Input leakage current | IL | Logic inputs | -1 | - | +1 | - | µA | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Output high voltage | VOH | IOUT = -1.6 mA | VDVDD - 0.4 | - | - | - | V | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |
| Output low voltage | VOL | IOUT = 1.6 mA | - | - | 0.4 | - | V | [Source: MAX31865 RTD-to-Digital Converter, p. 3] |

The ADC section states that a negative input voltage produces output code 0, and an input voltage greater than the reference voltage produces a full-scale output. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

## AC Electrical Characteristics: SPI Interface

Conditions for the AC SPI table are `3.0 V <= VDD <= 3.6 V`, `TA = -40°C to +125°C`, unless otherwise noted. Typical values are at `TA = +25°C`, `VDD = VDVDD = 3.3 V`. The table references Notes 3 and 7 and Figures 1 and 2. [Source: MAX31865 RTD-to-Digital Converter, p. 4]

| Parameter | Symbol | Conditions | Min | Typ | Max | Units | Source |
|---|---|---|---:|---:|---:|---|---|
| Data to SCLK setup | tDC | Notes 8, 9 | 35 | - | - | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| SCLK to data hold | tCDH | Notes 8, 9 | 35 | - | - | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| SCLK to data valid | tCDD | Notes 8, 9, 10 | - | - | 80 | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| SCLK low time | tCL | Note 9 | 100 | - | - | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| SCLK high time | tCH | Note 9 | 100 | - | - | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| SCLK frequency | tCLK | Note 9 | DC | - | 5.0 | MHz | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| SCLK rise and fall | tR, tF | Note 9 | - | - | 200 | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| CS to SCLK setup | tCC | Note 9 | 400 | - | - | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| SCLK to CS hold | tCCH | Note 9 | 100 | - | - | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| CS inactive time | tCWH | Note 9 | 400 | - | - | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| CS to output high-Z | tCDZ | Notes 8, 9 | - | - | 40 | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| Address 01h or 02h decoded to DRDY high | tDRDYH | After RTD register read access; Note 9 | - | - | 50 | ns | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |

## Electrical and Timing Notes

| Note | Text preserved from datasheet | Source |
|---:|---|---|
| 2 | All voltages are referenced to ground when common. Currents entering the IC are specified positive. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| 3 | Limits are 100% production tested at TA = +25°C and/or TA = +85°C. Limits over the operating temperature range and relevant supply voltage range are guaranteed by design and characterization. Typical values are not guaranteed. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| 4 | For 15-bit settling, a wait of at least 10.5 time constants of the input RC network is required. Max startup time is calculated with a 10 kΩ reference resistor and a 0.1 µF capacitor across the RTD inputs. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| 5 | The first conversion after enabling continuous conversion mode takes a time equal to the single conversion time for the respective notch frequency. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| 6 | Specified with no load on the bias pin as the sum of analog and digital currents. No active communication. If the RTD input voltage is greater than the input reference voltage, then an additional 400 µA IDD can be expected. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| 7 | All timing specifications are guaranteed by design. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| 8 | Measured at VIH = 0.7 x VDVDD or VIL = 0.3 x VDVDD and 10 ms maximum rise and fall times. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| 9 | Measured with 50 pF load. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |
| 10 | Measured at VOH = 0.7 x VDVDD or VOL = 0.3 x VDVDD. Measured from the 50% point of SCLK to the VOH minimum of SDO. | [Source: MAX31865 RTD-to-Digital Converter, p. 4] |

## SPI Timing Diagrams

Figure 1 is titled `Timing Diagram: SPI Read Data Transfer`. It shows CS active low, SCLK, SDI carrying a write address byte with address bits A7 through A0, and SDO carrying a read data byte D7 through D0 after the address byte. The figure labels timing parameters tCC, tCDH, tDC, tCDD, and tCDZ. The figure note states that SCLK can be either polarity and timing is shown for CPOL = 1. [Source: MAX31865 RTD-to-Digital Converter, p. 5]

Figure 2 is titled `Timing Diagram: SPI Write Data Transfer`. It shows CS active low, SCLK, and SDI carrying a write address byte followed by a write data byte D7 through D0, while SDO is not shown carrying data. The figure labels timing parameters tCC, tCL, tCH, tR, tF, tCCH, tCWH, tCDH, and tDC. The figure note states that SCLK can be either polarity and timing is shown for CPOL = 1. [Source: MAX31865 RTD-to-Digital Converter, p. 5]

## Typical Operating Characteristics

The typical operating characteristics page states conditions of `VDD = VDVDD = 3.3 V`, `TA = +25°C`, unless otherwise noted. [Source: MAX31865 RTD-to-Digital Converter, p. 6]

| Figure title | Axes/ranges and visible labels | Source |
|---|---|---|
| Supply current vs. temperature, ADC auto conversion mode | Y-axis is IDD in mA from 0 to 4; x-axis is temperature in °C with visible range -50 to 150. Traces are labeled `ANALOG IDD (BIAS PIN UNLOADED)` and `DIGITAL IDD`. | [Source: MAX31865 RTD-to-Digital Converter, p. 6] |
| Supply current vs. temperature, ADC normally off mode | Y-axis is IDD in mA from 0 to 4; x-axis is temperature in °C with visible range -50 to 150. Traces are labeled `ANALOG IDD (BIAS PIN UNLOADED)` and `DIGITAL IDD`. | [Source: MAX31865 RTD-to-Digital Converter, p. 6] |
| Leakage current per pin vs. temperature | Subtitle states `1 VOLT APPLIED TO FORCE+, FORCE2, RTDIN+, RTDIN- PINS`. Y-axis is current in nA from 0 to 140; x-axis is temperature in °C with visible range 50 to 150. | [Source: MAX31865 RTD-to-Digital Converter, p. 6] |
| Sinc filter operation input frequency vs. noise response | Y-axis is noise response in dB from -100 to 20; x-axis is input noise frequency in Hz with visible range 10 to 170. Notches are labeled 50 Hz and 60 Hz. | [Source: MAX31865 RTD-to-Digital Converter, p. 6] |
| ADC conversion error vs. RTD resistance, 4 kΩ RREF, 4-wire connection | Y-axis is error in Ω from -0.488 to +0.488; x-axis is RRTD in Ω with visible range 0 to 3500. Traces are labeled -40°C, +25°C, and +100°C; a bracket labels approximately ±0.1°C error. | [Source: MAX31865 RTD-to-Digital Converter, p. 6] |
| ADC conversion error vs. RTD resistance, 400 Ω RREF, 4-wire connection | Y-axis is error in Ω from -0.488 to +0.488; x-axis is RRTD in Ω with visible range 0 to 350. Traces are labeled -40°C, +25°C, and +100°C; a bracket labels approximately ±0.1°C error. | [Source: MAX31865 RTD-to-Digital Converter, p. 6] |

The typical operating-characteristic plots are graphical; the PDF does not provide tabulated curve data points. [Source: MAX31865 RTD-to-Digital Converter, p. 6]
