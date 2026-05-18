#include <math.h>

#include <unity.h>

#include "MAX31865/MAX31865.h"

void setUp() {}
void tearDown() {}

static void test_fault_status_decodes_documented_bits_only() {
    MAX31865FaultStatus fault{};
    TEST_ASSERT_TRUE(MAX31865::decodeFaultStatus(0xFF, fault));
    TEST_ASSERT_EQUAL_UINT8(0xFC, fault.raw);
    TEST_ASSERT_TRUE(fault.high_threshold);
    TEST_ASSERT_TRUE(fault.low_threshold);
    TEST_ASSERT_TRUE(fault.refin_high);
    TEST_ASSERT_TRUE(fault.refin_low);
    TEST_ASSERT_TRUE(fault.rtdin_low);
    TEST_ASSERT_TRUE(fault.over_under_voltage);

    TEST_ASSERT_FALSE(MAX31865::decodeFaultStatus(0x03, fault));
    TEST_ASSERT_EQUAL_UINT8(0x00, fault.raw);
}

static void test_rtd_code_resistance_and_temperature_roundtrip() {
    MAX31865 device;

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, MAX31865::codeToRatio(8192));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, device.codeToResistance(8192));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, device.resistanceToTemperature(100.0f));

    const float r100 = device.temperatureToResistance(100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 138.505f, r100);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 100.0f, device.resistanceToTemperature(r100));
}

static void test_pt1000_scaling_uses_configured_reference_and_nominal() {
    MAX31865 device;
    TEST_ASSERT_TRUE(device.setRtdParameters(4300.0f, 1000.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1000.0f, device.codeToResistance(7620));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, device.resistanceToTemperature(1000.0f));
}

static void test_timing_helpers_follow_filter_selection_defaults() {
    MAX31865 device;
    TEST_ASSERT_EQUAL_UINT32(max31865_cmd::SINGLE_CONVERSION_60HZ_MS,
                             device.getSingleConversionTimeMs());
    TEST_ASSERT_EQUAL_UINT32(max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS,
                             device.getContinuousConversionTimeMs());
    TEST_ASSERT_EQUAL_UINT32(max31865_cmd::BIAS_SETTLE_EXTRA_US,
                             device.getBiasSettleTimeUs());
}

static void test_status_helpers_and_default_health_aliases() {
    MAX31865 device;
    MAX31865Status ok = MAX31865Status::Ok();
    MAX31865Status err = MAX31865Status::Error(MAX31865Error::InvalidArgument, "bad");

    TEST_ASSERT_TRUE(ok.ok());
    TEST_ASSERT_TRUE(ok.isOk());
    TEST_ASSERT_FALSE(ok.inProgress());
    TEST_ASSERT_FALSE(err.ok());
    TEST_ASSERT_FALSE(device.isInitialized());
    TEST_ASSERT_FALSE(device.isOnline());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MAX31865DriverState::UNINIT),
                            static_cast<uint8_t>(device.driverState()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(device.driverState()),
                            static_cast<uint8_t>(device.healthState()));
}

static void test_settings_status_requires_initialized_driver() {
    MAX31865 device;
    MAX31865Settings settings{};
    MAX31865Status st = device.getSettingsStatus(settings);

    TEST_ASSERT_FALSE(st.ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MAX31865Error::NotInitialized),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_FALSE(device.isInitialized());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MAX31865DriverState::UNINIT),
                            static_cast<uint8_t>(device.driverState()));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fault_status_decodes_documented_bits_only);
    RUN_TEST(test_rtd_code_resistance_and_temperature_roundtrip);
    RUN_TEST(test_pt1000_scaling_uses_configured_reference_and_nominal);
    RUN_TEST(test_timing_helpers_follow_filter_selection_defaults);
    RUN_TEST(test_status_helpers_and_default_health_aliases);
    RUN_TEST(test_settings_status_requires_initialized_driver);
    return UNITY_END();
}
