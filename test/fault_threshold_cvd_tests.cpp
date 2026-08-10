#include "support/DriverFixture.h"
#include "support/TestHarness.h"

#include <limits>
#include <string.h>

using namespace max31865_test;

namespace {

bool faultDecoderMasksReservedBitsAndMapsEveryDocumentedBit()
{
    const MAX31865FaultStatus all = MAX31865::decodeFaultStatus(0xFFU);
    CHECK_EQ(0xFCU, all.raw);
    CHECK(all.any());
    CHECK(all.highThreshold);
    CHECK(all.lowThreshold);
    CHECK(all.refinHigh);
    CHECK(all.refinLow);
    CHECK(all.rtdinLow);
    CHECK(all.overUnderVoltage);

    const MAX31865FaultStatus reserved = MAX31865::decodeFaultStatus(0x03U);
    CHECK_EQ(0U, reserved.raw);
    CHECK(!reserved.any());
    CHECK(!reserved.highThreshold);
    CHECK(!reserved.lowThreshold);
    CHECK(!reserved.refinHigh);
    CHECK(!reserved.refinLow);
    CHECK(!reserved.rtdinLow);
    CHECK(!reserved.overUnderVoltage);

    const uint8_t bits[] = {
        max31865_cmd::FAULT_HIGH_THRESHOLD,
        max31865_cmd::FAULT_LOW_THRESHOLD,
        max31865_cmd::FAULT_REFIN_HIGH,
        max31865_cmd::FAULT_REFIN_LOW,
        max31865_cmd::FAULT_RTDIN_LOW,
        max31865_cmd::FAULT_OVER_UNDER_VOLTAGE};
    for (uint8_t bit : bits) {
        const MAX31865FaultStatus decoded = MAX31865::decodeFaultStatus(bit);
        CHECK_EQ(bit, decoded.raw);
        CHECK(decoded.any());
    }
    return true;
}

bool rawThresholdApiRoundTripsAndRejectsInvalidOrderingBeforeIo()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const MAX31865FaultThresholds thresholds{1234U, 30000U};
    CHECK(fixture.driver.setFaultThresholdsRaw(thresholds, 100U).ok());
    MAX31865FaultThresholds observed{};
    CHECK(fixture.driver.readFaultThresholdsRaw(observed, 20U).ok());
    CHECK_EQ(thresholds.lowCode, observed.lowCode);
    CHECK_EQ(thresholds.highCode, observed.highCode);

    uint8_t high[2] = {};
    uint8_t low[2] = {};
    CHECK(max31865EncodeThreshold(thresholds.highCode, high).ok());
    CHECK(max31865EncodeThreshold(thresholds.lowCode, low).ok());
    CHECK_EQ(high[0], fixture.scripted.device().registerValue(
                          max31865_cmd::REG_HIGH_FAULT_MSB));
    CHECK_EQ(high[1], fixture.scripted.device().registerValue(
                          max31865_cmd::REG_HIGH_FAULT_LSB));
    CHECK_EQ(low[0], fixture.scripted.device().registerValue(
                         max31865_cmd::REG_LOW_FAULT_MSB));
    CHECK_EQ(low[1], fixture.scripted.device().registerValue(
                         max31865_cmd::REG_LOW_FAULT_LSB));

    fixture.scripted.resetLog();
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setFaultThresholdsRaw({200U, 100U}, 20U).code);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver
                 .setFaultThresholdsRaw(
                     {0U, static_cast<uint16_t>(0x8000U)},
                     20U)
                 .code);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    return true;
}

bool engineeringThresholdApisQuantizeWithinOneCode()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.setFaultThresholdsResistance(
              50.0F,
              300.0F,
              100U)
              .ok());
    float lowOhms = -1.0F;
    float highOhms = -1.0F;
    CHECK(fixture.driver.readFaultThresholdsResistance(
              lowOhms,
              highOhms,
              20U)
              .ok());
    const float oneCodeOhms = 400.0F / 32768.0F;
    CHECK_NEAR(50.0F, lowOhms, oneCodeOhms);
    CHECK_NEAR(300.0F, highOhms, oneCodeOhms);

    CHECK(fixture.driver.setFaultThresholdsTemperature(
              -100.0F,
              500.0F,
              100U)
              .ok());
    float lowC = 0.0F;
    float highC = 0.0F;
    CHECK(fixture.driver.readFaultThresholdsTemperature(
              lowC,
              highC,
              20U)
              .ok());
    CHECK_NEAR(-100.0F, lowC, 0.05F);
    CHECK_NEAR(500.0F, highC, 0.05F);

    fixture.scripted.resetLog();
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setFaultThresholdsResistance(
                 200.0F,
                 100.0F,
                 20U)
                 .code);
    // Validate the caller's engineering-unit ordering before quantization.
    // These reversed values round to the same raw code and must still fail.
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setFaultThresholdsResistance(
                 100.0001F,
                 100.0F,
                 20U)
                 .code);
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setFaultThresholdsTemperature(
                 100.0F,
                 -100.0F,
                 20U)
                 .code);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    return true;
}

bool temperatureThresholdEndpointQuantizationRemainsReadable()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.setFaultThresholdsTemperature(
              -200.0F,
              850.0F,
              100U)
              .ok());

    MAX31865FaultThresholds raw{};
    CHECK(fixture.driver.readFaultThresholdsRaw(raw, 20U).ok());
    CHECK(raw.lowCode > 0U);

    float minimumResistance = 0.0F;
    float quantizedResistance = 0.0F;
    CHECK(fixture.driver.temperatureToResistance(
              -200.0F, minimumResistance)
              .ok());
    CHECK(fixture.driver.codeToResistance(
              raw.lowCode, quantizedResistance)
              .ok());
    CHECK(quantizedResistance < minimumResistance);

    // The general CVD inverse remains strict outside its configured domain.
    float strictTemperature = 123.0F;
    CHECK_EQ(
        MAX31865Error::ConversionOutOfRange,
        fixture.driver.resistanceToTemperature(
            quantizedResistance, strictTemperature).code);
    CHECK_NEAR(123.0F, strictTemperature, 0.0F);

    // The typed threshold reader recognizes the exact nearest code generated
    // for a domain endpoint and reports that endpoint transactionally.
    float lowC = 11.0F;
    float highC = 22.0F;
    CHECK(fixture.driver.readFaultThresholdsTemperature(
              lowC,
              highC,
              20U)
              .ok());
    CHECK_NEAR(-200.0F, lowC, 0.0F);
    CHECK_NEAR(850.0F, highC, 0.05F);

    MAX31865Settings settings{};
    CHECK(fixture.driver.readConfiguration(settings, 20U).ok());
    CHECK(settings.lowThresholdTemperatureValid);
    CHECK_NEAR(-200.0F, settings.lowThresholdC, 0.0F);

    // An adjacent raw code is more than the endpoint quantization error and
    // remains genuinely out of range; both caller outputs stay untouched.
    CHECK(fixture.driver.setFaultThresholdsRaw(
              MAX31865FaultThresholds{
                  static_cast<uint16_t>(raw.lowCode - 1U),
                  raw.highCode},
              100U)
              .ok());
    lowC = 33.0F;
    highC = 44.0F;
    CHECK_EQ(
        MAX31865Error::ConversionOutOfRange,
        fixture.driver.readFaultThresholdsTemperature(
            lowC,
            highC,
            20U).code);
    CHECK_NEAR(33.0F, lowC, 0.0F);
    CHECK_NEAR(44.0F, highC, 0.0F);
    return true;
}

bool thresholdReadsPreserveAllOutputsOnTransferFailure()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    MAX31865FaultThresholds raw{0x1111U, 0x2222U};
    CHECK_EQ(MAX31865Error::SpiTransferFailed,
             fixture.driver.readFaultThresholdsRaw(raw, 20U).code);
    CHECK_EQ(0x1111U, raw.lowCode);
    CHECK_EQ(0x2222U, raw.highCode);

    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    float low = 11.0F;
    float high = 22.0F;
    CHECK_EQ(MAX31865Error::SpiTransferFailed,
             fixture.driver.readFaultThresholdsResistance(
                 low,
                 high,
                 20U)
                 .code);
    CHECK_NEAR(11.0F, low, 0.0F);
    CHECK_NEAR(22.0F, high, 0.0F);
    return true;
}

bool latchedFaultReadAndClearAreExplicit()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const uint8_t faults = static_cast<uint8_t>(
        max31865_cmd::FAULT_LOW_THRESHOLD |
        max31865_cmd::FAULT_RTDIN_LOW |
        max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    fixture.scripted.device().forceReadySample(1000U, faults);
    MAX31865FaultStatus observed{};
    const MAX31865Status status = fixture.driver.readFaultStatus(
        observed,
        20U);
    CHECK_EQ(MAX31865Error::DeviceFault, status.code);
    CHECK_EQ(faults, static_cast<uint8_t>(status.detail));
    CHECK_EQ(faults, observed.raw);
    CHECK(observed.lowThreshold);
    CHECK(observed.rtdinLow);
    CHECK(observed.overUnderVoltage);
    CHECK_EQ(1U, fixture.driver.health().faultObservationCount);
    CHECK_EQ(MAX31865DriverState::DEGRADED,
             fixture.driver.health().driverState);

    CHECK(fixture.driver.clearFaults(20U).ok());
    CHECK_EQ(0U, fixture.scripted.device().faultLatch());
    CHECK(fixture.driver.readFaultStatus(observed, 20U).ok());
    CHECK_EQ(0U, observed.raw);
    CHECK_EQ(MAX31865DriverState::READY,
             fixture.driver.health().driverState);
    return true;
}

bool automaticFaultCycleUsesFreshLatchAndRestoresDesiredImage()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const uint8_t source = static_cast<uint8_t>(
        max31865_cmd::FAULT_REFIN_HIGH |
        max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    fixture.scripted.device().setFaultInputs(source);
    MAX31865FaultStatus observed{};
    const MAX31865Status status = fixture.driver.runAutomaticFaultDetection(
        observed,
        100U);
    CHECK_EQ(MAX31865Error::DeviceFault, status.code);
    CHECK_EQ(source, observed.raw);
    CHECK(observed.refinHigh);
    CHECK(observed.overUnderVoltage);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK_EQ(0U, fixture.scripted.device().registerValue(
                     max31865_cmd::REG_CONFIG));
    CHECK_EQ(1U, fixture.driver.health().faultObservationCount);
    CHECK_EQ(1U, fixture.driver.health().referenceFaultObservationCount);
    CHECK_EQ(1U, fixture.driver.health().voltageFaultObservationCount);

    fixture.scripted.device().setFaultInputs(0U);
    CHECK(fixture.driver.runAutomaticFaultDetection(observed, 100U).ok());
    CHECK_EQ(0U, observed.raw);
    CHECK_EQ(MAX31865DriverState::READY,
             fixture.driver.health().driverState);
    return true;
}

bool automaticCycleRejectsLongRcAndManualCycleHonorsIt()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    MAX31865RtdConfig rtd = fixture.driver.rtdConfig();
    rtd.inputFilterTimeConstantUs = 150U;
    fixture.scripted.resetLog();
    CHECK(fixture.driver.setRtdConfig(rtd).ok());
    CHECK_EQ(0U, fixture.scripted.eventCount());

    MAX31865FaultStatus observed{};
    CHECK_EQ(MAX31865Error::UnsupportedCommand,
             fixture.driver.runAutomaticFaultDetection(observed, 100U).code);
    CHECK_EQ(0U, fixture.scripted.eventCount());

    fixture.scripted.device().setFaultInputs(
        max31865_cmd::FAULT_REFIN_LOW);
    CHECK_EQ(MAX31865Error::DeviceFault,
             fixture.driver.runManualFaultDetection(observed, 100U).code);
    CHECK_EQ(max31865_cmd::FAULT_REFIN_LOW, observed.raw);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    bool foundSettle = false;
    const uint32_t expectedSettle =
        150U * max31865_cmd::MANUAL_FAULT_SETTLE_MULTIPLIER;
    for (size_t index = 0U; index < fixture.scripted.eventCount(); ++index) {
        const TransportEvent &event = fixture.scripted.event(index);
        if (event.kind == EventKind::DelayUs &&
            event.argument == expectedSettle) {
            foundSettle = true;
        }
    }
    CHECK(foundSettle);
    return true;
}

bool interruptedManualStepOneIsExplicitlyCompletedDuringCleanup()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        4U,
        MAX31865Error::SpiTransferFailed)));
    MAX31865FaultStatus observed{};
    CHECK_EQ(
        MAX31865Error::SpiTransferFailed,
        fixture.driver.runManualFaultDetection(observed, 100U).code);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK_EQ(
        max31865_cmd::CONFIG_FAULT_CYCLE_NONE,
        static_cast<uint8_t>(
            fixture.scripted.device().registerValue(
                max31865_cmd::REG_CONFIG) &
            max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    CHECK_EQ(MAX31865DriverState::DEGRADED,
             fixture.driver.health().driverState);
    return true;
}

bool expiredAutomaticCycleMakesImmediateIdleRestoreAttempt()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    // The second frame enables VBIAS. Let that write commit exactly as the
    // primary deadline expires; cleanup must not reuse the expired context and
    // silently skip the CONFIG restore.
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::Transfer,
        2U,
        10000U)));

    MAX31865FaultStatus observed{};
    observed.raw = 0xA5U;
    observed.highThreshold = true;
    const MAX31865Status status = fixture.driver.runAutomaticFaultDetection(
        observed,
        10U);
    CHECK_EQ(MAX31865Error::OperationTimeout, status.code);
    CHECK_EQ(0xA5U, observed.raw);
    CHECK(observed.highThreshold);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK_EQ(0U,
             static_cast<uint8_t>(
                 fixture.scripted.device().registerValue(
                     max31865_cmd::REG_CONFIG) &
                 (max31865_cmd::CONFIG_BIAS |
                  max31865_cmd::CONFIG_FAULT_CYCLE_MASK)));
    CHECK_EQ(7U,
             fixture.scripted.matchingEventCount(EventKind::Transfer));
    bool sawNoWaitCleanupLock = false;
    for (size_t index = 0U; index < fixture.scripted.eventCount(); ++index) {
        const TransportEvent &event = fixture.scripted.event(index);
        if (event.kind == EventKind::LockBus && event.matchingOrdinal >= 3U) {
            CHECK_EQ(0U, event.argument);
            sawNoWaitCleanupLock = true;
        }
    }
    CHECK(sawNoWaitCleanupLock);
    const MAX31865Health health = fixture.driver.health();
    CHECK(health.configurationKnown);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(MAX31865DriverState::DEGRADED, health.driverState);
    return true;
}

bool expiredManualStepOneIsQuarantinedUntilTimedRecovery()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    // Manual step 1 commits on the third frame. Expire the primary call there,
    // while FORCE- still needs its mandatory timing before step 2 is safe.
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::Transfer,
        3U,
        10000U)));

    MAX31865FaultStatus observed{};
    observed.raw = 0x5AU;
    const MAX31865Status status = fixture.driver.runManualFaultDetection(
        observed,
        10U);
    CHECK_EQ(MAX31865Error::RestoreFailed, status.code);
    const int32_t expectedDetail = static_cast<int32_t>(
        (static_cast<uint32_t>(MAX31865Error::OperationTimeout) << 8U) |
        static_cast<uint32_t>(MAX31865Error::OperationTimeout));
    CHECK_EQ(expectedDetail, status.detail);
    CHECK_EQ(0x5AU, observed.raw);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    CHECK_EQ(
        max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1,
        static_cast<uint8_t>(
            fixture.scripted.device().registerValue(
                max31865_cmd::REG_CONFIG) &
            max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    // The zero-wait cleanup only observes D3:D2=10. It must not issue step 2
    // before the required FORCE- open settling interval can be honored.
    CHECK_EQ(4U,
             fixture.scripted.matchingEventCount(EventKind::Transfer));
    bool sawNoWaitCleanupLock = false;
    for (size_t index = 0U; index < fixture.scripted.eventCount(); ++index) {
        const TransportEvent &event = fixture.scripted.event(index);
        if (event.kind == EventKind::LockBus && event.matchingOrdinal == 4U) {
            CHECK_EQ(0U, event.argument);
            sawNoWaitCleanupLock = true;
        }
    }
    CHECK(sawNoWaitCleanupLock);
    const MAX31865Health failed = fixture.driver.health();
    CHECK(!failed.configurationKnown);
    CHECK_EQ(1U, failed.operationTimeoutCount);
    CHECK_EQ(1U, failed.trackedFailureCount);

    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    CHECK(fixture.driver.recover(100U).ok());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK_EQ(
        max31865_cmd::CONFIG_FAULT_CYCLE_NONE,
        static_cast<uint8_t>(
            fixture.scripted.device().registerValue(
                max31865_cmd::REG_CONFIG) &
            max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    CHECK(fixture.driver.health().configurationKnown);
    return true;
}

bool faultCycleOutputWaitsForSuccessfulRestore()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.device().setFaultInputs(
        max31865_cmd::FAULT_REFIN_HIGH);
    fixture.scripted.resetLog();
    // The fifth frame reads and decodes FAULT_STATUS; fail the first cleanup
    // CONFIG read so the decoded candidate must remain private.
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        6U,
        MAX31865Error::SpiTransferFailed)));

    MAX31865FaultStatus observed{};
    observed.raw = 0xA5U;
    observed.lowThreshold = true;
    const MAX31865Status status = fixture.driver.runAutomaticFaultDetection(
        observed,
        100U);
    CHECK_EQ(MAX31865Error::RestoreFailed, status.code);
    const int32_t expectedDetail = static_cast<int32_t>(
        (static_cast<uint32_t>(MAX31865Error::DeviceFault) << 8U) |
        static_cast<uint32_t>(MAX31865Error::SpiTransferFailed));
    CHECK_EQ(expectedDetail, status.detail);
    CHECK_EQ(0xA5U, observed.raw);
    CHECK(observed.lowThreshold);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.faultObservationCount);
    CHECK_EQ(1U, health.spiTransferFailureCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    return true;
}

bool expiredRegisterReadbackRestoresSavedByteImmediately()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const uint8_t saved = fixture.scripted.device().registerValue(
        max31865_cmd::REG_LOW_FAULT_LSB);
    fixture.scripted.resetLog();
    // The second transfer writes the destructive test pattern. Let it commit
    // at the deadline and prove the saved byte is still restored with no-wait
    // bus callbacks.
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::Transfer,
        2U,
        10000U)));

    uint8_t observed = 0xA5U;
    const MAX31865Status status = fixture.driver.registerReadbackTest(
        observed,
        10U);
    CHECK_EQ(MAX31865Error::OperationTimeout, status.code);
    CHECK_EQ(0xA5U, observed);
    CHECK_EQ(saved,
             fixture.scripted.device().registerValue(
                 max31865_cmd::REG_LOW_FAULT_LSB));
    CHECK_EQ(5U,
             fixture.scripted.matchingEventCount(EventKind::Transfer));
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    const MAX31865Health health = fixture.driver.health();
    CHECK(health.configurationKnown);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    return true;
}

bool expiredRegisterReadbackReportsCleanupFailureComposition()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::Transfer,
        2U,
        10000U)));
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        3U,
        MAX31865Error::SpiTransferFailed)));

    uint8_t observed = 0xA5U;
    const MAX31865Status status = fixture.driver.registerReadbackTest(
        observed,
        10U);
    CHECK_EQ(MAX31865Error::RestoreFailed, status.code);
    const int32_t expectedDetail = static_cast<int32_t>(
        (static_cast<uint32_t>(MAX31865Error::OperationTimeout) << 8U) |
        static_cast<uint32_t>(MAX31865Error::SpiTransferFailed));
    CHECK_EQ(expectedDetail, status.detail);
    CHECK_EQ(0xA5U, observed);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    const MAX31865Health health = fixture.driver.health();
    CHECK(!health.configurationKnown);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.spiTransferFailureCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    return true;
}

bool deadlineCrossingAtReadbackCleanupBoundaryCannotReportSuccess()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const uint8_t saved = fixture.scripted.device().registerValue(
        max31865_cmd::REG_LOW_FAULT_LSB);
    fixture.scripted.resetLog();

    // A locked transaction currently samples nowMs thirteen times. The
    // operation-start sample plus three successful primary frames put the
    // cleanup-boundary check at relative call 41. Advance only that clock read
    // so every primary frame is valid but cleanup begins exactly at expiry.
    const uint32_t cleanupBoundaryCall =
        fixture.scripted.nowMsCallCount() + 41U;
    fixture.scripted.setNowMsAdvanceUsOnCall(
        cleanupBoundaryCall,
        100000U);

    uint8_t observed = 0xA5U;
    const MAX31865Status status = fixture.driver.registerReadbackTest(
        observed,
        100U);
    CHECK_EQ(MAX31865Error::OperationTimeout, status.code);
    CHECK_EQ(0xA5U, observed);
    CHECK_EQ(saved,
             fixture.scripted.device().registerValue(
                 max31865_cmd::REG_LOW_FAULT_LSB));
    CHECK_EQ(6U,
             fixture.scripted.matchingEventCount(EventKind::Transfer));
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    const MAX31865Health health = fixture.driver.health();
    CHECK(health.configurationKnown);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    return true;
}

bool cleanupBoundaryPreservesPrimaryFailureAndTimeoutProvenance()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const uint8_t saved = fixture.scripted.device().registerValue(
        max31865_cmd::REG_LOW_FAULT_LSB);
    // Override only the destructive pattern read (the second read of this
    // address) so primary work produces a meaningful verification failure.
    fixture.scripted.device().setRegisterReadOverride(
        max31865_cmd::REG_LOW_FAULT_LSB,
        saved,
        2U);
    fixture.scripted.resetLog();
    const uint32_t cleanupBoundaryCall =
        fixture.scripted.nowMsCallCount() + 41U;
    fixture.scripted.setNowMsAdvanceUsOnCall(
        cleanupBoundaryCall,
        100000U);

    uint8_t observed = 0xA5U;
    const MAX31865Status status = fixture.driver.registerReadbackTest(
        observed,
        100U);
    CHECK_EQ(MAX31865Error::RegisterVerifyFailed, status.code);
    CHECK_EQ(0xA5U, observed);
    CHECK_EQ(saved,
             fixture.scripted.device().registerValue(
                 max31865_cmd::REG_LOW_FAULT_LSB));
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    const MAX31865Health health = fixture.driver.health();
    CHECK(health.configurationKnown);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    return true;
}

bool diagnosticRegisterHelpersRemainTypedAndTransactional()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK_EQ(MAX31865Error::UnsupportedCommand,
             fixture.driver.writeRegister(
                 max31865_cmd::REG_RTD_MSB,
                 0x12U,
                 20U)
                 .code);
    CHECK_EQ(MAX31865Error::UnsupportedCommand,
             fixture.driver.writeRegister(
                 max31865_cmd::REG_CONFIG,
                 max31865_cmd::CONFIG_ONE_SHOT,
                 20U)
                 .code);
    CHECK_EQ(MAX31865Error::RegisterAddressInvalid,
             fixture.driver.writeRegister(8U, 0U, 20U).code);
    CHECK_EQ(0U, fixture.scripted.eventCount());

    const uint8_t persistent = static_cast<uint8_t>(
        max31865_cmd::CONFIG_BIAS |
        max31865_cmd::CONFIG_3WIRE |
        max31865_cmd::CONFIG_FILTER_50HZ);
    uint8_t readBack = 0U;
    CHECK(fixture.driver.writeRegisterVerified(
              max31865_cmd::REG_CONFIG,
              persistent,
              readBack,
              100U)
              .ok());
    CHECK_EQ(persistent, readBack);
    MAX31865Settings settings{};
    CHECK(fixture.driver.readConfiguration(settings, 20U).ok());
    CHECK_EQ(MAX31865WireMode::ThreeWire, settings.deviceConfig.wireMode);
    CHECK_EQ(MAX31865Filter::Hz50, settings.deviceConfig.filter);
    CHECK(settings.deviceConfig.biasEnabled);
    return true;
}

bool registerDumpIsNamedCompleteAndAcknowledgesBufferedRtd()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.device().forceReadySample(0x1234U);
    MAX31865RegisterDump dump[max31865_cmd::NUM_REGISTERS]{};
    size_t count = 99U;
    CHECK(fixture.driver.dumpRegisters(
              dump,
              max31865_cmd::NUM_REGISTERS,
              count,
              20U)
              .ok());
    CHECK_EQ(max31865_cmd::NUM_REGISTERS, count);
    for (size_t index = 0U; index < count; ++index) {
        CHECK_EQ(static_cast<uint8_t>(index), dump[index].address);
        CHECK_CSTR_EQ(max31865RegisterName(static_cast<uint8_t>(index)),
                      dump[index].name);
        CHECK_EQ(fixture.scripted.device().registerValue(
                     static_cast<uint8_t>(index)),
                 dump[index].value);
    }
    CHECK(!fixture.scripted.device().dataReady());
    CHECK_EQ(1U, fixture.scripted.device().rtdReadAcknowledgeCount());

    count = 77U;
    fixture.scripted.resetLog();
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.dumpRegisters(dump, 7U, count, 20U).code);
    CHECK_EQ(77U, count);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    return true;
}

bool localCodeRatioAndResistanceHelpersAreBounded()
{
    MAX31865 driver;
    float value = -1.0F;
    CHECK(MAX31865::codeToRatio(0U, value).ok());
    CHECK_NEAR(0.0F, value, 0.0F);
    CHECK(MAX31865::codeToRatio(8192U, value).ok());
    CHECK_NEAR(0.25F, value, 1.0e-7F);
    CHECK(MAX31865::codeToRatio(max31865_cmd::ADC_CODE_MAX, value).ok());
    CHECK_NEAR(32767.0F / 32768.0F, value, 1.0e-7F);
    value = 123.0F;
    CHECK_EQ(MAX31865Error::ConversionOutOfRange,
             MAX31865::codeToRatio(0x8000U, value).code);
    CHECK_NEAR(123.0F, value, 0.0F);

    CHECK(driver.codeToResistance(8192U, value).ok());
    CHECK_NEAR(100.0F, value, 0.001F);
    uint16_t code = 0U;
    CHECK(driver.resistanceToCode(100.0F, code).ok());
    CHECK_EQ(8192U, code);
    CHECK(driver.resistanceToCode(0.0F, code).ok());
    CHECK_EQ(0U, code);
    code = 0xA5A5U;
    CHECK_EQ(MAX31865Error::ConversionOutOfRange,
             driver.resistanceToCode(400.0F, code).code);
    CHECK_EQ(0xA5A5U, code);
    return true;
}

bool iec60751ForwardValuesAndInverseRoundTripsAgree()
{
    MAX31865 driver;
    struct Reference {
        float temperatureC;
        float resistanceOhms;
    };
    const Reference references[] = {
        {-200.0F, 18.5201F},
        {-100.0F, 60.2558F},
        {0.0F, 100.0F},
        {100.0F, 138.5055F},
        {400.0F, 247.0920F},
        {850.0F, 390.4811F}};
    for (const Reference &reference : references) {
        float resistance = -1.0F;
        CHECK(driver.temperatureToResistance(
                  reference.temperatureC,
                  resistance)
                  .ok());
        CHECK_NEAR(reference.resistanceOhms, resistance, 0.003F);
        float temperature = -999.0F;
        CHECK(driver.resistanceToTemperature(resistance, temperature).ok());
        CHECK_NEAR(reference.temperatureC, temperature, 0.002F);
    }

    for (int32_t integerC = -200; integerC <= 850; integerC += 7) {
        const float input = static_cast<float>(integerC) + 0.25F;
        if (input > 850.0F) {
            continue;
        }
        float resistance = 0.0F;
        float recovered = 0.0F;
        CHECK(driver.temperatureToResistance(input, resistance).ok());
        CHECK(driver.resistanceToTemperature(resistance, recovered).ok());
        CHECK_NEAR(input, recovered, 0.003F);
    }
    return true;
}

bool localConversionsRejectNonFiniteAndOutOfDomainTransactionally()
{
    MAX31865 driver;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    float output = 123.0F;
    CHECK_EQ(MAX31865Error::InvalidArgument,
             driver.temperatureToResistance(nan, output).code);
    CHECK_NEAR(123.0F, output, 0.0F);
    CHECK_EQ(MAX31865Error::InvalidArgument,
             driver.resistanceToTemperature(infinity, output).code);
    CHECK_NEAR(123.0F, output, 0.0F);
    CHECK_EQ(MAX31865Error::ConversionOutOfRange,
             driver.temperatureToResistance(-200.1F, output).code);
    CHECK_NEAR(123.0F, output, 0.0F);
    CHECK_EQ(MAX31865Error::ConversionOutOfRange,
             driver.temperatureToResistance(850.1F, output).code);
    CHECK_NEAR(123.0F, output, 0.0F);
    CHECK_EQ(MAX31865Error::ConversionOutOfRange,
             driver.resistanceToTemperature(10.0F, output).code);
    CHECK_NEAR(123.0F, output, 0.0F);
    return true;
}

bool maximumCodeRoundTripsAcrossNonBinaryReferenceValues()
{
    const float references[] = {350.1F, 430.123F, 4000.1F, 9999.9F};
    for (float reference : references)
    {
        DriverFixture fixture;
        fixture.config.rtd.referenceResistorOhms = reference;
        fixture.config.rtd.nominalResistanceOhms = 1.0F;
        CHECK(fixture.begin().ok());
        float resistance = 0.0F;
        CHECK(fixture.driver.codeToResistance(
                  max31865_cmd::ADC_CODE_MAX, resistance)
                  .ok());
        uint16_t recovered = 0U;
        CHECK(fixture.driver.resistanceToCode(resistance, recovered).ok());
        CHECK_EQ(max31865_cmd::ADC_CODE_MAX, recovered);
    }
    return true;
}

bool customPt1000ScalingAndTimingHelpersFollowConfiguration()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    MAX31865RtdConfig config = fixture.driver.rtdConfig();
    config.referenceResistorOhms = 4300.0F;
    config.nominalResistanceOhms = 1000.0F;
    config.inputFilterTimeConstantUs = 123U;
    fixture.scripted.resetLog();
    CHECK(fixture.driver.setRtdConfig(config).ok());
    CHECK_EQ(0U, fixture.scripted.eventCount());
    const MAX31865RtdConfig observed = fixture.driver.rtdConfig();
    CHECK_NEAR(4300.0F, observed.referenceResistorOhms, 0.0F);
    CHECK_NEAR(1000.0F, observed.nominalResistanceOhms, 0.0F);
    float resistance = 0.0F;
    uint16_t code = 0U;
    CHECK(fixture.driver.resistanceToCode(1000.0F, code).ok());
    CHECK(fixture.driver.codeToResistance(code, resistance).ok());
    CHECK_NEAR(1000.0F, resistance, 0.14F);
    float temperature = 0.0F;
    CHECK(fixture.driver.resistanceToTemperature(1000.0F, temperature).ok());
    CHECK_NEAR(0.0F, temperature, 0.002F);
    CHECK_EQ(2292U, fixture.driver.biasSettleTimeUs());

    CHECK_EQ(max31865_cmd::SINGLE_CONVERSION_60HZ_MS,
             fixture.driver.singleConversionTimeMs());
    CHECK_EQ(max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS,
             fixture.driver.continuousConversionTimeMs());
    CHECK(fixture.driver.setFilter(MAX31865Filter::Hz50, 100U).ok());
    CHECK_EQ(max31865_cmd::SINGLE_CONVERSION_50HZ_MS,
             fixture.driver.singleConversionTimeMs());
    CHECK_EQ(max31865_cmd::CONTINUOUS_CONVERSION_50HZ_MS,
             fixture.driver.continuousConversionTimeMs());
    return true;
}

bool customMonotonicCoefficientsMayContainZeroTerms()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    MAX31865RtdConfig config = fixture.driver.rtdConfig();
    config.minimumTemperatureC = 0.0F;
    config.maximumTemperatureC = 100.0F;
    config.coefficients.a = 0.004F;
    config.coefficients.b = 0.0F;
    config.coefficients.c = 0.0F;
    CHECK(fixture.driver.setRtdConfig(config).ok());

    float resistance = 0.0F;
    float temperature = 0.0F;
    CHECK(fixture.driver.temperatureToResistance(100.0F, resistance).ok());
    CHECK_NEAR(140.0F, resistance, 0.001F);
    CHECK(fixture.driver.resistanceToTemperature(resistance, temperature).ok());
    CHECK_NEAR(100.0F, temperature, 0.002F);

    // Integer/grid samples rise, but the analytic derivative becomes negative
    // just below the upper endpoint. Validation must reject the hidden turn.
    config.nominalResistanceOhms = 1.0F;
    config.minimumTemperatureC = 0.0F;
    config.maximumTemperatureC = 64.0F;
    config.coefficients.a = 0.1278F;
    config.coefficients.b = -0.001F;
    config.coefficients.c = 0.0F;
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setRtdConfig(config).code);

    config.coefficients.a = 0.0F;
    config.coefficients.b = 0.0F;
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setRtdConfig(config).code);

    // A zero derivative at one endpoint does not make an otherwise strictly
    // increasing curve ambiguous. Analytic validation must accept this
    // tangent-zero custom profile and the inverse must remain well-defined.
    MAX31865RtdConfig tangent = fixture.driver.rtdConfig();
    tangent.referenceResistorOhms = 350.0F;
    tangent.nominalResistanceOhms = 100.0F;
    tangent.minimumTemperatureC = 0.0F;
    tangent.maximumTemperatureC = 1.0F;
    tangent.coefficients.a = 0.0F;
    tangent.coefficients.b = 0.001F;
    tangent.coefficients.c = 0.0F;
    CHECK(fixture.driver.setRtdConfig(tangent).ok());
    CHECK(fixture.driver.temperatureToResistance(1.0F, resistance).ok());
    CHECK_NEAR(100.1F, resistance, 0.001F);
    CHECK(fixture.driver.resistanceToTemperature(resistance, temperature).ok());
    CHECK_NEAR(1.0F, temperature, 0.002F);

    // Extreme finite coefficients produce one very small derivative-extremum
    // root. A naive quadratic formula loses it to cancellation and can accept
    // the resulting narrow nonmonotonic interval between validation samples.
    MAX31865RtdConfig extreme = fixture.driver.rtdConfig();
    extreme.referenceResistorOhms = 400.0F;
    extreme.nominalResistanceOhms = 100.0F;
    extreme.minimumTemperatureC = -2.1e-12F;
    extreme.maximumTemperatureC = 1.0e-12F;
    extreme.coefficients.a = 3.0e8F;
    extreme.coefficients.b = 3.0e20F;
    extreme.coefficients.c = -1.0e30F;
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.setRtdConfig(extreme).code);
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"fault decoder", faultDecoderMasksReservedBitsAndMapsEveryDocumentedBit},
        {"raw thresholds", rawThresholdApiRoundTripsAndRejectsInvalidOrderingBeforeIo},
        {"engineering thresholds", engineeringThresholdApisQuantizeWithinOneCode},
        {"temperature threshold endpoint quantization",
         temperatureThresholdEndpointQuantizationRemainsReadable},
        {"transactional threshold reads", thresholdReadsPreserveAllOutputsOnTransferFailure},
        {"fault read/clear", latchedFaultReadAndClearAreExplicit},
        {"automatic fault cycle", automaticFaultCycleUsesFreshLatchAndRestoresDesiredImage},
        {"manual long-RC fault cycle", automaticCycleRejectsLongRcAndManualCycleHonorsIt},
        {"manual fault cleanup", interruptedManualStepOneIsExplicitlyCompletedDuringCleanup},
        {"expired automatic fault cleanup",
         expiredAutomaticCycleMakesImmediateIdleRestoreAttempt},
        {"expired manual FORCE- quarantine",
         expiredManualStepOneIsQuarantinedUntilTimedRecovery},
        {"fault-cycle output restore gate",
         faultCycleOutputWaitsForSuccessfulRestore},
        {"expired register-readback restore",
         expiredRegisterReadbackRestoresSavedByteImmediately},
        {"register-readback cleanup composition",
         expiredRegisterReadbackReportsCleanupFailureComposition},
        {"readback cleanup-boundary timeout",
         deadlineCrossingAtReadbackCleanupBoundaryCannotReportSuccess},
        {"cleanup-boundary failure provenance",
         cleanupBoundaryPreservesPrimaryFailureAndTimeoutProvenance},
        {"typed register diagnostics", diagnosticRegisterHelpersRemainTypedAndTransactional},
        {"register dump", registerDumpIsNamedCompleteAndAcknowledgesBufferedRtd},
        {"ratio/resistance helpers", localCodeRatioAndResistanceHelpersAreBounded},
        {"IEC 60751 round trips", iec60751ForwardValuesAndInverseRoundTripsAgree},
        {"local conversion bounds", localConversionsRejectNonFiniteAndOutOfDomainTransactionally},
        {"maximum-code round trips", maximumCodeRoundTripsAcrossNonBinaryReferenceValues},
        {"PT1000 and timing", customPt1000ScalingAndTimingHelpersFollowConfiguration},
        {"custom coefficient zero terms",
         customMonotonicCoefficientsMayContainZeroTerms}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
