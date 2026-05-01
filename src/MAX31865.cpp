#include "MAX31865/MAX31865.h"

#include <math.h>
#include <string.h>

namespace {

bool isWritableRegister(uint8_t reg) {
    return reg == max31865_cmd::REG_CONFIG ||
           reg == max31865_cmd::REG_HIGH_FAULT_MSB ||
           reg == max31865_cmd::REG_HIGH_FAULT_LSB ||
           reg == max31865_cmd::REG_LOW_FAULT_MSB ||
           reg == max31865_cmd::REG_LOW_FAULT_LSB;
}

bool validWireMode(MAX31865WireMode mode) {
    return mode == MAX31865WireMode::TwoWire ||
           mode == MAX31865WireMode::ThreeWire ||
           mode == MAX31865WireMode::FourWire;
}

bool validFilter(MAX31865Filter filter) {
    return filter == MAX31865Filter::Hz60 || filter == MAX31865Filter::Hz50;
}

bool validReferenceResistor(float ohms) {
    return isfinite(ohms) &&
           ohms >= max31865_cmd::REFERENCE_RESISTOR_MIN_OHMS &&
           ohms <= max31865_cmd::REFERENCE_RESISTOR_MAX_OHMS;
}

uint32_t normalizedSpiHz(uint32_t spiHz) {
    if (spiHz == 0U) {
        return MAX31865_DEFAULT_SPI_HZ;
    }
    return (spiHz > max31865_cmd::SPI_MAX_HZ) ? max31865_cmd::SPI_MAX_HZ : spiHz;
}

uint8_t verifyMaskForRegister(uint8_t addr) {
    if (addr == max31865_cmd::REG_CONFIG) {
        return max31865_cmd::CONFIG_BIAS |
               max31865_cmd::CONFIG_AUTO |
               max31865_cmd::CONFIG_3WIRE |
               max31865_cmd::CONFIG_FILTER_50HZ;
    }
    return 0xFF;
}

uint16_t clampCode(uint32_t code) {
    return (code > max31865_cmd::ADC_CODE_MAX)
               ? max31865_cmd::ADC_CODE_MAX
               : static_cast<uint16_t>(code);
}

uint8_t buildConfigByte(MAX31865WireMode wireMode,
                        MAX31865Filter filter,
                        bool bias,
                        bool autoConvert) {
    uint8_t value = 0;
    if (bias || autoConvert) {
        value |= max31865_cmd::CONFIG_BIAS;
    }
    if (autoConvert) {
        value |= max31865_cmd::CONFIG_AUTO;
    }
    if (wireMode == MAX31865WireMode::ThreeWire) {
        value |= max31865_cmd::CONFIG_3WIRE;
    }
    if (filter == MAX31865Filter::Hz50) {
        value |= max31865_cmd::CONFIG_FILTER_50HZ;
    }
    return value;
}

}  // namespace

const char* max31865StateName(MAX31865State state) {
    switch (state) {
        case MAX31865State::Uninitialized: return "Uninitialized";
        case MAX31865State::Ready: return "Ready";
        case MAX31865State::Configuring: return "Configuring";
        case MAX31865State::Converting: return "Converting";
        case MAX31865State::Recovering: return "Recovering";
        case MAX31865State::Fault: return "Fault";
        default: return "Unknown";
    }
}

const char* max31865DriverStateName(MAX31865DriverState state) {
    switch (state) {
        case MAX31865DriverState::UNINIT: return "UNINIT";
        case MAX31865DriverState::READY: return "READY";
        case MAX31865DriverState::DEGRADED: return "DEGRADED";
        case MAX31865DriverState::OFFLINE: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

const char* max31865ErrorName(MAX31865Error error) {
    switch (error) {
        case MAX31865Error::Ok: return "Ok";
        case MAX31865Error::NotInitialized: return "NotInitialized";
        case MAX31865Error::InvalidArgument: return "InvalidArgument";
        case MAX31865Error::InvalidConfig: return "InvalidConfig";
        case MAX31865Error::ResourceAllocationFailed: return "ResourceAllocationFailed";
        case MAX31865Error::SpiLockTimeout: return "SpiLockTimeout";
        case MAX31865Error::SpiTransferFailed: return "SpiTransferFailed";
        case MAX31865Error::RegisterVerifyFailed: return "RegisterVerifyFailed";
        case MAX31865Error::DeviceNotFound: return "DeviceNotFound";
        case MAX31865Error::ConversionNotReady: return "ConversionNotReady";
        case MAX31865Error::Timeout: return "Timeout";
        case MAX31865Error::Busy: return "Busy";
        case MAX31865Error::FaultPresent: return "FaultPresent";
        default: return "Unknown";
    }
}

MAX31865::MAX31865()
    : _spi(nullptr),
      _spiSettings(MAX31865_DEFAULT_SPI_HZ, MSBFIRST, SPI_MODE1),
      _spiMutex(nullptr),
      _spiHz(MAX31865_DEFAULT_SPI_HZ),
      _spiLockTimeoutMs(MAX31865_SPI_LOCK_TIMEOUT_MS),
      _csPin(-1),
      _drdyPin(-1),
      _initialized(false),
      _state(MAX31865State::Uninitialized),
      _driverState(MAX31865DriverState::UNINIT),
      _lastError(MAX31865Error::Ok),
      _offlineThreshold(MAX31865_DEFAULT_OFFLINE_THRESHOLD),
      _consecutiveFailures(0),
      _totalFailures(0),
      _totalSuccess(0),
      _lastOkMs(0),
      _lastErrorMs(0),
      _referenceResistorOhms(400.0f),
      _rtdNominalOhms(100.0f),
      _coefficients{3.90830e-3f, -5.77500e-7f, -4.18301e-12f},
      _inputFilterTimeConstantUs(0),
      _wireMode(MAX31865WireMode::FourWire),
      _filter(MAX31865Filter::Hz60),
      _biasEnabled(false),
      _autoConvert(false),
      _clearFaultsBeforeOneShot(true),
      _disableBiasAfterOneShot(true),
      _conversionStarted(false),
      _autoFirstConversionPending(false),
      _sampleAvailable(false),
      _conversionStartMs(0),
      _sampleCounter(0),
      _lastSampleTimestampMs(0),
      _lastSample{},
      _lastSampleValid(false),
      _totalReadCount(0),
      _keptSampleCount(0),
      _droppedCount(0),
      _overrunCount(0),
      _queueHighWater(0),
      _spiErrorCount(0),
      _drdyTimeoutCount(0),
      _spiLockTimeoutCount(0),
      _referenceAlarmCount(0),
      _lastFaultStatus(0) {}

MAX31865::~MAX31865() {
    end();
}

bool MAX31865::begin(const MAX31865BeginConfig& config) {
    if (config.spi == nullptr || config.pins.cs < 0) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    if (!validWireMode(config.wireMode) || !validFilter(config.filter) ||
        !validReferenceResistor(config.referenceResistorOhms) ||
        !isfinite(config.rtdNominalOhms) ||
        config.rtdNominalOhms <= 0.0f) {
        setFault(MAX31865Error::InvalidConfig);
        return false;
    }
    if (config.useCustomCoefficients &&
        (!isfinite(config.coefficients.a) || !isfinite(config.coefficients.b) ||
         !isfinite(config.coefficients.c) || config.coefficients.a == 0.0f ||
         config.coefficients.b == 0.0f)) {
        setFault(MAX31865Error::InvalidConfig);
        return false;
    }

    setState(MAX31865State::Configuring);
    setLastError(MAX31865Error::Ok);

    if (_spiMutex == nullptr) {
        _spiMutex = xSemaphoreCreateMutex();
        if (_spiMutex == nullptr) {
            setFault(MAX31865Error::ResourceAllocationFailed);
            return false;
        }
    }

    _spi = config.spi;
    _spiHz = normalizedSpiHz(config.spiHz);
    _spiSettings = SPISettings(_spiHz, MSBFIRST, SPI_MODE1);
    _csPin = config.pins.cs;
    _drdyPin = config.pins.drdy;
    _referenceResistorOhms = config.referenceResistorOhms;
    _rtdNominalOhms = config.rtdNominalOhms;
    if (config.useCustomCoefficients) {
        _coefficients = config.coefficients;
    } else {
        _coefficients = {3.90830e-3f, -5.77500e-7f, -4.18301e-12f};
    }
    _inputFilterTimeConstantUs = config.inputFilterTimeConstantUs;
    _wireMode = config.wireMode;
    _filter = config.filter;
    _biasEnabled = false;
    _autoConvert = false;
    _autoFirstConversionPending = false;
    _conversionStarted = false;
    _sampleAvailable = false;

    _spi->begin(config.pins.sck, config.pins.miso, config.pins.mosi, config.pins.cs);
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    if (_drdyPin >= 0) {
        pinMode(_drdyPin, INPUT);
    }

    _initialized = true;
    if (!applyConfig()) {
        _initialized = false;
        _driverState = MAX31865DriverState::UNINIT;
        _spi = nullptr;
        if (_spiMutex != nullptr) {
            vSemaphoreDelete(_spiMutex);
            _spiMutex = nullptr;
        }
        setFault(MAX31865Error::SpiTransferFailed);
        return false;
    }
    if (config.verifyProbe) {
        const MAX31865Status probeStatus = probe();
        if (!probeStatus.ok()) {
            _initialized = false;
            _driverState = MAX31865DriverState::UNINIT;
            _spi = nullptr;
            if (_spiMutex != nullptr) {
                vSemaphoreDelete(_spiMutex);
                _spiMutex = nullptr;
            }
            setFault(probeStatus.code);
            return false;
        }
    }

    setState(MAX31865State::Ready);
    _driverState = MAX31865DriverState::READY;
    setLastError(MAX31865Error::Ok);
    return true;
}

bool MAX31865::begin(SPIClass& spi,
                     int sckPin,
                     int misoPin,
                     int mosiPin,
                     int csPin,
                     int drdyPin,
                     uint32_t spiHz) {
    MAX31865BeginConfig config{};
    config.spi = &spi;
    config.pins = {sckPin, misoPin, mosiPin, csPin, drdyPin};
    config.spiHz = spiHz;
    config.verifyProbe = true;
    config.referenceResistorOhms = 400.0f;
    config.rtdNominalOhms = 100.0f;
    config.inputFilterTimeConstantUs = 0;
    config.wireMode = MAX31865WireMode::FourWire;
    config.filter = MAX31865Filter::Hz60;
    return begin(config);
}

void MAX31865::end() {
    if (_initialized) {
        (void)writeRegister(max31865_cmd::REG_CONFIG,
                            buildConfigByte(_wireMode, _filter, false, false));
    }
    _initialized = false;
    _state = MAX31865State::Uninitialized;
    _driverState = MAX31865DriverState::UNINIT;
    _conversionStarted = false;
    _autoFirstConversionPending = false;
    _sampleAvailable = false;
    _biasEnabled = false;
    _autoConvert = false;
    if (_spiMutex != nullptr) {
        vSemaphoreDelete(_spiMutex);
        _spiMutex = nullptr;
    }
}

void MAX31865::tick(uint32_t nowMs) {
    if (!_initialized) {
        return;
    }
    const uint32_t readyMs = (_autoConvert && !_autoFirstConversionPending)
                                 ? getContinuousConversionTimeMs()
                                 : getSingleConversionTimeMs();
    if (_conversionStarted && (nowMs - _conversionStartMs) >= readyMs) {
        _sampleAvailable = true;
    }
}

bool MAX31865::isOnline() const {
    return _driverState == MAX31865DriverState::READY ||
           _driverState == MAX31865DriverState::DEGRADED;
}

MAX31865Status MAX31865::lastOperationStatus() const {
    if (_lastError == MAX31865Error::Ok) {
        return MAX31865Status::Ok();
    }
    return MAX31865Status::Error(_lastError, max31865ErrorName(_lastError));
}

void MAX31865::setOfflineThreshold(uint8_t threshold) {
    _offlineThreshold = (threshold == 0U) ? 1U : threshold;
}

void MAX31865::setSpiLockTimeoutMs(uint32_t timeoutMs) {
    _spiLockTimeoutMs = (timeoutMs == 0U) ? MAX31865_SPI_LOCK_TIMEOUT_MS : timeoutMs;
}

MAX31865Health MAX31865::health() const {
    MAX31865Health out{};
    out.state = _state;
    out.driver_state = _driverState;
    out.last_error = _lastError;
    out.online = isOnline();
    out.converting = _conversionStarted;
    out.auto_convert = _autoConvert;
    out.last_fault_status = _lastFaultStatus;
    out.offline_threshold = _offlineThreshold;
    out.consecutive_failures = _consecutiveFailures;
    out.total_success = _totalSuccess;
    out.total_failures = _totalFailures;
    out.last_ok_ms = _lastOkMs;
    out.last_error_ms = _lastErrorMs;
    out.total_read_count = _totalReadCount;
    out.kept_sample_count = _keptSampleCount;
    out.dropped_count = _droppedCount;
    out.overrun_count = _overrunCount;
    out.buffer_depth = _sampleAvailable ? 1U : 0U;
    out.buffer_capacity = 1U;
    out.queue_high_water = _queueHighWater;
    out.spi_error_count = _spiErrorCount;
    out.reference_alarm_count = _referenceAlarmCount;
    out.drdy_timeout_count = _drdyTimeoutCount;
    out.spi_lock_timeout_count = _spiLockTimeoutCount;
    out.last_sample_timestamp_us = _lastSampleTimestampMs * 1000U;
    out.last_sample_age_us = (_lastSampleTimestampMs == 0U)
                                 ? 0U
                                 : (nowMs() - _lastSampleTimestampMs) * 1000U;
    return out;
}

void MAX31865::clearHealthCounters() {
    _consecutiveFailures = 0;
    _totalFailures = 0;
    _totalSuccess = 0;
    _totalReadCount = 0;
    _keptSampleCount = 0;
    _droppedCount = 0;
    _overrunCount = 0;
    _queueHighWater = 0;
    _spiErrorCount = 0;
    _drdyTimeoutCount = 0;
    _spiLockTimeoutCount = 0;
    _referenceAlarmCount = 0;
    _lastFaultStatus = 0;
}

MAX31865Status MAX31865::probe() {
    if (!_initialized || _spi == nullptr) {
        return MAX31865Status::Error(MAX31865Error::NotInitialized, "Driver not initialized");
    }
    uint8_t config = 0;
    if (!readRegisterNoHealth(max31865_cmd::REG_CONFIG, config)) {
        return MAX31865Status::Error(MAX31865Error::DeviceNotFound,
                                     "MAX31865 not responding");
    }
    if (config == 0xFF) {
        return MAX31865Status::Error(MAX31865Error::DeviceNotFound,
                                     "All-ones config read", config);
    }
    uint8_t saved = 0;
    if (!readRegisterNoHealth(max31865_cmd::REG_LOW_FAULT_LSB, saved)) {
        return MAX31865Status::Error(MAX31865Error::DeviceNotFound,
                                     "Threshold read failed");
    }
    const uint8_t pattern = static_cast<uint8_t>((saved ^ 0xA8U) & 0xFEU);
    if (!writeRegisterNoHealth(max31865_cmd::REG_LOW_FAULT_LSB, pattern)) {
        return MAX31865Status::Error(MAX31865Error::DeviceNotFound,
                                     "Threshold write failed");
    }
    uint8_t verify = 0;
    const bool readBackOk = readRegisterNoHealth(max31865_cmd::REG_LOW_FAULT_LSB, verify);
    const bool restoreOk = writeRegisterNoHealth(max31865_cmd::REG_LOW_FAULT_LSB, saved);
    if (!restoreOk) {
        return MAX31865Status::Error(MAX31865Error::SpiTransferFailed,
                                     "Threshold restore failed",
                                     static_cast<int32_t>(verify));
    }
    if (!readBackOk || verify != pattern) {
        return MAX31865Status::Error(MAX31865Error::DeviceNotFound,
                                     "Threshold readback mismatch",
                                     static_cast<int32_t>(verify));
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::recover() {
    if (!_initialized) {
        return MAX31865Status::Error(MAX31865Error::NotInitialized, "Driver not initialized");
    }
    setState(MAX31865State::Recovering);
    if (!applyConfig()) {
        setFault(MAX31865Error::SpiTransferFailed);
        return lastOperationStatus();
    }
    setState(MAX31865State::Ready);
    return MAX31865Status::Ok();
}

void MAX31865::setSpiHz(uint32_t spiHz) {
    _spiHz = normalizedSpiHz(spiHz);
    _spiSettings = SPISettings(_spiHz, MSBFIRST, SPI_MODE1);
}

bool MAX31865::setRtdParameters(float referenceResistorOhms,
                                float rtdNominalOhms,
                                const MAX31865RtdCoefficients* coefficients) {
    if (!validReferenceResistor(referenceResistorOhms) ||
        !isfinite(rtdNominalOhms) || rtdNominalOhms <= 0.0f) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    if (coefficients != nullptr &&
        (!isfinite(coefficients->a) || !isfinite(coefficients->b) ||
         !isfinite(coefficients->c) || coefficients->a == 0.0f ||
         coefficients->b == 0.0f)) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    _referenceResistorOhms = referenceResistorOhms;
    _rtdNominalOhms = rtdNominalOhms;
    if (coefficients != nullptr) {
        _coefficients = *coefficients;
    }
    setLastError(MAX31865Error::Ok);
    return true;
}

bool MAX31865::setBias(bool enable) {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if (_autoConvert && !enable) {
        setFault(MAX31865Error::Busy);
        return false;
    }
    uint8_t config = buildConfigByte(_wireMode, _filter, enable, _autoConvert);
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    _biasEnabled = enable;
    return true;
}

bool MAX31865::setAutoConvert(bool enable) {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if (enable && !_biasEnabled) {
        uint8_t biasOnly = buildConfigByte(_wireMode, _filter, true, false);
        if (!writeRegister(max31865_cmd::REG_CONFIG, biasOnly)) {
            return false;
        }
        _biasEnabled = true;
        delayUs(getBiasSettleTimeUs());
    }
    uint8_t config = buildConfigByte(_wireMode, _filter, enable || _biasEnabled, enable);
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    _autoConvert = enable;
    if (enable) {
        _biasEnabled = true;
        _conversionStarted = true;
        _autoFirstConversionPending = true;
        _sampleAvailable = false;
        _conversionStartMs = nowMs();
        setState(MAX31865State::Converting);
    } else {
        _conversionStarted = false;
        _autoFirstConversionPending = false;
        _sampleAvailable = false;
        setState(MAX31865State::Ready);
    }
    return true;
}

bool MAX31865::stop() {
    return setAutoConvert(false);
}

bool MAX31865::setWireMode(MAX31865WireMode mode) {
    if (!validWireMode(mode)) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if (_autoConvert) {
        setFault(MAX31865Error::Busy);
        return false;
    }
    const uint8_t config = buildConfigByte(mode, _filter, _biasEnabled, false);
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    _wireMode = mode;
    return true;
}

bool MAX31865::setFilter(MAX31865Filter filter) {
    if (!validFilter(filter)) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if (_autoConvert) {
        setFault(MAX31865Error::Busy);
        return false;
    }
    const uint8_t config = buildConfigByte(_wireMode, filter, _biasEnabled, false);
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    _filter = filter;
    return true;
}

bool MAX31865::configureMeasurement(MAX31865WireMode wireMode,
                                    MAX31865Filter filter,
                                    bool autoConvert) {
    if (!validWireMode(wireMode) || !validFilter(filter)) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    const bool wasAuto = _autoConvert;
    if (wasAuto && !setAutoConvert(false)) {
        return false;
    }
    const uint8_t config = buildConfigByte(wireMode, filter, _biasEnabled, false);
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    _wireMode = wireMode;
    _filter = filter;
    return setAutoConvert(autoConvert);
}

bool MAX31865::readSingle(MAX31865Sample& out, uint32_t timeoutMs) {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if (_conversionStarted) {
        setFault(MAX31865Error::Busy);
        return false;
    }
    if (_clearFaultsBeforeOneShot && !clearFaults()) {
        return false;
    }
    if (!setBias(true)) {
        return false;
    }
    delayUs(getBiasSettleTimeUs());

    uint8_t config = buildConfigByte(_wireMode, _filter, true, false);
    config |= max31865_cmd::CONFIG_ONE_SHOT;
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        if (_disableBiasAfterOneShot && !_autoConvert) {
            (void)setBias(false);
        }
        return false;
    }

    _conversionStarted = true;
    _sampleAvailable = false;
    _conversionStartMs = nowMs();
    setState(MAX31865State::Converting);

    const uint32_t start = nowMs();
    while ((nowMs() - start) <= timeoutMs) {
        if (conversionReady()) {
            bool ok = readSample(out);
            if (_disableBiasAfterOneShot && !_autoConvert) {
                (void)setBias(false);
            }
            return ok;
        }
        yield();
    }

    _conversionStarted = false;
    _autoFirstConversionPending = false;
    _sampleAvailable = false;
    recordFailure(MAX31865Error::Timeout);
    setState(MAX31865State::Ready);
    if (_disableBiasAfterOneShot && !_autoConvert) {
        (void)setBias(false);
    }
    return false;
}

bool MAX31865::poll(MAX31865Sample& out) {
    if (!conversionReady()) {
        return false;
    }
    return readSample(out);
}

MAX31865Status MAX31865::readIfReady(MAX31865Sample& out) {
    if (!_initialized) {
        return MAX31865Status::Error(MAX31865Error::NotInitialized, "Driver not initialized");
    }
    if (!conversionReady()) {
        return MAX31865Status::Error(MAX31865Error::ConversionNotReady, "Conversion not ready");
    }
    return readSample(out)
               ? MAX31865Status::Ok()
               : lastOperationStatus();
}

bool MAX31865::readSample(MAX31865Sample& out) {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if ((_conversionStarted || _autoConvert) && !conversionReady()) {
        setLastError(MAX31865Error::ConversionNotReady);
        return false;
    }
    _totalReadCount++;

    MAX31865RawRtd raw{};
    if (!readRawRtd(raw)) {
        _droppedCount++;
        return false;
    }
    out.timestamp_ms = nowMs();
    out.sample_counter = ++_sampleCounter;
    out.raw = raw;
    out.resistance_ohms = codeToResistance(raw.code);
    out.temperature_c = resistanceToTemperature(out.resistance_ohms);
    out.has_fault_status = false;
    out.fault_status = {};
    if (raw.fault) {
        out.has_fault_status = readFaultStatus(out.fault_status);
        _droppedCount++;
        recordFailure(MAX31865Error::FaultPresent);
        _conversionStarted = _autoConvert;
        _autoFirstConversionPending = false;
        _sampleAvailable = false;
        if (_autoConvert) {
            _conversionStartMs = nowMs();
            setState(MAX31865State::Converting);
        } else {
            setState(MAX31865State::Ready);
        }
        return false;
    }

    if (_autoConvert) {
        _conversionStarted = true;
        _autoFirstConversionPending = false;
        _sampleAvailable = false;
        _conversionStartMs = nowMs();
        setState(MAX31865State::Converting);
    } else {
        _conversionStarted = false;
        _autoFirstConversionPending = false;
        _sampleAvailable = false;
        setState(MAX31865State::Ready);
    }
    return cacheSample(out);
}

bool MAX31865::readRawRtd(MAX31865RawRtd& out) {
    uint8_t data[2] = {};
    if (!readRegs(max31865_cmd::REG_RTD_MSB, data, sizeof(data))) {
        return false;
    }
    out.raw_register = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) |
                                             static_cast<uint16_t>(data[1]));
    out.fault = (out.raw_register & max31865_cmd::RTD_FAULT_BIT) != 0;
    out.code = static_cast<uint16_t>(out.raw_register >> 1U);
    return true;
}

bool MAX31865::readResistance(float& ohms) {
    MAX31865Sample sample{};
    if (!readSample(sample)) {
        return false;
    }
    ohms = sample.resistance_ohms;
    return true;
}

bool MAX31865::readTemperature(float& celsius) {
    MAX31865Sample sample{};
    if (!readSample(sample)) {
        return false;
    }
    celsius = sample.temperature_c;
    return true;
}

bool MAX31865::readFaultStatus(MAX31865FaultStatus& out) {
    uint8_t raw = 0;
    if (!readReg(max31865_cmd::REG_FAULT_STATUS, raw)) {
        return false;
    }
    decodeFaultStatus(raw, out);
    _lastFaultStatus = out.raw;
    if (out.refin_high || out.refin_low) {
        if (_referenceAlarmCount < UINT32_MAX) {
            _referenceAlarmCount++;
        }
    }
    return true;
}

bool MAX31865::clearFaults() {
    uint8_t config = 0;
    if (!readRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    config &= static_cast<uint8_t>(~(max31865_cmd::CONFIG_ONE_SHOT |
                                     max31865_cmd::CONFIG_FAULT_CYCLE_MASK |
                                     max31865_cmd::CONFIG_FAULT_CLEAR));
    config |= max31865_cmd::CONFIG_FAULT_CLEAR;
    return writeRegister(max31865_cmd::REG_CONFIG, config);
}

bool MAX31865::runAutoFaultDetection(MAX31865FaultStatus& out, uint32_t timeoutMs) {
    if (_autoConvert || _conversionStarted) {
        setFault(MAX31865Error::Busy);
        return false;
    }
    if (!setBias(true)) {
        return false;
    }
    delayUs(getBiasSettleTimeUs());
    uint8_t config = buildConfigByte(_wireMode, _filter, true, false);
    config |= max31865_cmd::CONFIG_FAULT_CYCLE_AUTO;
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    delayUs(max31865_cmd::AUTO_FAULT_DETECTION_MAX_US);
    _autoConvert = false;
    _conversionStarted = false;
    _autoFirstConversionPending = false;
    _sampleAvailable = false;
    const bool ok = waitForFaultCycleDone(timeoutMs) && readFaultStatus(out);
    delayUs(getBiasSettleTimeUs());
    setState(MAX31865State::Ready);
    return ok;
}

bool MAX31865::runManualFaultDetection(MAX31865FaultStatus& out,
                                       uint32_t settleDelayUs,
                                       uint32_t timeoutMs) {
    if (_autoConvert || _conversionStarted) {
        setFault(MAX31865Error::Busy);
        return false;
    }
    if (!setBias(true)) {
        return false;
    }
    delayUs(getBiasSettleTimeUs());
    delayUs(settleDelayUs);
    uint8_t config = buildConfigByte(_wireMode, _filter, true, false);
    config |= max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1;
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    delayUs(max31865_cmd::MANUAL_FAULT_STEP_SETTLE_US + settleDelayUs);
    config &= static_cast<uint8_t>(~max31865_cmd::CONFIG_FAULT_CYCLE_MASK);
    config |= max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2;
    if (!writeRegister(max31865_cmd::REG_CONFIG, config)) {
        return false;
    }
    delayUs(max31865_cmd::MANUAL_FAULT_STEP_SETTLE_US);
    _autoConvert = false;
    _conversionStarted = false;
    _autoFirstConversionPending = false;
    _sampleAvailable = false;
    const bool ok = waitForFaultCycleDone(timeoutMs) && readFaultStatus(out);
    delayUs(getBiasSettleTimeUs());
    setState(MAX31865State::Ready);
    return ok;
}

bool MAX31865::setFaultThresholdsRaw(uint16_t lowCode, uint16_t highCode) {
    if (lowCode > max31865_cmd::ADC_CODE_MAX ||
        highCode > max31865_cmd::ADC_CODE_MAX ||
        lowCode > highCode) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    uint16_t highReg = static_cast<uint16_t>(highCode << 1U);
    uint16_t lowReg = static_cast<uint16_t>(lowCode << 1U);
    uint8_t data[4] = {
        static_cast<uint8_t>(highReg >> 8U),
        static_cast<uint8_t>(highReg & 0xFEU),
        static_cast<uint8_t>(lowReg >> 8U),
        static_cast<uint8_t>(lowReg & 0xFEU),
    };
    uint8_t tx[5] = {static_cast<uint8_t>(max31865_cmd::REG_HIGH_FAULT_MSB | max31865_cmd::WRITE_BIT),
                     data[0], data[1], data[2], data[3]};
    uint8_t rx[5] = {};
    return transfer(tx, rx, sizeof(tx));
}

bool MAX31865::getFaultThresholdsRaw(MAX31865FaultThresholds& out) {
    uint8_t data[4] = {};
    if (!readRegs(max31865_cmd::REG_HIGH_FAULT_MSB, data, sizeof(data))) {
        return false;
    }
    uint16_t high = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) | data[1]);
    uint16_t low = static_cast<uint16_t>((static_cast<uint16_t>(data[2]) << 8U) | data[3]);
    out.high_code = static_cast<uint16_t>(high >> 1U);
    out.low_code = static_cast<uint16_t>(low >> 1U);
    return true;
}

bool MAX31865::setFaultThresholdsResistance(float lowOhms, float highOhms) {
    if (!isfinite(lowOhms) || !isfinite(highOhms) || lowOhms < 0.0f ||
        highOhms < 0.0f || lowOhms > highOhms) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    return setFaultThresholdsRaw(resistanceToCode(lowOhms), resistanceToCode(highOhms));
}

bool MAX31865::getFaultThresholdsResistance(float& lowOhms, float& highOhms) {
    MAX31865FaultThresholds thresholds{};
    if (!getFaultThresholdsRaw(thresholds)) {
        return false;
    }
    lowOhms = codeToResistance(thresholds.low_code);
    highOhms = codeToResistance(thresholds.high_code);
    return true;
}

bool MAX31865::setFaultThresholdsTemperature(float lowC, float highC) {
    if (!isfinite(lowC) || !isfinite(highC) || lowC > highC) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    return setFaultThresholdsResistance(temperatureToResistance(lowC),
                                        temperatureToResistance(highC));
}

uint8_t MAX31865::readReg(uint8_t addr) {
    uint8_t value = 0;
    if (!readReg(addr, value)) {
        return 0xFF;
    }
    return value;
}

bool MAX31865::readReg(uint8_t addr, uint8_t& value) {
    return readRegister(addr, value);
}

bool MAX31865::readRegs(uint8_t startAddr, uint8_t* out, size_t len) {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if (out == nullptr || len == 0U || startAddr > max31865_cmd::REG_LAST ||
        (static_cast<uint16_t>(startAddr) + static_cast<uint16_t>(len - 1U)) >
            max31865_cmd::REG_LAST) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    uint8_t tx[1 + max31865_cmd::REG_LAST + 1] = {};
    uint8_t rx[1 + max31865_cmd::REG_LAST + 1] = {};
    tx[0] = static_cast<uint8_t>(startAddr & max31865_cmd::READ_MASK);
    memset(&tx[1], 0xFF, len);
    if (!transfer(tx, rx, len + 1U)) {
        return false;
    }
    memcpy(out, &rx[1], len);
    return true;
}

bool MAX31865::writeReg(uint8_t addr, uint8_t value) {
    return writeRegister(addr, value);
}

bool MAX31865::writeRegVerify(uint8_t addr, uint8_t value, uint8_t* readBack) {
    if (!writeReg(addr, value)) {
        return false;
    }
    uint8_t verify = 0;
    if (!readReg(addr, verify)) {
        return false;
    }
    if (readBack != nullptr) {
        *readBack = verify;
    }
    const uint8_t mask = verifyMaskForRegister(addr);
    if ((verify & mask) != (value & mask)) {
        setFault(MAX31865Error::RegisterVerifyFailed);
        return false;
    }
    return true;
}

size_t MAX31865::dumpRegisters(MAX31865RegisterDump* out, size_t max) {
    static const char* names[] = {
        "CONFIG", "RTD_MSB", "RTD_LSB", "HFAULT_MSB",
        "HFAULT_LSB", "LFAULT_MSB", "LFAULT_LSB", "FAULT"
    };
    if (out == nullptr) {
        return 0;
    }
    const size_t count = (max < 8U) ? max : 8U;
    for (size_t i = 0; i < count; ++i) {
        out[i].addr = static_cast<uint8_t>(i);
        out[i].name = names[i];
        out[i].value = readReg(static_cast<uint8_t>(i));
    }
    return count;
}

bool MAX31865::getSettings(MAX31865Settings& out) {
    uint8_t regs[8] = {};
    if (!readRegs(max31865_cmd::REG_CONFIG, regs, sizeof(regs))) {
        return false;
    }

    out.config_register = regs[max31865_cmd::REG_CONFIG];
    out.wire_mode = (out.config_register & max31865_cmd::CONFIG_3WIRE)
                        ? MAX31865WireMode::ThreeWire
                        : _wireMode;
    if (out.wire_mode == MAX31865WireMode::ThreeWire) {
        out.wire_mode = MAX31865WireMode::ThreeWire;
    } else if (_wireMode == MAX31865WireMode::TwoWire) {
        out.wire_mode = MAX31865WireMode::TwoWire;
    } else {
        out.wire_mode = MAX31865WireMode::FourWire;
    }
    out.filter = (out.config_register & max31865_cmd::CONFIG_FILTER_50HZ)
                     ? MAX31865Filter::Hz50
                     : MAX31865Filter::Hz60;
    out.bias_enabled = (out.config_register & max31865_cmd::CONFIG_BIAS) != 0;
    out.auto_convert = (out.config_register & max31865_cmd::CONFIG_AUTO) != 0;
    out.one_shot = (out.config_register & max31865_cmd::CONFIG_ONE_SHOT) != 0;
    out.fault_cycle = static_cast<uint8_t>(out.config_register & max31865_cmd::CONFIG_FAULT_CYCLE_MASK);

    uint16_t high = static_cast<uint16_t>((static_cast<uint16_t>(regs[max31865_cmd::REG_HIGH_FAULT_MSB]) << 8U) |
                                          regs[max31865_cmd::REG_HIGH_FAULT_LSB]);
    uint16_t low = static_cast<uint16_t>((static_cast<uint16_t>(regs[max31865_cmd::REG_LOW_FAULT_MSB]) << 8U) |
                                         regs[max31865_cmd::REG_LOW_FAULT_LSB]);
    out.thresholds.high_code = static_cast<uint16_t>(high >> 1U);
    out.thresholds.low_code = static_cast<uint16_t>(low >> 1U);
    out.low_threshold_ohms = codeToResistance(out.thresholds.low_code);
    out.high_threshold_ohms = codeToResistance(out.thresholds.high_code);
    out.low_threshold_c = resistanceToTemperature(out.low_threshold_ohms);
    out.high_threshold_c = resistanceToTemperature(out.high_threshold_ohms);
    return true;
}

bool MAX31865::resetRegisters() {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    const bool ok =
        writeRegister(max31865_cmd::REG_CONFIG, max31865_cmd::CONFIG_RESET) &&
        writeRegister(max31865_cmd::REG_HIGH_FAULT_MSB, max31865_cmd::HIGH_FAULT_MSB_RESET) &&
        writeRegister(max31865_cmd::REG_HIGH_FAULT_LSB, max31865_cmd::HIGH_FAULT_LSB_RESET) &&
        writeRegister(max31865_cmd::REG_LOW_FAULT_MSB, max31865_cmd::LOW_FAULT_MSB_RESET) &&
        writeRegister(max31865_cmd::REG_LOW_FAULT_LSB, max31865_cmd::LOW_FAULT_LSB_RESET);
    if (ok) {
        _biasEnabled = false;
        _autoConvert = false;
        _conversionStarted = false;
        _autoFirstConversionPending = false;
        _sampleAvailable = false;
        _wireMode = MAX31865WireMode::FourWire;
        _filter = MAX31865Filter::Hz60;
        _lastFaultStatus = 0;
        setState(MAX31865State::Ready);
    }
    return ok;
}

bool MAX31865::registerReadbackTest(uint8_t* readBack) {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    uint8_t saved = 0;
    if (!readReg(max31865_cmd::REG_LOW_FAULT_LSB, saved)) {
        return false;
    }
    const uint8_t pattern = static_cast<uint8_t>((saved ^ 0xA8U) & 0xFEU);
    uint8_t verify = 0;
    const bool writeOk = writeRegVerify(max31865_cmd::REG_LOW_FAULT_LSB, pattern, &verify);
    const MAX31865Error verifyError = _lastError;
    const bool restoreOk = writeRegister(max31865_cmd::REG_LOW_FAULT_LSB, saved);
    if (readBack != nullptr) {
        *readBack = verify;
    }
    if (!writeOk) {
        setFault(verifyError == MAX31865Error::Ok ? MAX31865Error::RegisterVerifyFailed
                                                  : verifyError);
        return false;
    }
    return restoreOk;
}

float MAX31865::codeToResistance(uint16_t code) const {
    return (static_cast<float>(code) * _referenceResistorOhms) /
           static_cast<float>(max31865_cmd::ADC_FULL_SCALE);
}

uint16_t MAX31865::resistanceToCode(float resistanceOhms) const {
    if (!isfinite(resistanceOhms) || resistanceOhms <= 0.0f) {
        return 0;
    }
    double scaled = (static_cast<double>(resistanceOhms) *
                     static_cast<double>(max31865_cmd::ADC_FULL_SCALE)) /
                    static_cast<double>(_referenceResistorOhms);
    return clampCode(static_cast<uint32_t>(scaled + 0.5));
}

float MAX31865::resistanceToTemperature(float resistanceOhms) const {
    if (!isfinite(resistanceOhms) || resistanceOhms <= 0.0f) {
        return NAN;
    }
    const double ratio = static_cast<double>(resistanceOhms) /
                         static_cast<double>(_rtdNominalOhms);
    const double a = _coefficients.a;
    const double b = _coefficients.b;
    const double c = _coefficients.c;
    if (resistanceOhms >= _rtdNominalOhms) {
        const double disc = (a * a) - (4.0 * b * (1.0 - ratio));
        if (disc < 0.0 || b == 0.0) {
            return NAN;
        }
        return static_cast<float>((-a + sqrt(disc)) / (2.0 * b));
    }

    double lo = -200.0;
    double hi = 0.0;
    for (uint8_t i = 0; i < 40; ++i) {
        const double mid = (lo + hi) * 0.5;
        const double r = 1.0 + (a * mid) + (b * mid * mid) +
                         (c * (mid - 100.0) * mid * mid * mid);
        if (r < ratio) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return static_cast<float>((lo + hi) * 0.5);
}

float MAX31865::temperatureToResistance(float temperatureC) const {
    if (!isfinite(temperatureC)) {
        return NAN;
    }
    const double t = temperatureC;
    const double c = (temperatureC < 0.0f) ? _coefficients.c : 0.0;
    const double ratio = 1.0 + (_coefficients.a * t) +
                         (_coefficients.b * t * t) +
                         (c * (t - 100.0) * t * t * t);
    return static_cast<float>(static_cast<double>(_rtdNominalOhms) * ratio);
}

uint16_t MAX31865::temperatureToCode(float temperatureC) const {
    return resistanceToCode(temperatureToResistance(temperatureC));
}

uint32_t MAX31865::getSingleConversionTimeMs() const {
    return (_filter == MAX31865Filter::Hz50)
               ? max31865_cmd::SINGLE_CONVERSION_50HZ_MS
               : max31865_cmd::SINGLE_CONVERSION_60HZ_MS;
}

uint32_t MAX31865::getContinuousConversionTimeMs() const {
    return (_filter == MAX31865Filter::Hz50)
               ? max31865_cmd::CONTINUOUS_CONVERSION_50HZ_MS
               : max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS;
}

uint32_t MAX31865::getBiasSettleTimeUs() const {
    const double scaled = (static_cast<double>(_inputFilterTimeConstantUs) *
                           static_cast<double>(max31865_cmd::BIAS_SETTLE_TIME_CONSTANTS)) +
                          static_cast<double>(max31865_cmd::BIAS_SETTLE_EXTRA_US);
    return static_cast<uint32_t>(scaled + 0.5);
}

float MAX31865::codeToRatio(uint16_t code) {
    return static_cast<float>(code) / static_cast<float>(max31865_cmd::ADC_FULL_SCALE);
}

bool MAX31865::decodeFaultStatus(uint8_t raw, MAX31865FaultStatus& out) {
    out.raw = static_cast<uint8_t>(raw & max31865_cmd::FAULT_DEFINED_MASK);
    out.high_threshold = (raw & max31865_cmd::FAULT_HIGH_THRESHOLD) != 0;
    out.low_threshold = (raw & max31865_cmd::FAULT_LOW_THRESHOLD) != 0;
    out.refin_high = (raw & max31865_cmd::FAULT_REFIN_HIGH) != 0;
    out.refin_low = (raw & max31865_cmd::FAULT_REFIN_LOW) != 0;
    out.rtdin_low = (raw & max31865_cmd::FAULT_RTDIN_LOW) != 0;
    out.over_under_voltage = (raw & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0;
    return out.any();
}

bool MAX31865::applyConfig() {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    return writeRegister(max31865_cmd::REG_CONFIG,
                         buildConfigByte(_wireMode, _filter, _biasEnabled, _autoConvert));
}

bool MAX31865::readRegister(uint8_t addr, uint8_t& value) {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if (addr > max31865_cmd::REG_LAST) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    uint8_t tx[2] = {static_cast<uint8_t>(addr & max31865_cmd::READ_MASK), 0xFF};
    uint8_t rx[2] = {};
    if (!transfer(tx, rx, sizeof(tx))) {
        return false;
    }
    value = rx[1];
    return true;
}

bool MAX31865::readRegisterNoHealth(uint8_t addr, uint8_t& value) {
    if (!_initialized || addr > max31865_cmd::REG_LAST) {
        return false;
    }
    uint8_t tx[2] = {static_cast<uint8_t>(addr & max31865_cmd::READ_MASK), 0xFF};
    uint8_t rx[2] = {};
    if (!transferRaw(tx, rx, sizeof(tx), false)) {
        return false;
    }
    value = rx[1];
    return true;
}

bool MAX31865::writeRegister(uint8_t addr, uint8_t value) {
    if (!_initialized) {
        setFault(MAX31865Error::NotInitialized);
        return false;
    }
    if (!isWritableRegister(addr)) {
        setFault(MAX31865Error::InvalidArgument);
        return false;
    }
    uint8_t tx[2] = {static_cast<uint8_t>(addr | max31865_cmd::WRITE_BIT), value};
    uint8_t rx[2] = {};
    return transfer(tx, rx, sizeof(tx));
}

bool MAX31865::writeRegisterNoHealth(uint8_t addr, uint8_t value) {
    if (!_initialized || !isWritableRegister(addr)) {
        return false;
    }
    uint8_t tx[2] = {static_cast<uint8_t>(addr | max31865_cmd::WRITE_BIT), value};
    uint8_t rx[2] = {};
    return transferRaw(tx, rx, sizeof(tx), false);
}

bool MAX31865::lockSpi(bool recordHealth) {
    if (_spiMutex == nullptr) {
        if (recordHealth) {
            recordFailure(MAX31865Error::NotInitialized);
        }
        return false;
    }
    if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(_spiLockTimeoutMs)) != pdTRUE) {
        if (recordHealth) {
            if (_spiLockTimeoutCount < UINT32_MAX) {
                _spiLockTimeoutCount++;
            }
            recordFailure(MAX31865Error::SpiLockTimeout);
        }
        return false;
    }
    return true;
}

void MAX31865::unlockSpi() {
    if (_spiMutex != nullptr) {
        xSemaphoreGive(_spiMutex);
    }
}

bool MAX31865::transferRaw(const uint8_t* tx, uint8_t* rx, size_t len, bool recordHealth) {
    if (_spi == nullptr || _csPin < 0 || tx == nullptr || rx == nullptr || len == 0U) {
        if (recordHealth) {
            setFault(MAX31865Error::InvalidArgument);
        }
        return false;
    }
    if (!lockSpi(recordHealth)) {
        return false;
    }
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    for (size_t i = 0; i < len; ++i) {
        rx[i] = _spi->transfer(tx[i]);
    }
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
    unlockSpi();
    if (recordHealth) {
        recordOk();
    }
    return true;
}

bool MAX31865::transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    return transferRaw(tx, rx, len, true);
}

bool MAX31865::waitForFaultCycleDone(uint32_t timeoutMs) {
    const uint32_t start = nowMs();
    while ((nowMs() - start) <= timeoutMs) {
        uint8_t config = 0;
        if (!readRegister(max31865_cmd::REG_CONFIG, config)) {
            return false;
        }
        if ((config & max31865_cmd::CONFIG_FAULT_CYCLE_MASK) == 0U) {
            return true;
        }
        yield();
    }
    recordFailure(MAX31865Error::Timeout);
    return false;
}

bool MAX31865::conversionReady() {
    if (_drdyPin >= 0 && digitalRead(_drdyPin) == LOW) {
        return true;
    }
    const uint32_t readyMs = (_autoConvert && !_autoFirstConversionPending)
                                 ? getContinuousConversionTimeMs()
                                 : getSingleConversionTimeMs();
    if (_conversionStarted && (nowMs() - _conversionStartMs) >= readyMs) {
        _sampleAvailable = true;
        return true;
    }
    return _sampleAvailable;
}

bool MAX31865::available() const {
    if (_sampleAvailable) {
        return true;
    }
    if (_drdyPin >= 0 && digitalRead(_drdyPin) == LOW) {
        return true;
    }
    const uint32_t readyMs = (_autoConvert && !_autoFirstConversionPending)
                                 ? getContinuousConversionTimeMs()
                                 : getSingleConversionTimeMs();
    return _conversionStarted && ((nowMs() - _conversionStartMs) >= readyMs);
}

bool MAX31865::cacheSample(MAX31865Sample& sample) {
    if (_lastSampleValid && _sampleAvailable) {
        _overrunCount++;
    }
    _lastSample = sample;
    _lastSampleValid = true;
    _lastSampleTimestampMs = sample.timestamp_ms;
    _keptSampleCount++;
    _queueHighWater = 1U;
    return true;
}

void MAX31865::setState(MAX31865State state) {
    _state = state;
}

void MAX31865::setFault(MAX31865Error error) {
    setState(MAX31865State::Fault);
    recordFailure(error);
}

void MAX31865::setLastError(MAX31865Error error) {
    _lastError = error;
}

void MAX31865::recordOk() {
    _lastOkMs = nowMs();
    _lastError = MAX31865Error::Ok;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
        _totalSuccess++;
    }
    if (_initialized) {
        _driverState = MAX31865DriverState::READY;
    }
}

void MAX31865::recordFailure(MAX31865Error error) {
    _lastError = error;
    _lastErrorMs = nowMs();
    if (_consecutiveFailures < UINT8_MAX) {
        _consecutiveFailures++;
    }
    if (_totalFailures < UINT32_MAX) {
        _totalFailures++;
    }
    if (error == MAX31865Error::SpiTransferFailed) {
        _spiErrorCount++;
    }
    if (error == MAX31865Error::Timeout) {
        if (_drdyTimeoutCount < UINT32_MAX) {
            _drdyTimeoutCount++;
        }
    }
    if (_initialized) {
        _driverState = (_consecutiveFailures >= _offlineThreshold)
                           ? MAX31865DriverState::OFFLINE
                           : MAX31865DriverState::DEGRADED;
    }
}

uint32_t MAX31865::nowMs() const {
    return millis();
}

void MAX31865::delayMs(uint32_t ms) const {
    delay(ms);
}

void MAX31865::delayUs(uint32_t us) const {
    while (us >= 1000U) {
        delayMs(1);
        us -= 1000U;
    }
    if (us > 0U) {
        delayMicroseconds(us);
    }
}
