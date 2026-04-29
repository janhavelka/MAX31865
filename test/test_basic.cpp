/// @file test_basic.cpp
/// @brief Native contract tests for MAX31865 driver.

#include <unity.h>

#include <cmath>
#include <cstring>

#include "Arduino.h"
#include "SPI.h"

SerialClass Serial;
SPIClass SPI;

#define private public
#include "MAX31865/MAX31865.h"
#undef private

using namespace MAX31865;

namespace {

struct FakeBus {
  uint8_t registers[8] = {};
  uint32_t nowMs = 1000;
  uint32_t transferCalls = 0;
  uint32_t failOnCall = 0;
  Status nextStatus = Status::Ok();
  uint32_t oneShotReadyAtMs = UINT32_MAX;
  uint32_t faultReadyAtMs = UINT32_MAX;

  FakeBus() { reset(); }

  void reset() {
    memset(registers, 0, sizeof(registers));
    registers[cmd::REG_CONFIG] = cmd::CONFIG_RESET;
    registers[cmd::REG_RTD_MSB] = cmd::RTD_MSB_RESET;
    registers[cmd::REG_RTD_LSB] = cmd::RTD_LSB_RESET;
    registers[cmd::REG_HIGH_FAULT_MSB] = cmd::HIGH_FAULT_MSB_RESET;
    registers[cmd::REG_HIGH_FAULT_LSB] = cmd::HIGH_FAULT_LSB_RESET;
    registers[cmd::REG_LOW_FAULT_MSB] = cmd::LOW_FAULT_MSB_RESET;
    registers[cmd::REG_LOW_FAULT_LSB] = cmd::LOW_FAULT_LSB_RESET;
    registers[cmd::REG_FAULT_STATUS] = cmd::FAULT_STATUS_RESET;
    oneShotReadyAtMs = UINT32_MAX;
    faultReadyAtMs = UINT32_MAX;
  }
};

void maybeComplete(FakeBus& bus) {
  if (bus.oneShotReadyAtMs != UINT32_MAX &&
      static_cast<int32_t>(bus.nowMs - bus.oneShotReadyAtMs) >= 0) {
    bus.registers[cmd::REG_CONFIG] =
        static_cast<uint8_t>(bus.registers[cmd::REG_CONFIG] & ~cmd::CONFIG_ONE_SHOT);
    bus.oneShotReadyAtMs = UINT32_MAX;
  }
  if (bus.faultReadyAtMs != UINT32_MAX &&
      static_cast<int32_t>(bus.nowMs - bus.faultReadyAtMs) >= 0) {
    bus.registers[cmd::REG_CONFIG] =
        static_cast<uint8_t>(bus.registers[cmd::REG_CONFIG] & ~cmd::CONFIG_FAULT_CYCLE_MASK);
    bus.faultReadyAtMs = UINT32_MAX;
  }
}

Status fakeTransfer(const uint8_t* tx, uint8_t* rx, size_t len, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->transferCalls++;
  if (!bus->nextStatus.ok()) {
    return bus->nextStatus;
  }
  if (bus->failOnCall != 0 && bus->transferCalls == bus->failOnCall) {
    bus->failOnCall = 0;
    return Status::Error(Err::SPI_ERROR, "forced SPI error", -7);
  }
  if (tx == nullptr || rx == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "bad fake transfer");
  }

  maybeComplete(*bus);
  memset(rx, 0, len);

  const bool write = (tx[0] & cmd::WRITE_BIT) != 0;
  uint8_t reg = static_cast<uint8_t>(tx[0] & cmd::READ_MASK);

  if (write) {
    for (size_t i = 1; i < len; ++i) {
      if (reg <= cmd::REG_LAST) {
        if (reg == cmd::REG_CONFIG) {
          uint8_t value = tx[i];
          if (value & cmd::CONFIG_FAULT_CLEAR) {
            bus->registers[cmd::REG_FAULT_STATUS] = 0;
            value &= static_cast<uint8_t>(~cmd::CONFIG_FAULT_CLEAR);
          }
          if (value & cmd::CONFIG_ONE_SHOT) {
            bus->oneShotReadyAtMs =
                bus->nowMs + ((value & cmd::CONFIG_FILTER_50HZ)
                                  ? cmd::SINGLE_CONVERSION_50HZ_MS
                                  : cmd::SINGLE_CONVERSION_60HZ_MS);
            value &= static_cast<uint8_t>(~cmd::CONFIG_ONE_SHOT);
          }
          if (value & cmd::CONFIG_FAULT_CYCLE_MASK) {
            bus->faultReadyAtMs = bus->nowMs + 1;
          }
          bus->registers[reg] = value;
        } else if (reg != cmd::REG_RTD_MSB && reg != cmd::REG_RTD_LSB &&
                   reg != cmd::REG_FAULT_STATUS) {
          bus->registers[reg] = tx[i];
        }
      }
      ++reg;
    }
    return Status::Ok();
  }

  for (size_t i = 1; i < len; ++i) {
    rx[i] = (reg <= cmd::REG_LAST) ? bus->registers[reg] : 0xFF;
    ++reg;
  }
  return Status::Ok();
}

uint32_t fakeNow(void* user) {
  return static_cast<FakeBus*>(user)->nowMs;
}

void fakeDelayMs(uint32_t ms, void* user) {
  static_cast<FakeBus*>(user)->nowMs += ms;
}

void fakeDelayUs(uint32_t us, void* user) {
  static_cast<FakeBus*>(user)->nowMs += (us + 999U) / 1000U;
}

void fakeYield(void* user) {
  static_cast<FakeBus*>(user)->nowMs += 1;
}

bool fakeDrdyLow(int, void*) {
  return false;
}

Config makeConfig(FakeBus& bus) {
  Config cfg;
  cfg.spiTransfer = fakeTransfer;
  cfg.spiUser = &bus;
  cfg.nowMs = fakeNow;
  cfg.delayMs = fakeDelayMs;
  cfg.delayUs = fakeDelayUs;
  cfg.cooperativeYield = fakeYield;
  cfg.timeUser = &bus;
  cfg.spiTimeoutMs = 10;
  cfg.referenceResistorOhms = 400.0f;
  cfg.rtdNominalOhms = 100.0f;
  cfg.inputFilterTimeConstantUs = 100;
  cfg.offlineThreshold = 2;
  return cfg;
}

void seedRtd(FakeBus& bus, uint16_t code, bool fault = false) {
  uint16_t raw = static_cast<uint16_t>(code << 1U);
  if (fault) {
    raw |= cmd::RTD_FAULT_BIT;
    bus.registers[cmd::REG_FAULT_STATUS] = cmd::FAULT_HIGH_THRESHOLD;
  }
  bus.registers[cmd::REG_RTD_MSB] = static_cast<uint8_t>(raw >> 8U);
  bus.registers[cmd::REG_RTD_LSB] = static_cast<uint8_t>(raw & 0xFFU);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_status_ok_and_error() {
  Status ok = Status::Ok();
  TEST_ASSERT_TRUE(ok.ok());
  TEST_ASSERT_TRUE(static_cast<bool>(ok));

  Status err = Status::Error(Err::SPI_ERROR, "spi", -3);
  TEST_ASSERT_FALSE(err.ok());
  TEST_ASSERT_TRUE(err.is(Err::SPI_ERROR));
  TEST_ASSERT_EQUAL_INT32(-3, err.detail);
}

void test_config_defaults() {
  Config cfg;
  TEST_ASSERT_EQUAL_FLOAT(400.0f, cfg.referenceResistorOhms);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, cfg.rtdNominalOhms);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WireMode::FOUR_WIRE),
                          static_cast<uint8_t>(cfg.wireMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Filter::HZ_60),
                          static_cast<uint8_t>(cfg.filter));
  TEST_ASSERT_TRUE(cfg.clearFaultsBeforeOneShot);
  TEST_ASSERT_TRUE(cfg.disableBiasAfterOneShot);
}

void test_begin_rejects_missing_transport() {
  MAX31865::MAX31865 dev;
  Config cfg;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
}

void test_begin_applies_default_config_and_health() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  Status st = dev.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.registers[cmd::REG_CONFIG]);
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalSuccess());
}

void test_probe_detects_all_ones_response() {
  FakeBus bus;
  bus.registers[cmd::REG_CONFIG] = 0xFF;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  bus.registers[cmd::REG_CONFIG] = 0xFF;
  Status st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
}

void test_read_write_registers_and_decoding() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_TRUE(dev.setWireMode(WireMode::THREE_WIRE).ok());
  TEST_ASSERT_TRUE(dev.setFilter(Filter::HZ_50).ok());
  TEST_ASSERT_TRUE(dev.setBias(true).ok());

  ConfigurationInfo info;
  TEST_ASSERT_TRUE(dev.readConfiguration(info).ok());
  TEST_ASSERT_TRUE(info.biasEnabled);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WireMode::THREE_WIRE),
                          static_cast<uint8_t>(info.wireMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Filter::HZ_50),
                          static_cast<uint8_t>(info.filter));
}

void test_filter_change_rejected_during_auto_conversion() {
  FakeBus bus;
  Config cfg = makeConfig(bus);
  cfg.autoConvert = true;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  Status st = dev.setFilter(Filter::HZ_50);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY), static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.stopAutoConversion().ok());
  TEST_ASSERT_TRUE(dev.setFilter(Filter::HZ_50).ok());
}

void test_threshold_encoding_uses_15_bit_codes() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.setFaultThresholdsRaw(100, 20000).ok());

  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((20000u << 1U) >> 8U),
                         bus.registers[cmd::REG_HIGH_FAULT_MSB]);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((20000u << 1U) & 0xFEU),
                         bus.registers[cmd::REG_HIGH_FAULT_LSB]);

  FaultThresholds readback;
  TEST_ASSERT_TRUE(dev.getFaultThresholdsRaw(readback).ok());
  TEST_ASSERT_EQUAL_UINT16(100u, readback.lowCode);
  TEST_ASSERT_EQUAL_UINT16(20000u, readback.highCode);
}

void test_raw_rtd_resistance_and_temperature_at_zero_c() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  seedRtd(bus, 8192);

  RawRtd raw;
  TEST_ASSERT_TRUE(dev.readRawRtd(raw).ok());
  TEST_ASSERT_EQUAL_HEX16(0x4000, raw.rawRegister);
  TEST_ASSERT_EQUAL_UINT16(8192u, raw.code);
  TEST_ASSERT_FALSE(raw.fault);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, dev.rawToResistance(raw));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dev.resistanceToTemperature(100.0f));
  TEST_ASSERT_EQUAL_UINT16(8192u, dev.temperatureToCode(0.0f));
}

void test_temperature_conversion_negative_and_positive() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  float rNeg = dev.temperatureToResistance(-100.0f);
  float tNeg = dev.resistanceToTemperature(rNeg);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, -100.0f, tNeg);

  float rPos = dev.temperatureToResistance(100.0f);
  float tPos = dev.resistanceToTemperature(rPos);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 100.0f, tPos);
}

void test_read_blocking_one_shot_reads_sample_and_disables_bias() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  seedRtd(bus, 8192);

  Reading sample;
  Status st = dev.readBlocking(sample, 200);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sample.temperatureC);
  TEST_ASSERT_FALSE(bus.registers[cmd::REG_CONFIG] & cmd::CONFIG_BIAS);
  TEST_ASSERT_TRUE(dev.getLastSample(sample).ok());
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.sampleTimestampMs());
}

void test_fault_bit_reads_fault_status_without_failing_sample() {
  FakeBus bus;
  Config cfg = makeConfig(bus);
  cfg.gpioRead = fakeDrdyLow;
  cfg.drdyPin = 4;
  cfg.gpioUser = &bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  seedRtd(bus, 8192, true);

  Reading sample;
  Status st = dev.readSample(sample);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(sample.raw.fault);
  TEST_ASSERT_TRUE(sample.hasFaultStatus);
  TEST_ASSERT_TRUE(sample.faultStatus.highThreshold);
}

void test_clear_faults_preserves_configuration_bits() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.setWireMode(WireMode::THREE_WIRE).ok());
  TEST_ASSERT_TRUE(dev.setFilter(Filter::HZ_50).ok());
  bus.registers[cmd::REG_FAULT_STATUS] = cmd::FAULT_LOW_THRESHOLD;

  TEST_ASSERT_TRUE(dev.clearFaults().ok());
  TEST_ASSERT_EQUAL_UINT8(0u, bus.registers[cmd::REG_FAULT_STATUS]);
  TEST_ASSERT_TRUE(bus.registers[cmd::REG_CONFIG] & cmd::CONFIG_3WIRE);
  TEST_ASSERT_TRUE(bus.registers[cmd::REG_CONFIG] & cmd::CONFIG_FILTER_50HZ);
}

void test_auto_fault_detection_completes_and_decodes() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  bus.registers[cmd::REG_FAULT_STATUS] = cmd::FAULT_REFIN_HIGH;

  FaultStatus fault;
  Status st = dev.runAutoFaultDetection(fault);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(fault.refInHigh);
}

void test_recover_reapplies_cached_config() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.setWireMode(WireMode::THREE_WIRE).ok());
  TEST_ASSERT_TRUE(dev.setBias(true).ok());

  bus.registers[cmd::REG_CONFIG] = 0x00;
  TEST_ASSERT_TRUE(dev.recover().ok());
  TEST_ASSERT_TRUE(bus.registers[cmd::REG_CONFIG] & cmd::CONFIG_3WIRE);
  TEST_ASSERT_TRUE(bus.registers[cmd::REG_CONFIG] & cmd::CONFIG_BIAS);
}

void test_health_moves_offline_after_failures() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.nextStatus = Status::Error(Err::SPI_ERROR, "forced");
  uint8_t value = 0;
  (void)dev.readRegister8(cmd::REG_CONFIG, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  (void)dev.readRegister8(cmd::REG_CONFIG, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(2u, dev.totalFailures());
}

void test_invalid_raw_access_rejected_without_transfer() {
  FakeBus bus;
  MAX31865::MAX31865 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  uint32_t before = bus.transferCalls;

  uint8_t value = 0;
  Status st = dev.readRegister8(8, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.writeRegister8(cmd::REG_RTD_MSB, 0x12);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(before, bus.transferCalls);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_status_ok_and_error);
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_begin_rejects_missing_transport);
  RUN_TEST(test_begin_applies_default_config_and_health);
  RUN_TEST(test_probe_detects_all_ones_response);
  RUN_TEST(test_read_write_registers_and_decoding);
  RUN_TEST(test_filter_change_rejected_during_auto_conversion);
  RUN_TEST(test_threshold_encoding_uses_15_bit_codes);
  RUN_TEST(test_raw_rtd_resistance_and_temperature_at_zero_c);
  RUN_TEST(test_temperature_conversion_negative_and_positive);
  RUN_TEST(test_read_blocking_one_shot_reads_sample_and_disables_bias);
  RUN_TEST(test_fault_bit_reads_fault_status_without_failing_sample);
  RUN_TEST(test_clear_faults_preserves_configuration_bits);
  RUN_TEST(test_auto_fault_detection_completes_and_decodes);
  RUN_TEST(test_recover_reapplies_cached_config);
  RUN_TEST(test_health_moves_offline_after_failures);
  RUN_TEST(test_invalid_raw_access_rejected_without_transfer);
  return UNITY_END();
}
