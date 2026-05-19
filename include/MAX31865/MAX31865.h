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

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stddef.h>
#include <stdint.h>

#include "MAX31865/CommandTable.h"
#include "MAX31865/Config.h"
#include "MAX31865/Status.h"
#include "MAX31865/Version.h"

#if MAX31865_HAS_ARDUINO_BACKEND
#include <Arduino.h>
#endif

/**
 * @defgroup max31865 MAX31865 ESP32 Driver
 * @brief Public Arduino API for MAX31865 configuration, acquisition, conversion, and diagnostics.
 * @{
 */
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
    /// True after begin() succeeds and before end() is called.
    bool isInitialized() const { return _initialized; }
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
     * @param out Converted sample output.
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
    /// @param out Converted sample output.
    /// @return true when RTD registers were read and converted.
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
    /// @param out Raw register decode output.
    /// @return true when both RTD bytes were read.
    bool readRawRtd(MAX31865RawRtd& out);
    /// Read resistance from an already-ready conversion.
    /// @param[out] ohms Converted resistance.
    /// @return true when a sample was read and converted.
    bool readResistance(float& ohms);
    /// Read temperature from an already-ready conversion.
    /// @param[out] celsius Converted temperature.
    /// @return true when a sample was read and converted.
    bool readTemperature(float& celsius);
    /// Read and decode the fault-status register.
    /// @param[out] out Decoded fault status.
    /// @return true when the fault-status register was read.
    bool readFaultStatus(MAX31865FaultStatus& out);
    /// Clear the latched fault-status register.
    /// @return true when the CONFIG fault-clear write succeeds.
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
    /// @param lowCode Low threshold code; must be 0..32767.
    /// @param highCode High threshold code; must be 0..32767.
    /// @return true when all threshold bytes were written.
    bool setFaultThresholdsRaw(uint16_t lowCode, uint16_t highCode);
    /// Read low/high thresholds as 15-bit RTD ADC codes.
    /// @param[out] out Threshold codes.
    /// @return true when all threshold bytes were read.
    bool getFaultThresholdsRaw(MAX31865FaultThresholds& out);
    /// Program low/high thresholds in ohms.
    /// @param lowOhms Low threshold resistance.
    /// @param highOhms High threshold resistance.
    /// @return true when values are finite and the raw thresholds are written.
    bool setFaultThresholdsResistance(float lowOhms, float highOhms);
    /// Read low/high thresholds in ohms.
    /// @param[out] lowOhms Low threshold resistance.
    /// @param[out] highOhms High threshold resistance.
    /// @return true when raw thresholds were read and converted.
    bool getFaultThresholdsResistance(float& lowOhms, float& highOhms);
    /// Program low/high thresholds in Celsius.
    /// @param lowC Low threshold temperature.
    /// @param highC High threshold temperature.
    /// @return true when values are finite and the raw thresholds are written.
    bool setFaultThresholdsTemperature(float lowC, float highC);

    /// Read one register for diagnostics, returning false on invalid address or bus failure.
    /// @param addr Register address.
    /// @param[out] value Register value.
    /// @return true when the register was read.
    bool readReg(uint8_t addr, uint8_t& value);
    /// Read one register for diagnostics; returns 0xFF on failure for compact sketches.
    /// @param addr Register address.
    /// @return Register value, or 0xFF on failure.
    uint8_t readReg(uint8_t addr);
    /// Read a contiguous register range for diagnostics.
    /// @param startAddr First register address.
    /// @param[out] out Destination buffer.
    /// @param len Number of registers to read.
    /// @return true when the whole range was read.
    bool readRegs(uint8_t startAddr, uint8_t* out, size_t len);
    /// Write one writable register for diagnostics.
    /// @param addr Writable register address.
    /// @param value Value to write.
    /// @return true when the register write succeeds.
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
    /// @param[out] out Destination rows.
    /// @param max Capacity of out.
    /// @return Number of rows written.
    size_t dumpRegisters(MAX31865RegisterDump* out, size_t max);
    /**
     * @brief Read and decode the current device settings.
     * @param out Register-derived settings snapshot.
     * @return true when all documented registers were read.
     */
    bool getSettings(MAX31865Settings& out);
    /**
     * @brief Read and decode the current device settings with explicit status.
     * @param out Register-derived settings snapshot.
     * @return Ok when all documented registers were read, otherwise last operation status.
     */
    MAX31865Status getSettingsStatus(MAX31865Settings& out);
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
    /// @param code 15-bit RTD ADC code.
    /// @return code / 32768.0.
    static float codeToRatio(uint16_t code);
    /// Decode the raw fault-status register.
    /// @param raw Raw fault-status register value.
    /// @param[out] out Decoded D7..D2 documented fault bits; D1..D0 are masked out.
    /// @return true when any documented fault bit is set.
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
    void resetBeginRuntimeState();
    bool waitForFaultCycleDone(uint32_t timeoutMs);
    bool conversionReady();
    bool cacheSample(MAX31865Sample& sample);
    void setState(MAX31865State state);
    void setFault(MAX31865Error error);
    void setLastError(MAX31865Error error);
    void recordOk();
    void recordFailure(MAX31865Error error);
    uint32_t nowMs() const;
    void delayMs(uint32_t ms) const;
    void delayUs(uint32_t us) const;
    void yieldForDriver() const;
    bool readDrdyReady() const;

    SPIClass* _spi;
    SemaphoreHandle_t _spiMutex;
    uint32_t _spiHz;
    uint32_t _spiLockTimeoutMs;
    int _csPin;
    int _drdyPin;
    MAX31865TransportConfig _transport;

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
