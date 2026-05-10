# maxim temperature sensor tutorial rev0 2017 09

- Source PDF: `docs/source-pdfs/application-notes/maxim_temperature_sensor_tutorial_rev0_2017-09.pdf`
- Extraction date: 2026-05-09
- Page count: 10
- SHA256: `79e1d15b10d5dd4ba0df6ef6636348039971718dc7ebd354a8a124ca79d3b743`

## Page 1

TEMPERATURE SENSOR
Tutorial
www.maximintegrated.com/temp-sensor

## Page 2

T emperature Sensor Tutorial
www.maximintegrated.com
2 www.maximintegrated.com/temp-sensors
T ypes of T emperature Sensors
Thermocouples, RTDs, Thermistors and
Local T emperature Sensor ICs
The availability of temperature sensors in different forms
and with various functions make them suitable for specific
applications. Within modern electronics, there are five types
of temperature sensors that are most commonly used:
thermocouples, resistance temperature detectors (RTDs),
thermistors, local temperature sensor ICs, and remote thermal
diode temperature sensor ICs.
Thermocouples, RTDs, and thermistors are sensing elements
that respond to temperature in a measurable way. They
are normally connected to circuits that convert the sensor
signal into a usable analog or digital value. The circuits are
commonly built from analog-to-digital converters (ADCs),
amplifiers, voltage references, and other active and passive
components, or they can be dedicated sensor signal
conditioner ICs.
Local temperature sensor ICs utilize the physical properties
of transistors on the die as the sensing element. Additional
circuitry, such as an ADC, gain stages, and level shifters, are
used to create a sensor with an analog or digital interface.
Remote thermal diode temperature sensors employ an
external bipolar transistor as the sensing element and include
all the signal conditioning circuitry necessary to measure
temperature using one or more external transistors. In this
tutorial, we will consider both local and remote thermal diode
sensors, how they read temperature, and in what applications
they are best used.
T emperature Sensor Tutorial
Temperature sensors surround us. They are in our buildings and homes measuring
temperature for HVAC systems, refrigerators, freezers, and computers. Industrial
applications such as motor controls, assembly lines, processing, and manufacturing
all require constant monitoring and control of temperature.
Consequently, many different types of temperature sensors exist to accommodate this
wide variety of temperature sensing needs and applications. Some sensors are designed
for measuring ambient air temperature in buildings for climate control, while others are
designed for use in harsh environments measuring the temperature of liquids like coolants
used in automobiles.
This tutorial covers the different types of temperature sensors, their advantages and
disadvantages, and the important considerations in selecting a temperature sensor.
We will investigate and explore the types of sensors best suited for various applications.
Finally, we’ll touch on recent advances that are pushing state-of-the art improvements
to develop even better temperature sensors.

## Page 3

T emperature Sensor Tutorial
www.maximintegrated.com
3www.maximintegrated.com/temp-sensors
Thermocouples
Thermocouples are used extensively in a variety of industrial,
automotive, and consumer equipment. They are self-powered,
requiring no excitation, and operate over a much wider
temperature range (up to +2000°C) than other common
sensors. Bare thermocouple junctions can have quick response
times, allowing measurements to be done without significant
delays in system operation.
Thermocouples sense temperature using the Seebeck
effect, which occurs when there is a temperature differential
between junctions of dissimilar metals. The temperature
differential between the heated area and the cooler portion
causes a voltage difference between the two junctions. This
voltage difference can be used to calculate temperature.
Thermocouples are made by joining two wires of dissimilar
metals. The resulting output voltage is small (about 40µV
per °C for a K type), and requires moderately complex signal
conditioning (including cold-junction compensation and
amplification). There are several thermocouple types, which
are designated by letters. The most widely used is the K type.
Table 1 summarizes the characteristics of a few common
thermocouple types. Note that the sensitivities and usable
temperature ranges differ among the various types.
Type T emperature Range (°C)
(Short T erm) Sensitivity (µV/°C) Conductor Alloys
K −180 to +1300 41
Chromel (90% Ni, 10% Cr)
Alumel (95% Ni, 2% Mn, 2% Al, and 1% Si)
J −180 to +800 55
100% Fe
Constantan (55% Cu, 45% Ni)
N −270 to +1300 39
Nicrosil (84.1% Ni, 14.4% Cr, 1.4% Si, 0.1% Mg)
Nisil (95.6% Ni, 4.4% Si)
R −50 to +1700 10
87% Pt, 13% Rh
100% Pt
S −50 to +1750 10
90% Pt, 10% Rh
100% Pt
B 0 to +1820 10
70% Pt, 30% Rh
94% Pt, 6% Rh
T −250 to +400 43
100% Cu
Constantan
E −40 to +900 68
Chromel
Constantan
Table 1. Characteristics of Some Thermocouple Types

## Page 4

T emperature Sensor Tutorial
www.maximintegrated.com
4 www.maximintegrated.com/temp-sensors
to correct for amplifier offset voltage, as well as resistor,
temperature sensor, voltage reference errors, and linearization.
This must be performed to correct for the effect of the
thermocouple’s nonlinear temperature-voltage relationship.
Dedicated integrated circuits such as the MAX31855 and
MAX31856 ease the process of designing thermocouple
signal conditioning circuits by incorporating a low-noise
precision gain stage, a cold junction compensation sensor, and
a high-resolution ADC. The MAX31856, shown in Figure 3, also
includes input protection and linearization for eight popular
thermocouple types.
Despite the obvious benefits of thermocouples, measuring
temperature with them can be challenging due to their small
output voltage, which requires precise amplification. Their
susceptibility to external noise, especially when long wires
are used between the thermocouple and the measuring
circuit, also poses a challenge. Another complication arises
from additional thermocouples that are created at the point
of contact where the thermocouple wires meet the copper
wires (or traces). The copper wires connect to the signal
conditioning circuitry, creating another thermocouple.
This point is called the cold junction, as shown in Figure 1.
The junction between metal A and metal B is the main
thermocouple junction (also called the “hot junction,” although
its temperature can be lower than that of the cold junction).
The net effect is that the output voltage of this circuit appears
to be the voltage due to the thermocouple minus the voltage
of a similar thermocouple at the cold junction temperature.
For example; if the thermocouple is at +525°C and the
cold junction is at +25°C, V
OUT will indicate +500°C. To
compensate for the effect of the cold junction, measure the
cold junction temperature and add the thermocouple voltage
that would be produced by that temperature to the value
indicated by V
OUT:
VOUT = VTC - VCJ
VTC = VOUT + VCJ
Cold junction compensation is done by placing a temperature
sensor at the cold junction location and using the measured
temperature to compensate for the cold junction temperature.
A complete thermocouple-to-digital circuit
1 including cold-
junction sensing might look like the block diagram shown in
Figure 2. Precision op amp and resistors provide gain to the
thermocouple output signal, which is generally in the millivolt
range. A temperature sensor at the cold junction location
monitors that value and an ADC provides output data at
the required resolution. In general, calibration is necessary
Figure 1. Thermocouple Circuit
Figure 2. Thermocouple-to-Digital Compensation Circuit
Figure 3. MAX31856 Simple Block Diagram
METAL B
METAL A
COLD JUNCTION
V
HOT JUNCTION
MEASUREMENT
THERMOCOUPLE
TEMPERATURE
SENSOR
PRECISION RESISTORS
ADC
(12 BITS TO
24 BITS)
VOLTAGE
REFERENCE
IN1
IN2
TO µC
PGA
INPUT
PROTECTION
AND FAUL T
DETECTION
TEMPERATURE
SENSOR
19-BIT
ADC
LINEARIZATION AND
COLD-JUNCTION
COMPENSATION
CONTROL
AND
INTERFACE
T-
T+
MAX31856

## Page 5

T emperature Sensor Tutorial
www.maximintegrated.com
5www.maximintegrated.com/temp-sensors
RTD (Resistance T emperature Detector)
The electrical resistivity of any metal varies with temperature.
RTDs are temperature sensors that are based on this
behavior. They are effectively resistors with well-defined
resistance vs. temperature characteristics. Platinum is the
most common and accurate wire material used in RTDs
because of its chemical stability and relatively linear response
to temperature changes. Platinum RTDs are also referred
to as PRTDs and are often available with 100Ω and 1kΩ
resistances (at 0°C), which are referred to as PT100 and
PT1000. Nickel, copper, and other metals may also be used
to make RTDs. Characteristics of platinum RTDs include wide
temperature range (up to 750°C or higher), excellent accuracy
and repeatability, and reasonable linearity. Because of their
accuracy, stability, and wide temperature range, RTDs are used
in a variety of precision applications including instruments and
process control.
The resistance vs. temperature curve is reasonably linear, but
has some curvature, as described by the Callendar-Van Dusen
equation:
R(T) = R0(1 + aT + bT
2 + c(T - 100)T3)
Where:
T = temperature (°C)
R(T) = resistance at T
R0 = resistance at T = 0°C
IEC 751 specifies the following Callendar-Van Dusen
coefficient values:
a = 3.90830 x 10
-3
b = -5.77500 x 10-7
c = -4.18301 x 10-12 for -200°C ≤ T ≤ 0°C, 0 for 0°C ≤ T ≤ +850°C
Bare RTD elements generally have higher thermal mass than
bare thermocouples, and so will respond more slowly to
changes in temperature. RTDs and thermocouples are often
encased in protective stainless-steel sheaths. In which case,
the mass of the entire probe will be similar for both types of
sensors, resulting in similar response times.
Signal conditioning plays an important role in RTD
temperature measurement. An excitation current flows
through the RTD, and the voltage across the RTD is measured.
If the excitation current is known (or can be derived), the
RTD’s resistance can be calculated. Configurations may be
two-/three-/four-wire, as shown below in Figure 4. A two-wire
configuration, as shown in Figure 4a, is useful for cases where
the lead length is short enough that wire resistance doesn’t
significantly affect measurement accuracy. In a three-wire
configuration, shown in Figure 4b, a third wire connects to the
RTD probe and carries the excitation current. This provides a
way to cancel the effect of wire resistance, provided the wire
resistances are equal. A four-wire Kelvin configuration, as
shown in Figure 4c, is the most accurate, as its separate force
and sense leads eliminate the effect of wire resistance.
Various circuit architectures implement RTD temperature
measurement. Most implementations require two excitation
current sources and a high-resolution analog-input signal
chain. Many high-resolution ADCs feature current sources
and multiple analog inputs that are appropriate for use in RTD
or other temperature measurement circuits. The MAX11410,
shown in Figure 5, is an example of a high-resolution ADC that
can source a selectable current value from any analog input
pin, providing the excitation current for any RTD configuration.
The MAX31865 is an example of a dedicated RTD signal-
conditioning circuit. As seen in Figure 6, the MAX31865
provides a completely integrated excitation and measurement
circuit, allowing for two-/three-/four-wire configurations with
15-bit resolution. The chip accommodates both PT100 and
PT1000 RTDs and features protection against overvoltage
faults on the input lines. A product like the MAX31865 greatly
accelerates design time when implementing an RTD solution.
Figure 4. Two-Wire, Three-Wire, and Four-Wire RTD
2-WIRE, 3WIRE AND 4-WIRE, RESISTANCE TEMPERATURE DETECTOR (RTD)
RESISTIVE
MEASUREMENT
CURRENT
CURRENT
CURRENT
COMMON
+
+
+
COMMON
COMMON
RESISTIVE
MEASUREMENT
RESISTIVE
MEASUREMENT
A
B
C
CURRENT

## Page 6

T emperature Sensor Tutorial
www.maximintegrated.com
6 www.maximintegrated.com/temp-sensors
Figure 5. Dual RTD Measurement Circuit
REF1P
REF1N
AIN0/REF0P
INPUT
MULTIPLEXER

REFERENCE
MULTIPLEXER

10µA TO
1600µA
0.5µA,
1µA, 10µA
0.5µA,
1µA, 10µA
GND
THIRD-ORDER
DELTA-SIGMA
MODULATOR
DIGITAL
FILTERS
(FIR AND
 SINC)

DIGITAL
CONTROL
LOGIC

CLOCK
GENERATOR
1.8V
REGULATOR
AIN1/REF0N
AIN2
AIN3
AIN4
AIN5
AIN6
AIN7
AIN8
AIN9
REF2P
REF2N
GN D
A VDD
AGND
GPIO0/
EXT_CLK
GPIO1
CS#
SCLK
DIN
DOUT/INT#
VDDRE G
CAPREG
VDDIOCAPP CAPN
PGA
REF0P
REF0 N
BIAS
VOLTAGE
1nF
100nF
3.3V
1kΩ
1kΩ
100nF
3.3V
R REF
4kΩ
1kΩ
1kΩ
1kΩ
1kΩ
1kΩ
1kΩ
1kΩ
1kΩ
SHORT FOR
2-WIRE RTD
SHORT FOR 2-
AND 3-WIRE RTD

3.3V
100nF
A. TWO-RTD T EMPERATURE MEASUREMENT CIRCUIT
MAX11410

## Page 7

T emperature Sensor Tutorial
www.maximintegrated.com
7www.maximintegrated.com/temp-sensors
Figure 6. MAX31865 in Two-/Three-/Four-Wire Configurations
BIAS
REFIN+
GND1
GND2
DGND
REFIN-DRDY
ISENSORSD I
SCLK
CS
SD O
N.C.
FORCE-
R REF
RT D
R CABLE
R CABLE
0.1µ F
FORCE+
FORCE2
RTDIN+
RTDIN-
C I *
R CABLE
0.1µF
V DD V DD
DV DD
V DD
HOST
INTERFACE
MAX31865
BIAS
REFIN+
GND1
GND2
DGND
REFIN-DRDY
ISENSORSD I
SCLK
CS
SD O
N.C.
FORCE-
R REF
RTD
R CABLE
R CABLE
0.1µ F
FORCE+
FORCE2
RTDIN+
RTDIN-
C I *
R CABLE
0.1µF
V DD
3-WIRE SENSOR CONNECTION
BIAS
REFIN+
DV DD
V DD
GND1
GND2
DGND
REFIN-DRDY
ISENSORSD I
SCLKHOST
INTERFACE
CS
SD O
N.C.
FORCE-
R REF
RT D
0.1µ F
V DD
FORCE+
FORCE2
RTDIN+
RTDIN-
C I *
*C I  = 10nF FOR 1k Ω RTD
 100nF FOR 100 Ω RTD *C I  = 10nF FOR 1k Ω RTD
 100nF FOR 100 Ω RTD
0.1µ F
V DD
2-WIRE SENSOR CONNECTION
V DD
DV DD
V DD
HOST
INTERFACE
MAX31865 MAX31865
R CABLE
4-WIRE SENSOR CONNECTION
*C I  = 10nF FOR 1k Ω RTD
 100nF FOR 100 Ω RTD

## Page 8

T emperature Sensor Tutorial
www.maximintegrated.com
8 www.maximintegrated.com/temp-sensors
The MAX31740 fan controller, in Figure 8, uses an NTC
thermistor input to sense temperature. The IC then generates
a PWM output for fan-speed control. This simple fan-speed
controller provides low-cost system cooling independent of
software.
T emperature Sensor ICs
Local and Remote Thermal Diode Sensors
A local temperature sensor is the common name for an
integrated circuit that measures its own die temperature
using the physical properties of bipolar transistors. Some local
temperature sensors have analog outputs (either voltage or
current), while others include an internal ADC and produce a
digital output in one of several formats as seen in Figure 9. I
2C,
SMBus, 1-Wire®, and SPI are the most common, but PWM and
other output formats are available as well.
Thermistors
Thermistors, like RTDs, change resistance in response to
temperature changes. While RTDs usually consist of a pure
metal, thermistors are generally made from a polymer or
ceramic material. In general, thermistors are less expensive
and less accurate than RTDs, but there are exceptions. Most
thermistors are available in a two-wire configuration, like the
RTD shown in Figure 4a. Negative Temperature Coefficient
(NTC) thermistors are commonly used for measurement
applications. As the name suggests, an NTC thermistor’s
resistance decreases as temperature increases. A typical
temperature range for a thermistor is -90°C to +130°C—much
lower than thermocouples and RTDs—although wider range
thermistors are available.
Thermistors have very nonlinear temperature-resistance
relationships, and therefore, require significant linearity
correction. The Steinhart-Hart equation describes the
resistance of a thermistor as a function of temperature and is
used to approximate individual thermistor curves.
The equation is:
R is the thermistor resistance.
T is temperature in Kelvin.
A, B, and C are curve-fitting constants determined through a
calibration process for a given thermistor material and,
ln is the natural log function (log to the base e).
Most thermistor manufacturers provide the A, B, and C
coefficients for a typical temperature range. A common
approach to using a thermistor for temperature measurement
is shown in Figure 7, where a thermistor and fixed-value
resistor form a voltage-divider with an output that is digitized
by an ADC.
Figure 7. Thermistor-to-ADC Interface
ADC
THERMISTOR
R1
VIN
VCC
VCC
VREFIN
Figure 8. Two-Wire Fan-Speed Controller
2-WIRE FAN-SPEED CONTROLLER
N
VDD
D0SENSE
DMIN
FREQ
PWM_OUT
SLOPEGND
33Hz
RSLOPE
CF
VFAN
CB
RB
RST
RD1
RD2
VDD
MAX31740
3I  = A + Bln(R) + C[ln(R)]T

## Page 9

T emperature Sensor Tutorial
www.maximintegrated.com
9www.maximintegrated.com/temp-sensors
Local temperature sensors are useful in a variety of
applications. Frequently, they sense the temperature of a
printed circuit board (PCB) or the ambient air around it. From
PCs to industrial control systems to routers to consumer
equipment, temperature sensing and control plays a critical
role in system performance.
Figure 9. Local Temperature Sensor with an Internal ADC
MAX6629
MAX6630
MAX6631
MAX6632
VOLTAGE
REFERENCE
SPI-COMPATIBLE
INTERFACE
TEMPERATURE
SENSOR
12-BIT + SIGN
/uni03A3-/uni0394 ADC
CS
SCK
SO
Newer local temperature sensors expand applications by
providing tremendous flexibility in implementation. The
MAX31875 features a 0.84mm x 0.84mm x 0.35mm wafer-
level package (WLP) that allows the IC to fit in just about
any space. The average power-supply current depends on
resolution and conversion rate, but is well below 10µA in many
configurations. This means the MAX31875 works in a variety
of battery-powered applications. Accuracy is ±0.6°C typical
from -10°C to +100°C.
A remote digital temperature sensor, also called a remote
sensor or a thermal diode sensor, falls into a different category
as it measures the temperature of an external transistor—
either a discrete transistor or one that is integrated on the die
of another IC. Even though the discrete transistor is located
away from the temperature sensor chip, the mechanism of
sensing temperature is similar to local temperature sensors.
Some microprocessors, FPGAs, and ASICs often include one
or more bipolar sensing transistors (usually called “thermal
diodes”) as seen in Figure 10. These thermal diodes allow the
die temperature of the target IC to be accurately measured.
Figure 10. Remote Temperature Sensor Monitoring the Temperature of a Sensing Transistor (or Thermal Diode) on the Die of an External IC
TO MASTER
IC1
+3.3V
+3.3V
IC2
TO MASTER
IC1
+3.3V
+3.3V
IC2
VDD
DXP1
DXN
DXP2
DXP3
SDA
ADD
THERM
GND
SCL
(10 µMAX®)
VDD
DXP1
DXN1
DXP2
DXN2
DXP3
DXN3
SDA
ADD
THERM
GND
SCL
(12 TDFN)
MAX31730
MAX31730

## Page 10

Learn more
For more information, visit:
www.maximintegrated.com/temp-sensors
© 2017 Maxim Integrated Products, Inc. All rights reserved. Maxim Integrated and the Maxim Integrated logo are trademarks of Maxim Integrated Products, Inc., in the United States and
other jurisdictions throughout the world. All other company names may be trade names or trademarks of their respective owners.
Rev. 0; September 2017
T emperature Sensor Tutorial www.maximintegrated.com
Conclusion
While the science and art of temperature sensor development
is robust and well understood, each design has different
requirements and challenges. Undoubtedly, temperature
sensing will continue to be necessary and beneficial with the
continued development of new manufacturing processes
and the advent of various IoT applications. In addition, more
solution options will become available, such as those highly
accurate temperature sensors featured in Maxim’s Thermal
Management Handbook.
Thermocouples, RTDs, thermistors, and temperature
sensor ICs are the main temperature sensor categories.
Thermocouples are inexpensive, durable, self-powering, and
measure a wide range of temperatures. Platinum RTDs also
measure a wide range of temperatures, although not quite
as wide as most thermocouples. They can also provide very
accurate and repeatable measurements. Thermistors are
generally durable and small. Temperature sensor ICs have
tremendous flexibility in implementation. Although they do
not share the wide operating temperature range of RTDs and
thermocouples, they are easy to use and available in very
small packages.
References
1. K. Lacanette, “Tutorial 4679: Thermal Management Handbook,” Maxim Integrated.
https:/ /www.maximintegrated.com/ en/ app-notes/index.mvp/id/ 4679
T emperature Sensor Resources
T emperature Sensor ICs
Thermal Management Handbook
Trademarks
1-Wire and μMAX are registered trademarks of Maxim Integrated Products, Inc.
When looking for temperature sensing solutions, consider the
many available options to find the one that best matches the
requirements of your design. The sensor types covered in this
tutorial meet the needs and requirements for almost every
application.
