/**
 * @file Status.h
 * @brief MAX31865 status, lifecycle, sample, fault, and health contracts.
 */

#pragma once

#include <stdint.h>

/** @brief Stable result codes returned by every fallible public operation. */
enum class MAX31865Error : uint8_t
{
    Ok = 0, ///< Operation completed successfully.

    NotInitialized, ///< No transport is bound.
    InvalidArgument, ///< A caller argument is invalid.
    InvalidState, ///< The lifecycle or observed state forbids the operation.
    Busy, ///< A synchronous conversion precondition reports busy.
    UnsupportedCommand, ///< The requested sequence is not authoritative/safe.
    NoData, ///< One readiness check found no fresh conversion.
    TimingUnavailable, ///< Required finite timing cannot be established.
    RegisterAddressInvalid, ///< Register address/span is outside 0..7.

    BusLockTimeout, ///< The application bus arbiter timed out.
    BusLockFailed, ///< The application bus arbiter failed otherwise.
    SpiTransferFailed, ///< A fixed full-duplex SPI transfer failed.
    ChipSelectFailed, ///< Manual chip-select control failed.
    GpioFailed, ///< The optional DRDY GPIO read failed.
    OperationTimeout, ///< The whole public-operation deadline expired.
    DrdyTimeout, ///< Conversion readiness missed its deadline.

    RegisterVerifyFailed, ///< Readback differs from the intended register value.
    ProbeMismatch, ///< Raw probe registers do not match the desired image.
    ConfigurationUnknown, ///< Desired/observed device state cannot be trusted.
    DeviceFault, ///< RTD data reports one or more latched device faults.
    RestoreFailed, ///< Required cleanup failed after a primary result.
    ConversionOutOfRange ///< A local RTD/temperature conversion is out of range.
};

/** @brief Complete operation result with static message storage. */
typedef struct MAX31865Status
{
    MAX31865Error code; ///< Stable machine-readable result code.
    const char *msg; ///< Static-lifetime diagnostic string only.
    int32_t detail; ///< Operation/backend-specific signed detail.

    /** @return True exactly when code is MAX31865Error::Ok. */
    bool ok() const { return code == MAX31865Error::Ok; }

    /** @return A complete successful result. */
    static MAX31865Status Ok()
    {
        const MAX31865Status status = {MAX31865Error::Ok, "OK", 0};
        return status;
    }

    /** @return A complete error result containing the supplied fields. */
    static MAX31865Status Error(
        MAX31865Error code,
        const char *msg,
        int32_t detail = 0)
    {
        const MAX31865Status status = {code, msg, detail};
        return status;
    }
} MAX31865Status;

/** @brief Stable hardware lifecycle; synchronous transient work is not exposed. */
enum class MAX31865State : uint8_t
{
    Uninitialized = 0, ///< No transport binding exists.
    Ready, ///< Configuration is known and no conversion is armed.
    Converting, ///< A one-shot or continuous conversion is active.
    Fault ///< Hardware selection or observed configuration is uncertain.
};

/** @brief Passive driver health derived from binding, lifecycle, and failures. */
enum class MAX31865DriverState : uint8_t
{
    UNINIT = 0, ///< Lifecycle is Uninitialized.
    READY, ///< Operational with no tracked failure streak.
    DEGRADED, ///< Operational with a sub-threshold tracked failure streak.
    OFFLINE ///< Fault lifecycle or failure streak reached its threshold.
};

/** @brief Decoded documented bits from the latched FAULT_STATUS register. */
struct MAX31865FaultStatus
{
    uint8_t raw; ///< D7:D2 with documented meanings; D1:D0 are masked out.
    bool highThreshold; ///< RTD result reached/exceeded the high threshold.
    bool lowThreshold; ///< RTD result reached/fell below the low threshold.
    bool refinHigh; ///< REFIN- exceeded 0.85 x VBIAS in fault detection.
    bool refinLow; ///< REFIN- was below 0.85 x VBIAS with FORCE- open.
    bool rtdinLow; ///< RTDIN- was below 0.85 x VBIAS with FORCE- open.
    bool overUnderVoltage; ///< A protected input exceeded its voltage range.

    /** @return True when at least one documented fault is asserted. */
    bool any() const
    {
        return highThreshold || lowThreshold || refinHigh || refinLow ||
               rtdinLow || overUnderVoltage;
    }
};

/** @brief MAX31865Sample::flags validity bits. */
enum : uint8_t
{
    MAX31865_SAMPLE_FLAG_FRAME_VALID = 1U << 0, ///< RTD bytes were read atomically.
    MAX31865_SAMPLE_FLAG_DATA_VALID = 1U << 1, ///< Code/resistance/temperature are usable.
    MAX31865_SAMPLE_FLAG_FAULT_STATUS = 1U << 2, ///< faultStatus is present.
    MAX31865_SAMPLE_FLAG_READ_TIMESTAMP = 1U << 3, ///< readTimestampUs is valid.
    MAX31865_SAMPLE_FLAG_READY_TIMESTAMP = 1U << 4 ///< readyTimestampUs is valid.
};

/** @brief One decoded RTD frame with explicit per-field validity. */
struct MAX31865Sample
{
    uint16_t rawRegister; ///< Unshifted RTD MSB/LSB register pair.
    uint16_t rawCode; ///< Right-shifted 15-bit RTD/reference ratio code.
    uint32_t sampleCounter; ///< Saturating driver-local valid-sample count.
    uint32_t readTimestampUs; ///< Transport timestamp; valid with READ_TIMESTAMP.
    uint32_t readyTimestampUs; ///< Caller timestamp; valid with READY_TIMESTAMP.
    uint8_t channelId; ///< Caller-owned logical channel tag.
    float resistanceOhms; ///< Calculated RTD resistance; valid with DATA_VALID.
    float temperatureC; ///< Callendar-Van Dusen temperature; valid with DATA_VALID.
    MAX31865FaultStatus faultStatus; ///< Decoded status with FAULT_STATUS flag.
    uint8_t flags; ///< Bitwise MAX31865_SAMPLE_FLAG_* validity map.
};

/** @brief Optional caller metadata copied into a committed sample. */
struct MAX31865ReadOptions
{
    bool hasReadyTimestamp; ///< True when readyTimestampUs is caller-valid.
    uint32_t readyTimestampUs; ///< Caller clock in microseconds.
    uint8_t channelId; ///< Caller-owned logical channel tag.
};

/** @brief One address/value row used by dumpRegisters(). */
struct MAX31865RegisterDump
{
    uint8_t address; ///< Register address in 0..7.
    const char *name; ///< Static register name.
    uint8_t value; ///< Observed register value.
};

/** @brief Zero-I/O lifecycle, passive health, counters, and timestamps. */
struct MAX31865Health
{
    MAX31865State state; ///< Stable lifecycle snapshot.
    MAX31865DriverState driverState; ///< Derived passive health classification.
    MAX31865Status lastOperation; ///< Complete last public-operation result.
    bool online; ///< True exactly for READY or DEGRADED driverState.
    bool configurationKnown; ///< True only for a verified observed image.
    bool hasLastFaultStatus; ///< Validity for lastFaultStatus.
    uint8_t lastFaultStatus; ///< Last documented FAULT_STATUS bits observed.
    uint8_t offlineThreshold; ///< Nonzero failure-streak threshold.
    uint8_t consecutiveFailures; ///< Saturating tracked-failure streak.

    uint32_t trackedSuccessCount; ///< Saturating lifetime public successes.
    uint32_t trackedFailureCount; ///< Saturating lifetime public failures.
    uint32_t sampleFrameAttemptCount; ///< Saturating RTD-frame attempts.
    uint32_t sampleFrameSuccessCount; ///< Saturating fault-free valid frames.
    uint32_t sampleFrameFailureCount; ///< Saturating failed/faulted frames.
    uint32_t noDataCount; ///< Saturating nonblocking no-data observations.
    uint32_t droppedSampleCount; ///< Saturating failed sample acquisitions.
    uint32_t overrunCount; ///< Saturating elapsed-time overwrite observations.
    uint32_t busLockTimeoutCount; ///< Saturating originating lock timeouts.
    uint32_t busLockFailureCount; ///< Saturating other lock failures.
    uint32_t spiTransferFailureCount; ///< Saturating transfer callback failures.
    uint32_t chipSelectFailureCount; ///< Saturating CS callback failures.
    uint32_t gpioFailureCount; ///< Saturating DRDY GPIO callback failures.
    uint32_t drdyTimeoutCount; ///< Saturating conversion readiness timeouts.
    uint32_t operationTimeoutCount; ///< Saturating other deadline expirations.
    uint32_t faultObservationCount; ///< Saturating faulted RTD frames/cycles.
    uint32_t thresholdFaultObservationCount; ///< High/low threshold observations.
    uint32_t referenceFaultObservationCount; ///< REFIN-/RTDIN- observations.
    uint32_t voltageFaultObservationCount; ///< Over/undervoltage observations.

    bool hasLastOkMs; ///< Validity for lastOkMs; timestamp zero is valid.
    bool hasLastErrorMs; ///< Validity for lastErrorMs; timestamp zero is valid.
    bool hasLastSampleTimestamp; ///< Validity for lastSampleTimestampUs.
    uint32_t lastOkMs; ///< Start time of the last tracked successful operation.
    uint32_t lastErrorMs; ///< Start time of the last tracked failed operation.
    uint32_t lastSampleTimestampUs; ///< Last committed DATA_VALID read time in us.
};

/** @return Static name for a lifecycle state. */
const char *max31865StateName(MAX31865State state);
/** @return Static name for a passive driver state. */
const char *max31865DriverStateName(MAX31865DriverState state);
/** @return Static name for a public error code. */
const char *max31865ErrorName(MAX31865Error error);
