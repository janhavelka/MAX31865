# Known limitations

- Host tests and ESP32 target compilation do not prove physical framing,
  analog accuracy, RTD self-heating, EMC behavior, cable-fault coverage, or
  long-duration reliability. Those remain HIL responsibilities.
- MAX31865 has no device-ID register, hardware RESET pin, or software-reset
  command. `probe()` proves readable register consistency only, and
  `restoreWritableDefaults()` is not a silicon reset.
- The driver has one-sample freshness state, not a FIFO or queue. Accurate edge
  timestamps and lossless acquisition require application-owned DRDY capture,
  scheduling, and buffering.
- When DRDY is configured it is the readiness authority. A stuck-high pin
  causes bounded `DrdyTimeout`; elapsed time never substitutes for a result.
  At/after the maximum horizon, FAULT_STATUS may be read to distinguish a D2
  ADC halt from ordinary not-ready state.
- Without DRDY, service `tick()`/readiness at least once per 2^31 ms (about
  24.9 days); longer modulo-clock gaps make past/future target ordering
  ambiguous. The elapsed path adds a 1 ms quantization guard.
- Protected-input D2 faults halt ADC conversion updates, so 55/66 ms is not an
  absolute stop/readiness bound while that physical condition persists.
- Reading RTD registers 01h/02h acknowledges DRDY high. Raw reads and
  `dumpRegisters()` can consume readiness and are Ready-only diagnostics.
- Arduino SPI cannot report wire-level failure or cancel an in-flight transfer.
  Register mismatch may be the only visible evidence of a physical bus fault.
- Methods are synchronous, not ISR-safe, and not internally thread-safe. The
  application owns instance serialization and shared-SPI arbitration.
- No task, ISR bridge, queue, logger, retry/recovery loop, persistence layer,
  telemetry, calibration store, or machine-safety policy is included.
- No native ESP-IDF backend/example is bundled. ESP-IDF component metadata
  exposes the neutral core; the application owns and validates all callbacks.
- Default CVD conversion is limited to -200 to +850 degrees C. Custom domains
  and coefficients are accepted only after analytic derivative/extrema checks
  over the complete domain, plus finite strictly increasing grid and
  selected-RREF representability checks. This validation does not certify a
  nonstandard sensor curve.
- Hardware has only a three-wire compensation bit. Two-wire and four-wire both
  read back with that bit clear, so the driver reports its retained desired
  two/four-wire enum rather than claiming that the register identifies wiring.
- A disconnected RTDIN+ in 3-/4-wire circuits can float and evade reliable
  threshold detection. The documented 10 Mohm RTDIN+-to-BIAS workaround is a
  hardware design choice and needs board-level validation.
- Automatic fault-cycle timing is supported only for external RC <= 100 us.
  Larger RC requires the manual sequence.
- Data-sheet register prose describes inclusive high/low threshold comparison,
  while its flowchart presentation is not perfectly explicit at the equality
  edge. Do not assign a safety guarantee to that one-code boundary without HIL.
- `end()` and destruction deliberately perform no I/O. Applications requiring
  a proven normally-off ADC must call bounded `stop()` before releasing the
  transport.
