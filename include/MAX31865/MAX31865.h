/**
 * @file MAX31865.h
 * @brief Framework-neutral managed synchronous MAX31865 driver API.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MAX31865/CommandTable.h"
#include "MAX31865/Config.h"
#include "MAX31865/Protocol.h"
#include "MAX31865/Status.h"
#include "MAX31865/Transport.h"
#include "MAX31865/Version.h"

/** @cond INTERNAL
 * Unsupported private implementation declarations. They are forward-declared
 * here only because the class stores no public definition for them.
 */
struct MAX31865BusSession;
struct MAX31865OperationContext;
enum class MAX31865TrackingOutcome : uint8_t;
/** @endcond */

/**
 * @brief Deterministic, application-scheduled synchronous MAX31865 driver.
 *
 * The class owns chip-level protocol and desired/observed state while borrowing
 * every transport resource. It performs no hidden retry, recovery, task
 * scheduling, logging, allocation, or application policy. Public methods are
 * not ISR-safe or internally thread-safe; applications serialize each complete
 * driver call and use one transport arbiter shared by every SPI-bus client.
 * Lifecycle, argument, and timeout validation completes before a protocol
 * operation is created; a rejected precondition invokes no transport callback
 * and is not a tracked health failure.
 */
class MAX31865 final
{
public:
    /** @brief Construct inert in Uninitialized state without callbacks. */
    MAX31865();
    /** @brief Destroy with zero callbacks and zero device I/O. */
    ~MAX31865();

    MAX31865(const MAX31865 &) = delete;
    MAX31865 &operator=(const MAX31865 &) = delete;
    MAX31865(MAX31865 &&) = delete;
    MAX31865 &operator=(MAX31865 &&) = delete;

    /**
     * @brief Bind a validated borrowed transport and apply the startup image.
     * @return Complete status. Invalid input performs no callback. A valid
     * binding remains available for recover() after an I/O failure.
     * @note `config.powerReadyDelayMs` is one explicit pre-protocol sleep and
     * is not charged to `config.defaultOperationTimeoutMs`; total begin wall
     * time can include that delay plus one bounded protocol deadline.
     */
    MAX31865Status begin(const MAX31865BeginConfig &config);
    /** @brief Zero-I/O, idempotent unbind restoring constructor defaults. */
    void end();

    /** @return Current stable hardware lifecycle; performs no callback. */
    MAX31865State state() const;
    /** @return Complete terminal status of the last public operation. */
    MAX31865Status lastOperationStatus() const;
    /** @return Zero-I/O passive health and lifetime-counter snapshot. */
    MAX31865Health health() const;
    /** @brief Clear lifetime/observation counters without changing state. */
    void clearLifetimeCounters();
    /** @brief Set a nonzero passive-health failure threshold. */
    MAX31865Status setOfflineThreshold(uint8_t threshold);
    /** @return Stored default whole-operation timeout in milliseconds. */
    uint32_t defaultOperationTimeoutMs() const;

    /**
     * @brief Advance only elapsed-time readiness/overrun bookkeeping.
     * @param nowMs Caller timestamp on the same modulo-2^32 timebase as the
     * transport clock. This method performs no callback and never reads DRDY.
     * @note When DRDY is absent, call at least once per signed half-range
     * (less than 2^31 ms, about 24.9 days) so modulo target ordering remains
     * unambiguous.
     */
    void tick(uint32_t nowMs);

    /**
     * @brief Read-only raw writable-image probe that preserves counters and
     * lastOperationStatus(). A mismatch invalidates observed configuration.
     * @param[out] out Committed only after all required reads succeed.
     * @note Unlike devices with an ID register, MAX31865 identity can only be
     * inferred from verified register behavior. This probe performs no write.
     */
    MAX31865Status probe(MAX31865DeviceInfo &out);
    /**
     * @brief Probe with an explicit nonzero whole-operation deadline.
     * @param[out] out Committed only after every required read succeeds.
     * @param timeoutMs Nonzero deadline shared by the complete probe.
     * @note Like probe(out), this overload preserves health counters and
     * lastOperationStatus().
     */
    MAX31865Status probe(MAX31865DeviceInfo &out, uint32_t timeoutMs);
    /**
     * @brief Explicit tracked recovery that reapplies the complete desired image.
     * @param timeoutMs Nonzero whole-operation deadline.
     * @note MAX31865 has no software-reset command or RESET pin. Recovery first
     * establishes CS high and normally-off CONFIG, then restores and verifies
     * persistent state. It never restarts conversion implicitly.
     */
    MAX31865Status recover(uint32_t timeoutMs);

    /** @name Typed configuration, legal only in Ready. */
    /** @{ */
    MAX31865Status applyConfiguration(
        const MAX31865DeviceConfig &config,
        uint32_t timeoutMs);
    MAX31865Status readConfiguration(
        MAX31865Settings &out,
        uint32_t timeoutMs);
    MAX31865Status configureMeasurement(
        MAX31865WireMode wireMode,
        MAX31865Filter filter,
        uint32_t timeoutMs);
    MAX31865Status setBias(bool enabled, uint32_t timeoutMs);
    MAX31865Status setWireMode(MAX31865WireMode mode, uint32_t timeoutMs);
    MAX31865Status setFilter(MAX31865Filter filter, uint32_t timeoutMs);
    /**
     * @brief Replace local RTD scaling/curve configuration without device I/O.
     * @note Legal only in Ready. Raw device thresholds are unchanged.
     */
    MAX31865Status setRtdConfig(const MAX31865RtdConfig &config);
    /** @return Current local RTD scaling configuration; zero I/O. */
    MAX31865RtdConfig rtdConfig() const;
    /** @} */

    /** @name Bounded conversion control. */
    /** @{ */
    /** @brief Enable VBIAS, settle, flush stale DRDY, and start auto conversion. */
    MAX31865Status startContinuous(uint32_t timeoutMs);
    /** @brief Enable/settle VBIAS, flush stale DRDY, and trigger one one-shot. */
    MAX31865Status triggerSingleConversion(uint32_t timeoutMs);
    /**
     * @brief Disable conversion and restore the configured idle VBIAS state.
     * @note A one-shot already committed by a CS rising edge cannot be cancelled
     * authoritatively. When one is still armed, stop waits for a proven result,
     * discards it, and returns Ready only after a proven normally-off CONFIG
     * write. A protected-input voltage fault can halt the ADC beyond the normal
     * 55/66 ms maximum; DeviceFault/timeout then preserves the quarantine for a
     * later stop() or recover() call.
     */
    MAX31865Status stop(uint32_t timeoutMs);
    /** @} */

    /** @name Synchronous sampling. */
    /** @{ */
    /**
     * @brief Check readiness exactly once.
     * @note DRDY is readiness-authoritative when provided. If it remains high
     * after the maximum horizon, one bounded FAULT_STATUS read detects D2 ADC
     * halt. Without DRDY, elapsed timing plus the same D2 check is used. Output
     * is preserved on failure.
     */
    MAX31865Status dataReady(bool &out);
    /**
     * @brief Check readiness once with an explicit nonzero I/O deadline.
     * @param[out] out Committed only on success.
     * @param timeoutMs Nonzero deadline for GPIO/SPI callbacks in this check.
     */
    MAX31865Status dataReady(bool &out, uint32_t timeoutMs);
    /** @brief Check once and consume one fresh armed conversion. */
    MAX31865Status poll(
        MAX31865Sample &out,
        const MAX31865ReadOptions *options = nullptr);
    /**
     * @brief Poll once with an explicit nonzero whole-operation deadline.
     * @param[out] out Committed only for an acquired frame (including a
     * DeviceFault frame); otherwise preserved.
     * @param timeoutMs Nonzero deadline shared by readiness, data, and cleanup.
     * @param options Optional caller metadata copied only with a frame.
     */
    MAX31865Status poll(
        MAX31865Sample &out,
        uint32_t timeoutMs,
        const MAX31865ReadOptions *options = nullptr);
    /**
     * @brief Read RTD registers immediately without a freshness check.
     * @warning In Ready this can return the buffered previous conversion. It is
     * legal while Converting only for continuous mode, never an armed one-shot.
     */
    MAX31865Status readSample(
        MAX31865Sample &out,
        const MAX31865ReadOptions *options = nullptr);
    /**
     * @brief Read immediately with an explicit nonzero I/O deadline.
     * @param[out] out Preserved unless a complete RTD frame is committed.
     * @param timeoutMs Nonzero deadline shared by required register reads.
     * @param options Optional caller metadata copied only with a frame.
     */
    MAX31865Status readSample(
        MAX31865Sample &out,
        uint32_t timeoutMs,
        const MAX31865ReadOptions *options = nullptr);
    /**
     * @brief Wait within one deadline and consume an already armed result.
     * @param[out] out Committed only for an acquired frame (including a
     * DeviceFault frame); otherwise preserved.
     * @param timeoutMs Zero performs one readiness check without sleeping and
     * returns NoData unless the result is already ready; otherwise the nonzero
     * readiness/data/cleanup deadline.
     * @param options Optional caller metadata copied only with a frame.
     */
    MAX31865Status readSingle(
        MAX31865Sample &out,
        uint32_t timeoutMs,
        const MAX31865ReadOptions *options = nullptr);
    /**
     * @brief Trigger, wait for, read, and restore idle VBIAS under one deadline.
     * @note A readiness timeout leaves the conversion armed so readSingle() or
     * stop() can finish it without accepting a stale result.
     */
    MAX31865Status readOneShot(
        MAX31865Sample &out,
        uint32_t timeoutMs,
        const MAX31865ReadOptions *options = nullptr);
    /** @} */

    /** @name Fault status, fault cycles, and thresholds. */
    /** @{ */
    /**
     * @brief Read/decode latched fault status; asserted bits return DeviceFault.
     * @param[out] out Committed after a complete status frame.
     * @param timeoutMs Zero permits one immediate attempt; otherwise the
     * nonzero whole-operation deadline.
     */
    MAX31865Status readFaultStatus(
        MAX31865FaultStatus &out,
        uint32_t timeoutMs);
    /** @brief Clear latched fault bits with the required isolated D1 command. */
    MAX31865Status clearFaults(uint32_t timeoutMs);
    /**
     * @brief Clear stale faults and run one fresh automatic fault cycle.
     * @note Rejected before I/O when the configured external RC time constant is
     * greater than 100 us, for which the data sheet requires manual timing.
     * @note After a transient command may commit, deadline expiry permits one
     * no-wait restore attempt; output is committed only after restore succeeds.
     */
    MAX31865Status runAutomaticFaultDetection(
        MAX31865FaultStatus &out,
        uint32_t timeoutMs);
    /**
     * @brief Clear stale faults and run both manual fault-detection phases.
     * @note Waits for both internal step-1 phases and then at least five complete
     * configured external RC time constants before issuing manual step 2.
     * @note An expired no-wait cleanup never advances FORCE- before that timing;
     * unresolved step 1 remains Fault until recover(timeout) completes it.
     */
    MAX31865Status runManualFaultDetection(
        MAX31865FaultStatus &out,
        uint32_t timeoutMs);

    /** @brief Set ordered inclusive 15-bit thresholds under one nonzero deadline. */
    MAX31865Status setFaultThresholdsRaw(
        const MAX31865FaultThresholds &thresholds,
        uint32_t timeoutMs);
    /** @brief Read raw thresholds; preserves out on any failure. */
    MAX31865Status readFaultThresholdsRaw(
        MAX31865FaultThresholds &out,
        uint32_t timeoutMs);
    /** @brief Set finite, nonnegative, ordered resistance thresholds. */
    MAX31865Status setFaultThresholdsResistance(
        float lowOhms,
        float highOhms,
        uint32_t timeoutMs);
    /** @brief Read resistance thresholds; preserves both outputs on failure. */
    MAX31865Status readFaultThresholdsResistance(
        float &lowOhms,
        float &highOhms,
        uint32_t timeoutMs);
    /** @brief Set ordered in-domain CVD temperature thresholds. */
    MAX31865Status setFaultThresholdsTemperature(
        float lowC,
        float highC,
        uint32_t timeoutMs);
    /** @brief Read temperature thresholds; preserves both outputs on failure. */
    MAX31865Status readFaultThresholdsTemperature(
        float &lowC,
        float &highC,
        uint32_t timeoutMs);
    /** @} */

    /** @name Validated register diagnostics, legal only in Ready. */
    /** @{ */
    /**
     * @brief Read one register for diagnostics.
     * @param address Register address 00h..07h.
     * @param[out] out Committed only after a complete successful frame.
     * @param timeoutMs Zero permits one immediate attempt; otherwise the
     * nonzero whole-operation deadline.
     * @note Reading RTD_MSB or RTD_LSB acknowledges readiness and drives DRDY
     * high, so this is not acquisition-neutral.
     */
    MAX31865Status readRegister(
        uint8_t address,
        uint8_t &out,
        uint32_t timeoutMs);
    /**
     * @brief Read one contiguous diagnostic register span.
     * @param startAddress First register address 00h..07h.
     * @param[out] out Caller buffer preserved on failure.
     * @param length Number of contiguous bytes, bounded by the register map.
     * @param timeoutMs Must be nonzero when length is greater than one; zero is
     * accepted only for a one-byte immediate attempt.
     * @note Any span containing RTD_MSB or RTD_LSB acknowledges readiness and
     * drives DRDY high, even when the caller discards those bytes.
     */
    MAX31865Status readRegisters(
        uint8_t startAddress,
        uint8_t *out,
        size_t length,
        uint32_t timeoutMs);
    /**
     * @brief Apply one persistent writable byte through the typed image engine.
     * @note CONFIG command bits D5/D3:D1 are rejected; use typed operations.
     */
    MAX31865Status writeRegister(
        uint8_t address,
        uint8_t value,
        uint32_t timeoutMs);
    /**
     * @brief Apply through the image engine and add target diagnostic readback.
     * @param address Persistent writable address (CONFIG or threshold byte).
     * @param value Candidate persistent value; CONFIG command bits are invalid.
     * @param[out] readBack Committed only after apply, full-image verification,
     * and the final target read all succeed.
     * @param timeoutMs Nonzero deadline shared by apply and verification.
     */
    MAX31865Status writeRegisterVerified(
        uint8_t address,
        uint8_t value,
        uint8_t &readBack,
        uint32_t timeoutMs);
    /**
     * @brief Read all eight registers and build a named dump transactionally.
     * @warning Reading RTD address 01h/02h acknowledges DRDY and can consume a
     * pending readiness indication. This method is therefore Ready-only.
     */
    MAX31865Status dumpRegisters(
        MAX31865RegisterDump *out,
        size_t capacity,
        size_t &count,
        uint32_t timeoutMs);
    /**
     * @brief Restore writable POR values and explicitly clear latched faults.
     * @note This is not a device reset; read-only RTD data is unaffected and a
     * persistent voltage fault can reassert immediately.
     */
    MAX31865Status restoreWritableDefaults(uint32_t timeoutMs);
    /**
     * @brief Destructive threshold-byte write/read/restore communication test.
     * @note A committed pattern receives one no-wait restore attempt after
     * deadline expiry; readBack is published only after restore verification.
     */
    MAX31865Status registerReadbackTest(
        uint8_t &readBack,
        uint32_t timeoutMs);
    /** @} */

    /** @name Local status-returning conversion helpers; no callbacks. */
    /** @{ */
    static MAX31865Status codeToRatio(uint16_t code, float &out);
    MAX31865Status codeToResistance(uint16_t code, float &out) const;
    MAX31865Status resistanceToCode(float resistanceOhms, uint16_t &out) const;
    MAX31865Status resistanceToTemperature(float resistanceOhms, float &out) const;
    MAX31865Status temperatureToResistance(float temperatureC, float &out) const;
    MAX31865Status temperatureToCode(float temperatureC, uint16_t &out) const;
    /** @return Maximum one-shot conversion duration for the desired filter. */
    uint32_t singleConversionTimeMs() const;
    /** @return Maximum continuous-conversion period for the desired filter. */
    uint32_t continuousConversionTimeMs() const;
    /** @return Ceil(10.5 * external RC) + 1 ms, saturated to uint32_t. */
    uint32_t biasSettleTimeUs() const;
    /** @return Decoded documented D7:D2 fault bits. */
    static MAX31865FaultStatus decodeFaultStatus(uint8_t raw);
    /** @} */

private:
    MAX31865Status validateTransport(const MAX31865Transport &transport) const;
    MAX31865Status validateDeviceConfig(const MAX31865DeviceConfig &config) const;
    MAX31865Status validateRtdConfig(const MAX31865RtdConfig &config) const;
    MAX31865Status validateTimeout(uint32_t timeoutMs, bool allowZero) const;
    MAX31865Status requireReady() const;
    MAX31865Status requireOperational() const;

    MAX31865OperationContext makeOperation(uint32_t timeoutMs) const;
    uint32_t deadlineRemainingMs(const MAX31865OperationContext &operation) const;
    bool deadlineExpired(const MAX31865OperationContext &operation) const;
    MAX31865Status sleepWithinDeadline(
        uint32_t delayUs,
        MAX31865OperationContext &operation,
        MAX31865Error timeoutCode);

    MAX31865Status synchronizeChipSelect(MAX31865OperationContext &operation);
    MAX31865Status openSession(
        MAX31865BusSession &session,
        MAX31865OperationContext &operation);
    MAX31865Status transferInSession(
        MAX31865BusSession &session,
        const uint8_t *tx,
        uint8_t *rx,
        size_t length,
        MAX31865OperationContext &operation);
    MAX31865Status closeSession(
        MAX31865BusSession &session,
        MAX31865OperationContext &operation);
    MAX31865Status transact(
        const uint8_t *tx,
        uint8_t *rx,
        size_t length,
        MAX31865OperationContext &operation);

    MAX31865Status readRegistersInternal(
        uint8_t startAddress,
        uint8_t *out,
        size_t length,
        MAX31865OperationContext &operation);
    MAX31865Status writeRegistersInternal(
        uint8_t startAddress,
        const uint8_t *values,
        size_t length,
        MAX31865OperationContext &operation);
    MAX31865Status readPersistentImage(
        uint8_t &config,
        uint8_t thresholds[4],
        MAX31865OperationContext &operation);
    MAX31865Status verifyDesiredImage(MAX31865OperationContext &operation);
    MAX31865Status applyConfigurationInternal(
        const MAX31865DeviceConfig &config,
        bool commitDesired,
        MAX31865OperationContext &operation);
    MAX31865Status prepareRegisterWriteCandidate(
        uint8_t address,
        uint8_t value,
        MAX31865DeviceConfig &out) const;
    MAX31865Status faultThresholdCodeToTemperature(
        uint16_t code,
        float &out) const;
    MAX31865Status writeRuntimeConfig(
        bool bias,
        bool continuous,
        uint8_t commandBits,
        MAX31865OperationContext &operation);

    MAX31865Status clearFaultsInternal(MAX31865OperationContext &operation);
    MAX31865Status readFaultStatusInternal(
        MAX31865FaultStatus &out,
        MAX31865OperationContext &operation,
        bool countObservation);
    MAX31865Status waitForFaultCycle(
        MAX31865OperationContext &operation);
    MAX31865Status resolveFaultCycleInternal(
        MAX31865OperationContext &operation);
    MAX31865Status flushRtdReadyState(MAX31865OperationContext &operation);
    MAX31865Status dataReadyInternal(
        bool &out,
        MAX31865OperationContext &operation);
    MAX31865Status waitForReadyInternal(MAX31865OperationContext &operation);
    MAX31865Status startContinuousInternal(MAX31865OperationContext &operation);
    MAX31865Status triggerSingleInternal(MAX31865OperationContext &operation);
    MAX31865Status stopInternal(MAX31865OperationContext &operation);
    MAX31865Status restoreIdleAfterPrimaryFailure(
        const MAX31865Status &primary,
        MAX31865OperationContext &operation);
    MAX31865Status restoreIdleAfterConsumedOneShot(
        const MAX31865Status &primary,
        MAX31865OperationContext &operation);
    MAX31865Status readSampleInternal(
        MAX31865Sample &out,
        const MAX31865ReadOptions *options,
        MAX31865OperationContext &operation,
        bool requireFresh);
    void updateTimedReadiness(uint32_t nowMs);
    void commitSample(MAX31865Sample &out, MAX31865Sample &candidate);

    MAX31865Status finishOperation(
        const MAX31865Status &status,
        MAX31865TrackingOutcome tracking,
        const MAX31865OperationContext &operation);
    MAX31865TrackingOutcome trackingFor(
        const MAX31865Status &status,
        bool trackedSuccess) const;
    bool isTrackedFailure(MAX31865Error error) const;
    void recordFaultObservation(const MAX31865FaultStatus &fault);
    MAX31865DriverState derivedDriverState() const;
    void invalidateObservedState();
    void resetLocalState();

    MAX31865Transport _transport;
    MAX31865DeviceConfig _desiredConfig;
    MAX31865RtdConfig _rtdConfig;
    uint8_t _observedRegisters[max31865_cmd::NUM_REGISTERS];
    uint8_t _observedValidMask;
    bool _configurationKnown;
    bool _continuous;
    bool _oneShotArmed;
    bool _oneShotCommitPending;
    bool _oneShotCommitUncertain;
    bool _freshResultArmed;
    bool _readyLatched;
    bool _timedReadinessHalted;
    bool _chipSelectUncertain;
    uint32_t _conversionStartMs;
    uint32_t _nextReadyMs;
    uint32_t _defaultOperationTimeoutMs;
    MAX31865State _state;
    MAX31865Status _lastOperation;
    uint8_t _offlineThreshold;
    uint8_t _consecutiveFailures;
    uint32_t _sampleCounter;

    uint32_t _trackedSuccessCount;
    uint32_t _trackedFailureCount;
    uint32_t _sampleFrameAttemptCount;
    uint32_t _sampleFrameSuccessCount;
    uint32_t _sampleFrameFailureCount;
    uint32_t _noDataCount;
    uint32_t _droppedSampleCount;
    uint32_t _overrunCount;
    uint32_t _busLockTimeoutCount;
    uint32_t _busLockFailureCount;
    uint32_t _spiTransferFailureCount;
    uint32_t _chipSelectFailureCount;
    uint32_t _gpioFailureCount;
    uint32_t _drdyTimeoutCount;
    uint32_t _operationTimeoutCount;
    uint32_t _faultObservationCount;
    uint32_t _thresholdFaultObservationCount;
    uint32_t _referenceFaultObservationCount;
    uint32_t _voltageFaultObservationCount;

    bool _hasLastFaultStatus;
    uint8_t _lastFaultStatus;
    bool _hasLastOkMs;
    bool _hasLastErrorMs;
    bool _hasLastSampleTimestamp;
    uint32_t _lastOkMs;
    uint32_t _lastErrorMs;
    uint32_t _lastSampleTimestampUs;
};
