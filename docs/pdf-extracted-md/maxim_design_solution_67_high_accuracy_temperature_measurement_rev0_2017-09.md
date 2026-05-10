# maxim design solution 67 high accuracy temperature measurement rev0 2017 09

- Source PDF: `docs/source-pdfs/application-notes/maxim_design_solution_67_high_accuracy_temperature_measurement_rev0_2017-09.pdf`
- Extraction date: 2026-05-09
- Page count: 4
- SHA256: `89dc5c12c3baaa6c2419e453d449bb67eae87e408bc60ddb7630584d5e8f07ea`

## Page 1

DESIGN SOLUTIONS
www.maximintegrated.com/ design-solutions
1
SENSORS
coefficient and a limited temperature range. The nonlinearity
of the thermistor makes it challenging to achieve a high level
of accuracy.
RTD PYROELECTRIC DEVICE TRANSISTOR OR
DIODE
THERMISTOR
SILICON
SENSOR
SIGNAL
CONDITIONING
CIRCUIT
THERMOCOUPLE SILICON
THERMOMETER
THERMOPILE
Figure 2. Common Temperature Sensors
The construction of the thermocouple is metal to metal. The
electromotive force (EMF) output of a thermocouple is in
millivolts instead of resistance and is nonlinear. The major
advantage of the thermocouple, as well as the pyroelectric
device, is that it can sense extremely high temperatures. The
thermocouple produces EMF microvolts per degree Celsius
output signals. Because of the small output voltage changes,
the thermocouple signal chain is prone to EMI and IC device
noise. Therefore, high-accuracy thermocouple designs can be
challenging.
The diode and signal thermometer are both silicon based,
providing linear response that is better than the RTD sensor.
Since this temperature sensor exists in silicon, the temperature
range is limited from -55°C to +150°C.
The RTD is a perfect sensor for the pharmaceutical precision
application. The four characteristics that separate the RTD
from the others are stability, accuracy, better linearity than
thermocouples and thermistors, and a wide temperature range.
Introduction
In pharmaceutical processing, inaccurate temperature
conditions and measurements could ruin an entire batch
of products. For example, a misread of the pharmaceutical
process temperature could produce improper medical mixtures,
compromising the quality of medicine and jeopardizing the
overall production cost, thus causing losses exceeding hundreds
of thousands of dollars. In precision industrial systems such as
this, it is extremely important to know the exact temperatures
of the chemicals.

Figure 1. Pharmaceutical Monitoring System
Common T emperature Sensors
It is important to understand the characteristics and limits for
various temperature sensors to make the best choice for your
application. Commonly available temperature sensors include
the resistance temperature detector (RTD), pyroelectric
device, silicon transistor or diode, thermocouple, thermopile,
thermistor, and silicon thermometer (Figure 2).
The RTD and thermistor temperature sensing elements change
resistive values with temperature changes. The platinum
RTD has a relatively positive linear temperature coefficient of
0.00385Ω/Ω/°C.
In contrast, the high-resistance negative temperature
coefficient (NTC) thermistor has a fairly nonlinear temperature
Achieve High-Accuracy Temperature Measurement in Your
Precision Designs

## Page 2

www.maximintegrated.com/ design-solutions
2
TEMPERATURE °C
5
4
3
2
1
0
-300 -200 0 100 200 300 400 500 600 700 800 900-100
% ERROR
Figure 4. PT-100 RTD Linearity Error vs. Temperature
The RTD resistance (as it changes with temperature) can be
difficult to sense given cable impedance. Reasonable values for
the cable resistance (R
CABLE) can be as high as 50Ω (Figure 5).
2-WIRE RTD
3-WIRE RTD
4-WIRE RTD
PR-100 (RTD)
PR-100 (RTD)
PR-100 (RTD)
C1
C1
RCABLE
RCABLE
RCABLE
RCABLE
RCABLE
RCABLE
RCABLE
RCABLE
RCABLE
FORCE+
RTDIN+
RTDIN-
FORCE-
FORCE+
RTDIN+
FORCE-
RTDIN-RTDIN+
RTDIN-
FORCE-
FORCE+
RTDIN 2
Figure 5. 2-Wire, 3-Wire, and 4-Wire PT100 RTD Configurations with
MAX31865 Pin Connections
The overtemperature resistor range of PT-100 is from 18.52Ω at
-200°C to 390Ω at +850°C. With this RTD resistor range, it is
easy to see that the 2-wire RTD and 10Ω cable resistance creates
~25°C error across the entire temperature range. Additionally,
the temperature coefficient of the cable or lead can further
contribute to measurement errors.
The 3-wire and 4-wire RTD hardware implementations
significantly reduce these errors.
Implementing the RTD System
There are several ways to implement the RTD sensing circuit,
including the discrete design approach or the fully contained
integrated approach.
The discrete design requires a precision amplifier and current
source (Figure 6).
The RTD Sensor
There are several varieties of RTD resistive material, such as
nickel (Ni), copper (Cu), or platinum (Pt). The more common
RTD material is platinum because of its chemical stability and
relatively linear response to temperature changes.
Platinum RTDs are available in a variety of 0°C resistive values,
however the 100Ω (PT-100) and 1000Ω (PT-1000) are most
common across applications. Because of this popularity, the
MAX31865; RTD-to-digital-converter, accommodates both
RTD resistance values.
The resistances over temperature of these elements are very
stable with a temperature coefficient of 0.003925Ω/Ω/ºC. The
RTD resistive response, with some curvature, closely matches a
straight-line approximation (Figure 3).
TEMPERATURE (°C)
RESISTANCE (Ω)
700600400 5000 100 200300-100
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
Figure 3. PT-100 RTD Resistance vs. Temperature
The linearity of the RTD element is predictable and can be
described with the Callendar-Van Dusen equation (Eq. 1).
R(T) = R
0 (1 + aT + bT2 + c(T - 100) T3) (Eq. 1)
In this equation, R(T) is equal to the RTD resistance over
temperature, R
0 is RTD resistance at 0°C, and T is the
temperature in degrees Celsius. Additionally,
a = 3.9083 × 10-3
b = -5.775 × 10-7
c = -4.18301 × 10-12 for -200°C ± T ± 0°C
c = 0 for 0°C ± T ± +850°C
Using the Callendar-Van Dusen equation, the maximum RTD
end-point linearity error, whether you use the PT-100 or the PT-
1000, is approximately 4.34% (Figure 4).

## Page 3

www.maximintegrated.com/ design-solutions
3
In this complete system, the MAX31865 performs error-
checking, prevents input overvoltages, and filters 50Hz or 60Hz
line signals.
The MAX31865 is effectively positioned to interface directly
to the RTD sensor. The MAX31865 builds a voltage-divider
between the RTD and external precision resistor, R
REF (see the
blue arrow in Figure 7). This circuit does not need an exact
current source, rather the RTD current is a byproduct of the
R
RTD/RREF ratio. With the MAX31865, the derivation of the RTD
resistance value is easy (see Eq 2).
RRTD = (ADC CODE ×  RREF)
215
(Eq. 2)
Where RREF is the reference resistor in Figure 7.
The MAX31865, designed to accommodate the 2-/3-/ 4-
wire RTD configurations, offers an elegant solution. Now the
precision is dependent on a single discrete resistor, R
REF. A 15-
bit ΔΣ ADC acquires the RTD sensor resistance. To further
improve the accuracy of the RTD sensing element, use the
Callendar-Van Dusen calibration equation (Eq. 1).
The MAX31865 conversion engine is a 15-bit ΔΣ ADC. With
the signal conditioning circuit implemented primarily as a
digital circuit, the MAX31865 RTD-digital converter provides:
1. Repeatable Results
• The stability of this system is dependent on the R
REF and
RTD resistor ratio and the low noise characteristics of
a ΔΣ ADC.
2. Input Protection
• The RTD input pins (FORCE+, FORCE2, FORCE-,
RTDIN+, and RTDIN-) are protected against high
voltages of up to ±45V. Analog switches open when
the applied external voltage is greater than V
DD +
100mV or less than VGND1 – 400mV.
3. Fault Detection
• Identifies conditions such as an open RTD element,
input pins tied to ground or V DD, or inputs shorted
together.
4. 50Hz or 60Hz Rejection Capability
• The MAX31865’s internal ΔΣ ADC has a Sinc digital
filter that is programmable to reject a frequency of
50Hz or 60Hz.
+
PGA ADC
/uni0394/uni03A3
IRTD
RRTD
RCABLE
RCABLE
VIN+
VIN-
RCABLE
RCABLE
Figure 6. Example Discrete 4-Wire RTD Circuit
This RTD system measures temperature over a wide range of
-200°C to +850°C. The design uses an RTD in a 4-wire
configuration. The current source (IRTD) excites the 4-wire
RTD. The differential voltage that develops across the RTD is
gained by the programmable-gain amplifier (PGA). The signal is
then converted into a digital output code.
This system can accurately measure the RTD change in
resistance, however, there are three components (PGA, I
RTD, and
ΔΣ ADC) and there is no accommodation for error conditions.
Additionally, in this circuit there is no overvoltage input-
protection or RTD-connection fault detections.
The Integrated System
The RTD, in conjunction with the MAX31865 RTD-to-digital
converter, provides a complete solution. This device is well
suited for high-precision applications by providing a 0.03125°C
resolution across a -200°C to +850°C temperature range, with
a 0.5°C level of accuracy (Figure 7).
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
MAX31865
Figure 7. MAX31865 3-Wire RTD Sensor Connection

## Page 4

Maxim Integrated and the Maxim logo are registered trademarks of Maxim Integrated
Corporation. All other trademarks are the property of their respective owners.
Maxim Integrated
160 Rio Robles
San Jose, CA 95134 USA
408-601-1000
maximintegrated.com/ design-solutions
4
Glossary
RTD: Resistance temperature detector
PGA: Programmable gain amplifier
NTC: Negative temperature coefficient
EMF: Electromotive force
Ni: Nickel
Cu: Copper
Pt: Platinum
R
CABLE: Cable resistance
Learn more:
Tutorial 4679: Thermal Management Handbook
Application Note 6262: RTD Measurement System Design Essen-
tials
MAX31865 RTD-to-Digital Converter
Conclusion
Industrial temperature measurement circuits such as
pharmaceutical systems require a highly accurate temperature
sensor and precision ADC. The selection of the temperature
sensor type is critical, however, the ability to utilize the output
signal of the sensor is paramount.
The ideal temperature sensor for this application is the RTD.
RTDs are commonly used because of their accuracy, stability,
and wide temperature range. The easy-to-use MAX31865
RTD-to-digital converter captures the small RTD resistance
value to provide a repeatable, precise digital-to-temperature
conversion, with accuracy of 0.5°C.
The MAX31865 is well-suited for these tasks, with a built-in
RTD interface and a 15-bit Δ Σ ADC. This device has numerous
error correction mechanisms, which insures a stable and
reliable conversion result. The MAX31865 has a the 0.5°C
accuracy over a -200°C to 850°C range to service these more
critical temperature challenges.
Design Solutions No. 67
Find More Design Solutions
Need Design Support?
Call 888 MAXIM-IC (888 629-4642)
Rev 0; September 2017
