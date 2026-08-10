#include "support/DriverFixture.h"
#include "support/TestHarness.h"

#include <string.h>

using namespace max31865_test;

namespace {

bool probeReturnsCompleteRawImageWithoutHealthAccounting()
{
    DriverFixture fixture;
    fixture.config.initialDeviceConfig.wireMode = MAX31865WireMode::ThreeWire;
    fixture.config.initialDeviceConfig.filter = MAX31865Filter::Hz50;
    fixture.config.initialDeviceConfig.thresholds = {321U, 23456U};
    CHECK(fixture.begin().ok());
    const MAX31865Health before = fixture.driver.health();
    MAX31865DeviceInfo info{};
    CHECK(fixture.driver.probe(info).ok());
    CHECK_EQ(static_cast<uint8_t>(
                 max31865_cmd::CONFIG_3WIRE |
                 max31865_cmd::CONFIG_FILTER_50HZ),
             info.rawConfig);
    uint8_t high[2] = {};
    uint8_t low[2] = {};
    CHECK(max31865EncodeThreshold(23456U, high).ok());
    CHECK(max31865EncodeThreshold(321U, low).ok());
    CHECK_EQ(static_cast<uint16_t>(
                 (static_cast<uint16_t>(high[0]) << 8U) | high[1]),
             info.rawHighThresholdRegister);
    CHECK_EQ(static_cast<uint16_t>(
                 (static_cast<uint16_t>(low[0]) << 8U) | low[1]),
             info.rawLowThresholdRegister);
    CHECK_EQ(0U, info.rawFaultStatus);
    CHECK(info.configurationMatches);
    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(before.trackedSuccessCount, after.trackedSuccessCount);
    CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
    CHECK(fixture.driver.lastOperationStatus().ok());
    return true;
}

bool passiveHealthControlsPreserveFailureStreakAndState()
{
    DriverFixture fixture;
    fixture.config.offlineThreshold = 3U;
    CHECK(fixture.begin().ok());
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setOfflineThreshold(0U).code);
    CHECK_EQ(3U, fixture.driver.health().offlineThreshold);
    CHECK(fixture.driver.setOfflineThreshold(2U).ok());
    CHECK_EQ(2U, fixture.driver.health().offlineThreshold);

    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    uint8_t value = 0U;
    CHECK_EQ(MAX31865Error::SpiTransferFailed,
             fixture.driver.readRegister(0U, value, 20U).code);
    CHECK_EQ(1U, fixture.driver.health().consecutiveFailures);
    CHECK_EQ(1U, fixture.driver.health().trackedFailureCount);
    CHECK_EQ(1U, fixture.driver.health().spiTransferFailureCount);
    CHECK(fixture.driver.setOfflineThreshold(1U).ok());
    CHECK_EQ(MAX31865DriverState::OFFLINE,
             fixture.driver.health().driverState);

    fixture.driver.clearLifetimeCounters();
    const MAX31865Health cleared = fixture.driver.health();
    CHECK_EQ(0U, cleared.trackedSuccessCount);
    CHECK_EQ(0U, cleared.trackedFailureCount);
    CHECK_EQ(0U, cleared.spiTransferFailureCount);
    CHECK_EQ(1U, cleared.consecutiveFailures);
    CHECK_EQ(MAX31865DriverState::OFFLINE, cleared.driverState);

    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    CHECK(fixture.driver.readRegister(0U, value, 20U).ok());
    CHECK_EQ(0U, fixture.driver.health().consecutiveFailures);
    CHECK_EQ(MAX31865DriverState::READY,
             fixture.driver.health().driverState);
    return true;
}

bool convenienceConfigurationMethodsShareTheTypedEngine()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.configureMeasurement(
              MAX31865WireMode::TwoWire,
              MAX31865Filter::Hz50,
              100U)
              .ok());
    CHECK(fixture.driver.setBias(true, 100U).ok());
    CHECK(fixture.driver.setWireMode(MAX31865WireMode::FourWire, 100U).ok());
    CHECK(fixture.driver.setFilter(MAX31865Filter::Hz60, 100U).ok());
    MAX31865Settings settings{};
    CHECK(fixture.driver.readConfiguration(settings, 20U).ok());
    CHECK_EQ(MAX31865WireMode::FourWire, settings.deviceConfig.wireMode);
    CHECK_EQ(MAX31865Filter::Hz60, settings.deviceConfig.filter);
    CHECK(settings.deviceConfig.biasEnabled);

    fixture.scripted.resetLog();
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setWireMode(
                 static_cast<MAX31865WireMode>(0xFFU),
                 20U)
                 .code);
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setFilter(
                 static_cast<MAX31865Filter>(0xFFU),
                 20U)
                 .code);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    return true;
}

bool bulkRegisterReadsValidateAndCommitTransactionally()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.device().forceReadySample(0x1234U);
    uint8_t registers[max31865_cmd::NUM_REGISTERS] = {};
    CHECK(fixture.driver.readRegisters(
              max31865_cmd::REG_CONFIG,
              registers,
              sizeof(registers),
              20U)
              .ok());
    CHECK_EQ(fixture.scripted.device().registerValue(
                 max31865_cmd::REG_CONFIG),
             registers[max31865_cmd::REG_CONFIG]);
    CHECK_EQ(0x24U, registers[max31865_cmd::REG_RTD_MSB]);
    CHECK_EQ(0x68U, registers[max31865_cmd::REG_RTD_LSB]);
    CHECK_EQ(1U, fixture.scripted.device().rtdReadAcknowledgeCount());

    memset(registers, 0xA5, sizeof(registers));
    fixture.scripted.resetLog();
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.readRegisters(0U, nullptr, 1U, 0U).code);
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.readRegisters(0U, registers, 0U, 20U).code);
    CHECK_EQ(MAX31865Error::RegisterAddressInvalid,
             fixture.driver.readRegisters(7U, registers, 2U, 20U).code);
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.readRegisters(0U, registers, 2U, 0U).code);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    for (uint8_t value : registers) {
        CHECK_EQ(0xA5U, value);
    }
    return true;
}

bool persistentSingleRegisterWritesMaskDefinedBitsAndUpdateDesiredImage()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.writeRegister(
              max31865_cmd::REG_HIGH_FAULT_MSB,
              0xF0U,
              100U)
              .ok());
    uint8_t readBack = 0xFFU;
    CHECK(fixture.driver.writeRegisterVerified(
              max31865_cmd::REG_HIGH_FAULT_LSB,
              0x35U,
              readBack,
              100U)
              .ok());
    CHECK_EQ(0x34U, readBack);
    MAX31865FaultThresholds thresholds{};
    CHECK(fixture.driver.readFaultThresholdsRaw(thresholds, 20U).ok());
    CHECK_EQ(static_cast<uint16_t>(0xF034U >> 1U), thresholds.highCode);
    CHECK_EQ(0U, thresholds.lowCode);
    return true;
}

bool writableDefaultsAndCommunicationTestRestoreStableState()
{
    DriverFixture fixture;
    fixture.config.initialDeviceConfig.wireMode = MAX31865WireMode::ThreeWire;
    fixture.config.initialDeviceConfig.filter = MAX31865Filter::Hz50;
    fixture.config.initialDeviceConfig.biasEnabled = true;
    fixture.config.initialDeviceConfig.thresholds = {1000U, 20000U};
    CHECK(fixture.begin().ok());
    fixture.scripted.device().forceReadySample(
        1500U,
        max31865_cmd::FAULT_REFIN_LOW);
    CHECK(fixture.driver.restoreWritableDefaults(100U).ok());
    MAX31865Settings settings{};
    CHECK(fixture.driver.readConfiguration(settings, 20U).ok());
    CHECK_EQ(MAX31865WireMode::FourWire, settings.deviceConfig.wireMode);
    CHECK_EQ(MAX31865Filter::Hz60, settings.deviceConfig.filter);
    CHECK(!settings.deviceConfig.biasEnabled);
    CHECK_EQ(0U, settings.deviceConfig.thresholds.lowCode);
    CHECK_EQ(max31865_cmd::ADC_CODE_MAX,
             settings.deviceConfig.thresholds.highCode);
    CHECK_EQ(0U, fixture.scripted.device().faultLatch());

    const uint8_t saved = fixture.scripted.device().registerValue(
        max31865_cmd::REG_LOW_FAULT_LSB);
    uint8_t observed = 0U;
    CHECK(fixture.driver.registerReadbackTest(observed, 100U).ok());
    CHECK_EQ(static_cast<uint8_t>(
                 (saved ^ 0xA8U) &
                 max31865_cmd::THRESHOLD_LSB_DEFINED_MASK),
             observed);
    CHECK_EQ(saved, fixture.scripted.device().registerValue(
                        max31865_cmd::REG_LOW_FAULT_LSB));
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    return true;
}

bool temperatureCodeHelperRoundTripsAcrossBothCvdBranches()
{
    MAX31865 driver;
    const float inputs[] = {-150.0F, -1.0F, 0.0F, 250.0F, 800.0F};
    for (float input : inputs) {
        uint16_t code = 0U;
        CHECK(driver.temperatureToCode(input, code).ok());
        float resistance = 0.0F;
        float recovered = 0.0F;
        CHECK(driver.codeToResistance(code, resistance).ok());
        CHECK(driver.resistanceToTemperature(resistance, recovered).ok());
        CHECK_NEAR(input, recovered, 0.04F);
    }
    uint16_t output = 0xA5A5U;
    CHECK_EQ(MAX31865Error::ConversionOutOfRange,
             driver.temperatureToCode(851.0F, output).code);
    CHECK_EQ(0xA5A5U, output);
    return true;
}

bool sampleWithoutNowUsLeavesTimestampExplicitlyInvalid()
{
    DriverFixture fixture(true, true, false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8192U));
    MAX31865Sample sample{};
    CHECK(fixture.driver.readOneShot(sample, 100U).ok());
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_DATA_VALID) != 0U);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_READ_TIMESTAMP) == 0U);
    CHECK(!fixture.driver.health().hasLastSampleTimestamp);
    return true;
}

bool dedicatedBusTransportNeverInvokesLockCallbacks()
{
    DriverFixture fixture(true, false, true);
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    uint8_t value = 0U;
    CHECK(fixture.driver.readRegister(0U, value, 20U).ok());
    CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::LockBus));
    CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::UnlockBus));
    CHECK_EQ(1U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"successful probe", probeReturnsCompleteRawImageWithoutHealthAccounting},
        {"health control semantics", passiveHealthControlsPreserveFailureStreakAndState},
        {"configuration convenience methods",
         convenienceConfigurationMethodsShareTheTypedEngine},
        {"bulk register reads", bulkRegisterReadsValidateAndCommitTransactionally},
        {"single register writes",
         persistentSingleRegisterWritesMaskDefinedBitsAndUpdateDesiredImage},
        {"defaults and communication test",
         writableDefaultsAndCommunicationTestRestoreStableState},
        {"temperature-to-code", temperatureCodeHelperRoundTripsAcrossBothCvdBranches},
        {"optional sample timestamp", sampleWithoutNowUsLeavesTimestampExplicitlyInvalid},
        {"dedicated bus", dedicatedBusTransportNeverInvokesLockCallbacks}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
