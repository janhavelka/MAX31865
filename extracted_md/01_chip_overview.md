# MAX31865 Chip Overview

## Device Identity

The device covered by the primary datasheet is the `MAX31865 RTD-to-Digital Converter`. [Source: MAX31865 RTD-to-Digital Converter, p. 1]

The MAX31865 is described as a resistance-to-digital converter optimized for platinum resistance temperature detectors (RTDs). An external resistor sets sensitivity for the RTD being used, and a precision delta-sigma ADC converts the ratio of RTD resistance to reference resistance into digital form. [Source: MAX31865 RTD-to-Digital Converter, p. 1]

The device includes programmable detection of RTD and cable open and short conditions. Its RTD-related inputs are protected against overvoltage faults as large as ±45 V. [Source: MAX31865 RTD-to-Digital Converter, p. 1]

## Applications

The datasheet lists these applications: industrial equipment, medical equipment, and instrumentation. [Source: MAX31865 RTD-to-Digital Converter, p. 1]

## Feature Summary

| Feature area | Documented facts |
|---|---|
| RTD support | Handles 100 Ω to 1 kΩ platinum RTDs at 0°C, including PT100 to PT1000. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Sensor wiring | Compatible with 2-, 3-, and 4-wire sensor connections. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Interface | Uses an SPI-compatible interface. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Packages | Available in 20-pin TQFN and SSOP packages. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| ADC resolution | 15-bit ADC resolution. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Nominal temperature resolution | 0.03125°C nominal temperature resolution; the datasheet notes that this varies due to RTD nonlinearity. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Total accuracy | 0.5°C max total accuracy over all operating conditions, stated as 0.05% of full scale. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Reference inputs | Fully differential VREF inputs. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Conversion time | 21 ms max conversion time is listed in the feature summary; detailed conversion timing depends on filter and conversion mode and is extracted in `03_electrical_and_timing.md`. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Fault detection | Fault detection covers open RTD element, RTD shorted to out-of-range voltage, and short across RTD element. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |
| Input protection | ±45 V input protection is listed in the feature summary. [Source: MAX31865 RTD-to-Digital Converter, p. 1] |

## Functional Blocks

The datasheet describes the MAX31865 as having a built-in 15-bit ADC, input protection, a digital controller, an SPI-compatible interface, and associated control logic. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The block diagram shows these internal blocks or functions: VBIAS generator, 15-bit delta-sigma ADC, data registers, serial logic, 50/60 Hz digital sinc filter, master-initiated fault-detection cycle, digital comparator for fault detection, and ADC state machine. [Source: MAX31865 RTD-to-Digital Converter, p. 9]

The same block diagram shows ±45 V protection on FORCE+, FORCE2, RTDIN+, RTDIN-, and FORCE-. [Source: MAX31865 RTD-to-Digital Converter, p. 9]

The block diagram shows serial logic connected to SCLK, SDO, SDI, and CS; data registers connected between serial logic and the digital logic; the 15-bit delta-sigma ADC feeding the 50/60 Hz digital sinc filter; and DRDY driven from the digital logic. [Source: MAX31865 RTD-to-Digital Converter, p. 9]

The block diagram labels a `3-WIRE ONLY` path from FORCE2 into the ADC input selection network. [Source: MAX31865 RTD-to-Digital Converter, p. 9]

## Supported Sensor Types

The signal-conditioning circuitry is optimized to work with PT100 through PT1000 RTDs, and the datasheet states that thermistors are also supported. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

For other resistive sensors such as NTC or PTC thermistors, the datasheet states to select an RREF greater than or equal to the sensor's maximum resistance over the temperature range of interest; the output data is the ratio of sensor resistance to reference resistance. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

The tutorial states that the MAX31865 is a dedicated RTD signal-conditioning circuit that allows two-/three-/four-wire configurations with 15-bit resolution, accommodates PT100 and PT1000 RTDs, and features protection against overvoltage faults on input lines. [Source: Temperature Sensor Tutorial, p. 5]

## RTD Background Values and Equations

The datasheet states that platinum is the most common and most accurate RTD wire material, and that platinum RTDs are referred to as PT-RTDs. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The datasheet states that common PT-RTD nominal resistance values at 0°C are 100 Ω and 1 kΩ, although other values are available. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The datasheet lists platinum RTD characteristics as a wide temperature range to over +800°C, excellent accuracy and repeatability, and reasonable linearity. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The datasheet defines the average slope between 0°C and +100°C as alpha (α), and states that two widely used alpha values are 0.00385 and 0.00392, corresponding to the IEC 751 (PT100) and SAMA standards. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The datasheet provides the Callendar-Van Dusen equation for RTD resistance:

```text
R(T) = R0(1 + aT + bT^2 + c(T - 100)T^3)
```

where `T` is temperature in °C, `R(T)` is resistance at `T`, and `R0` is resistance at `T = 0°C`. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

IEC 751 coefficient values in the datasheet are: `a = 3.90830 x 10^-3`, `b = -5.77500 x 10^-7`, and `c = -4.18301 x 10^-12` for `-200°C <= T <= 0°C`, with `c = 0` for `0°C <= T <= +850°C`. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The tutorial gives the same Callendar-Van Dusen equation and the same IEC 751 coefficient values. [Source: Temperature Sensor Tutorial, p. 5]

The design solution states that the PT100 overtemperature resistance range is 18.52 Ω at -200°C to 390 Ω at +850°C. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 2]

The design solution states that, using the Callendar-Van Dusen equation, the maximum RTD end-point linearity error is approximately 4.34% whether using PT100 or PT1000. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 2]

The design solution states that the MAX31865 with an RTD provides 0.03125°C resolution across a -200°C to +850°C temperature range with 0.5°C accuracy; this statement is preserved as supplemental wording, and the range/IC-operating-range ambiguity is recorded in `08_variant_differences_and_open_questions.md`. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 3]

## RTD Measurement Principle

To measure RTD resistance, the datasheet shows RREF and the RTD connected in series, with bias voltage applied to the top of RREF. The reference-resistor current also flows through the RTD. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The voltage across RREF is the ADC reference voltage, and the voltage across the RTD is applied to differential ADC inputs RTDIN+ and RTDIN-. The ADC output is therefore equal to the ratio of RTD resistance to reference resistance. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The datasheet states that a reference resistor equal to four times the RTD 0°C resistance is optimum for a platinum RTD; it gives 400 Ω for PT100 and 4 kΩ for PT1000. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The design solution also states that the MAX31865 builds a voltage divider between the RTD and external precision RREF, does not need an exact current source, and makes RTD current a byproduct of the RRTD/RREF ratio. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 3]

The ADC has fully differential analog inputs RTDIN+ and RTDIN-, fully differential reference inputs REFIN+ and REFIN-, and an output code representing the ratio between analog input voltage and reference voltage. A negative input voltage produces output code 0; an input voltage greater than the reference voltage produces a full-scale output. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

The design solution describes the MAX31865 conversion engine as a 15-bit delta-sigma ADC and states that its signal-conditioning circuit is implemented primarily as a digital circuit. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 3]

## RTD Data to Resistance and Temperature

The datasheet gives the RTD resistance equation:

```text
RRTD = (ADC Code x RREF) / 2^15
```

where `ADC Code` is the 15-bit ADC result from RTD Data registers 01h-02h and `RREF` is the reference resistor resistance. [Source: MAX31865 RTD-to-Digital Converter, p. 19; MAX31865 RTD-to-Digital Converter, p. 20]

The design solution provides the same relationship as Equation 2. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 3]

After RTD resistance is known, the datasheet states that the selected RTD's well-defined resistive properties can be used to determine temperature by calculations or lookup tables. [Source: MAX31865 RTD-to-Digital Converter, p. 20]

For the range -100°C to +100°C, the datasheet gives this approximate temperature relationship:

```text
Temperature (°C) ~= (ADC code / 32) - 256
```

The same paragraph states that this approximation gives 0°C error at 0°C, -1.75°C error at -100°C, and -1.4°C error at +100°C, assuming an IEC 751 RTD and RREF equal to four times the 0°C RTD resistance. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

For high precision, the datasheet states to use the Callendar-Van Dusen equation or a lookup table to correct predictable RTD nonlinearity. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

## Wiring-Topology Behavior

A 2-wire connection can give acceptable results when the RTD is close to the MAX31865. For PT100, the datasheet states that 0.4 Ω series resistance causes about 1°C error, so cable-resistance error can become excessive as cable length increases. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The design solution states that reasonable RCABLE values can be as high as 50 Ω and that a 2-wire RTD with 10 Ω cable resistance creates approximately 25°C error across the entire temperature range. [Source: Achieve High-Accuracy Temperature Measurement in Your Precision Designs, p. 2]

The 4-wire connection eliminates cable-resistance errors by using separate force and sense leads. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The 3-wire connection uses one less conductor than a 4-wire connection. The datasheet states that the voltage between FORCE+ and RTDIN+ is subtracted from `(RTDIN+ - RTDIN-)` to compensate for the voltage drop across the return wire, using the FORCE2 sampling input. If cable resistances are well matched, the cable-resistance error is cancelled. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The 3-wire bit in the Configuration register selects 3-wire operation. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The tutorial describes a 3-wire RTD configuration as using a third wire that carries excitation current and can cancel wire resistance if wire resistances are equal; it describes a four-wire Kelvin configuration as the most accurate because separate force and sense leads eliminate wire-resistance effects. [Source: Temperature Sensor Tutorial, p. 5]

## Supplemental Reference Design Context

The MAXREFDES42 quick-start guide is for the MAXREFDES42# IO-Link RTD temperature sensor, not the bare MAX31865 chip. It states that users read and write desired indexes by referring to an IODD HTML index definition file for the MAXREFDES42 design. [Source: MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 5]

The MAXREFDES42 quick-start guide identifies index 261 as the MAX31865 RTD code register value variable, and states that this RTD code register value is shifted by 1 bit, so the value read back needs to be divided by two. [Source: MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 15; MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 16]

The same quick-start guide identifies index 268 as the ambient temperature in degrees C variable and shows reading it as a hex value converted to float32 in its example. [Source: MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 17; MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version), p. 18]
