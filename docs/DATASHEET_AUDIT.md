# MAX31865 data-sheet and register audit

## Authority and currentness

The implementation was audited against Analog Devices'
[MAX31865 product page](https://www.analog.com/en/products/max31865.html) and
[MAX31865 RTD-to-Digital Converter, Rev. 3 (7/15)](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31865.pdf).
The official product source was checked again on 2026-08-07 and still served
Rev. 3 as the current functional data sheet. No separate functional errata was
listed in the checked official source set.

ADI product-information notifications
[2571B](https://www.analog.com/media/en/pcn/pcn_2571b.pdf) and
[2571D](https://www.analog.com/media/en/pcn/pcn_2571d.pdf), dated 2026-04-20,
describe package marking/labeling changes and state no fit, form, function,
quality, or reliability impact. They do not change the register or timing model.

The repository retains the audited Rev. 3 PDF and extracted notes under
`docs/source-pdfs/` and `docs/extracted-md/`. Official online documents remain
the release-time authority.

## Register map

| Address | Register | POR | Access/side effect |
| --- | --- | --- | --- |
| 00h | CONFIG | 00h | Persistent VBIAS, auto, 3-wire, filter fields plus self-clearing/temporary one-shot, fault-cycle, and fault-clear commands. |
| 01h | RTD MSB | 00h | Read-only RTD data; reading the RTD pair acknowledges DRDY high. |
| 02h | RTD LSB/fault | 00h | RTD D7:D1, fault indicator D0; part of DRDY acknowledgement. |
| 03h | High threshold MSB | FFh | Writable left-shifted 15-bit threshold. |
| 04h | High threshold LSB | FFh | D7:D1 defined; D0 don't-care. |
| 05h | Low threshold MSB | 00h | Writable left-shifted 15-bit threshold. |
| 06h | Low threshold LSB | 00h | D7:D1 defined; D0 don't-care. |
| 07h | FAULT_STATUS | 00h | Latched D7:D2 faults; D1:D0 don't-care. |

CONFIG mappings are D7 VBIAS, D6 automatic conversion, D5 one-shot, D4
three-wire, D3:D2 fault-cycle control, D1 fault clear, and D0 50 Hz selection
(clear selects 60 Hz). The implementation isolates command bits from the
persistent desired image and masks threshold D0 during readback verification.

The decoded fault mask is D7 high threshold, D6 low threshold, D5 REFIN high,
D4 REFIN low, D3 RTDIN low, and D2 protected-input over/undervoltage.

## SPI audit

- Address D7 selects write; D6:D0 carry address. Reads leave D7 clear.
- Bytes are MSB first. Valid timing is mode 1 or mode 3 (CPHA=1).
- Maximum SCLK is 5 MHz.
- The audited core applies conservative 1 us guards for the 400 ns CS-to-SCLK
  setup, 100 ns final-SCLK-to-CS hold, and 400 ns CS-high inactive minima.
- One-shot and fault-cycle commands take effect on CS rising. The core therefore
  requires explicit CS control and holds a bus lock across the complete frame.

## Conversion and settling audit

| Behavior | Audited maximum/requirement |
| --- | --- |
| Bias/input settling before conversion | At least 10.5 external RC time constants plus 1 ms. |
| One-shot, 60 Hz notch | 55 ms maximum. |
| One-shot, 50 Hz notch | 66 ms maximum. |
| Later continuous period, 60 Hz | 17.6 ms maximum; core rounds conservatively to 18 ms. |
| Later continuous period, 50 Hz | 21 ms maximum. |
| Automatic fault cycle | 600 us maximum and only suitable for RC <= 100 us. |
| Manual step 1 before FORCE- opens | Two internal 100 us phases (200 us). |
| Manual external settling | At least five RC time constants after FORCE- opens. |
| Manual step 2 | Two internal comparisons, conservatively 200 us. |

The first continuous result uses the one-shot maximum. Wired DRDY is active low
and readiness-authoritative; elapsed timing is used only when DRDY is absent.
After a maximum horizon with no result, FAULT_STATUS D2 is inspected because a
protected-input voltage condition halts ADC updates beyond those maxima.
Reading the RTD pair drives DRDY high, so start paths flush old RTD data before
arming freshness. Settings reads avoid bursting through 01h/02h for the same
reason.

## Scaling and CVD audit

The 15-bit code represents `RRTD / RREF = code / 32768`. Threshold APIs use the
unshifted 15-bit code and the codec performs the register left shift. The
default IEC 60751 coefficients are A=3.9083e-3, B=-5.775e-7, and C=-4.18301e-12.
The default conversion domain is explicitly -200 to +850 degrees C. Forward
conversion uses the negative-temperature C term below zero; inverse conversion
uses fixed-count bisection over a validated monotonic domain.

RREF is validated in the documented recommended 350 ohm to 10 kohm range. The
RTD R0 remains configurable rather than artificially restricted to PT100/PT1000,
but the full configured curve must remain below the ADC/RREF representable
maximum.

## Fault and diagnostic audit

Fresh automatic/manual fault detection clears latched status before starting.
Cleanup restores and verifies desired normally-off/idle-bias state. A raw
CONFIG write cannot inject transient command bits; typed operations own their
timing. `restoreWritableDefaults()` is described as a writable-image restore,
not reset, because no reset mechanism exists.

`probe()` is read-only. The deliberately destructive communication test uses a
threshold byte and restores/verifies it. `dumpRegisters()` warns that reading
RTD data consumes readiness. Threshold comparison is documented as high at or
above and low at or below; the equality edge should still be confirmed in HIL
when it matters to system policy.

For 3-/4-wire hardware, a broken RTDIN+ lead can float ADC+ and produce an
unpredictable reading that does not reliably trip a threshold. The data sheet
recommends 10 Mohm from RTDIN+ to BIAS to force a full-scale indication; this is
a board-level mitigation, not something software can synthesize.
