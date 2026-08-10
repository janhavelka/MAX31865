# Hardware bringup

## Before power

1. Confirm the exact MAX31865 package orientation, exposed-pad/ground
   connections, 3.0-3.6 V supplies, and local 0.1 uF VDD/DVDD decoupling.
2. Review SCLK, SDI/MOSI, SDO/MISO, active-low CS, and optional active-low DRDY.
   The examples contain placeholder ESP32-S2/S3 pins only.
3. Confirm the 2-, 3-, or 4-wire RTD circuit against the official data-sheet
   application diagram. The software wire-mode bit alone cannot correct an
   incorrectly routed FORCE/RTDIN network. Only three-wire mode has a CONFIG
   bit; two-wire and four-wire both encode it clear and are distinguished by
   wiring plus the driver's retained desired enum. Set the example-owned
   `WIRE_MODE` constant to match.
4. Select the 50 or 60 Hz notch for the installation mains environment and set
   the example-owned `FILTER` constant explicitly.
5. Enter the measured precision RREF. The recommended range is 350 ohm to
   10 kohm; approximately four times RTD R0 is the device recommendation
   (400 ohm for PT100, 4 kohm for PT1000).
6. Calculate or measure the differential RTDIN input-filter time constant and
   enter it in `MAX31865RtdConfig`. The driver uses it for real settling and
   fault-cycle timing; it is not merely descriptive metadata.

In 3-/4-wire circuits a disconnected RTDIN+ lead leaves ADC+ floating, so the
reported code can depend on PCB/noise/temperature and may not cross a threshold.
When reliable full-scale indication is required, the data sheet recommends a
10 Mohm resistor from RTDIN+ to BIAS. Validate its effect on the actual board.

## First communication

Build `ex_bringup_s2` or `ex_bringup_s3`. Start at the example's 1 MHz,
MSB-first SPI mode 1. With a logic analyzer verify:

- CS is high while idle and has no setup glitch;
- SCLK idles low and data changes/samples according to mode 1;
- address bit D7 is clear for reads and set for writes;
- each frame is contiguous under one CS-low interval;
- CS rising commits one-shot/fault commands;
- SCLK is at or below 5 MHz and setup/hold/inactive minima are met.

`begin()` writes and verifies the managed image. A following `probe()` is
read-only and reports image consistency, not silicon identity. The device has no
ID register. An all-ones/all-zero bus can still require board-level diagnosis.

## Conversion checks

Run one-shot first. Verify VBIAS enable, at least `ceil(10.5 * RC) + 1 ms`
settling, then the expected maximum conversion interval: 55 ms for 60 Hz or
66 ms for 50 Hz. Compare raw code and resistance with precision-resistor inputs
before trusting temperature conversion.

For continuous mode, the first result uses the single-conversion maximum;
later maximum periods are 18 ms at 60 Hz and 21 ms at 50 Hz. With DRDY wired,
the library uses the pin exclusively. Verify DRDY goes low on completion and
returns high when RTD data is read. Without DRDY, verify elapsed-time behavior
under scheduler load.

## Fault checks

Use known resistor/open/short fixtures and current-limited procedures. Confirm
all documented bits: high threshold, low threshold, REFIN high, REFIN low,
RTDIN low, and over/undervoltage. Faults are latched and a fresh cycle clears
old status first.

Automatic fault detection is valid only when the external RC time constant is
no more than 100 us. Otherwise use manual detection. On a logic analyzer,
manual step 2 must occur only after two 100 us internal step-1 phases and at
least five additional external RC time constants.

Threshold registers store a left-shifted 15-bit code; D0 of each threshold LSB
is don't-care. Verify high/low behavior around both boundaries on real silicon
before using a threshold as an alarm decision.

## Evidence

Record board revision, serial number, fitted RREF/RTD/filter values, wiring,
power supply, firmware version/commit, compiler environment, temperature
reference, analyzer traces, raw logs, test result, date, and reviewer. Target
compilation or a passing host model is not HIL evidence.

The optional `tools/run_max31865_hil.py` serial harness can stage bounded raw,
JSON, and Markdown evidence from example 06. Its default plan is read-only and
its electrical actions require explicit flags. A parser verdict is not an HIL
acceptance decision; this repository currently contains no dated board-run
artifact or accepted hardware result.
