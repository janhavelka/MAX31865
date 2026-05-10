# max31865 datasheet rev3 2015 07

- Source PDF: `docs/source-pdfs/datasheets/max31865_datasheet_rev3_2015-07.pdf`
- Extraction date: 2026-05-09
- Page count: 26
- SHA256: `88f540410eb8c2ac4c051594c92927fc8c5876446bec1c2448934fc2605395f7`

## Page 1

General Description
The MAX31865 is an easy-to-use resistance-to-digital
converter optimized for platinum resistance temperature
detectors (RTDs). An external resistor sets the sensitivity
for the RTD being used and a precision delta-sigma ADC
converts the ratio of the RTD resistance to the reference
resistance into digital form. The MAX31865’s inputs are
protected against overvoltage faults as large as Q45V.
Programmable detection of RTD and cable open and
short conditions is included.
Applications
 ● Industrial Equipment
 ● Medical Equipment
 ● Instrumentation
Benefits and Features
 ● Integration Lowers System Cost, Simplifies Design
Efforts, and Reduces Design Cycle Time
• Simple Conversion of Platinum RTD Resistance to
Digital Value
• Handles 100Ω to 1kΩ (at 0°C) Platinum RTDs
(PT100 to PT1000)
• Compatible with 2-, 3-, and 4-Wire Sensor Connections
• SPI-Compatible Interface
• 20-Pin TQFN and SSOP Packages
 ● High Accuracy Facilitates Meeting Error Budgets
• 15-Bit ADC Resolution; Nominal Temperature
Resolution 0.03125NC (Varies Due to RTD Nonlin-
earity)
• Total Accuracy Over All Operating Conditions:
0.5NC (0.05% of Full Scale) max
• Fully Differential VREF Inputs
• 21ms (max) Conversion Time
 ● Integrated Fault Detection Increases System
Reliability
• ±45V Input Protection
• Fault Detection (Open RTD Element, RTD Shorted to
Out-of-Range Voltage, or Short Across RTD Element)
Ordering Information appears at end of data sheet.
Typical Application Circuits continued at end of data sheet.
19-6478; Rev 3; 7/15
MAX31865
BIAS
REFIN+
DVDD
VDD
GND1
GND2
DGND
REFIN-DRDY
ISENSORSDI
SCLKHOST
INTERFACE
CS
SDO
N.C.
FORCE-
RREF
RTD
RCABLE
RCABLE
0.1µF
VDD
FORCE+
FORCE2
RTDIN+
RTDIN-
CI*
RCABLE
RCABLE
0.1µF
VDD
4-WIRE SENSOR CONNECTION
*CI = 10nF FOR 1kΩ RTD
 100nF FOR 100Ω RTD
Typical Application Circuits
MAX31865 RTD-to-Digital Converter

## Page 2

Voltage Range on VDD Relative to GND1 ............-0.3V to +4.0V
Voltage Range on BIAS, REFIN+,
REFIN-, ISENSOR .................................-0.3V to (VDD + 0.3V)
Voltage Range on FORCE+, FORCE2,
FORCE-, RTDIN+, RTDIN- Relative to GND1 ....-50V to +50V
Voltage Range on DVDD Relative to DGND  ........-0.3V to +4.0V
Voltage Range on All Digital Pins
Relative to DGND .............................-0.3V to (VDVDD + 0.3V)
Continuous Power Dissipation (TA = +70NC)
TQFN (derate 34.5mW/NC above +70NC)...............2758.6mW
 SSOP (derate 11.9mW/°C above +70° C)...............952.4mW
ESD Protection (all pins, Human Body Model)  ...................±2kV
Operating Temperature Range ........................ -40NC to +125NC
Junction Temperature .....................................................+150NC
Storage Temperature Range ............................-65NC to +150NC
Soldering Temperature (reflow) ......................................+260NC
Lead Temperature (soldering,10s) .................................+300NC
(Note 1)
TQFN
Junction-to-Ambient Thermal Resistance (qJA) ..........29°C/W
Junction-to-Case Thermal Resistance (qJC) .................2°C/W
SSOP
 Junction-to-Ambient Thermal Resistance (qJA) ..........84°C/W
 Junction-to-Case Thermal Resistance (qJC) ...............32°C/W
(TA = -40NC to +125NC, unless otherwise noted.) (Notes 2 and 3)
(3.0V P VDD P 3.6V, TA = -40NC to +125 NC, unless otherwise noted. Typical values are T A= +25NC, VDD = VDVDD = 3.3V.) (Notes 2
and 3)
PARAMETER SYMBOL CONDITIONS MIN TYP MAX UNITS
VDD VDD 3.0 3.3 3.6 V
DVDD VDVDD 3.0 3.3 3.6 V
Input Logic 0 VIL CS, SDI, SCLK -0.3 0.3 x
VDVDD
V
Input Logic 1 VIH CS, SDI, SCLK 0.7 x
VDVDD
VDVDD
+ 0.3 V
Analog Voltages
(FORCE+,FORCE2, FORCE-,
RTDIN+, RTDIN-)
Normal conversion results 0 VBIAS V
Reference Resistor RREF 350 10k I
Cable Resistance RCABLE Per lead 0 50 I
PARAMETER SYMBOL CONDITIONS MIN TYP MAX UNITS
ADC Resolution No missing codes 15 Bits
ADC Full-Scale Input Voltage
(RTDIN+ - RTDIN-)
REFIN+ -
REFIN- V
Stresses beyond those listed under “Absolute Maximum Ratings” may cause permanent damage to the device. These are stress ratings only, and functional operation
of the device at these or any other conditions beyond those indicated in the operational sections of the specifications is not implied. Exposure to absolute maximum
rating conditions for extended periods may affect device reliability.
Absolute Maximum Ratings
Package Thermal Characteristics
Note 1: Package thermal resistances were obtained using the method described in JEDEC specification JESD51-7, using a four-layer
board. For detailed information on package thermal considerations, refer to www.maximintegrated.com/thermal-tutorial.
Recommended DC Operating Conditions
Electrical Characteristics
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 2

## Page 3

(3.0V P VDD P 3.6V, TA = -40NC to +125 NC, unless otherwise noted. Typical values are T A= +25NC, VDD = VDVDD = 3.3V.) (Notes 2
and 3)
PARAMETER SYMBOL CONDITIONS MIN TYP MAX UNITS
ADC Common-Mode Input
Range 0 VBIAS V
Input Leakage Current
RTDIN+, RTDIN-, 0NC to +70NC, on-state 2
nARTDIN+, RTDIN-, -40NC to +85NC, on-state 5
RTDIN+, RTDIN-, -40NC to 100NC, on-state 14
Bias Voltage VBIAS 1.95 2.00 2.06 V
Bias Voltage Output Current IOUT 0.2 5.75 mA
Bias Voltage Load Regulation IOUT P 5.75mA 30 mV/mA
Bias Voltage Startup Time (Note 4) 10 ms
ADC Full-Scale Error ±1 LSB
ADC Integral Nonlinearity Differential Input, endpoint fit, 0.3 x V BIAS
P VREF P VBIAS
±1 LSB
ADC Offset Error -3 +3 LSB
Noise (over Nyquist Bandwidth) Input referred 150 FV RMS
Common-Mode Rejection 90 dB
50/60Hz Noise Rejection Fundamental and harmonics 82 dB
Temperature Conversion Time
(Note 5) tCONV
Continuous conversion (60Hz notch) 16.7 17.6
ms
Single conversion (60Hz notch) 52 55
Single conversion (50Hz notch) 62.5 66
Continuous conversion (50Hz notch) 20 21
Automatic Fault Detection Cycle
Time From CS high to cycle complete 550 600 Fs
Power-Supply Rejection 1 LSB/V
Power-Supply Current (Note 6)
IDD
Shutdown Bias off, ADC off 1.5 3 mA
IDD Bias on, active conversion 2 3.5 mA
Power-On Reset Voltage
Threshold 2 2.27 V
Power-On Reset Voltage
Hysteresis 120 mV
Input Capacitance CIN Logic inputs 6 pF
Input Leakage Current IL Logic inputs -1 +1 FA
Output High Voltage VOH IOUT = -1.6mA VDVDD
- 0.4 V
Output Low Voltage VOL IOUT = 1.6mA 0.4 V
Electrical Characteristics (continued)
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 3

## Page 4

Note 2: All voltages are referenced to ground when common. Currents entering the IC are specified positive.
Note 3: Limits are 100% production tested at T A= +25°C and/or TA= +85°C. Limits over the operating temperature range and
relevant supply voltage range are guaranteed by design and characterization. Typical values are not guaranteed.
Note 4: For 15-bit settling, a wait of at least 10.5 time constants of the input RC network is required. Max startup time is calculated
with a 10kω reference resistor and a 0.1µF capacitor across the RTD inputs.
Note 5: The first conversion after enabling continuous conversion mode takes a time equal to the single conversion time for the
respective notch frequency.
Note 6: Specified with no load on the bias pin as the sum of analog and digital currents. No active communication. If the RTD
input voltage is greater than the input reference voltage, then an additional 400µA I DD can be expected.
Note 7: All timing specifications are guaranteed by design.
Note 8: Measured at VIH = 0.7V x VDVDD or VIL = 0.3 x VDVDD and 10ms maximum rise and fall times.
Note 9: Measured with 50pF load.
Note 10: Measured at VOH = 0.7 x VDVDD or VOL = 0.3 x VDVDD. Measured from the 50% point of SCLK to the VOH minimum of SDO.
(3.0V P VDD P 3.6V, TA = -40NC to +125 NC, unless otherwise noted. Typical values are T A= +25NC, VDD = VDVDD = 3.3V.) (Notes 3
and 7) (Figure 1 and Figure 2)
PARAMETER SYMBOL CONDITIONS MIN TYP MAX UNITS
Data to SCLK Setup tDC (Notes 8, 9) 35 ns
SCLK to Data Hold tCDH (Notes 8, 9) 35 ns
SCLK to Data Valid tCDD (Notes 8, 9, 10) 80 ns
SCLK Low Time tCL (Note 9) 100 ns
SCLK High Time tCH (Note 9) 100 ns
SCLK Frequency tCLK (Note 9) DC 5.0 MHz
SCLK Rise and Fall tR, tF (Note 9) 200 ns
CS to SCLK Setup tCC (Note 9) 400 ns
SCLK to CS Hold tCCH (Note 9) 100 ns
CS Inactive Time tCWH (Note 9) 400 ns
CS to Output High-Z tCDZ (Notes 8, 9) 40 ns
Address 01h or 02h Decoded to
DRDY High tDRDYH After RTD register read access (Note 9) 50 ns
AC Electrical Characteristics: SPI Interface
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 4

## Page 5

Figure 1. Timing Diagram: SPI Read Data Transfer
Figure 2. Timing Diagram: SPI Write Data Transfer
CS
SCLK
SDI
SDO
tCC
tCDH
A7 A6 A0
D7 D6 D1 D0
tDC
tCDD tCDD
tCDZ
NOTE: SCLK CAN BE EITHER POLARITY, TIMING SHOWN FOR CPOL = 1.
WRITE ADDRESS BYTE READ DATA BYTE
SCLK
SDI
NOTE: SCLK CAN BE EITHER POLARITY, TIMING SHOWN FOR CPOL = 1.
WRITE ADDRESS BYTE WRITE DATA BYTE
tCC
tCDH
tCH
tR
tF tCCH
tCWH
tCDH
A7 A6 A0 D7 D0
tDC
tCL
CS
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 5

## Page 6

(VDD = VDVDD = 3.3V, TA = +25°C, unless otherwise noted.)
ADC CONVERSION ERROR vs. RTD RESISTANCE
(400Ω RREF, 4-WIRE CONNECTION)
MAX31865 toc06
RRTD (Ω)
ERROR (Ω)
30025020015010050
-0.244
0
0.244
˜ ±0.1°C
ERROR
0.488
-0.488
0 350
+25°C
-40°C
+100°C
ADC CONVERSION ERROR vs. RTD RESISTANCE
(4kΩ RREF, 4-WIRE CONNECTION)
MAX31865 toc05
RRTD (Ω)
ERROR (Ω)
30002500200015001000500
-0.244
0
0.244
˜ ±0.1°C
ERROR
0.488
-0.488
0 3500
+25°C
-40°C
+100°C
SINC FILTER OPERATION
INPUT FREQUENCY vs. NOISE RESPONSE
MAX31865 toc04
INPUT NOISE FREQUENCY (Hz)
NOISE RESPONSE (dB)
1309050
-80
-60
-40
-20
0
20
-100
10 170
60Hz50Hz
LEAKAGE CURRENT PER PIN vs. TEMPERATURE
 (1 VOLT APPLIED TO FORCE+, FORCE2, RTDIN+, RTDIN- PINS)
MAX31865 toc03
TEMPERATURE (°C)
CURRENT (nA)
12510075
20
40
60
80
100
120
140
0
50 150
SUPPLY CURRENT vs. TEMPERATURE
(ADC NORMALLY OFF MODE)
MAX31865 toc02
TEMPERATURE (°C)
IDD (mA)
100500
1
2
3
4
0
-50 150
ANALOG IDD
(BIAS PIN UNLOADED)
DIGITAL IDD
SUPPLY CURRENT vs. TEMPERATURE
(ADC AUTO CONVERSION MODE)
MAX31865 toc01
TEMPERATURE (°C)
IDD (mA)
100500
1
2
3
4
0
-50 150
ANALOG IDD
(BIAS PIN UNLOADED)
DIGITAL IDD
Typical Operating Characteristics
MAX31865 RTD-to-Digital Converter
Maxim Integrated  │ 6
www.maximintegrated.com

## Page 7

MAX31865
TQFN
(5mm x 5mm)
TOP VIEW
19
20 EP+
18
17
7
6
8
REFIN+
ISENSOR
FORCE+
9
BIAS
SDO
SCLK
SDI
DGND
1 2
DRDY
4 5
15 14 12 11
DVDD
VDD
FORCE-
RTDIN-
RTDIN+
FORCE2
REFIN- CS
3
13
N.C.
16 10 GND2GND1
20
19
18
17
16
15
13
1
2
3
4
5
6
8
N.C.
GND
DGND
SDOBIAS
V
DD
DVDD
DRDY
TOP VIEW
MAX31865
CS
SCLK
GND2FORCE+
REFIN-
147 SDIISENSOR
1110 RTDIN-RTDIN+
129 FORCE-FORCE2
REFIN+
SSOP
+
Pin Configurations
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 7

## Page 8

PIN
NAME FUNCTION
TQFN SSOP
1 4 BIAS Bias Voltage Output (VBIAS)
2 5 REFIN+ Positive Reference Voltage Input. Connect to BIAS. Connect the reference resistor between
REFIN+ and REFIN-.
3 6 REFIN- Negative Reference Voltage Input. Connect the reference resistor between REFIN+ and REFIN-.
4 7 ISENSOR Low Side of RREF. Connect to REFIN-.
5 8 FORCE+ High-Side RTD Drive. Connect to FORCE2 when using the 3-wire connection configuration.
Protected to ±45V.
6 9 FORCE2
Positive Input Used in 3-Wire Only. When in the 3-wire connection configuration, connect to
FORCE+. When in the 2-wire or 4-wire connection configuration, connect to ground. Protected to
±45V.
7 10 RTDIN+ Positive RTD Input. Protected to ±45V.
8 11 RTDIN- Negative RTD Input. Protected to ±45V.
9 12 FORCE- Low-Side RTD Return. Protected to ±45V.
10 13 GND2 Analog Ground. Connect to GND1.
11 14 SDI Serial-Data Input
12 15 SCLK Serial-Data Clock Input
13 16 CS Active-Low Chip Select. Set CS low to enable the serial interface.
14 17 SDO Serial-Data Output
15 18 DGND Digital Ground
16 19 GND1 Analog Ground. Connect to GND2.
17 20 N.C. Do Not Connect
18 1 DRDY
Active-Low, Push-Pull, Data-Ready Output. DRDY goes low when a new conversion result is
available in the data register. When a read operation of an RTD resistance data register occurs,
DRDY returns high.
19 2 DVDD Digital Supply Voltage Input. Connect to a 3.3V power supply. Bypass to DGND with a 0.1µF
bypass capacitor.
20 3 VDD
Analog Supply Voltage Input. Connect to a 3.3V power supply. Bypass to GND1 with a 0.1µF
bypass capacitor.
— — EP Exposed Pad (Bottom Side of Package). Connect to GND1. Applies to TQFN package only.
Pin Description
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 8

## Page 9

DATA REGISTERS
VBIAS
GENERATOR
DIGITAL LOGIC
SERIAL
LOGIC
BIAS
VDD
VDD VDVDD
REFIN+
REFIN-
ISENSOR
FORCE+
FORCE2
RTDIN+
FORCE-
RTDIN-
DVDD
SCLK
SDO
SDI
CS
DRDY
DGND
3-WIRE
    ONLY
50/60Hz DIGITAL
SINC FILTER
ADC STATE
MACHINE
DIGITAL
COMPARATOR
FOR
FAULT DETECTION
MASTER-INITIATED
FAULT-DETECTION
CYCLE
15-BIT
Σ∆ ADC
MAX31865
GND1
±45V PROTECTION
GND2
Block Diagram
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 9

## Page 10

Detailed Description
The MAX31865 is a sophisticated RTD-to-digital converter
with a built-in 15-bit analog-to-digital converter (ADC),
input protection, a digital controller, an SPI-compatible
interface, and associated control logic. The signal
conditioning circuitry is optimized to work with PT100
through PT1000 RTDs. Thermistors are also supported.
Temperature Conversion
Resistance temperature detectors (RTDs) are sensors
whose resistance varies with temperature. Platinum
is the most common, most accurate wire material;
platinum RTDs are referred to as PT-RTDs. Nickel,
copper, and other metals may also be used to make
RTDs. Characteristics of platinum RTDs include a wide
temperature range (to over +800 NC), excellent accuracy
and repeatability, and reasonable linearity.
For PT-RTDs, the most common values for nominal
resistance at 0 NC are 100 I and 1k I, though other
values are available. The average slope between 0 NC
and +100 NC is called alpha ( α). This value depends on
the impurities and their concentrations in the platinum.
The two most widely used values for alpha are 0.00385
and 0.00392, corresponding to the IEC 751 (PT100) and
SAMA standards.
The resistance vs. temperature curve is reasonably linear,
but has some curvature, as described by the Callendar-
Van Dusen equation:
R(T) = R0(1 + aT + bT2 + c(T - 100)T3)
where:
T = temperature (NC)
R(T) = resistance at T
R0 = resistance at T = 0 NC
IEC 751 specifies α = 0.00385055 and the following
Callendar-Van Dusen coefficient values:
a = 3.90830 x 10-3
b = -5.77500 x 10-7
c = -4.18301 x 10 -12 for -200 NC P T P 0NC, 0 for 0 NC P T
P +850NC
Figure 3 shows the curve of resistance vs. temperature
for a PT100 RTD along with a straight-line approximation
based on the slope between 0 NC and +100NC.
To measure the RTD’s resistance, connect a reference
resistor (R REF) and RTD in series and apply the bias
voltage to the top of R REF as shown in the Typical
Application Circuits . The reference resistor current also
flows through the RTD. The voltage across the reference
resistor is the reference voltage for the ADC. The voltage
across the RTD is applied to the ADC’s differential inputs
(RTDIN+ and RTDIN-). The ADC therefore produces
a digital output that is equal to the ratio of the RTD
resistance to the reference resistance. A reference
resistor equal to four times the RTD’s 0 NC resistance is
optimum for a platinum RTD. Therefore, a PT100 uses
a 400 I reference resistor, and a PT1000 uses a 4k I
reference resistor.
A 2-wire connection (see the Typical Application Circuits)
can give acceptable results when the RTD is located
close to the MAX31865. Note that, for a PT100, series
resistance of 0.4I causes an error of approximately 1 NC.
Therefore, as the cable length increases, the error due to
cable resistance can become excessive.
The 4-wire connection eliminates errors due to cable
resistance by using separate force and sense leads.
A 3-wire connection is a compromise approach that
uses one less conductor than the 4-wire approach. To
compensate for the voltage drop across the return wire,
the voltage between FORCE+ and RTDIN+ is subtracted
from (RTDIN+ - RTDIN-). This is accomplished using the
FORCE2 sampling input. If the cable resistances are well-
matched, the error due to cable resistance is cancelled.
Select 3-wire operation by setting the 3-wire bit in the
Configuration register to 1.
Figure 3. PT100 RTD resistance vs. temperature.
PT100 RTD RESISTANCE
vs. TEMPERATURE
TEMPERATURE (°C)
RESISTANCE (Ω)
700600400 5000 100 200 300-100
50
100
150
200
250
300
350
400
450
0
-200
STRAIGHT-LINE
APPROXIMATION
RTD RESISTANCE
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 10

## Page 11

Linearizing Temperature Data
For a temperature range of -100 NC to +100 NC, a good
approximation of temperature can be made by simply
using the RTD data as shown below:
Temperature (NC) ≈ (ADC code/32) – 256
This equation gives 0 NC error at 0 NC, -1.75 NC error
at -100 NC, and -1.4 NC error at +100 NC (assuming an
IEC751 RTD and R REF equal to four times the 0 NC
RTD resistance). For high precision, use the Callendar-
Van Dusen equation (in the Temperature Conversion
section) or a lookup table to correct the RTD’s predictable
nonlinearity.
Using Thermistors
Other resistive sensors, such as thermistors (NTCs or
PTCs) may be used. Select an R REF that is greater than
or equal to the sensor’s maximum resistance over the
temperature range of interest. The output data is the ratio
of the sensor resistance to the reference resistance.
Analog-to-Digital Converter (ADC)
The ADC has fully differential analog inputs, RTDIN+
and RTDIN-, and fully differential reference inputs,
REFIN+ and REFIN-. The output code represents the
ratio between the analog input voltage and the reference
voltage. A negative input voltage produces an output code
of 0. An input voltage greater than the reference voltage
produces a full-scale output.
Input noise is attenuated by a third-order digital “sinc”
filter. Noise from 50Hz or 60Hz power sources (including
harmonics of the ac power’s fundamental frequency) is
attenuated by 82dB.
Fault Detection and Input Protection
The MAX31865 detects a variety of faults that can occur
with the external RTD and 2-, 3-, or 4-wire cables. Some
faults are detected on every conversion, while others are
detected only when a fault detection cycle is requested by
the master. During a fault detection cycle the MAX31865
has the ability to disconnect the FORCE- input from its
GND2 return path by means of and internal analog switch.
The conditions that generate a fault are listed below, see
Figure 4 for a fault detection flowchart.
• Detected at any point in time
 Overvoltage (> V DD) or undervoltage (< GND1) con -
dition on FORCE+, FORCE2, RTDIN+, RTDIN-, or
FORCE- pins
• Detected every ADC conversion
 Greater than or equal to threshold high conversion result
 Less than or equal to threshold low conversion result
• Detected on demand by initiating a Fault Detection
Cycle (Configuration Register bits (D[3:2])
 V REFIN- > 0.85 x VBIAS
 V REFIN- < 0.85 x VBIAS when FORCE- input switch is
open
 V RTDIN- < 0.85 x VBIAS when FORCE- input switch is
open
FORCE+, FORCE2, FORCE-, RTDIN+, and RTDIN- are
protected against input voltages up to Q45V. Signals
applied to these pins are gated by analog switches that
open when the applied voltage is typically greater than
VDD + 100mV or less than GND1 - 400mV. Note that
when a voltage fault occurs, the protection circuits may
allow approximately 350 FA of current flow. This fault-
induced leakage current does not cause any damage to
the MAX31865.
When an overvoltage or undervoltage condition is
detected, bit D2 of the Fault Status register is set and the
ADC halts conversion updates until the fault is no longer
detected, at which point conversions resume.
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 11

## Page 12

Figure 4. Fault Detection Flowcharts

MASTER-INITIATED FAULT-DETECTION CYCLE - AUTOMATIC MODE
MASTER-INITIATED FAULT-DETECTION CYCLE - MANUAL MODE
N
IS FORCE+,
FORCE2, FORCE-,
RTDIN+, RTDIN-,
PINS > VDD OR <
GND

PROTECT PINS AGAINST
±45V
Y
SET BIT D2 OF FAULT
STATUS REGISTER
ADC HALTS UPDATES
SET BIT D0 OF RTD DATA
LSB REGISTER
EVERY CONVERSION FAULT DETECTIONMONITOR PINS
ALWAYS ACTIVE FAULT DETECTION
FAULT DETECTION
MASTER WRITES
100X010Xb TO
CONFIGURATION
REGISTER
NNIS VREFIN-
>
0.85 x VBIAS
100µs
DELAY
FORCE-INPUT
SWITCH
REMAINS
CLOSED
OPEN
FORCE-
INPUT
SWITCH

FORCE-INPUT
SWITCH
CLOSED
CONFIGURATION
REGISTER SET TO
100X000Xb TO
END FAULT
DETECTION
CYCLE
IS VREFIN-
<
0.85 x VBIAS
IS RTDIN-
<
0.85 x VBIAS
N
SET BIT D5 OF FAULT
STATUS REGISTER
SET BIT D0 OF RTD DATA
LSB REGISTER
100µs
DELAY
100µs
DELAY
210µs
DELAY
Y
SET BIT D4 OF FAULT
STATUS REGISTER
SET BIT D0 OF RTD DATA
LSB REGISTER
Y
SET BIT D3 OF FAULT
STATUS REGISTER
SET BIT D0 OF RTD DATA
LSB REGISTER
Y
Y
100µs
DELAY
OPEN
FORCE-
INPUT
SWITCH
MASTER WRITES
100X100Xb TO
CONFIGURATION
REGISTER
NIS VREFIN-
>
0.85 x VBIAS
100µs
DELAY
FORCE-INPUT
SWITCH
REMAINS
CLOSED
DID
MASTER WRITE
100X110Xb TO
CONFIGURATION
REGISTER
100µs
DELAY
100µs
DELAY
Y
SET BIT D5 OF FAULT
STATUS REGISTER
SET BIT D0 OF RTD DATA
LSB REGISTER
NIS VREFIN-
<
0.85 x VBIAS
SET BIT D4 OF FAULT
STATUS REGISTER
SET BIT D0 OF RTD DATA
LSB REGISTER
Y
N
FORCE-INPUT
SWITCH
CLOSED
CONFIGURATION
REGISTER SET TO
100X000Xb TO
END FAULT
DETECTION
CYCLE
IS RTDIN-
<
0.85 x VBIAS
N
SET BIT D3 OF FAULT
STATUS REGISTER
SET BIT D0 OF RTD DATA
LSB REGISTER
Y
PERFORM
CONVERSION
NN
IS
RTD RESISTANCE
VALUE > HIGH
THRESHOLD
REGISTER
Y

SET BIT D7 OF FAULT
STATUS REGISTER
IS
RTD RESISTANCE
VALUE < LOW
THRESHOLD
REGISTER
CONVERSION
INITIATED
Y
SET BIT D6 OF FAULT
STATUS REGISTER
SET BIT D0 OF RTD DATA
LSB REGISTER
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 12

## Page 13

Internal Registers
Communication is through eight 8-bit registers that
contain conversion, status, and configuration data.
All programming is done by selecting the appropriate
address of the desired register location. Table 1 illustrates
the addresses for the registers.
The registers are accessed using the 0Xh addresses for
reads and the 8Xh addresses for writes. Data is read from
or written to the registers MSB first.
Configuration Register (00h)
The configuration register selects the conversion mode
(automatic or triggered by the 1-shot command), enables
and disables BIAS pin output voltage V BIAS, initiates
1-shot conversions, selects the RTD connection (either
3-wire or 2-wire/4-wire), initiates a full fault detection
cycle, clears the Fault Status register, and selects the
filter notch frequencies. The effects of the configuration
bits are described below.
BIAS (D7)
When no conversions are being performed, V BIAS may
be disabled to reduce power dissipation. Write 1 to this
bit to enable V BIAS before beginning a single (1-Shot)
conversion. When automatic (continuous) conversion
mode is selected, VBIAS remains on continuously.
Conversion Mode (D6)
Write 1 to this bit to select automatic conversion mode, in
which conversions occur continuously at a 50/60Hz rate.
Write 0 to this bit to exit automatic conversion mode and
enter the “Normally Off” mode. 1-shot conversions may be
initiated from this mode.
1-Shot (D5)
When the conversion mode is set to “Normally Off”, write
1 to this bit to start a conversion. This causes a single
resistance conversion to take place. The conversion
is triggered when CS goes high after writing a 1 to
this bit. Note that if a multibyte write is performed, the
conversion is triggered when CS goes high at the end
of the transaction. If V BIAS is on (as selected by the
Configuration Register), the RTD voltage is sampled
when CS goes high and the conversion begins. Note
that if V BIAS is off (to reduce supply current between
conversions), any filter capacitors at the RTDIN inputs
need to charge before an accurate conversion can be
performed. Therefore, enable VBIAS and wait at least 10.5
time constants of the input RC network plus an additional
1ms before initiating the conversion. Note that a single
conversion requires approximately 52ms in 60Hz filter
mode or 62.5ms in 50Hz filter mode to complete. 1-Shot
is a self-clearing bit.
Table 1. Register Addresses and POR State
Table 2. Configuration Register Definition
REGISTER NAME READ ADDRESS (HEX) WRITE ADDRESS (HEX) POR STATE READ/WRITE
Configuration 00h 80h 00h R/W
RTD MSBs 01h — 00h R
RTD LSBs 02h — 00h R
High Fault Threshold MSB 03h 83h FFh R/W
High Fault Threshold LSB 04h 84h FFh R/W
Low Fault Threshold MSB 05h 85h 00h R/W
Low Fault Threshold LSB 06h 86h 00h R/W
Fault Status 07h — 00h R
D7 D6 D5 D4 D3 D2 D1 D0
VBIAS
1 = ON
0 = OFF
Conversion
mode
1 = Auto
0 = Normally off
1-shot
1 = 1-shot
(auto-clear)
3-wire
1 = 3-wire RTD
0 = 2-wire or
4-wire
Fault Detection
Cycle Control
(see Table 3)
Fault Status
Clear
1 = Clear
(auto-clear)
50/60Hz filter
select
1 = 50Hz
0 = 60Hz
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 13

## Page 14

3-Wire (D4)
Write 1 to this bit when using a 3-wire RTD connection. In
this mode the voltage between FORCE+ and RTDIN+ is
subtracted from (RTDIN+ - RTDIN-) to compensate for the
IR errors caused by using a single wire for the FORCE-
and RTDIN- connections. When using 2-wire or 4-wire
connections, write 0 to this bit.
Fault Detection Cycle (D3:D2)
The master initiated fault detection cycle has two modes
of operation, manual and automatic mode timing. If the
external RTD interface circuitry includes an input filter with
a time constant greater than 100 Fs, the fault detection
cycle timing should be controlled in the manual mode
operation. The fault detection cycle checks for three faults
by making the following voltage comparisons and setting
the associated bits in the Fault Status Register:
1) Is the voltage at REFIN- greater than 85% x V BIAS?
(Fault Status Register bit D5)
2) Is the voltage at REFIN- less than 85% x V BIAS when
FORCE- input switch is open? (Fault Status Register
bit D4)
3) Is the voltage at RTDIN- less than 85% x V BIAS when
FORCE- input switch is open? (Fault Status Register
bit D3)
Note: All voltages are referenced to GND1.
The Applications Information provides tables for decoding
possible causes of set fault status bits.
To enter the automatic fault detection cycle, write
100X010Xb to the Configuration register. The ADC is
now in “Normally Off” mode. The automatic fault detection
cycle inserts 100 Fs delays before checking for faults,
thereby allowing the external input filter to settle. The
Fault Detect Cycle bits (D[3:2]) self-clear to 00b upon
completion.
To enter the manual fault detection cycle, first ensure that
VBIAS has been on for at least 5 time constants. Next,
write 100X100Xb to the Configuration register. The ADC
is now in “Normally Off” mode. The MAX31865 checks for
faults while the FORCE- input switch is closed, and when
the check completes, the FORCE-input switch opens. The
Fault Detect Cycle bits (D[3:2]), remain set to 10b. Again,
wait at least 5 time constants, and then write 100X110Xb
to the Configuration register. The MAX31865 now checks
for faults while the FORCE- inputs switch is open; when
the check completes, the FORCE- input switch closes
and the Fault Detect Cycle bits (D[3:2])  self-clear to 00b.
Note that if 1 is written to D5 (1-Shot) and D2 or D3 in a
single write, both commands are ignored. If 100X110Xb
is set without a prior initiation of the first manual step
(setting 100X100Xb), the automatic fault detection mode
is run instead.
Fault Status Clear (D1)
Write a 1 to this bit while writing 0 to bits D5, D3, and D2
to return all fault status bits (D[7:2]) in the Fault Status
Register to 0. Note that bit D2 in the Fault Register, and
subsequently bit D0 in the RTD LSB register may be set
again immediately after resetting if an over/undervoltage
fault persists. The fault status clear bit D1, self-clears to 0.
50/60Hz (D0)
This bit selects the notch frequencies for the noise
rejection filter. Write 0 to this bit to reject 60Hz and
its harmonics; write 1 to this bit to reject 50Hz and its
harmonics. Note: Do not change the notch frequency
while in auto conversion mode.
X = Don’t care
Table 3. Fault-Detection Cycle Control Bits
D3 D2 CONFIGURATION REGISTER
WRITE (BINARY) WRITE ACTION READ MEANING
0 0 XXXX00XXb No action Fault detection finished
0 1 100X010Xb Fault detection with automatic delay Automatic fault detection still running
1 0 100X100Xb Run fault detection with manual delay
(cycle 1)
Manual cycle 1 still running; waiting for
user to write 11
1 1 100X110Xb Finish fault detection with manual delay
(cycle 2) Manual cycle 2 still running
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 14

## Page 15

RTD Resistance Registers (01h−02h)
Two 8-bit registers, RTD MSBs and RTD LSBs, contain
the RTD resistance data. The data format is shown in
Table 4. The data format is simply the 15-bit ratio of RTD
resistance to reference resistance. D0 of the RTD LSBs
register is a Fault bit that indicates whether any RTD
faults have been detected.
Table 4. RTD Resistance Registers Definition
Table 5. RTD Resistance-Data Relationship
Note: D0 (“Fault”) is assumed to be 0.
REGISTER RTD MSBS (01h) REGISTER RTD LSBS (02h) REGISTER
Bit D7 D6 D5 D4 D3 D2 D1 D0 D7 D6 D5 D4 D3 D2 D1 D0
RTD
Resistance
Data
MSB — — — — — — — — — — — — — LSB Fault
Bit
Weighting 214 213 212 211 210 29 28 27 26 25 24 23 22 21 20 —
Decimal
Value 16384 8192 4096 2048 1024 512 256 128 64 32 16 8 4 2 1 —
RRTD/RREF
BINARY HEX DECIMALRTD MSBs (01h) RTD LSBs (02h) RTD MSBs (01h) RTD LSBs (02h)
0.025 0000 0110 0110 0110b 06h 66h 819
0.125 0010 0000 0000 0000b 20h 00h 4096
0.25 0100 0000 0000 0000b 40h 00h 8192
0.50 1000 0000 0000 0000b 80h 00h 16,384
0.75 1100 0000 0000 0000b C0h 00h 24,576
0.999 1111 1111 1111 1110b FFh FEh 32,767
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 15

## Page 16

Fault Threshold Registers (03h–06h)
The High Fault Threshold and Low Fault Threshold
registers select the trip thresholds for RTD fault detection.
The results of RTD conversions are compared with the
values in these registers to generate the “Fault” (D[7:6])
bits in the Fault Status register. The RTD Data Registers,
High Fault Threshold Registers, and Low Fault Threshold
Registers all have the same format.
The RTD High bit in the Fault Status Register is set if the
RTD resistance register value is greater than or equal to
the value in the High Fault Threshold register. The POR
value of the High Fault Threshold register is FFFFh.
The RTD Low bit in the Fault Status Register is set if the
RTD resistance value is less than or equal to the value in
the Low Fault Threshold register. The POR value of the
Low Fault Threshold register is 0000h.
Fault Status Register (07h)
The Fault Status register latches any detected fault bits;
writing a 1 to the Fault Status Clear bit in the Configuration
Register returns all fault status bits to 0.
Serial Interface
The MAX31865 supports SPI modes 1 and 3. Four
pins are used for SPI-compatible communications: SDO
(serial-data out), SDI (serial-data in), CS (chip select),
and SCLK (serial clock). SDI and SDO are the serial-
data input and output pins for the devices, respectively.
The CS input initiates and terminates a data transfer.
SCLK synchronizes data movement between the master
(microcontroller) and the slave (MAX31865).
The serial clock (SCLK), which is generated by the
microcontroller, is active only when CS is low and dur-
ing address and data transfer to any device on the SPI
Table 7. Fault Status Register Definition
Table 6. Fault Threshold Registers Definition
X = Don’t care
X = Don’t care
REGISTER HIGH FAULT THRESHOLD MSB (03h) REGISTER HIGH FAULT THRESHOLD LSB (04h) REGISTER
LOW FAULT THRESHOLD MSB (05h) REGISTER LOW FAULT THRESHOLD LSB (06h) REGISTER
Bit D7 D6 D5 D4 D3 D2 D1 D0 D7 D6 D5 D4 D3 D2 D1 D0
RTD
Resistance
Data
MSB — — — — — — — — — — — — — LSB X
Bit
Weighting 214 213 212 211 210 29 28 27 26 25 24 23 22 21 20 —
Decimal
Value 16384 8192 4096 2048 1024 512 256 128 64 32 16 8 4 2 1 —
D7 D6 D5 D4 D3 D2 D1 D0
RTD High
Threshold
RTD Low
Threshold
REFIN- >
0.85 x VBIAS
REFIN- < 0.85 x VBIAS
(FORCE- open)
RTDIN- < 0.85 x
VBIAS (FORCE- open)
Overvoltage/
undervoltage fault x x
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 16

## Page 17

bus. The inactive clock polarity is programmable in
some microcontrollers. The MAX31865 automatically
accommodates either clock polarity by sampling SCLK
when CS becomes active to determine the polarity of the
inactive clock. Input data (SDI) is latched on the internal
strobe edge and output data (SDO) is shifted out on
the shift edge (see Table 8 and Figure 5). There is one
clock for each bit transferred. Address and data bits are
transferred in groups of eight, MSB first.
Address and Data Bytes
Address and data bytes are shifted MSB-first into the
serial-data input (SDI) and out of the serial-data output
(SDO). Any transfer requires the address of the byte to
specify a write or a read, followed by one or more bytes of
data. Data is transferred out of the SDO for a read opera-
tion and into the SDI for a write operation. The address
byte is always the first byte transferred after CS is driven
low. The MSB (A7) of this byte determines whether the
following byte is written or read. If A7 is 0, one or more
byte reads follow the address byte. If A7 is 1, one or more
byte writes follow the address byte.
For a single-byte transfer, 1 byte is read or written and
then CS is driven high (see Figure 6 and Figure 7). For
a multiple-byte transfer, multiple bytes can be read or
written after the address has been written (see Figure 8).
The address continues to increment through all memory
locations as long as CS remains low. If data continues to
be clocked in or out, the address loops from 7Fh/FFh to
00h/80h. Invalid memory addresses report an FFh value.
Attempting to write to a read-only register results in no
change to that register’s contents.
Figure 5. Serial Clock as a Function of Microcontroller Clock Polarity (CPOL)
Table 8. Function Table
Note: CPHA bit polarity must be set to 1.
*CPOL is the clock polarity bit that is set in the control register of the microcontroller.
**SDO remains at high impedance until 8 bits of data are ready to be shifted out during a read.
MODE CS SCLK SDI SDO
Disable Reset High Input disabled Input disabled High impedance
Write Low CPOL = 1*, SCLK rising Data bit latch High impedanceCPOL = 0, SCLK falling
Read Low CPOL = 1, SCLK falling X Next data bit shift**CPOL = 0, SCLK rising
SHIFT
SHIFT INTERNAL STROBE
INTERNAL STROBE
CPOL = 1
CPOL = 0
SCLK
SCLK
NOTE: CPOL IS A BIT THAT IS SET IN THE MICROCONTROLLER’S CONTROL REGISTER.
CS
CS
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 17

## Page 18

Figure 6. SPI Single-Byte Read
Figure 7. SPI Single-Byte Write
Figure 8. SPI Multibyte Transfer
A7
SCLK
SDI
SDO HIGH-Z D7 D6 D5 D4 D3 D2 D1 D0
A6 A5 A4 A3 A2 A1 A0
CS
A7
SCLK
SDI
SDO HIGH-Z
A6 A5 A4 A3 A2 A1 A0 D7 D6 D5 D4 D3 D2 D1 D0
CS
ADDRESS
BYTE
SCLK
SDIWRITE
READ
SDI
SDO
DATA
BYTE 0
DATA
BYTE 0
DATA
BYTE 1
DATA
BYTE N
DATA
BYTE 1
DATA
BYTE N
ADDRESS
BYTE
CS
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 18

## Page 19

DRDY
The DRDY output goes low when a new conversion result
is available in the RTD Data Registers. When a read-
operation of the RTD Data Registers completes, DRDY
returns high.
Applications Information
For operation in noisy environments, a filter capacitor
may be placed across the RTDIN+ and RTDIN- inputs.
After an overvoltage or undervoltage fault, after a fault
detection cycle, or after enabling V BIAS, always allow for
the settling time of the input filter before restarting the
ADC. A delay time of at least five time constants plus an
additional 1ms (for the protection devices to stabilize) is
recommended to achieve specified accuracy.
Converting RTD Data Register
Values to Temperature
The ratiometric ADC conversion results found in the RTD
Data Registers can be converted to temperature with a
few calculations.
First, the Resistance of the RTD needs to be determined
with the following equation:
RRTD = (ADC Code x R REF)/215
Figure 9. DRDY Operation
SDO
RTD DATA
RTD DATA
ADDRESS
RTD REGISTER
CONTENTS
DRDY
CS
SDI
CONVERSION n CONVERSION n+1 CONVERSION n+2
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 19

## Page 20

ADC Code = 15-bit ADC results from RTD Data registers
(01h–02h)
RREF = Resistance of the reference resistor
Once the resistance of the RTD is known, the well-defined
resistive properties of the selected RTD can be used to
determine temperature by either calculations or lookup
tables.
In the typical case of a PT100 RTD with a 400 I high
precision low drift reference resistor, Table 9  shows
examples of temperature and resistance values with the
corresponding ADC code results.
Table 9. Temperature Example for PT100 with 400Ω RREF
TEMPERATURE
(°C)
RTD RESISTANCE
(ω )
RTD DATA REG
(01h-02h) (hex) ADC CODE (dec) ADC CODE/32-256
(°C)
-200 18.52 0BDAh 1517 -208.59
-175 29.22 12B4h 2394 -181.19
-150 39.72 196Ch 3254 -154.31
-125 50.06 200Ah 4101 -127.84
-100 60.26 2690h 4936 -101.75
-75 70.33 2D04h 5762 -75.94
-50 80.31 3366h 6579 -50.41
-40 84.27 35EEh 6903 -40.28
-30 88.22 3876h 7227 -30.16
-20 92.16 3AFCh 7550 -20.06
-10 96.09 3D7Eh 7871 -10.03
0 100.00 4000h 8192 0.00
10 103.90 4280h 8512 10.00
20 107.79 44FCh 8830 19.94
30 111.67 4778h 9148 29.88
40 115.54 49F2h 9465 39.78
50 119.40 4C6Ah 9781 49.66
60 123.24 4EE0h 10096 59.50
70 127.08 5154h 10410 69.31
80 130.90 53C6h 10723 79.09
90 134.71 5636h 11035 88.84
100 138.51 58A4h 11346 98.56
110 142.29 5B12h 11657 108.28
120 146.07 5D7Ch 11966 117.94
130 149.83 5FE4h 12274 127.56
140 153.58 624Ch 12582 137.19
150 157.33 64B0h 12888 146.75
160 161.05 6714h 13194 156.31
170 164.77 6974h 13498 165.81
180 168.48 6BD4h 13802 175.31
190 172.17 6E30h 14104 184.75
200 175.86 708Ch 14406 194.19
225 185.01 7668h 15156 217.63
250 194.10 7C3Ah 15901 240.91
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 20

## Page 21

Detecting RTDIN+ Cable Faults
In the 3- and 4-wire RTD connection configuration, a
broken or disconnected RTDIN+ cable results in an
unbiased ADC+ input into the MAX31865. This causes
unpredictable ADC conversion results, which can be
influenced by PCB layout, external circuit noise, and
ambient temperature. This cable fault condition can go
undetected depending upon the values set in the fault
threshold registers. If this condition is of interest, add a
10Mω resistor from the RTDIN+ pin to the BIAS pin. Doing
so results in a full-scale RTD resistance measurement if
the RTDIN+ lead is broken or disconnected.
Decoding RTD and Cable Fault Conditions
An open RTD element or a short across the RTD element
are detected on every conversion based on the resistance
data. An open RTD element results in a full-scale reading.
Set the threshold for open RTD element detection using
the High Fault Threshold registers. If the conversion result
is greater than or equal to the threshold value, the RTD
High bit in the Fault Status register is set at the end of the
conversion. An open RTD element can also be detected
on demand by testing for V REFIN- > 0.85 x V BIAS. A
shorted RTD element produces a conversion result near
zero. Set the threshold for shorted RTD detection in the
Low Fault Threshold registers.
Table 10, Table 11, and Table 12 summarize how RTD and
cable faults are detected for 2-, 3-, and 4-wire setups and
provide a description for the most common cause.
Fault Status bits are latched until the Fault Clear bit in the
Configuration register is set. This allows intermittent faults
to be captured.
Power-Supply Decoupling
To achieve the best results when using the device,
decouple the VDD and DVDD power supplies with a 0.1µF
capacitor. Use a high-quality, ceramic, surface-mount
capacitor if possible. Surface-mount components mini -
mize lead inductance, which improves performance, and
ceramic capacitors tend to have adequate high-frequency
response for decoupling applications.
Table 9. Temperature Example for PT100 with 400Ω RREF (continued)
TEMPERATURE
(°C)
RTD RESISTANCE
(ω )
RTD DATA REG
(01h-02h) (hex) ADC CODE (dec) ADC CODE/32-256
(°C)
275 203.11 81FEh 16639 263.97
300 212.05 87B6h 17371 286.84
325 220.92 8D64h 18098 309.56
350 229.72 9304h 18818 332.06
375 238.44 989Ah 19533 354.41
400 247.09 9E24h 20242 376.56
425 255.67 A3A2h 20945 398.53
450 264.18 A914h 21642 420.31
475 272.61 AE7Ah 22333 441.91
500 280.98 B3D4h 23018 463.31
525 289.27 B922h 23697 484.53
550 297.49 BE64h 24370 505.56
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 21

## Page 22

Table 10. Decoding RTD Faults for 2-Wire Setups When Fault Bit in RTD Data LSB
Register = 1
Table 11. Decoding RTD Faults for 3-Wire Setups When Fault Bit in RTD Data LSB
Register = 1
FAULT
STATUS
BIT SET
DESCRIPTION OF POSSIBLE
CAUSE CONDITION DETECTED DESCRIPTION OF
RESULTING DATA
D7 Open RTD element Measured resistance greater than High Fault
Threshold value Full scale
D6 Shorted RTD element Measured resistance less than Low Fault
Threshold value Near zeroRTDIN+ shorted low
D5
Open RTD
VREFIN- > 0.85 x VBIAS
Full scale
RTDIN+ shorted high Indeterminate
RTDIN- shorted high Indeterminate
D4 RTDIN- shorted low VREFIN- < 0.85 x VBIAS (FORCE- open) Appear to be valid
D3 RTDIN- shorted low VRTDIN- < 0.85 x VBIAS (FORCE- open) Appear to be valid
RTDIN+ shorted low Near zero
D2 Overvoltage or undervoltage fault Any protected input voltage >VDD or <GND1 Indeterminate
FAULT
STATUS
BIT SET
DESCRIPTION OF POSSIBLE
CAUSE CONDITION DETECTED DESCRIPTION OF
RESULTING DATA
D7
Open RTD element
Measured resistance greater than High Fault
Threshold value Full scale
RTDIN+ shorted high and not
connected to RTD
Force+ shorted high and
connected to RTD
D6
RTDIN+ shorted to RTDIN-
Measured resistance less than Low Fault
Threshold value Near zeroRTDIN+ shorted low and not
connected to RTD
Force+ shorted low
D5
Open RTD element
VREFIN- > 0.85 x VBIAS
Full scaleForce+ shorted high and
connected to RTD
Force+ unconnected
IndeterminateForce+ shorted high and not
connected to RTD
RTDIN- shorted high
D4 RTDIN- shorted low VREFIN- < 0.85 x VBIAS (FORCE- open) Appear to be valid
D3
Force+ shorted low
VRTDIN- < 0.85 x VBIAS (FORCE- open) Near zeroRTDIN+ shorted low and
connected to RTD
RTDIN- shorted low Appear to be valid
D2 Overvoltage or undervoltage fault Any protected input voltage >VDD or < GND1 Indeterminate
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 22

## Page 23

Table 12. Decoding RTD Faults for 4-Wire Setups When Fault Bit in RTD Data LSB
Register = 1
FAULT
STATUS
BIT SET
DESCRIPTION OF POSSIBLE
CAUSE CONDITION DETECTED DESCRIPTION OF
RESULTING DATA
D7
Open RTD element
Measured resistance greater than High Fault
Threshold value Full scale
RTDIN+ shorted high and not
connected to RTD
Force+ shorted high and
connected to RTD
D6
RTDIN+ shorted to RTDIN-
Measured resistance less than Low Fault Threshold
value Near zero
RTDIN+ shorted low and not
connected to RTD
RTDIN- shorted high and not
connected to RTD
Force+ shorted low
D5
Open RTD element
VREFIN- > 0.85 x VBIAS
Full scaleForce+ shorted high and
connected to RTD
Force- unconnected
Indeterminate
Force+ unconnected
Force+ shorted high and not
connected to RTD
Force- shorted high and not
connected to RTD
Force- shorted high and connected
to RTD
Force- shorted low and not
connected to RTD
D4
Force- shorted low and connected
to RTD
VREFIN- < 0.85 x VBIAS (FORCE- open)
Indeterminate
RTDIN- shorted low and connected
to RTD Appear to be valid
D3
Force+ shorted low
VRTDIN- < 0.85 x VBIAS (FORCE- open)
Near zeroRTDIN+ shorted low and
connected to RTD
RTDIN- shorted low and connected
to RTD
Appear to be validRTDIN- shorted low and not
connected to RTD
Force- shorted low
D2 Overvoltage or undervoltage fault Any protected input voltage >VDD or < GND1 Indeterminate
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 23

## Page 24

MAX31865
BIAS
REFIN+
DVDD
VDD
GND1
GND2
DGND
REFIN-DRDY
ISENSORSDI
SCLKHOST
INTERFACE
CS
SDO
N.C.
FORCE-
RREF
RTD
RCABLE
RCABLE
0.1µF
VDD
FORCE+
FORCE2
RTDIN+
RTDIN-
CI*
RCABLE
0.1µF
VDD
3-WIRE SENSOR CONNECTION
MAX31865
BIAS
REFIN+
DVDD
VDD
GND1
GND2
DGND
REFIN-DRDY
ISENSORSDI
SCLKHOST
INTERFACE
CS
SDO
N.C.
FORCE-
RREF
RTD
0.1µF
VDD
FORCE+
FORCE2
RTDIN+
RTDIN-
CI*
*CI = 10nF FOR 1kΩ RTD
 100nF FOR 100Ω RTD
0.1µF
VDD
2-WIRE SENSOR CONNECTION
Typical Application Circuits (continued)
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 24

## Page 25

+Denotes a lead(Pb)-free/RoHS-compliant package.
T = Tape and reel.
*EP = Exposed pad.
PART TEMP RANGE PIN-PACKAGE
MAX31865AAP+ -40°C to +125°C 20 SSOP
MAX31865AAP+T  -40°C to +125°C 20 SSOP
MAX31865ATP+ -40°C to +125°C 20 TQFN-EP*
MAX31865ATP+T -40°C to +125°C 20 TQFN-EP* PACKAGE
TYPE
PACKAGE
CODE
OUTLINE
NO.
LAND
PATTERN NO.
20 TQFN-EP T2055+5 21-0140 90-0010
20 SSOP A20+1 21-0056   90-0094
Ordering Information Package Information
For the latest package outline information and land patterns
(footprints), go to www.maximintegrated.com/packages. Note
that a “+”, “#”, or “-” in the package code indicates RoHS status
only. Package drawings may show a different suffix character, but
the drawing pertains to the package regardless of RoHS status.
MAX31865 RTD-to-Digital Converter
www.maximintegrated.com
Maxim Integrated  │ 25

## Page 26

REVISION
NUMBER
REVISION
DATE DESCRIPTION PAGES
CHANGED
0 10/12 Initial release —
1 1/15 Revised Benefits and Features section 1
2 5/15 Added the SSOP package to the data sheet and updated input voltage protection as
±45V.
1, 2, 7, 10, 11,
24
3 7/15 Updated Table 6 16
Revision History
Maxim Integrated cannot assume responsibility for use of any circuitry other than circuitry entirely embodied in a Maxim Integrated product. No circuit patent licenses
are implied. Maxim Integrated reserves the right to change the circuitry and specifications without notice at any time. The parametric values (min and max limits)
shown in the Electrical Characteristics table are guaranteed. Other parametric values quoted in this data sheet are provided for guidance.
Maxim Integrated and the Maxim Integrated logo are trademarks of Maxim Integrated Products, Inc.
MAX31865 RTD-to-Digital Converter
© 2015 Maxim Integrated Products, Inc.  │ 26
For pricing, delivery, and ordering information, please contact Maxim Direct at 1-888-629-4642, or visit Maxim Integrated’s website at www.maximintegrated.com.
