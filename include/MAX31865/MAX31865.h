/// @file MAX31865.h
/// @brief Main driver class for the MAX31865 RTD-to-digital converter.
#pragma once

#include <cstddef>
#include <cstdint>

#include "MAX31865/CommandTable.h"
#include "MAX31865/Config.h"
#include "MAX31865/Status.h"
#include "MAX31865/Version.h"

namespace MAX31865 {

/// Coarse driver state for health monitoring.
enum class DriverState : uint8_t {
  UNINIT,
  READY,
  DEGRADED,
  OFFLINE
};

/// Decoded configuration register.
struct ConfigurationInfo {
  uint8_t raw = 0;
  bool biasEnabled = false;
  bool autoConvert = false;
  bool oneShot = false;
  WireMode wireMode = WireMode::FOUR_WIRE;
  uint8_t faultCycle = 0;
  bool faultClear = false;
  Filter filter = Filter::HZ_60;
};

/// Decoded fault-status register.
struct FaultStatus {
  uint8_t raw = 0;
  bool highThreshold = false;
  bool lowThreshold = false;
  bool refInHigh = false;
  bool refInLow = false;
  bool rtdInLow = false;
  bool overUnderVoltage = false;

  constexpr bool any() const {
    return highThreshold || lowThreshold || refInHigh || refInLow ||
           rtdInLow || overUnderVoltage;
  }
};

/// Raw RTD register sample.
struct RawRtd {
  uint16_t rawRegister = 0;
  uint16_t code = 0;
  bool fault = false;
};

/// Decoded measurement sample.
struct Reading {
  RawRtd raw{};
  float resistanceOhms = 0.0f;
  float temperatureC = 0.0f;
  uint32_t timestampMs = 0;
  bool hasFaultStatus = false;
  FaultStatus faultStatus{};
};

/// Fault threshold pair in 15-bit ADC-code form.
struct FaultThresholds {
  uint16_t lowCode = 0;
  uint16_t highCode = cmd::ADC_CODE_MAX;
};

/// Snapshot of cached configuration and runtime state.
struct SettingsSnapshot {
  bool initialized = false;
  DriverState state = DriverState::UNINIT;
  uint32_t spiTimeoutMs = 0;
  uint8_t offlineThreshold = 0;
  bool hasNowMsHook = false;
  bool hasDelayMsHook = false;
  bool hasDelayUsHook = false;
  bool hasYieldHook = false;
  bool hasGpioReadHook = false;
  int drdyPin = -1;
  float referenceResistorOhms = 0.0f;
  float rtdNominalOhms = 0.0f;
  uint32_t inputFilterTimeConstantUs = 0;
  WireMode wireMode = WireMode::FOUR_WIRE;
  Filter filter = Filter::HZ_60;
  bool enableBias = false;
  bool autoConvert = false;
  bool clearFaultsBeforeOneShot = false;
  bool disableBiasAfterOneShot = false;
  bool conversionStarted = false;
  bool sampleAvailable = false;
  uint32_t conversionStartMs = 0;
  uint32_t lastOkMs = 0;
  uint32_t lastErrorMs = 0;
  Status lastError = Status::Ok();
  uint8_t consecutiveFailures = 0;
  uint32_t totalFailures = 0;
  uint32_t totalSuccess = 0;
};

/// MAX31865 driver class.
class MAX31865 {
public:
  // === Lifecycle ===
  Status begin(const Config& config);
  void tick(uint32_t nowMs);
  void end();

  bool isInitialized() const { return _initialized; }
  const Config& getConfig() const { return _config; }

  // === Diagnostics ===
  Status probe();
  Status recover();
  Status getSettings(SettingsSnapshot& out) const;

  DriverState state() const { return _driverState; }
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }
  uint32_t lastOkMs() const { return _lastOkMs; }
  uint32_t lastErrorMs() const { return _lastErrorMs; }
  Status lastError() const { return _lastError; }
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }
  uint32_t totalFailures() const { return _totalFailures; }
  uint32_t totalSuccess() const { return _totalSuccess; }

  // === Measurement API ===
  Status startOneShot();
  bool conversionReady();
  Status readSample(Reading& out);
  Status readBlocking(Reading& out, uint32_t timeoutMs = 200);
  Status readOneShot(Reading& out, uint32_t timeoutMs = 200) {
    return readBlocking(out, timeoutMs);
  }
  Status readRawRtd(RawRtd& out);
  Status readResistance(float& ohms);
  Status readTemperature(float& celsius);
  Status getLastSample(Reading& out) const;
  uint32_t sampleTimestampMs() const;
  uint32_t sampleAgeMs(uint32_t nowMs) const;

  // === Configuration ===
  Status setBias(bool enable);
  Status setAutoConvert(bool enable);
  Status startAutoConversion() { return setAutoConvert(true); }
  Status stopAutoConversion() { return setAutoConvert(false); }
  Status setWireMode(WireMode mode);
  Status setFilter(Filter filter);
  Status configureMeasurement(WireMode wireMode, Filter filter, bool autoConvert);

  WireMode getWireMode() const { return _config.wireMode; }
  Filter getFilter() const { return _config.filter; }
  bool biasEnabledCached() const { return _config.enableBias; }
  bool autoConvertCached() const { return _config.autoConvert; }

  Status readConfiguration(uint8_t& raw);
  Status readConfiguration(ConfigurationInfo& out);
  Status writeConfiguration(uint8_t raw);

  // === Faults and thresholds ===
  Status readFaultStatus(FaultStatus& out);
  Status clearFaults();
  Status setFaultThresholdsRaw(uint16_t lowCode, uint16_t highCode);
  Status getFaultThresholdsRaw(FaultThresholds& out);
  Status setFaultThresholdsResistance(float lowOhms, float highOhms);
  Status getFaultThresholdsResistance(float& lowOhms, float& highOhms);
  Status setFaultThresholdsTemperature(float lowC, float highC);
  Status runAutoFaultDetection(FaultStatus& out, uint32_t timeoutMs = 10);
  Status runManualFaultDetection(FaultStatus& out, uint32_t settleDelayUs,
                                 uint32_t timeoutMs = 10);

  // === Raw register access ===
  Status readRegisters(uint8_t startReg, uint8_t* buffer, size_t len);
  Status writeRegisters(uint8_t startReg, const uint8_t* buffer, size_t len);
  Status readRegister8(uint8_t reg, uint8_t& value);
  Status writeRegister8(uint8_t reg, uint8_t value);

  // === Utility ===
  void decodeConfiguration(uint8_t raw, ConfigurationInfo& out) const;
  void decodeFaultStatus(uint8_t raw, FaultStatus& out) const;
  float codeToResistance(uint16_t code) const;
  uint16_t resistanceToCode(float resistanceOhms) const;
  float rawToResistance(const RawRtd& raw) const;
  float resistanceToTemperature(float resistanceOhms) const;
  float temperatureToResistance(float temperatureC) const;
  uint16_t temperatureToCode(float temperatureC) const;
  uint32_t getSingleConversionTimeMs() const;
  uint32_t getContinuousConversionTimeMs() const;
  uint32_t getBiasSettleTimeUs() const;

private:
  Status _applyCachedConfig();
  Status _readRegisterRaw(uint8_t reg, uint8_t& value);
  Status _writeRegisterRaw(uint8_t reg, uint8_t value);
  Status _transfer(const uint8_t* tx, uint8_t* rx, size_t len, bool tracked);
  Status _waitForFaultCycleDone(uint32_t timeoutMs);
  Status _updateHealth(const Status& st);
  Status _recordFailure(const Status& st);
  void _cacheSample(const Reading& sample);
  uint32_t _nowMs() const;
  void _delayMs(uint32_t ms) const;
  void _delayUs(uint32_t us) const;
  void _cooperativeYield() const;

  Config _config{};
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;

  bool _conversionStarted = false;
  bool _sampleAvailable = false;
  uint32_t _conversionStartMs = 0;
  uint32_t _lastSampleTimestampMs = 0;
  Reading _lastSample{};
  bool _lastSampleValid = false;

  uint32_t _lastOkMs = 0;
  uint32_t _lastErrorMs = 0;
  Status _lastError = Status::Ok();
  uint8_t _consecutiveFailures = 0;
  uint32_t _totalFailures = 0;
  uint32_t _totalSuccess = 0;
};

}  // namespace MAX31865
