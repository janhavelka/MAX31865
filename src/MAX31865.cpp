/// @file MAX31865.cpp
/// @brief Implementation of MAX31865 driver.

#include "MAX31865/MAX31865.h"

#include <Arduino.h>
#include <cmath>
#include <cstring>
#include <limits>

namespace MAX31865 {
namespace {

bool isValidWireMode(WireMode mode) {
  return mode == WireMode::TWO_WIRE ||
         mode == WireMode::THREE_WIRE ||
         mode == WireMode::FOUR_WIRE;
}

bool isValidFilter(Filter filter) {
  return filter == Filter::HZ_60 || filter == Filter::HZ_50;
}

bool isValidRegister(uint8_t reg) {
  return reg <= cmd::REG_LAST;
}

bool isWritableRegister(uint8_t reg) {
  return reg == cmd::REG_CONFIG ||
         reg == cmd::REG_HIGH_FAULT_MSB ||
         reg == cmd::REG_HIGH_FAULT_LSB ||
         reg == cmd::REG_LOW_FAULT_MSB ||
         reg == cmd::REG_LOW_FAULT_LSB;
}

bool isWritableBlock(uint8_t startReg, size_t len) {
  if (len == 0 || startReg > cmd::REG_LAST) {
    return false;
  }
  const uint16_t endReg = static_cast<uint16_t>(startReg) +
                          static_cast<uint16_t>(len - 1U);
  if (endReg > cmd::REG_LAST) {
    return false;
  }
  for (uint16_t reg = startReg; reg <= endReg; ++reg) {
    if (!isWritableRegister(static_cast<uint8_t>(reg))) {
      return false;
    }
  }
  return true;
}

uint16_t clampCode(uint32_t code) {
  return (code > cmd::ADC_CODE_MAX) ? cmd::ADC_CODE_MAX
                                    : static_cast<uint16_t>(code);
}

uint32_t elapsedMs(uint32_t nowMs, uint32_t sinceMs) {
  return nowMs - sinceMs;
}

uint8_t buildConfigByte(const Config& config) {
  uint8_t value = 0;
  if (config.enableBias || config.autoConvert) {
    value |= cmd::CONFIG_BIAS;
  }
  if (config.autoConvert) {
    value |= cmd::CONFIG_AUTO;
  }
  if (config.wireMode == WireMode::THREE_WIRE) {
    value |= cmd::CONFIG_3WIRE;
  }
  if (config.filter == Filter::HZ_50) {
    value |= cmd::CONFIG_FILTER_50HZ;
  }
  return value;
}

}  // namespace

// ============================================================================
// Lifecycle
// ============================================================================

Status MAX31865::begin(const Config& config) {
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _conversionStarted = false;
  _sampleAvailable = false;
  _lastSampleValid = false;
  _conversionStartMs = 0;
  _lastSampleTimestampMs = 0;
  _lastSample = Reading{};

  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;

  if (config.spiTransfer == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "SPI transfer callback required");
  }
  if (config.spiTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "SPI timeout must be > 0");
  }
  if (!std::isfinite(config.referenceResistorOhms) ||
      !std::isfinite(config.rtdNominalOhms) ||
      config.referenceResistorOhms <= 0.0f ||
      config.rtdNominalOhms <= 0.0f) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid RTD/reference resistance");
  }
  if (!std::isfinite(config.coefficients.a) ||
      !std::isfinite(config.coefficients.b) ||
      !std::isfinite(config.coefficients.c)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid RTD coefficients");
  }
  if (!isValidWireMode(config.wireMode) || !isValidFilter(config.filter)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid wire mode or filter");
  }

  _config = config;
  if (_config.offlineThreshold == 0) {
    _config.offlineThreshold = 1;
  }

  Status st = _applyCachedConfig();
  if (!st.ok()) {
    _driverState = DriverState::OFFLINE;
    return st;
  }

  _initialized = true;
  _driverState = DriverState::READY;
  if (_config.autoConvert) {
    _conversionStarted = true;
    _conversionStartMs = _nowMs();
  }
  return Status::Ok();
}

void MAX31865::tick(uint32_t nowMs) {
  if (!_initialized) {
    return;
  }

  if (_conversionStarted &&
      elapsedMs(nowMs, _conversionStartMs) >= getSingleConversionTimeMs()) {
    _conversionStarted = false;
    _sampleAvailable = true;
    return;
  }

  if (_config.autoConvert &&
      elapsedMs(nowMs, _conversionStartMs) >= getContinuousConversionTimeMs()) {
    _sampleAvailable = true;
    _conversionStartMs = nowMs;
  }
}

void MAX31865::end() {
  if (_initialized) {
    uint8_t cfg = 0;
    if (_config.wireMode == WireMode::THREE_WIRE) {
      cfg |= cmd::CONFIG_3WIRE;
    }
    if (_config.filter == Filter::HZ_50) {
      cfg |= cmd::CONFIG_FILTER_50HZ;
    }
    (void)_writeRegisterRaw(cmd::REG_CONFIG, cfg);
  }

  _initialized = false;
  _driverState = DriverState::UNINIT;
  _conversionStarted = false;
  _sampleAvailable = false;
  _lastSampleValid = false;
  _config.enableBias = false;
  _config.autoConvert = false;
}

// ============================================================================
// Diagnostics
// ============================================================================

Status MAX31865::probe() {
  uint8_t cfg = 0;
  Status st = _readRegisterRaw(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return Status::Error(Err::DEVICE_NOT_FOUND, "MAX31865 not responding", st.detail);
  }

  // D5 and D1 are self-clearing command bits. Seeing every bit set is a strong
  // indication of a floating MISO line or an invalid read response.
  if (cfg == 0xFF) {
    return Status::Error(Err::DEVICE_NOT_FOUND, "Invalid all-ones configuration read", cfg);
  }
  return Status::Ok();
}

Status MAX31865::recover() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint8_t cfg = 0;
  Status st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }
  if (cfg == 0xFF) {
    return _recordFailure(Status::Error(Err::DEVICE_NOT_FOUND,
                                        "Invalid all-ones configuration read", cfg));
  }
  return _applyCachedConfig();
}

Status MAX31865::getSettings(SettingsSnapshot& out) const {
  out.initialized = _initialized;
  out.state = _driverState;
  out.spiTimeoutMs = _config.spiTimeoutMs;
  out.offlineThreshold = _config.offlineThreshold;
  out.hasNowMsHook = (_config.nowMs != nullptr);
  out.hasDelayMsHook = (_config.delayMs != nullptr);
  out.hasDelayUsHook = (_config.delayUs != nullptr);
  out.hasYieldHook = (_config.cooperativeYield != nullptr);
  out.hasGpioReadHook = (_config.gpioRead != nullptr);
  out.drdyPin = _config.drdyPin;
  out.referenceResistorOhms = _config.referenceResistorOhms;
  out.rtdNominalOhms = _config.rtdNominalOhms;
  out.inputFilterTimeConstantUs = _config.inputFilterTimeConstantUs;
  out.wireMode = _config.wireMode;
  out.filter = _config.filter;
  out.enableBias = _config.enableBias;
  out.autoConvert = _config.autoConvert;
  out.clearFaultsBeforeOneShot = _config.clearFaultsBeforeOneShot;
  out.disableBiasAfterOneShot = _config.disableBiasAfterOneShot;
  out.conversionStarted = _conversionStarted;
  out.sampleAvailable = _sampleAvailable;
  out.conversionStartMs = _conversionStartMs;
  out.lastOkMs = _lastOkMs;
  out.lastErrorMs = _lastErrorMs;
  out.lastError = _lastError;
  out.consecutiveFailures = _consecutiveFailures;
  out.totalFailures = _totalFailures;
  out.totalSuccess = _totalSuccess;
  return Status::Ok();
}

// ============================================================================
// Measurement API
// ============================================================================

Status MAX31865::startOneShot() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  Status st = Status::Ok();
  if (_config.clearFaultsBeforeOneShot) {
    st = clearFaults();
    if (!st.ok()) {
      return st;
    }
  }

  st = setBias(true);
  if (!st.ok()) {
    return st;
  }

  const uint32_t settleUs = getBiasSettleTimeUs();
  if (settleUs > 0) {
    _delayUs(settleUs);
  }

  uint8_t cfg = 0;
  st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  cfg |= cmd::CONFIG_BIAS | cmd::CONFIG_ONE_SHOT;
  cfg &= static_cast<uint8_t>(~(cmd::CONFIG_AUTO |
                                cmd::CONFIG_FAULT_CYCLE_MASK |
                                cmd::CONFIG_FAULT_CLEAR));

  st = writeRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  _config.autoConvert = false;
  _config.enableBias = true;
  _conversionStarted = true;
  _sampleAvailable = false;
  _conversionStartMs = _nowMs();
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

bool MAX31865::conversionReady() {
  if (!_initialized) {
    return false;
  }

  if (_config.gpioRead != nullptr && _config.drdyPin >= 0) {
    return !_config.gpioRead(_config.drdyPin, _config.gpioUser);
  }

  if (_conversionStarted &&
      elapsedMs(_nowMs(), _conversionStartMs) >= getSingleConversionTimeMs()) {
    _conversionStarted = false;
    _sampleAvailable = true;
  }

  if (_config.autoConvert &&
      elapsedMs(_nowMs(), _conversionStartMs) >= getContinuousConversionTimeMs()) {
    _sampleAvailable = true;
  }

  return _sampleAvailable;
}

Status MAX31865::readSample(Reading& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if ((_conversionStarted || _config.autoConvert) && !conversionReady()) {
    return Status::Error(Err::CONVERSION_NOT_READY, "Conversion not ready");
  }

  RawRtd raw;
  Status st = readRawRtd(raw);
  if (!st.ok()) {
    return st;
  }

  out.raw = raw;
  out.resistanceOhms = rawToResistance(raw);
  out.temperatureC = resistanceToTemperature(out.resistanceOhms);
  out.timestampMs = _nowMs();
  out.hasFaultStatus = false;
  out.faultStatus = FaultStatus{};

  if (raw.fault) {
    FaultStatus fault;
    st = readFaultStatus(fault);
    if (!st.ok()) {
      return st;
    }
    out.hasFaultStatus = true;
    out.faultStatus = fault;
  }

  _conversionStarted = false;
  _sampleAvailable = _config.autoConvert;
  if (_config.autoConvert) {
    _conversionStartMs = _nowMs();
  }
  _cacheSample(out);

  if (_config.disableBiasAfterOneShot && !_config.autoConvert) {
    Status biasSt = setBias(false);
    if (!biasSt.ok()) {
      return biasSt;
    }
  }

  return Status::Ok();
}

Status MAX31865::readBlocking(Reading& out, uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Status st = startOneShot();
  if (!st.inProgress() && !st.ok()) {
    return st;
  }

  const uint32_t startMs = _nowMs();
  while (elapsedMs(_nowMs(), startMs) <= timeoutMs) {
    if (conversionReady()) {
      return readSample(out);
    }
    _cooperativeYield();
  }

  _conversionStarted = false;
  _sampleAvailable = false;
  return _recordFailure(Status::Error(Err::TIMEOUT, "One-shot conversion timeout"));
}

Status MAX31865::readRawRtd(RawRtd& out) {
  uint8_t data[2] = {0, 0};
  Status st = readRegisters(cmd::REG_RTD_MSB, data, sizeof(data));
  if (!st.ok()) {
    return st;
  }

  out.rawRegister = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) |
                                          static_cast<uint16_t>(data[1]));
  out.fault = (out.rawRegister & cmd::RTD_FAULT_BIT) != 0;
  out.code = static_cast<uint16_t>(out.rawRegister >> 1U);
  return Status::Ok();
}

Status MAX31865::readResistance(float& ohms) {
  Reading sample;
  Status st = readSample(sample);
  if (!st.ok()) {
    return st;
  }
  ohms = sample.resistanceOhms;
  return Status::Ok();
}

Status MAX31865::readTemperature(float& celsius) {
  Reading sample;
  Status st = readSample(sample);
  if (!st.ok()) {
    return st;
  }
  celsius = sample.temperatureC;
  return Status::Ok();
}

Status MAX31865::getLastSample(Reading& out) const {
  if (!_lastSampleValid) {
    return Status::Error(Err::MEASUREMENT_NOT_READY, "No cached sample");
  }
  out = _lastSample;
  return Status::Ok();
}

uint32_t MAX31865::sampleTimestampMs() const {
  return _lastSampleTimestampMs;
}

uint32_t MAX31865::sampleAgeMs(uint32_t nowMs) const {
  if (_lastSampleTimestampMs == 0) {
    return 0;
  }
  return nowMs - _lastSampleTimestampMs;
}

// ============================================================================
// Configuration
// ============================================================================

Status MAX31865::setBias(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.autoConvert && !enable) {
    return Status::Error(Err::BUSY, "Cannot disable bias during auto conversion");
  }

  uint8_t cfg = 0;
  Status st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  if (enable) {
    cfg |= cmd::CONFIG_BIAS;
  } else {
    cfg &= static_cast<uint8_t>(~cmd::CONFIG_BIAS);
  }

  cfg &= static_cast<uint8_t>(~(cmd::CONFIG_ONE_SHOT |
                                cmd::CONFIG_FAULT_CYCLE_MASK |
                                cmd::CONFIG_FAULT_CLEAR));
  st = writeRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }
  _config.enableBias = enable;
  return Status::Ok();
}

Status MAX31865::setAutoConvert(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint8_t cfg = 0;
  Status st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  cfg &= static_cast<uint8_t>(~(cmd::CONFIG_ONE_SHOT |
                                cmd::CONFIG_FAULT_CYCLE_MASK |
                                cmd::CONFIG_FAULT_CLEAR));
  if (enable) {
    cfg |= cmd::CONFIG_AUTO | cmd::CONFIG_BIAS;
  } else {
    cfg &= static_cast<uint8_t>(~cmd::CONFIG_AUTO);
  }

  st = writeRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  _config.autoConvert = enable;
  if (enable) {
    _config.enableBias = true;
    _conversionStarted = true;
    _sampleAvailable = false;
    _conversionStartMs = _nowMs();
  } else {
    _conversionStarted = false;
    _sampleAvailable = false;
  }
  return Status::Ok();
}

Status MAX31865::setWireMode(WireMode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidWireMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid wire mode");
  }

  uint8_t cfg = 0;
  Status st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  if (mode == WireMode::THREE_WIRE) {
    cfg |= cmd::CONFIG_3WIRE;
  } else {
    cfg &= static_cast<uint8_t>(~cmd::CONFIG_3WIRE);
  }
  cfg &= static_cast<uint8_t>(~(cmd::CONFIG_ONE_SHOT |
                                cmd::CONFIG_FAULT_CYCLE_MASK |
                                cmd::CONFIG_FAULT_CLEAR));
  st = writeRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }
  _config.wireMode = mode;
  return Status::Ok();
}

Status MAX31865::setFilter(Filter filter) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidFilter(filter)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid filter");
  }
  if (_config.autoConvert) {
    return Status::Error(Err::BUSY, "Stop auto conversion before changing filter");
  }

  uint8_t cfg = 0;
  Status st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  if (filter == Filter::HZ_50) {
    cfg |= cmd::CONFIG_FILTER_50HZ;
  } else {
    cfg &= static_cast<uint8_t>(~cmd::CONFIG_FILTER_50HZ);
  }
  cfg &= static_cast<uint8_t>(~(cmd::CONFIG_ONE_SHOT |
                                cmd::CONFIG_FAULT_CYCLE_MASK |
                                cmd::CONFIG_FAULT_CLEAR));
  st = writeRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }
  _config.filter = filter;
  return Status::Ok();
}

Status MAX31865::configureMeasurement(WireMode wireMode, Filter filter, bool autoConvert) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidWireMode(wireMode) || !isValidFilter(filter)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid measurement configuration");
  }

  Config previous = _config;
  if (_config.autoConvert) {
    Status stopSt = setAutoConvert(false);
    if (!stopSt.ok()) {
      return stopSt;
    }
  }

  Status st = setWireMode(wireMode);
  if (st.ok()) {
    st = setFilter(filter);
  }
  if (st.ok()) {
    st = setAutoConvert(autoConvert);
  }
  if (!st.ok()) {
    _config = previous;
  }
  return st;
}

Status MAX31865::readConfiguration(uint8_t& raw) {
  return readRegister8(cmd::REG_CONFIG, raw);
}

Status MAX31865::readConfiguration(ConfigurationInfo& out) {
  uint8_t raw = 0;
  Status st = readConfiguration(raw);
  if (!st.ok()) {
    return st;
  }
  decodeConfiguration(raw, out);
  return Status::Ok();
}

Status MAX31865::writeConfiguration(uint8_t raw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status st = writeRegister8(cmd::REG_CONFIG, raw);
  if (!st.ok()) {
    return st;
  }

  ConfigurationInfo info;
  decodeConfiguration(raw, info);
  _config.enableBias = info.biasEnabled;
  _config.autoConvert = info.autoConvert;
  _config.wireMode = info.wireMode;
  _config.filter = info.filter;
  _conversionStarted = info.autoConvert;
  _sampleAvailable = false;
  _conversionStartMs = info.autoConvert ? _nowMs() : 0;
  return Status::Ok();
}

// ============================================================================
// Faults and thresholds
// ============================================================================

Status MAX31865::readFaultStatus(FaultStatus& out) {
  uint8_t raw = 0;
  Status st = readRegister8(cmd::REG_FAULT_STATUS, raw);
  if (!st.ok()) {
    return st;
  }
  decodeFaultStatus(raw, out);
  return Status::Ok();
}

Status MAX31865::clearFaults() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint8_t cfg = 0;
  Status st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  cfg &= static_cast<uint8_t>(~(cmd::CONFIG_ONE_SHOT |
                                cmd::CONFIG_FAULT_CYCLE_MASK |
                                cmd::CONFIG_FAULT_CLEAR));
  cfg |= cmd::CONFIG_FAULT_CLEAR;
  return writeRegister8(cmd::REG_CONFIG, cfg);
}

Status MAX31865::setFaultThresholdsRaw(uint16_t lowCode, uint16_t highCode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (lowCode > cmd::ADC_CODE_MAX || highCode > cmd::ADC_CODE_MAX ||
      lowCode > highCode) {
    return Status::Error(Err::INVALID_PARAM, "Invalid fault thresholds");
  }

  const uint16_t highReg = static_cast<uint16_t>(highCode << 1U);
  const uint16_t lowReg = static_cast<uint16_t>(lowCode << 1U);
  const uint8_t data[4] = {
      static_cast<uint8_t>(highReg >> 8U),
      static_cast<uint8_t>(highReg & 0xFEU),
      static_cast<uint8_t>(lowReg >> 8U),
      static_cast<uint8_t>(lowReg & 0xFEU)
  };

  return writeRegisters(cmd::REG_HIGH_FAULT_MSB, data, sizeof(data));
}

Status MAX31865::getFaultThresholdsRaw(FaultThresholds& out) {
  uint8_t data[4] = {0, 0, 0, 0};
  Status st = readRegisters(cmd::REG_HIGH_FAULT_MSB, data, sizeof(data));
  if (!st.ok()) {
    return st;
  }

  const uint16_t highReg = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) |
                                                 static_cast<uint16_t>(data[1]));
  const uint16_t lowReg = static_cast<uint16_t>((static_cast<uint16_t>(data[2]) << 8U) |
                                                static_cast<uint16_t>(data[3]));
  out.highCode = static_cast<uint16_t>(highReg >> 1U);
  out.lowCode = static_cast<uint16_t>(lowReg >> 1U);
  return Status::Ok();
}

Status MAX31865::setFaultThresholdsResistance(float lowOhms, float highOhms) {
  if (!std::isfinite(lowOhms) || !std::isfinite(highOhms) ||
      lowOhms < 0.0f || highOhms < 0.0f || lowOhms > highOhms) {
    return Status::Error(Err::INVALID_PARAM, "Invalid resistance thresholds");
  }
  return setFaultThresholdsRaw(resistanceToCode(lowOhms), resistanceToCode(highOhms));
}

Status MAX31865::getFaultThresholdsResistance(float& lowOhms, float& highOhms) {
  FaultThresholds thresholds;
  Status st = getFaultThresholdsRaw(thresholds);
  if (!st.ok()) {
    return st;
  }
  lowOhms = codeToResistance(thresholds.lowCode);
  highOhms = codeToResistance(thresholds.highCode);
  return Status::Ok();
}

Status MAX31865::setFaultThresholdsTemperature(float lowC, float highC) {
  if (!std::isfinite(lowC) || !std::isfinite(highC) || lowC > highC) {
    return Status::Error(Err::INVALID_PARAM, "Invalid temperature thresholds");
  }
  return setFaultThresholdsResistance(temperatureToResistance(lowC),
                                      temperatureToResistance(highC));
}

Status MAX31865::runAutoFaultDetection(FaultStatus& out, uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Status st = setBias(true);
  if (!st.ok()) {
    return st;
  }

  uint8_t cfg = 0;
  st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  cfg |= cmd::CONFIG_BIAS;
  cfg &= static_cast<uint8_t>(~(cmd::CONFIG_AUTO |
                                cmd::CONFIG_ONE_SHOT |
                                cmd::CONFIG_FAULT_CYCLE_MASK |
                                cmd::CONFIG_FAULT_CLEAR));
  cfg |= cmd::CONFIG_FAULT_CYCLE_AUTO;
  st = writeRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  _delayUs(cmd::AUTO_FAULT_DETECTION_MAX_US);
  st = _waitForFaultCycleDone(timeoutMs);
  if (!st.ok()) {
    return st;
  }
  return readFaultStatus(out);
}

Status MAX31865::runManualFaultDetection(FaultStatus& out, uint32_t settleDelayUs,
                                         uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Status st = setBias(true);
  if (!st.ok()) {
    return st;
  }
  _delayUs(settleDelayUs);

  uint8_t cfg = 0;
  st = readRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  cfg |= cmd::CONFIG_BIAS;
  cfg &= static_cast<uint8_t>(~(cmd::CONFIG_AUTO |
                                cmd::CONFIG_ONE_SHOT |
                                cmd::CONFIG_FAULT_CYCLE_MASK |
                                cmd::CONFIG_FAULT_CLEAR));
  cfg |= cmd::CONFIG_FAULT_CYCLE_MANUAL_1;
  st = writeRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  _delayUs(cmd::MANUAL_FAULT_STEP_SETTLE_US + settleDelayUs);

  cfg &= static_cast<uint8_t>(~cmd::CONFIG_FAULT_CYCLE_MASK);
  cfg |= cmd::CONFIG_FAULT_CYCLE_MANUAL_2;
  st = writeRegister8(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }

  _delayUs(cmd::MANUAL_FAULT_STEP_SETTLE_US);
  st = _waitForFaultCycleDone(timeoutMs);
  if (!st.ok()) {
    return st;
  }
  return readFaultStatus(out);
}

// ============================================================================
// Raw register access
// ============================================================================

Status MAX31865::readRegisters(uint8_t startReg, uint8_t* buffer, size_t len) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (buffer == nullptr || len == 0 || startReg > cmd::REG_LAST ||
      static_cast<uint16_t>(startReg) + static_cast<uint16_t>(len - 1U) > cmd::REG_LAST) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register read block");
  }

  uint8_t tx[1 + cmd::REG_LAST + 1] = {};
  uint8_t rx[1 + cmd::REG_LAST + 1] = {};
  tx[0] = static_cast<uint8_t>(startReg & cmd::READ_MASK);
  for (size_t i = 1; i <= len; ++i) {
    tx[i] = 0xFF;
  }

  Status st = _transfer(tx, rx, len + 1U, true);
  if (!st.ok()) {
    return st;
  }
  std::memcpy(buffer, &rx[1], len);
  return Status::Ok();
}

Status MAX31865::writeRegisters(uint8_t startReg, const uint8_t* buffer, size_t len) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (buffer == nullptr || !isWritableBlock(startReg, len)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register write block");
  }

  uint8_t tx[1 + cmd::REG_LAST + 1] = {};
  uint8_t rx[1 + cmd::REG_LAST + 1] = {};
  tx[0] = static_cast<uint8_t>(startReg | cmd::WRITE_BIT);
  std::memcpy(&tx[1], buffer, len);

  return _transfer(tx, rx, len + 1U, true);
}

Status MAX31865::readRegister8(uint8_t reg, uint8_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return _readRegisterRaw(reg, value);
}

Status MAX31865::writeRegister8(uint8_t reg, uint8_t value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return _writeRegisterRaw(reg, value);
}

// ============================================================================
// Utility
// ============================================================================

void MAX31865::decodeConfiguration(uint8_t raw, ConfigurationInfo& out) const {
  out.raw = raw;
  out.biasEnabled = (raw & cmd::CONFIG_BIAS) != 0;
  out.autoConvert = (raw & cmd::CONFIG_AUTO) != 0;
  out.oneShot = (raw & cmd::CONFIG_ONE_SHOT) != 0;
  out.wireMode = (raw & cmd::CONFIG_3WIRE) ? WireMode::THREE_WIRE : _config.wireMode;
  if ((raw & cmd::CONFIG_3WIRE) == 0 && out.wireMode == WireMode::THREE_WIRE) {
    out.wireMode = WireMode::FOUR_WIRE;
  }
  out.faultCycle = static_cast<uint8_t>((raw & cmd::CONFIG_FAULT_CYCLE_MASK) >>
                                        cmd::BIT_FAULT_CYCLE);
  out.faultClear = (raw & cmd::CONFIG_FAULT_CLEAR) != 0;
  out.filter = (raw & cmd::CONFIG_FILTER_50HZ) ? Filter::HZ_50 : Filter::HZ_60;
}

void MAX31865::decodeFaultStatus(uint8_t raw, FaultStatus& out) const {
  out.raw = static_cast<uint8_t>(raw & cmd::FAULT_DEFINED_MASK);
  out.highThreshold = (raw & cmd::FAULT_HIGH_THRESHOLD) != 0;
  out.lowThreshold = (raw & cmd::FAULT_LOW_THRESHOLD) != 0;
  out.refInHigh = (raw & cmd::FAULT_REFIN_HIGH) != 0;
  out.refInLow = (raw & cmd::FAULT_REFIN_LOW) != 0;
  out.rtdInLow = (raw & cmd::FAULT_RTDIN_LOW) != 0;
  out.overUnderVoltage = (raw & cmd::FAULT_OVER_UNDER_VOLTAGE) != 0;
}

float MAX31865::codeToResistance(uint16_t code) const {
  return (static_cast<float>(code) * _config.referenceResistorOhms) /
         static_cast<float>(cmd::ADC_FULL_SCALE);
}

uint16_t MAX31865::resistanceToCode(float resistanceOhms) const {
  if (!std::isfinite(resistanceOhms) || resistanceOhms <= 0.0f) {
    return 0;
  }
  const double scaled = (static_cast<double>(resistanceOhms) *
                         static_cast<double>(cmd::ADC_FULL_SCALE)) /
                        static_cast<double>(_config.referenceResistorOhms);
  if (scaled <= 0.0) {
    return 0;
  }
  return clampCode(static_cast<uint32_t>(scaled + 0.5));
}

float MAX31865::rawToResistance(const RawRtd& raw) const {
  return codeToResistance(raw.code);
}

float MAX31865::resistanceToTemperature(float resistanceOhms) const {
  if (!std::isfinite(resistanceOhms) || resistanceOhms <= 0.0f) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  const double ratio = static_cast<double>(resistanceOhms) /
                       static_cast<double>(_config.rtdNominalOhms);
  const double a = _config.coefficients.a;
  const double b = _config.coefficients.b;
  const double c = _config.coefficients.c;

  if (resistanceOhms >= _config.rtdNominalOhms) {
    const double discriminant = (a * a) - (4.0 * b * (1.0 - ratio));
    if (discriminant < 0.0 || b == 0.0) {
      return std::numeric_limits<float>::quiet_NaN();
    }
    return static_cast<float>((-a + std::sqrt(discriminant)) / (2.0 * b));
  }

  // Below 0 C the CVD equation includes the c term. It is monotonic for the
  // supported platinum RTD range, so bisection is robust and deterministic.
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
  if (!std::isfinite(temperatureC)) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  const double t = temperatureC;
  const double a = _config.coefficients.a;
  const double b = _config.coefficients.b;
  const double c = (temperatureC < 0.0f) ? _config.coefficients.c : 0.0;
  const double ratio = 1.0 + (a * t) + (b * t * t) +
                       (c * (t - 100.0) * t * t * t);
  return static_cast<float>(static_cast<double>(_config.rtdNominalOhms) * ratio);
}

uint16_t MAX31865::temperatureToCode(float temperatureC) const {
  return resistanceToCode(temperatureToResistance(temperatureC));
}

uint32_t MAX31865::getSingleConversionTimeMs() const {
  return (_config.filter == Filter::HZ_50) ? cmd::SINGLE_CONVERSION_50HZ_MS
                                           : cmd::SINGLE_CONVERSION_60HZ_MS;
}

uint32_t MAX31865::getContinuousConversionTimeMs() const {
  return (_config.filter == Filter::HZ_50) ? cmd::CONTINUOUS_CONVERSION_50HZ_MS
                                           : cmd::CONTINUOUS_CONVERSION_60HZ_MS;
}

uint32_t MAX31865::getBiasSettleTimeUs() const {
  const double scaled = (static_cast<double>(_config.inputFilterTimeConstantUs) *
                         static_cast<double>(cmd::BIAS_SETTLE_TIME_CONSTANTS)) +
                        static_cast<double>(cmd::BIAS_SETTLE_EXTRA_US);
  if (scaled <= 0.0) {
    return cmd::BIAS_SETTLE_EXTRA_US;
  }
  if (scaled > static_cast<double>(UINT32_MAX)) {
    return UINT32_MAX;
  }
  return static_cast<uint32_t>(scaled + 0.5);
}

// ============================================================================
// Private helpers
// ============================================================================

Status MAX31865::_applyCachedConfig() {
  uint8_t cfg = buildConfigByte(_config);
  Status st = _writeRegisterRaw(cmd::REG_CONFIG, cfg);
  if (!st.ok()) {
    return st;
  }
  _sampleAvailable = false;
  _conversionStarted = _config.autoConvert;
  _conversionStartMs = _config.autoConvert ? _nowMs() : 0;
  return Status::Ok();
}

Status MAX31865::_readRegisterRaw(uint8_t reg, uint8_t& value) {
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register address");
  }

  uint8_t tx[2] = {static_cast<uint8_t>(reg & cmd::READ_MASK), 0xFF};
  uint8_t rx[2] = {0, 0};
  Status st = _transfer(tx, rx, sizeof(tx), true);
  if (!st.ok()) {
    return st;
  }
  value = rx[1];
  return Status::Ok();
}

Status MAX31865::_writeRegisterRaw(uint8_t reg, uint8_t value) {
  if (!isWritableRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid writable register address");
  }

  uint8_t tx[2] = {static_cast<uint8_t>(reg | cmd::WRITE_BIT), value};
  uint8_t rx[2] = {0, 0};
  return _transfer(tx, rx, sizeof(tx), true);
}

Status MAX31865::_transfer(const uint8_t* tx, uint8_t* rx, size_t len, bool tracked) {
  if (_config.spiTransfer == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "SPI transfer callback missing");
  }
  if (tx == nullptr || rx == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid SPI transfer buffers");
  }

  Status st = _config.spiTransfer(tx, rx, len, _config.spiTimeoutMs, _config.spiUser);
  if (!tracked) {
    return st;
  }
  return _updateHealth(st);
}

Status MAX31865::_waitForFaultCycleDone(uint32_t timeoutMs) {
  const uint32_t startMs = _nowMs();
  while (elapsedMs(_nowMs(), startMs) <= timeoutMs) {
    uint8_t cfg = 0;
    Status st = readRegister8(cmd::REG_CONFIG, cfg);
    if (!st.ok()) {
      return st;
    }
    if ((cfg & cmd::CONFIG_FAULT_CYCLE_MASK) == 0) {
      return Status::Ok();
    }
    _cooperativeYield();
  }
  return _recordFailure(Status::Error(Err::TIMEOUT, "Fault cycle timeout"));
}

Status MAX31865::_updateHealth(const Status& st) {
  const uint32_t now = _nowMs();

  if (st.inProgress()) {
    return st;
  }
  if (st.ok()) {
    _lastOkMs = now;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }
    if (_initialized) {
      _driverState = DriverState::READY;
    }
    return st;
  }

  return _recordFailure(st);
}

Status MAX31865::_recordFailure(const Status& st) {
  if (st.ok() || st.inProgress()) {
    return st;
  }

  _lastErrorMs = _nowMs();
  _lastError = st;
  if (_consecutiveFailures < UINT8_MAX) {
    _consecutiveFailures++;
  }
  if (_totalFailures < UINT32_MAX) {
    _totalFailures++;
  }
  if (_initialized) {
    _driverState = (_consecutiveFailures >= _config.offlineThreshold)
                       ? DriverState::OFFLINE
                       : DriverState::DEGRADED;
  }
  return st;
}

void MAX31865::_cacheSample(const Reading& sample) {
  _lastSample = sample;
  _lastSampleValid = true;
  _lastSampleTimestampMs = sample.timestampMs;
}

uint32_t MAX31865::_nowMs() const {
  if (_config.nowMs != nullptr) {
    return _config.nowMs(_config.timeUser);
  }
  return millis();
}

void MAX31865::_delayMs(uint32_t ms) const {
  if (_config.delayMs != nullptr) {
    _config.delayMs(ms, _config.timeUser);
    return;
  }
  delay(ms);
}

void MAX31865::_delayUs(uint32_t us) const {
  if (_config.delayUs != nullptr) {
    _config.delayUs(us, _config.timeUser);
    return;
  }
  while (us >= 1000U) {
    _delayMs(1);
    us -= 1000U;
  }
  if (us > 0U) {
    delayMicroseconds(us);
  }
}

void MAX31865::_cooperativeYield() const {
  if (_config.cooperativeYield != nullptr) {
    _config.cooperativeYield(_config.timeUser);
    return;
  }
  yield();
}

}  // namespace MAX31865
