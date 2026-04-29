/**
 * @file MAX31865.h
 * @brief Arduino/ESP32 driver for the Maxim MAX31865 RTD-to-digital converter.
 */

#ifndef MAX31865_H_
#define MAX31865_H_

#include <Arduino.h>
#include <SPI.h>
#include <stddef.h>
#include <stdint.h>

#include "MAX31865/Version.h"
#include "MAX31865/max31865_driver.h"

#ifndef MAX31865_DEFAULT_SPI_HZ
#define MAX31865_DEFAULT_SPI_HZ 1000000U
#endif

#ifndef MAX31865_DEFAULT_OFFLINE_THRESHOLD
#define MAX31865_DEFAULT_OFFLINE_THRESHOLD 5
#endif

enum class MAX31865WireMode : uint8_t {
    TwoWire = 2,
    ThreeWire = 3,
    FourWire = 4
};

enum class MAX31865Filter : uint8_t {
    Hz60 = 0,
    Hz50 = 1
};

enum class MAX31865State : uint8_t {
    Uninitialized = 0,
    Ready,
    Configuring,
    Converting,
    Recovering,
    Fault
};

enum class MAX31865DriverState : uint8_t {
    UNINIT = 0,
    READY,
    DEGRADED,
    OFFLINE
};

enum class MAX31865Error : uint8_t {
    Ok = 0,
    NotInitialized,
    InvalidArgument,
    InvalidConfig,
    SpiTransferFailed,
    DeviceNotFound,
    ConversionNotReady,
    Timeout,
    Busy,
    FaultPresent
};

typedef struct MAX31865Status {
    MAX31865Error code;
    const char* msg;
    int32_t detail;

    bool ok() const { return code == MAX31865Error::Ok; }

    static MAX31865Status Ok() {
        MAX31865Status status = {MAX31865Error::Ok, "OK", 0};
        return status;
    }

    static MAX31865Status Error(MAX31865Error code, const char* msg, int32_t detail = 0) {
        MAX31865Status status = {code, msg, detail};
        return status;
    }
} MAX31865Status;

typedef struct MAX31865Pins {
    int sck;
    int miso;
    int mosi;
    int cs;
    int drdy;
} MAX31865Pins;

typedef struct MAX31865RtdCoefficients {
    float a;
    float b;
    float c;
} MAX31865RtdCoefficients;

typedef struct MAX31865BeginConfig {
    SPIClass* spi;
    MAX31865Pins pins;
    uint32_t spiHz;
    bool verifyProbe;
    float referenceResistorOhms;
    float rtdNominalOhms;
    uint32_t inputFilterTimeConstantUs;
    MAX31865WireMode wireMode;
    MAX31865Filter filter;
} MAX31865BeginConfig;

typedef struct MAX31865RawRtd {
    uint16_t raw_register;
    uint16_t code;
    bool fault;
} MAX31865RawRtd;

typedef struct MAX31865FaultStatus {
    uint8_t raw;
    bool high_threshold;
    bool low_threshold;
    bool refin_high;
    bool refin_low;
    bool rtdin_low;
    bool over_under_voltage;

    bool any() const {
        return high_threshold || low_threshold || refin_high || refin_low ||
               rtdin_low || over_under_voltage;
    }
} MAX31865FaultStatus;

typedef struct MAX31865Sample {
    uint32_t timestamp_ms;
    uint32_t sample_counter;
    MAX31865RawRtd raw;
    float resistance_ohms;
    float temperature_c;
    bool has_fault_status;
    MAX31865FaultStatus fault_status;
} MAX31865Sample;

typedef struct MAX31865RegisterDump {
    uint8_t addr;
    const char* name;
    uint8_t value;
} MAX31865RegisterDump;

typedef struct MAX31865FaultThresholds {
    uint16_t low_code;
    uint16_t high_code;
} MAX31865FaultThresholds;

typedef struct MAX31865Health {
    MAX31865State state;
    MAX31865DriverState driver_state;
    MAX31865Error last_error;
    bool online;
    bool converting;
    bool auto_convert;
    uint8_t last_fault_status;
    uint8_t offline_threshold;
    uint8_t consecutive_failures;
    uint32_t total_success;
    uint32_t total_failures;
    uint32_t last_ok_ms;
    uint32_t last_error_ms;
    uint32_t total_read_count;
    uint32_t kept_sample_count;
    size_t dropped_count;
    size_t overrun_count;
    size_t buffer_depth;
    size_t buffer_capacity;
    size_t queue_high_water;
    uint32_t spi_error_count;
    uint32_t crc_error_count;
    uint32_t status_reset_count;
    uint32_t pga_low_alarm_count;
    uint32_t pga_high_alarm_count;
    uint32_t reference_alarm_count;
    uint32_t drdy_timeout_count;
    uint32_t missed_drdy_count;
    uint32_t spi_lock_timeout_count;
    uint32_t task_timeout_count;
    uint32_t last_sample_timestamp_us;
    uint32_t last_sample_age_us;
} MAX31865Health;

const char* max31865StateName(MAX31865State state);
const char* max31865DriverStateName(MAX31865DriverState state);
const char* max31865ErrorName(MAX31865Error error);

class MAX31865 {
public:
    MAX31865();
    ~MAX31865();

    bool begin(const MAX31865BeginConfig& config);
    bool begin(SPIClass& spi,
               int sckPin,
               int misoPin,
               int mosiPin,
               int csPin,
               int drdyPin = -1,
               uint32_t spiHz = MAX31865_DEFAULT_SPI_HZ);
    void end();
    void tick(uint32_t nowMs);

    MAX31865State state() const { return _state; }
    MAX31865DriverState driverState() const { return _driverState; }
    MAX31865DriverState healthState() const { return _driverState; }
    bool isOnline() const;
    MAX31865Error lastError() const { return _lastError; }
    const char* lastErrorName() const { return max31865ErrorName(_lastError); }
    MAX31865Status lastOperationStatus() const;
    uint32_t lastOkMs() const { return _lastOkMs; }
    uint32_t lastErrorMs() const { return _lastErrorMs; }
    uint8_t consecutiveFailures() const { return _consecutiveFailures; }
    uint32_t totalFailures() const { return _totalFailures; }
    uint32_t totalSuccess() const { return _totalSuccess; }
    void setOfflineThreshold(uint8_t threshold);
    uint8_t offlineThreshold() const { return _offlineThreshold; }
    MAX31865Health health() const;
    void clearHealthCounters();
    MAX31865Status probe();
    MAX31865Status recover();

    void setSpiHz(uint32_t spiHz);
    uint32_t spiHz() const { return _spiHz; }

    bool setBias(bool enable);
    bool setAutoConvert(bool enable);
    bool startContinuous() { return setAutoConvert(true); }
    bool stop();
    bool setWireMode(MAX31865WireMode mode);
    bool setFilter(MAX31865Filter filter);
    bool configureMeasurement(MAX31865WireMode wireMode, MAX31865Filter filter, bool autoConvert);

    bool readSingle(MAX31865Sample& out, uint32_t timeoutMs = 200);
    bool poll(MAX31865Sample& out);
    bool available() const { return _sampleAvailable; }
    bool readSample(MAX31865Sample& out);
    size_t droppedCount() const { return _droppedCount; }
    size_t overrunCount() const { return _overrunCount; }
    uint32_t totalReadCount() const { return _totalReadCount; }
    uint32_t keptSampleCount() const { return _keptSampleCount; }

    bool readRawRtd(MAX31865RawRtd& out);
    bool readResistance(float& ohms);
    bool readTemperature(float& celsius);
    bool readFaultStatus(MAX31865FaultStatus& out);
    bool clearFaults();
    bool runAutoFaultDetection(MAX31865FaultStatus& out, uint32_t timeoutMs = 10);
    bool runManualFaultDetection(MAX31865FaultStatus& out,
                                 uint32_t settleDelayUs,
                                 uint32_t timeoutMs = 10);

    bool setFaultThresholdsRaw(uint16_t lowCode, uint16_t highCode);
    bool getFaultThresholdsRaw(MAX31865FaultThresholds& out);
    bool setFaultThresholdsResistance(float lowOhms, float highOhms);
    bool getFaultThresholdsResistance(float& lowOhms, float& highOhms);
    bool setFaultThresholdsTemperature(float lowC, float highC);

    uint8_t readReg(uint8_t addr);
    bool readRegs(uint8_t startAddr, uint8_t* out, size_t len);
    bool writeReg(uint8_t addr, uint8_t value);
    bool writeRegVerify(uint8_t addr, uint8_t value, uint8_t* readBack = nullptr);
    size_t dumpRegisters(MAX31865RegisterDump* out, size_t max);

    float codeToResistance(uint16_t code) const;
    uint16_t resistanceToCode(float resistanceOhms) const;
    float resistanceToTemperature(float resistanceOhms) const;
    float temperatureToResistance(float temperatureC) const;
    uint16_t temperatureToCode(float temperatureC) const;
    uint32_t getSingleConversionTimeMs() const;
    uint32_t getContinuousConversionTimeMs() const;
    uint32_t getBiasSettleTimeUs() const;

    static float codeToRatio(uint16_t code);
    static bool decodeFaultStatus(uint8_t raw, MAX31865FaultStatus& out);

private:
    bool applyConfig();
    bool readRegister(uint8_t addr, uint8_t& value);
    bool writeRegister(uint8_t addr, uint8_t value);
    bool transfer(const uint8_t* tx, uint8_t* rx, size_t len);
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
    uint32_t _spiHz;
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
    uint32_t _spiErrorCount;
    uint32_t _drdyTimeoutCount;
    uint8_t _lastFaultStatus;
};

#endif  // MAX31865_H_
