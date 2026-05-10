# Pinout and Signals

## Package Pin Configurations

The datasheet provides top-view pin configurations for a 20-pin TQFN package marked `5 mm x 5 mm` and a 20-pin SSOP package. [Source: MAX31865 RTD-to-Digital Converter, p. 7]

The TQFN figure shows an exposed pad (`EP`) on the bottom side of the package; the pin-description table states that the exposed pad applies to the TQFN package only and must connect to GND1. [Source: MAX31865 RTD-to-Digital Converter, p. 7; MAX31865 RTD-to-Digital Converter, p. 8]

Pin-name ambiguity: the SSOP top-view pin-configuration figure labels pin 19 as `GND`, while the pin-description table identifies SSOP pin 19 as `GND1`. This extraction preserves the table name `GND1` for the pin-description table and records the figure/table naming difference as an ambiguity. [Conflict: MAX31865 RTD-to-Digital Converter, p. 7 vs MAX31865 RTD-to-Digital Converter, p. 8]

## Pin Description Table

All rows in this table are from the datasheet pin-description table. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

| TQFN pin | SSOP pin | Pin name | Documented function |
|---:|---:|---|---|
| 1 | 4 | BIAS | Bias voltage output (`VBIAS`). |
| 2 | 5 | REFIN+ | Positive reference voltage input. Connect to BIAS. Connect the reference resistor between REFIN+ and REFIN-. |
| 3 | 6 | REFIN- | Negative reference voltage input. Connect the reference resistor between REFIN+ and REFIN-. |
| 4 | 7 | ISENSOR | Low side of RREF. Connect to REFIN-. |
| 5 | 8 | FORCE+ | High-side RTD drive. Connect to FORCE2 when using the 3-wire connection configuration. Protected to +/-45 V. |
| 6 | 9 | FORCE2 | Positive input used in 3-wire only. In 3-wire configuration, connect to FORCE+. In 2-wire or 4-wire configuration, connect to ground. Protected to +/-45 V. |
| 7 | 10 | RTDIN+ | Positive RTD input. Protected to +/-45 V. |
| 8 | 11 | RTDIN- | Negative RTD input. Protected to +/-45 V. |
| 9 | 12 | FORCE- | Low-side RTD return. Protected to +/-45 V. |
| 10 | 13 | GND2 | Analog ground. Connect to GND1. |
| 11 | 14 | SDI | Serial-data input. |
| 12 | 15 | SCLK | Serial-data clock input. |
| 13 | 16 | CS | Active-low chip select. Set CS low to enable the serial interface. |
| 14 | 17 | SDO | Serial-data output. |
| 15 | 18 | DGND | Digital ground. |
| 16 | 19 | GND1 | Analog ground. Connect to GND2. |
| 17 | 20 | N.C. | Do not connect. |
| 18 | 1 | DRDY | Active-low, push-pull, data-ready output. DRDY goes low when a new conversion result is available in the data register. When a read operation of an RTD resistance data register occurs, DRDY returns high. |
| 19 | 2 | DVDD | Digital supply voltage input. Connect to a 3.3 V power supply. Bypass to DGND with a 0.1 uF bypass capacitor. |
| 20 | 3 | VDD | Analog supply voltage input. Connect to a 3.3 V power supply. Bypass to GND1 with a 0.1 uF bypass capacitor. |
| - | - | EP | Exposed pad on bottom side of package. Connect to GND1. Applies to TQFN package only. |

## Signal Groups

### Analog, RTD, and Reference Pins

The RTD-measurement path uses BIAS, REFIN+, REFIN-, ISENSOR, FORCE+, FORCE2, RTDIN+, RTDIN-, FORCE-, GND1, and GND2 in the application circuits. [Source: MAX31865 RTD-to-Digital Converter, p. 1; MAX31865 RTD-to-Digital Converter, p. 24]

The block diagram shows BIAS driven by a VBIAS generator and shows REFIN+, REFIN-, ISENSOR, FORCE+, FORCE2, RTDIN+, RTDIN-, and FORCE- feeding the protected RTD/ADC path. [Source: MAX31865 RTD-to-Digital Converter, p. 9]

FORCE+, FORCE2, FORCE-, RTDIN+, and RTDIN- are protected against input voltages up to +/-45 V. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

Signals applied to FORCE+, FORCE2, FORCE-, RTDIN+, and RTDIN- are gated by analog switches that open when the applied voltage is typically greater than VDD + 100 mV or less than GND1 - 400 mV. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

When a voltage fault occurs on protected inputs, the datasheet states that the protection circuits may allow approximately 350 uA of current flow and that this fault-induced leakage current does not damage the MAX31865. [Source: MAX31865 RTD-to-Digital Converter, p. 11]

### SPI Pins

The SPI-compatible interface uses SDO, SDI, CS, and SCLK. SDI and SDO are serial data input and output pins. CS initiates and terminates a data transfer. SCLK synchronizes data movement between the microcontroller and the MAX31865. [Source: MAX31865 RTD-to-Digital Converter, p. 16]

When CS is high, the function table lists the interface in `Disable Reset` mode, with SCLK input disabled, SDI input disabled, and SDO high impedance. [Source: MAX31865 RTD-to-Digital Converter, p. 17]

### DRDY

DRDY is active-low and push-pull according to the pin-description table. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

DRDY goes low when a new conversion result is available in the RTD Data Registers. When a read operation of the RTD Data Registers completes, DRDY returns high. [Source: MAX31865 RTD-to-Digital Converter, p. 19]

The AC timing table also specifies `Address 01h or 02h Decoded to DRDY High` (`tDRDYH`) as 50 ns after RTD register read access. [Source: MAX31865 RTD-to-Digital Converter, p. 4]

## Typical Application-Circuit Signal Connections

The datasheet and tutorial show 2-wire, 3-wire, and 4-wire MAX31865 application circuits. The figures include RREF, an RTD, RCABLE elements, host SPI interface signals, VDD/DVDD bypass capacitors, and an input capacitor CI across the RTD input nodes. [Sources: MAX31865 RTD-to-Digital Converter, p. 1; MAX31865 RTD-to-Digital Converter, p. 24; Temperature Sensor Tutorial, p. 7]

### Common Connections in the Shown Circuits

The application circuits show REFIN+ connected to BIAS and the upper side of RREF, while REFIN- and ISENSOR are tied to the lower side of RREF. [Sources: MAX31865 RTD-to-Digital Converter, p. 1; MAX31865 RTD-to-Digital Converter, p. 24; Temperature Sensor Tutorial, p. 7]

The application circuits show 0.1 uF bypass capacitors from VDD and DVDD supply nodes to ground. [Sources: MAX31865 RTD-to-Digital Converter, p. 1; MAX31865 RTD-to-Digital Converter, p. 24; Temperature Sensor Tutorial, p. 7]

The application circuits label CI as 10 nF for a 1 kOhm RTD and 100 nF for a 100 Ohm RTD. [Sources: MAX31865 RTD-to-Digital Converter, p. 1; MAX31865 RTD-to-Digital Converter, p. 24; Temperature Sensor Tutorial, p. 7]

### 4-Wire Sensor Connection

The 4-wire typical application circuit shows separate cable-resistance elements for FORCE+, RTDIN+, RTDIN-, and FORCE- paths to the RTD, with FORCE2 connected to ground. [Sources: MAX31865 RTD-to-Digital Converter, p. 1; Temperature Sensor Tutorial, p. 7]

In the 4-wire figure, FORCE+ drives one end of the RTD through a cable-resistance element, FORCE- returns from the other end through a separate cable-resistance element, RTDIN+ senses the high RTD node through a separate cable-resistance element, and RTDIN- senses the low RTD node through a separate cable-resistance element. [Source: MAX31865 RTD-to-Digital Converter, p. 1]

The datasheet text states that the 4-wire connection eliminates errors due to cable resistance by using separate force and sense leads. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

### 3-Wire Sensor Connection

The 3-wire typical application circuit is shown in the datasheet and tutorial with three cable-resistance elements and the MAX31865 host interface, RREF, RTD, and CI components. [Sources: MAX31865 RTD-to-Digital Converter, p. 24; Temperature Sensor Tutorial, p. 7]

In the 3-wire figure, FORCE+ and FORCE2 are tied together at the MAX31865 side and drive one RTD lead through a cable-resistance element; RTDIN+ senses the same end of the RTD through a second cable-resistance element; RTDIN- and FORCE- connect to the other RTD end through the third cable-resistance element. [Source: MAX31865 RTD-to-Digital Converter, p. 24]

The pin-description table states that FORCE+ connects to FORCE2 when using the 3-wire connection configuration, and FORCE2 is used in 3-wire only. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

The datasheet states that 3-wire operation is selected by setting the 3-wire bit in the Configuration register to 1. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

The 3-wire compensation method subtracts the voltage between FORCE+ and RTDIN+ from `(RTDIN+ - RTDIN-)` using the FORCE2 sampling input. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

### 2-Wire Sensor Connection

The 2-wire typical application circuit is shown in the datasheet and tutorial with the RTD connected through two leads, CI across RTDIN+ and RTDIN-, RREF connected between REFIN+ and REFIN-/ISENSOR, and the host SPI interface pins shown. [Sources: MAX31865 RTD-to-Digital Converter, p. 24; Temperature Sensor Tutorial, p. 7]

In the 2-wire figure, FORCE+ and RTDIN+ connect to one RTD terminal, while RTDIN- and FORCE- connect to the other RTD terminal; FORCE2 is connected to ground. [Source: MAX31865 RTD-to-Digital Converter, p. 24]

The pin-description table states that FORCE2 is connected to ground when using a 2-wire connection. [Source: MAX31865 RTD-to-Digital Converter, p. 8]

The datasheet text states that 2-wire operation can give acceptable results when the RTD is close to the MAX31865, but cable resistance can create excessive error as cable length increases. [Source: MAX31865 RTD-to-Digital Converter, p. 10]

## Documented Pin/Signal Gaps

No separate hardware reset pin is shown in the pinout or pin-description table. [Source: MAX31865 RTD-to-Digital Converter, p. 7; MAX31865 RTD-to-Digital Converter, p. 8]

No chip-address pins, strap pins, interrupt-mask pins, FIFO pins, external clock pins, or oscillator pins are shown in the pinout or pin-description table. [Source: MAX31865 RTD-to-Digital Converter, p. 7; MAX31865 RTD-to-Digital Converter, p. 8]
