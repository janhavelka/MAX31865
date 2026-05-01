/**
 * @file MAX31865.h
 * @brief Arduino/ESP32 driver for the Maxim MAX31865 RTD-to-digital converter.
 *
 * The MAX31865 class owns SPI transactions, /CS framing, register access,
 * conversion timing, RTD scaling, threshold programming, fault-cycle commands,
 * and low-level health diagnostics. Application code should use this public
 * API instead of issuing raw MAX31865 register transactions directly except
 * through the explicit register diagnostic helpers.
 */

#ifndef MAX31865_H_
#define MAX31865_H_

#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stddef.h>
#include <stdint.h>

#include "MAX31865/Version.h"
#include "MAX31865/max31865_driver.h"

#ifndef MAX31865_DEFAULT_SPI_HZ
/// Default SPI clock used when MAX31865BeginConfig::spiHz is zero.
#define MAX31865_DEFAULT_SPI_HZ 1000000U
#endif

#ifndef MAX31865_DEFAULT_OFFLINE_THRESHOLD
/// Consecutive tracked failures before healthState() becomes OFFLINE.
#define MAX31865_DEFAULT_OFFLINE_THRESHOLD 5
#endif

/**
 * @brief Default timeout for serializing SPI transactions on ESP32.
 */
#ifndef MAX31865_SPI_LOCK_TIMEOUT_MS
#define MAX31865_SPI_LOCK_TIMEOUT_MS 50
#endif

/**
 * @defgroup max31865_driver MAX31865 ESP32 Driver
 * @brief Public Arduino API for MAX31865 configuration, acquisition, conversion, and diagnostics.
 * @{
 */

/**
 * @brief RTD wiring compensation mode.
 *
 * The MAX31865 register has one explicit bit for 3-wire mode. Two-wire and
 * four-wire operation both use the cleared register bit; the driver keeps the
 * requested 2-wire/4-wire distinction for diagnostics and UI reporting.
 */
enum class MAX31865WireMode : uint8_t {
    TwoWire = 2,   ///< 2-wire RTD connection; application compensates lead resistance.
    ThreeWire = 3, ///< 3-wire lead compensation enabled in the MAX31865.
    FourWire = 4   ///< 4-wire Kelvin RTD connection.
};

/**
 * @brief Digital notch-filter selection.
 */
enum class MAX31865Filter : uint8_t {
    Hz60 = 0, ///< 60 Hz rejection, shorter conversion time.
    Hz50 = 1  ///< 50 Hz rejection, longer conversion time.
};

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
 * @brief Physical MCU pin assignment for one MAX31865 device.
 */
typedef struct MAX31865Pins {
    int sck;  ///< SPI SCLK pin.
    int miso; ///< SPI MISO pin connected to MAX31865 SDO.
    int mosi; ///< SPI MOSI pin connected to MAX31865 SDI.
    int cs;   ///< MAX31865 /CS pin. Required.
    int drdy; ///< Optional MAX31865 /DRDY pin, or -1 when not wired.
} MAX31865Pins;

/**
 * @brief Callendar-Van Dusen coefficients used by resistanceToTemperature().
 */
typedef struct MAX31865RtdCoefficients {
    float a; ///< Linear coefficient, normally 3.90830e-3 for IEC 60751 platinum.
    float b; ///< Quadratic coefficient, normally -5.77500e-7.
    float c; ///< Negative-temperature coefficient, normally -4.18301e-12.
} MAX31865RtdCoefficients;

/**
 * @brief begin() configuration bundle.
 */
typedef struct MAX31865BeginConfig {
    SPIClass* spi;                   ///< SPI bus object. Required.
    MAX31865Pins pins;               ///< Physical wiring.
    uint32_t spiHz;                  ///< SPI clock in Hz. Zero selects MAX31865_DEFAULT_SPI_HZ.
    bool verifyProbe;                ///< Run probe() after initial register configuration.
    float referenceResistorOhms;     ///< Precision reference resistor value used by the board.
    float rtdNominalOhms;            ///< Nominal RTD value at 0 C, for example 100 or 1000.
    uint32_t inputFilterTimeConstantUs; ///< External input-filter RC time constant in microseconds.
    MAX31865WireMode wireMode;       ///< RTD wiring mode.
    MAX31865Filter filter;           ///< Digital notch-filter selection.
    bool useCustomCoefficients;      ///< Use coefficients instead of IEC 60751 defaults when true.
    MAX31865RtdCoefficients coefficients; ///< Optional custom RTD coefficients.
} MAX31865BeginConfig;

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
 * @brief Fault thresholds expressed as 15-bit RTD ADC codes.
 */
typedef struct MAX31865FaultThresholds {
    uint16_t low_code;  ///< Low fault threshold, 0..32767.
    uint16_t high_code; ///< High fault threshold, 0..32767.
} MAX31865FaultThresholds;

/**
 * @brief Snapshot of current configuration decoded from registers and cached settings.
 */
typedef struct MAX31865Settings {
    uint8_t config_register;                ///< Raw CONFIG register.
    MAX31865WireMode wire_mode;             ///< Decoded/cached wire mode.
    MAX31865Filter filter;                  ///< Decoded filter mode.
    bool bias_enabled;                      ///< VBIAS bit state.
    bool auto_convert;                      ///< Auto-conversion bit state.
    bool one_shot;                          ///< One-shot bit state at snapshot time.
    uint8_t fault_cycle;                    ///< Raw fault-cycle field from CONFIG[3:2].
    MAX31865FaultThresholds thresholds;     ///< Decoded threshold ADC codes.
    float low_threshold_ohms;               ///< Low threshold converted to ohms.
    float high_threshold_ohms;              ///< High threshold converted to ohms.
    float low_threshold_c;                  ///< Low threshold converted to Celsius.
    float high_threshold_c;                 ///< High threshold converted to Celsius.
} MAX31865Settings;

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

/**
 * @brief MAX31865 device driver.
 */
class MAX31865 {
public:
    /// Construct an uninitialized driver object.
    MAX31865();

    /// End the driver and release internal resources.
    ~MAX31865();

    /**
     * @brief Initialize SPI, GPIO, cached scaling settings, and device registers.
     * @param config Typed begin configuration.
     * @return true when the driver is ready for commands.
     */
    bool begin(const MAX31865BeginConfig& config);

    /**
     * @brief Compact begin overload using default PT100/400-ohm scaling.
     * @param spi SPI bus object.
     * @param sckPin SPI SCLK pin.
     * @param misoPin SPI MISO pin connected to MAX31865 SDO.
     * @param mosiPin SPI MOSI pin connected to MAX31865 SDI.
     * @param csPin MAX31865 /CS pin.
     * @param drdyPin Optional /DRDY pin, or -1 when not wired.
     * @param spiHz SPI clock in Hz; zero selects the default.
     * @return true when begin() and probe verification succeed.
     */
    bool begin(SPIClass& spi,
               int sckPin,
               int misoPin,
               int mosiPin,
               int csPin,
               int drdyPin = -1,
               uint32_t spiHz = MAX31865_DEFAULT_SPI_HZ);

    /// Disable conversion, release the SPI mutex, and return to Uninitialized state.
    void end();

    /**
     * @brief Service cached conversion-ready state from an application loop.
     * @param nowMs Current millis() value supplied by the caller.
     */
    void tick(uint32_t nowMs);

    /// Detailed lifecycle state.
    MAX31865State state() const { return _state; }
    /// Coarse health state.
    MAX31865DriverState driverState() const { return _driverState; }
    /// Alias for driverState(), matching the unified example contract.
    MAX31865DriverState healthState() const { return _driverState; }
    /// True when the driver is READY or DEGRADED.
    bool isOnline() const;
    /// Last tracked error code.
    MAX31865Error lastError() const { return _lastError; }
    /// Last tracked error code as a static string.
    const char* lastErrorName() const { return max31865ErrorName(_lastError); }
    /// Last tracked operation as a MAX31865Status object.
    MAX31865Status lastOperationStatus() const;
    /// millis() timestamp of last tracked success.
    uint32_t lastOkMs() const { return _lastOkMs; }
    /// millis() timestamp of last tracked failure.
    uint32_t lastErrorMs() const { return _lastErrorMs; }
    /// Consecutive tracked failures.
    uint8_t consecutiveFailures() const { return _consecutiveFailures; }
    /// Total tracked failures.
    uint32_t totalFailures() const { return _totalFailures; }
    /// Total tracked successes.
    uint32_t totalSuccess() const { return _totalSuccess; }
    /// Configure failures required before the health state becomes OFFLINE.
    void setOfflineThreshold(uint8_t threshold);
    /// Current OFFLINE threshold.
    uint8_t offlineThreshold() const { return _offlineThreshold; }
    /// Current SPI lock timeout in milliseconds.
    uint32_t spiLockTimeoutMs() const { return _spiLockTimeoutMs; }
    /// Set SPI lock timeout in milliseconds; zero selects the default.
    void setSpiLockTimeoutMs(uint32_t timeoutMs);
    /// Snapshot detailed health and counters.
    MAX31865Health health() const;
    /// Reset health counters without changing device configuration.
    void clearHealthCounters();
    /// Probe the configuration register without changing health counters.
    MAX31865Status probe();
    /// Re-apply cached configuration after a recoverable bus/device fault.
    MAX31865Status recover();

    /// Set SPI clock in Hz; zero selects MAX31865_DEFAULT_SPI_HZ.
    void setSpiHz(uint32_t spiHz);
    /// Current SPI clock in Hz.
    uint32_t spiHz() const { return _spiHz; }
    /// Cached RTD wiring mode.
    MAX31865WireMode wireMode() const { return _wireMode; }
    /// Cached filter mode.
    MAX31865Filter filter() const { return _filter; }
    /// Cached VBIAS state.
    bool biasEnabled() const { return _biasEnabled; }
    /// Cached continuous-conversion state.
    bool autoConvertEnabled() const { return _autoConvert; }
    /// Configured precision reference resistor value in ohms.
    float referenceResistorOhms() const { return _referenceResistorOhms; }
    /// Configured RTD nominal value at 0 C.
    float rtdNominalOhms() const { return _rtdNominalOhms; }
    /// Configured input-filter RC time constant in microseconds.
    uint32_t inputFilterTimeConstantUs() const { return _inputFilterTimeConstantUs; }
    /// Current Callendar-Van Dusen coefficients.
    MAX31865RtdCoefficients rtdCoefficients() const { return _coefficients; }
    /**
     * @brief Update RTD scaling parameters and optional coefficients.
     * @param referenceResistorOhms Precision reference resistor in ohms.
     * @param rtdNominalOhms RTD nominal resistance at 0 C.
     * @param coefficients Optional Callendar-Van Dusen coefficients. When null,
     * the existing coefficients are kept.
     * @return true when parameters are finite and inside supported limits.
     *
     * This only changes conversion math. It does not write MAX31865 registers.
     */
    bool setRtdParameters(float referenceResistorOhms,
                          float rtdNominalOhms,
                          const MAX31865RtdCoefficients* coefficients = nullptr);

    /// Enable or disable VBIAS.
    bool setBias(bool enable);
    /// Enable or disable continuous conversion mode.
    bool setAutoConvert(bool enable);
    /// Enable continuous conversion mode.
    bool startContinuous() { return setAutoConvert(true); }
    /// Stop continuous conversion mode.
    bool stop();
    /// Set RTD wiring mode and write the CONFIG register.
    bool setWireMode(MAX31865WireMode mode);
    /// Set notch-filter mode and write the CONFIG register.
    bool setFilter(MAX31865Filter filter);
    /// Atomically configure wiring/filter and optional continuous conversion.
    bool configureMeasurement(MAX31865WireMode wireMode, MAX31865Filter filter, bool autoConvert);

    /**
     * @brief Perform a blocking one-shot conversion and convert the sample.
     * @param out Converted sample output.
     * @param timeoutMs Maximum wait for conversion readiness.
     * @return true when a non-faulted sample was read and converted.
     *
     * The method clears latched faults, enables VBIAS, waits the configured
     * input-filter settle time, starts one-shot conversion, and optionally turns
     * VBIAS back off. If the RTD fault bit is set, it decodes the fault register
     * and returns false with lastError() set to FaultPresent.
     */
    bool readSingle(MAX31865Sample& out, uint32_t timeoutMs = 200);
    /**
     * @brief Nonblocking sample read when conversion data is ready.
     * @return true when a sample was read. A simple not-ready condition returns
     * false without changing the health counters.
     */
    bool poll(MAX31865Sample& out);
    /// True when DRDY, elapsed timing, or the one-sample cache indicates data is ready.
    bool available() const;
    /// Alias for available() with clearer production-call-site wording.
    bool isDataReady() const { return available(); }
    /**
     * @brief Return a status code instead of changing diagnostics when data is not ready.
     * @param out Converted sample output.
     * @return Ok on sample read, ConversionNotReady when data is not ready, or
     * the last operation status on a real read failure.
     */
    MAX31865Status readIfReady(MAX31865Sample& out);
    /// Read and convert the current RTD registers.
    bool readSample(MAX31865Sample& out);
    /// Number of failed sample reads.
    size_t droppedCount() const { return _droppedCount; }
    /// Number of cached samples overwritten before application read.
    size_t overrunCount() const { return _overrunCount; }
    /// Number of sample read attempts.
    uint32_t totalReadCount() const { return _totalReadCount; }
    /// Number of samples successfully converted and cached.
    uint32_t keptSampleCount() const { return _keptSampleCount; }

    /// Read the raw RTD register pair.
    bool readRawRtd(MAX31865RawRtd& out);
    /// Read resistance from an already-ready conversion.
    bool readResistance(float& ohms);
    /// Read temperature from an already-ready conversion.
    bool readTemperature(float& celsius);
    /// Read and decode the fault-status register.
    bool readFaultStatus(MAX31865FaultStatus& out);
    /// Clear the latched fault-status register.
    bool clearFaults();
    /**
     * @brief Run the MAX31865 automatic fault-detection cycle.
     * @param out Decoded fault status.
     * @param timeoutMs Maximum wait for the fault-cycle field to clear.
     * @return true when the cycle completed and the fault register was read.
     *
     * Continuous conversion must be stopped before calling this method. The
     * method leaves VBIAS enabled and conversion normally off, matching the
     * device fault-detection sequence.
     */
    bool runAutoFaultDetection(MAX31865FaultStatus& out, uint32_t timeoutMs = 10);
    /**
     * @brief Run the MAX31865 manual two-step fault-detection cycle.
     * @param out Decoded fault status.
     * @param settleDelayUs External-settling delay inserted before the first
     * manual fault step and again in the inter-step delay.
     * @param timeoutMs Maximum wait for the fault-cycle field to clear.
     * @return true when the cycle completed and the fault register was read.
     */
    bool runManualFaultDetection(MAX31865FaultStatus& out,
                                 uint32_t settleDelayUs,
                                 uint32_t timeoutMs = 10);

    /// Program low/high thresholds as 15-bit RTD ADC codes.
    bool setFaultThresholdsRaw(uint16_t lowCode, uint16_t highCode);
    /// Read low/high thresholds as 15-bit RTD ADC codes.
    bool getFaultThresholdsRaw(MAX31865FaultThresholds& out);
    /// Program low/high thresholds in ohms.
    bool setFaultThresholdsResistance(float lowOhms, float highOhms);
    /// Read low/high thresholds in ohms.
    bool getFaultThresholdsResistance(float& lowOhms, float& highOhms);
    /// Program low/high thresholds in Celsius.
    bool setFaultThresholdsTemperature(float lowC, float highC);

    /// Read one register for diagnostics, returning false on invalid address or bus failure.
    bool readReg(uint8_t addr, uint8_t& value);
    /// Read one register for diagnostics; returns 0xFF on failure for compact sketches.
    uint8_t readReg(uint8_t addr);
    /// Read a contiguous register range for diagnostics.
    bool readRegs(uint8_t startAddr, uint8_t* out, size_t len);
    /// Write one writable register for diagnostics.
    bool writeReg(uint8_t addr, uint8_t value);
    /**
     * @brief Write and verify one writable register for diagnostics.
     * @param addr Register address.
     * @param value Value to write.
     * @param readBack Optional readback output.
     * @return true when the writable, non-self-clearing bits match readback.
     *
     * CONFIG self-clearing command bits are masked from the comparison.
     */
    bool writeRegVerify(uint8_t addr, uint8_t value, uint8_t* readBack = nullptr);
    /// Dump the documented register map.
    size_t dumpRegisters(MAX31865RegisterDump* out, size_t max);
    /**
     * @brief Read and decode the current device settings.
     * @param out Register-derived settings snapshot.
     * @return true when all documented registers were read.
     */
    bool getSettings(MAX31865Settings& out);
    /**
     * @brief Restore CONFIG and threshold registers to documented power-on defaults.
     * @return true when every writable reset value was written.
     */
    bool resetRegisters();
    /**
     * @brief Safe write/readback test using a threshold register, then restoring it.
     * @param readBack Optional observed test-pattern readback.
     * @return true when the test pattern was verified and the original value restored.
     */
    bool registerReadbackTest(uint8_t* readBack = nullptr);

    /// Convert a 15-bit RTD ADC code to resistance.
    float codeToResistance(uint16_t code) const;
    /// Convert resistance to the nearest 15-bit RTD ADC code.
    uint16_t resistanceToCode(float resistanceOhms) const;
    /// Convert resistance to Celsius using Callendar-Van Dusen equations.
    float resistanceToTemperature(float resistanceOhms) const;
    /// Convert Celsius to RTD resistance.
    float temperatureToResistance(float temperatureC) const;
    /// Convert Celsius to the nearest 15-bit RTD ADC code.
    uint16_t temperatureToCode(float temperatureC) const;
    /// Datasheet conversion time for the current filter mode.
    uint32_t getSingleConversionTimeMs() const;
    /// Datasheet continuous-conversion cadence for the current filter mode.
    uint32_t getContinuousConversionTimeMs() const;
    /// Bias-settle delay derived from inputFilterTimeConstantUs.
    uint32_t getBiasSettleTimeUs() const;

    /// Convert a 15-bit RTD ADC code to full-scale ratio.
    static float codeToRatio(uint16_t code);
    /// Decode the raw fault-status register; return true when any fault bit is set.
    static bool decodeFaultStatus(uint8_t raw, MAX31865FaultStatus& out);

private:
    bool applyConfig();
    bool readRegister(uint8_t addr, uint8_t& value);
    bool readRegisterNoHealth(uint8_t addr, uint8_t& value);
    bool writeRegister(uint8_t addr, uint8_t value);
    bool writeRegisterNoHealth(uint8_t addr, uint8_t value);
    bool transfer(const uint8_t* tx, uint8_t* rx, size_t len);
    bool transferRaw(const uint8_t* tx, uint8_t* rx, size_t len, bool recordHealth);
    bool lockSpi(bool recordHealth);
    void unlockSpi();
    bool waitForFaultCycleDone(uint32_t timeoutMs);
    bool conversionReady();
    bool cacheSample(MAX31865Sample& sample);
    void setState(MAX31865State state);
    void setFault(MAX31865Error error);
    void setLastError(MAX31865Error error);
    void recordOk();
    void recordFailure(MAX31865Error error);
    bool isInitialized() const { return _initialized; }
    uint32_t nowMs() const;
    void delayMs(uint32_t ms) const;
    void delayUs(uint32_t us) const;

    SPIClass* _spi;
    SPISettings _spiSettings;
    SemaphoreHandle_t _spiMutex;
    uint32_t _spiHz;
    uint32_t _spiLockTimeoutMs;
    int _csPin;
    int _drdyPin;

    bool _initialized;
    MAX31865State _state;
    MAX31865DriverState _driverState;
    MAX31865Error _lastError;
    uint8_t _offlineThreshold;
    uint8_t _consecutiveFailures;
    uint32_t _totalFailures;
    uint32_t _totalSuccess;
    uint32_t _lastOkMs;
    uint32_t _lastErrorMs;

    float _referenceResistorOhms;
    float _rtdNominalOhms;
    MAX31865RtdCoefficients _coefficients;
    uint32_t _inputFilterTimeConstantUs;
    MAX31865WireMode _wireMode;
    MAX31865Filter _filter;
    bool _biasEnabled;
    bool _autoConvert;
    bool _clearFaultsBeforeOneShot;
    bool _disableBiasAfterOneShot;

    bool _conversionStarted;
    bool _autoFirstConversionPending;
    bool _sampleAvailable;
    uint32_t _conversionStartMs;
    uint32_t _sampleCounter;
    uint32_t _lastSampleTimestampMs;
    MAX31865Sample _lastSample;
    bool _lastSampleValid;

    uint32_t _totalReadCount;
    uint32_t _keptSampleCount;
    size_t _droppedCount;
    size_t _overrunCount;
    size_t _queueHighWater;
    uint32_t _spiErrorCount;
    uint32_t _drdyTimeoutCount;
    uint32_t _spiLockTimeoutCount;
    uint32_t _referenceAlarmCount;
    uint8_t _lastFaultStatus;
};

/** @} */

#endif  // MAX31865_H_
