/**
 * @file Status.h
 * @brief Status, sample, and health types for the MAX31865 driver.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Detailed lifecycle state of the driver.
 */
enum class MAX31865State : uint8_t {
    Uninitialized = 0, ///< Object exists but begin() has not completed.
    Ready,            ///< Device is configured enough for commands and reads.
    Configuring,      ///< Configuration or register programming is in progress.
    Converting,       ///< A one-shot or continuous conversion is active.
    Recovering,       ///< Recovery or register restore is in progress.
    Fault             ///< Last tracked operation put the driver into a fault state.
};

/**
 * @brief Coarse health state shared by the unified sensor-driver examples.
 */
enum class MAX31865DriverState : uint8_t {
    UNINIT = 0, ///< begin() has not completed or end() was called.
    READY,      ///< Operational with no consecutive tracked failures.
    DEGRADED,   ///< Operational, but recent tracked operations have failed.
    OFFLINE     ///< Consecutive failures reached offlineThreshold().
};

/**
 * @brief Last-operation error code.
 *
 * Legacy-style methods return bool for compact Arduino call sites. Use
 * lastError(), lastErrorName(), lastOperationStatus(), or health() when callers
 * need the failure cause.
 */
enum class MAX31865Error : uint8_t {
    Ok = 0,                 ///< Last operation completed successfully.
    NotInitialized,         ///< begin() has not completed successfully.
    InvalidArgument,        ///< Caller supplied an invalid parameter.
    InvalidConfig,          ///< begin() or RTD scaling configuration is invalid.
    ResourceAllocationFailed, ///< Internal resource allocation failed.
    SpiLockTimeout,         ///< SPI mutex could not be acquired in time.
    SpiTransferFailed,      ///< SPI transaction did not complete as expected.
    RegisterVerifyFailed,   ///< Register readback did not match the write.
    DeviceNotFound,         ///< Probe rejected the observed bus response.
    ConversionNotReady,     ///< Nonblocking read was requested before data was ready.
    Timeout,                ///< Timed out waiting for conversion/fault-cycle completion.
    Busy,                   ///< Operation is blocked by current conversion state.
    FaultPresent            ///< RTD data reported the MAX31865 fault flag.
};

/**
 * @brief Status-return helper used by probe(), recover(), and CLI diagnostics.
 */
typedef struct MAX31865Status {
    MAX31865Error code; ///< Status/error code.
    const char* msg;    ///< Static human-readable message.
    int32_t detail;     ///< Optional numeric detail, often a raw register value.

    /// True when code is MAX31865Error::Ok.
    bool ok() const { return code == MAX31865Error::Ok; }

    /// Construct an OK status.
    static MAX31865Status Ok() {
        MAX31865Status status = {MAX31865Error::Ok, "OK", 0};
        return status;
    }

    /// Construct an error status.
    static MAX31865Status Error(MAX31865Error code, const char* msg, int32_t detail = 0) {
        MAX31865Status status = {code, msg, detail};
        return status;
    }
} MAX31865Status;

/**
 * @brief Raw RTD register pair decoded into the 15-bit ADC code and fault flag.
 */
typedef struct MAX31865RawRtd {
    uint16_t raw_register; ///< Unshifted RTD MSB/LSB register value.
    uint16_t code;         ///< 15-bit RTD ADC code, right-shifted by one.
    bool fault;            ///< True when raw_register bit 0 reported a fault.
} MAX31865RawRtd;

/**
 * @brief Decoded MAX31865 fault-status register.
 */
typedef struct MAX31865FaultStatus {
    uint8_t raw;              ///< Fault-status register masked to documented bits.
    bool high_threshold;      ///< RTD code exceeded the high threshold.
    bool low_threshold;       ///< RTD code was below the low threshold.
    bool refin_high;          ///< REFIN- > 0.85 * VBIAS during fault detection.
    bool refin_low;           ///< REFIN- < 0.85 * VBIAS during fault detection.
    bool rtdin_low;           ///< RTDIN- < 0.85 * VBIAS during fault detection.
    bool over_under_voltage;  ///< Protected input overvoltage/undervoltage detected.

    /// True when any documented fault bit is asserted.
    bool any() const {
        return high_threshold || low_threshold || refin_high || refin_low ||
               rtdin_low || over_under_voltage;
    }
} MAX31865FaultStatus;

/**
 * @brief Converted RTD sample from a one-shot, poll, or continuous read path.
 */
typedef struct MAX31865Sample {
    uint32_t timestamp_ms;             ///< Capture timestamp from millis().
    uint32_t sample_counter;           ///< Monotonic sample counter produced by the driver.
    MAX31865RawRtd raw;                ///< Raw RTD register data.
    float resistance_ohms;             ///< Resistance derived from raw.code and reference resistor.
    float temperature_c;               ///< Temperature derived from resistance_ohms.
    bool has_fault_status;             ///< True when fault_status was read because raw.fault was set.
    MAX31865FaultStatus fault_status;  ///< Decoded fault bits when has_fault_status is true.
} MAX31865Sample;

/**
 * @brief Register/value row returned by dumpRegisters().
 */
typedef struct MAX31865RegisterDump {
    uint8_t addr;       ///< Register address.
    const char* name;   ///< Static register name.
    uint8_t value;      ///< Register value.
} MAX31865RegisterDump;

/**
 * @brief Detailed driver health and diagnostic counters.
 */
typedef struct MAX31865Health {
    MAX31865State state;             ///< Detailed lifecycle state.
    MAX31865DriverState driver_state; ///< Coarse health state.
    MAX31865Error last_error;        ///< Last tracked error.
    bool online;                     ///< True when driver_state is READY or DEGRADED.
    bool converting;                 ///< True when a conversion is currently active.
    bool auto_convert;               ///< True when continuous conversion mode is enabled.
    uint8_t last_fault_status;       ///< Last decoded fault-status register.
    uint8_t offline_threshold;       ///< Failure threshold for OFFLINE state.
    uint8_t consecutive_failures;    ///< Consecutive tracked failures.
    uint32_t total_success;          ///< Tracked successful SPI/driver operations.
    uint32_t total_failures;         ///< Tracked failed SPI/driver operations.
    uint32_t last_ok_ms;             ///< millis() timestamp of last tracked success.
    uint32_t last_error_ms;          ///< millis() timestamp of last tracked failure.
    uint32_t total_read_count;       ///< Sample read attempts.
    uint32_t kept_sample_count;      ///< Samples successfully converted and cached.
    size_t dropped_count;            ///< Failed sample reads.
    size_t overrun_count;            ///< Cached sample overwritten before application read.
    size_t buffer_depth;             ///< 0 or 1 for this one-sample cache.
    size_t buffer_capacity;          ///< Always 1 for this driver.
    size_t queue_high_water;         ///< Maximum observed one-sample cache depth.
    uint32_t spi_error_count;        ///< SPI transaction failures.
    uint32_t crc_error_count;        ///< Reserved for shared health-view parity; always zero.
    uint32_t status_reset_count;     ///< Reserved for shared health-view parity; always zero.
    uint32_t pga_low_alarm_count;    ///< Reserved for shared health-view parity; always zero.
    uint32_t pga_high_alarm_count;   ///< Reserved for shared health-view parity; always zero.
    uint32_t reference_alarm_count;  ///< Count of decoded reference fault observations.
    uint32_t drdy_timeout_count;     ///< Conversion/fault-cycle timeout count.
    uint32_t missed_drdy_count;      ///< Reserved for DRDY interrupt ports; always zero today.
    uint32_t spi_lock_timeout_count; ///< SPI mutex acquisition timeout count.
    uint32_t task_timeout_count;     ///< Reserved for task-based ports; always zero today.
    uint32_t last_sample_timestamp_us; ///< Last sample timestamp converted to microseconds.
    uint32_t last_sample_age_us;     ///< Age of last sample in microseconds.
} MAX31865Health;

/**
 * @brief Convert a detailed lifecycle state to a static string.
 */
const char* max31865StateName(MAX31865State state);

/**
 * @brief Convert a coarse health state to a static string.
 */
const char* max31865DriverStateName(MAX31865DriverState state);

/**
 * @brief Convert an error code to a static string.
 */
const char* max31865ErrorName(MAX31865Error error);
