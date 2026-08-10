#include <unity.h>

#include "MAX31865/MAX31865.h"

void setUp() {}
void tearDown() {}

static void test_fault_status_decodes_documented_bits_only() {
    const MAX31865FaultStatus fault = MAX31865::decodeFaultStatus(0xFFU);
    TEST_ASSERT_EQUAL_UINT8(0xFCU, fault.raw);
    TEST_ASSERT_TRUE(fault.highThreshold);
    TEST_ASSERT_TRUE(fault.lowThreshold);
    TEST_ASSERT_TRUE(fault.refinHigh);
    TEST_ASSERT_TRUE(fault.refinLow);
    TEST_ASSERT_TRUE(fault.rtdinLow);
    TEST_ASSERT_TRUE(fault.overUnderVoltage);

    const MAX31865FaultStatus reserved = MAX31865::decodeFaultStatus(0x03U);
    TEST_ASSERT_EQUAL_UINT8(0x00U, reserved.raw);
    TEST_ASSERT_FALSE(reserved.any());
}

static void test_rtd_code_resistance_and_temperature_roundtrip() {
    MAX31865 device;
    float value = 0.0F;
    TEST_ASSERT_TRUE(MAX31865::codeToRatio(8192U, value).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.25F, value);
    TEST_ASSERT_TRUE(device.codeToResistance(8192U, value).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 100.0F, value);
    TEST_ASSERT_TRUE(device.resistanceToTemperature(100.0F, value).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.05F, 0.0F, value);

    float r100 = 0.0F;
    TEST_ASSERT_TRUE(device.temperatureToResistance(100.0F, r100).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.05F, 138.505F, r100);
    TEST_ASSERT_TRUE(device.resistanceToTemperature(r100, value).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.2F, 100.0F, value);
}

static void test_timing_helpers_follow_filter_selection_defaults() {
    MAX31865 device;
    TEST_ASSERT_EQUAL_UINT32(max31865_cmd::SINGLE_CONVERSION_60HZ_MS,
                             device.singleConversionTimeMs());
    TEST_ASSERT_EQUAL_UINT32(max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS,
                             device.continuousConversionTimeMs());
    TEST_ASSERT_EQUAL_UINT32(2050U, device.biasSettleTimeUs());
}

static void test_status_helpers_and_default_health() {
    MAX31865 device;
    MAX31865Status ok = MAX31865Status::Ok();
    MAX31865Status err = MAX31865Status::Error(MAX31865Error::InvalidArgument, "bad");

    TEST_ASSERT_TRUE(ok.ok());
    TEST_ASSERT_FALSE(err.ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MAX31865State::Uninitialized),
                            static_cast<uint8_t>(device.state()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MAX31865DriverState::UNINIT),
                            static_cast<uint8_t>(device.health().driverState));
    TEST_ASSERT_FALSE(device.health().online);
}

static void test_read_configuration_requires_initialized_driver() {
    MAX31865 device;
    MAX31865Settings settings{};
    MAX31865Status st = device.readConfiguration(settings, 10U);

    TEST_ASSERT_FALSE(st.ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MAX31865Error::NotInitialized),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MAX31865DriverState::UNINIT),
                            static_cast<uint8_t>(device.health().driverState));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fault_status_decodes_documented_bits_only);
    RUN_TEST(test_rtd_code_resistance_and_temperature_roundtrip);
    RUN_TEST(test_timing_helpers_follow_filter_selection_defaults);
    RUN_TEST(test_status_helpers_and_default_health);
    RUN_TEST(test_read_configuration_requires_initialized_driver);
    return UNITY_END();
}
