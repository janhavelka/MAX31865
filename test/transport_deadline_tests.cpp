#include "support/DriverFixture.h"
#include "support/TestHarness.h"

#include <string.h>

using namespace max31865_test;

namespace {

bool writeRawConfig(
    ScriptedMax31865Transport &scripted,
    uint8_t value)
{
    const uint8_t tx[2] = {
        static_cast<uint8_t>(
            max31865_cmd::WRITE_BIT | max31865_cmd::REG_CONFIG),
        value};
    uint8_t rx[2]{};
    if (!scripted.setChipSelect(true).ok())
    {
        return false;
    }
    const MAX31865Status transfer = scripted.transfer(
        tx, rx, sizeof(tx), 100U);
    const MAX31865Status deasserted = scripted.setChipSelect(false);
    return transfer.ok() && deasserted.ok();
}

bool constructorAndEndAreZeroIoAndFullyResetState()
{
    DriverFixture fixture;
    const MAX31865Health initial = fixture.driver.health();
    CHECK_EQ(MAX31865State::Uninitialized, initial.state);
    CHECK_EQ(MAX31865DriverState::UNINIT, initial.driverState);
    CHECK(!initial.online);
    CHECK(!initial.configurationKnown);
    CHECK_EQ(5U, initial.offlineThreshold);
    CHECK_EQ(0U, initial.consecutiveFailures);
    CHECK_EQ(0U, initial.trackedSuccessCount);
    CHECK_EQ(0U, initial.trackedFailureCount);
    CHECK(initial.lastOperation.ok());
    CHECK_EQ(250U, fixture.driver.defaultOperationTimeoutMs());

    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::NotInitialized,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 10U)
                 .code);
    CHECK_EQ(0xA5U, output);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    fixture.driver.tick(123U);
    fixture.driver.end();
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(MAX31865State::Uninitialized, fixture.driver.state());
    CHECK(fixture.driver.lastOperationStatus().ok());
    return true;
}

bool invalidBeginConfigurationsPerformNoCallbacks()
{
    {
        DriverFixture fixture;
        fixture.config.transport.transfer = nullptr;
        CHECK_EQ(MAX31865Error::InvalidArgument, fixture.begin().code);
        CHECK_EQ(0U, fixture.scripted.eventCount());
        CHECK_EQ(MAX31865State::Uninitialized, fixture.driver.state());
    }
    {
        DriverFixture fixture;
        fixture.config.transport.unlockBus = nullptr;
        CHECK_EQ(MAX31865Error::InvalidArgument, fixture.begin().code);
        CHECK_EQ(0U, fixture.scripted.eventCount());
    }
    {
        DriverFixture fixture;
        fixture.config.transport.readPin = nullptr;
        CHECK_EQ(MAX31865Error::InvalidArgument, fixture.begin().code);
        CHECK_EQ(0U, fixture.scripted.eventCount());
    }
    {
        DriverFixture fixture;
        fixture.config.initialDeviceConfig.thresholds = {100U, 99U};
        CHECK_EQ(MAX31865Error::InvalidArgument, fixture.begin().code);
        CHECK_EQ(0U, fixture.scripted.eventCount());
    }
    {
        DriverFixture fixture;
        fixture.config.rtd.referenceResistorOhms = 349.0F;
        CHECK_EQ(MAX31865Error::InvalidArgument, fixture.begin().code);
        CHECK_EQ(0U, fixture.scripted.eventCount());
    }
    {
        DriverFixture fixture;
        fixture.config.defaultOperationTimeoutMs = 0U;
        CHECK_EQ(MAX31865Error::InvalidArgument, fixture.begin().code);
        CHECK_EQ(0U, fixture.scripted.eventCount());
    }
    {
        DriverFixture fixture;
        fixture.config.offlineThreshold = 0U;
        CHECK_EQ(MAX31865Error::InvalidArgument, fixture.begin().code);
        CHECK_EQ(0U, fixture.scripted.eventCount());
    }
    return true;
}

bool publicPreconditionsAreCallbackFreeAndPreserveOutputs()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();

    const MAX31865Health baseline = fixture.driver.health();
    MAX31865DeviceConfig invalid = max31865DefaultDeviceConfig();
    invalid.thresholds = {100U, 99U};
    uint32_t clockCalls = fixture.scripted.nowMsCallCount();
    CHECK_EQ(
        MAX31865Error::InvalidArgument,
        fixture.driver.applyConfiguration(invalid, 100U).code);
    CHECK_EQ(clockCalls, fixture.scripted.nowMsCallCount());
    CHECK_EQ(0U, fixture.scripted.eventCount());

    uint8_t registerValue = 0xA5U;
    clockCalls = fixture.scripted.nowMsCallCount();
    CHECK_EQ(
        MAX31865Error::RegisterAddressInvalid,
        fixture.driver.readRegister(0xFFU, registerValue, 100U).code);
    CHECK_EQ(0xA5U, registerValue);
    CHECK_EQ(clockCalls, fixture.scripted.nowMsCallCount());
    CHECK_EQ(0U, fixture.scripted.eventCount());

    MAX31865Sample sample{};
    memset(&sample, 0xA5, sizeof(sample));
    const MAX31865Sample sampleBefore = sample;
    clockCalls = fixture.scripted.nowMsCallCount();
    CHECK_EQ(
        MAX31865Error::InvalidState,
        fixture.driver.readSingle(sample, 100U).code);
    CHECK(memcmp(&sample, &sampleBefore, sizeof(sample)) == 0);
    CHECK_EQ(clockCalls, fixture.scripted.nowMsCallCount());
    CHECK_EQ(0U, fixture.scripted.eventCount());

    MAX31865DeviceInfo info{};
    memset(&info, 0xA5, sizeof(info));
    const MAX31865DeviceInfo infoBefore = info;
    clockCalls = fixture.scripted.nowMsCallCount();
    CHECK_EQ(
        MAX31865Error::InvalidArgument,
        fixture.driver.probe(info, 0U).code);
    CHECK(memcmp(&info, &infoBefore, sizeof(info)) == 0);
    CHECK_EQ(clockCalls, fixture.scripted.nowMsCallCount());
    CHECK_EQ(0U, fixture.scripted.eventCount());

    clockCalls = fixture.scripted.nowMsCallCount();
    CHECK_EQ(MAX31865Error::InvalidArgument, fixture.driver.recover(0U).code);
    CHECK_EQ(clockCalls, fixture.scripted.nowMsCallCount());
    CHECK_EQ(0U, fixture.scripted.eventCount());

    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(baseline.trackedSuccessCount, after.trackedSuccessCount);
    CHECK_EQ(baseline.trackedFailureCount, after.trackedFailureCount);
    CHECK_EQ(baseline.consecutiveFailures, after.consecutiveFailures);
    CHECK_EQ(baseline.hasLastOkMs, after.hasLastOkMs);
    CHECK_EQ(baseline.lastOkMs, after.lastOkMs);
    CHECK_EQ(baseline.hasLastErrorMs, after.hasLastErrorMs);
    CHECK_EQ(baseline.lastErrorMs, after.lastErrorMs);
    return true;
}

bool powerReadyFailureDoesNotCreateAProtocolOperation()
{
    DriverFixture fixture;
    fixture.config.powerReadyDelayMs = 5U;
    fixture.scripted.setSleepAdvancesTime(false);
    CHECK_EQ(0U, fixture.scripted.nowMsCallCount());
    CHECK_EQ(MAX31865Error::TimingUnavailable, fixture.begin().code);
    CHECK_EQ(2U, fixture.scripted.nowMsCallCount());
    CHECK_EQ(1U, fixture.scripted.eventCount());
    CHECK_EQ(EventKind::SleepMs, fixture.scripted.event(0U).kind);
    CHECK_EQ(0U, fixture.driver.health().trackedFailureCount);
    return true;
}

bool healthTimestampsUseTheCapturedOperationStart()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());

    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    fixture.scripted.setTimeUs(123000ULL);
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::Transfer,
        1U,
        7000U)));
    uint8_t value = 0U;
    CHECK(fixture.driver.readRegister(
              max31865_cmd::REG_CONFIG,
              value,
              50U)
              .ok());
    MAX31865Health health = fixture.driver.health();
    CHECK(health.hasLastOkMs);
    CHECK_EQ(123U, health.lastOkMs);
    CHECK(fixture.scripted.timeUs() >= 130000ULL);

    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    fixture.scripted.setTimeUs(200000ULL);
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::LockBus,
        1U,
        MAX31865Error::BusLockTimeout,
        FailureEffect::NotApplied,
        5000U)));
    CHECK_EQ(
        MAX31865Error::BusLockTimeout,
        fixture.driver.readRegister(
            max31865_cmd::REG_CONFIG,
            value,
            50U)
            .code);
    health = fixture.driver.health();
    CHECK(health.hasLastErrorMs);
    CHECK_EQ(200U, health.lastErrorMs);
    CHECK(fixture.scripted.timeUs() >= 205000ULL);
    return true;
}

bool successfulBeginAppliesAndVerifiesCompleteImage()
{
    DriverFixture fixture;
    fixture.config.initialDeviceConfig.wireMode = MAX31865WireMode::ThreeWire;
    fixture.config.initialDeviceConfig.filter = MAX31865Filter::Hz50;
    fixture.config.initialDeviceConfig.biasEnabled = true;
    fixture.config.initialDeviceConfig.thresholds = {123U, 30000U};
    fixture.config.defaultOperationTimeoutMs = 200U;
    fixture.config.offlineThreshold = 3U;
    CHECK(fixture.begin().ok());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(MAX31865DriverState::READY, health.driverState);
    CHECK(health.online);
    CHECK(health.configurationKnown);
    CHECK_EQ(3U, health.offlineThreshold);
    CHECK_EQ(1U, health.trackedSuccessCount);
    CHECK_EQ(0U, health.trackedFailureCount);
    CHECK(health.hasLastOkMs);
    CHECK_EQ(200U, fixture.driver.defaultOperationTimeoutMs());
    CHECK_EQ(static_cast<uint8_t>(
                 max31865_cmd::CONFIG_BIAS |
                 max31865_cmd::CONFIG_3WIRE |
                 max31865_cmd::CONFIG_FILTER_50HZ),
             fixture.scripted.device().registerValue(
                 max31865_cmd::REG_CONFIG));

    uint8_t highBytes[2] = {};
    uint8_t lowBytes[2] = {};
    CHECK(max31865EncodeThreshold(30000U, highBytes).ok());
    CHECK(max31865EncodeThreshold(123U, lowBytes).ok());
    CHECK_EQ(highBytes[0], fixture.scripted.device().registerValue(
                               max31865_cmd::REG_HIGH_FAULT_MSB));
    CHECK_EQ(highBytes[1], fixture.scripted.device().registerValue(
                               max31865_cmd::REG_HIGH_FAULT_LSB));
    CHECK_EQ(lowBytes[0], fixture.scripted.device().registerValue(
                              max31865_cmd::REG_LOW_FAULT_MSB));
    CHECK_EQ(lowBytes[1], fixture.scripted.device().registerValue(
                              max31865_cmd::REG_LOW_FAULT_LSB));

    fixture.scripted.resetLog();
    CHECK_EQ(MAX31865Error::InvalidState, fixture.begin().code);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    fixture.driver.end();
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(MAX31865State::Uninitialized, fixture.driver.state());
    return true;
}

bool unusedDrdyCallbackIsAcceptedWhenCapabilityIsFalse()
{
    DriverFixture fixture;
    fixture.config.transport.capabilities.hasDrdy = false;
    CHECK(fixture.config.transport.readPin != nullptr);
    CHECK(fixture.begin().ok());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    return true;
}

bool cachedNoConversionReadinessIsCallbackFree()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    const uint32_t clockCalls = fixture.scripted.nowMsCallCount();
    const MAX31865Health before = fixture.driver.health();
    bool ready = true;
    CHECK(fixture.driver.dataReady(ready, 20U).ok());
    CHECK(!ready);
    CHECK_EQ(clockCalls, fixture.scripted.nowMsCallCount());
    CHECK_EQ(0U, fixture.scripted.eventCount());
    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(before.trackedSuccessCount, after.trackedSuccessCount);
    CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
    CHECK_EQ(before.consecutiveFailures, after.consecutiveFailures);
    CHECK_EQ(before.lastOkMs, after.lastOkMs);
    CHECK_EQ(before.lastErrorMs, after.lastErrorMs);

    MAX31865Sample sample{};
    memset(&sample, 0xA5, sizeof(sample));
    const MAX31865Sample sampleBefore = sample;
    const uint32_t noDataBefore = after.noDataCount;
    CHECK_EQ(MAX31865Error::NoData,
             fixture.driver.poll(sample, 20U).code);
    CHECK(memcmp(&sample, &sampleBefore, sizeof(sample)) == 0);
    CHECK_EQ(clockCalls, fixture.scripted.nowMsCallCount());
    CHECK_EQ(0U, fixture.scripted.eventCount());
    const MAX31865Health afterPoll = fixture.driver.health();
    CHECK_EQ(noDataBefore + 1U, afterPoll.noDataCount);
    CHECK_EQ(before.trackedSuccessCount,
             afterPoll.trackedSuccessCount);
    CHECK_EQ(before.trackedFailureCount,
             afterPoll.trackedFailureCount);
    return true;
}

bool nonadvancingSleepFailsFinitelyAndRestoresIdleState()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.setSleepAdvancesTime(false);
    fixture.scripted.resetLog();
    CHECK_EQ(
        MAX31865Error::TimingUnavailable,
        fixture.driver.startContinuous(20U).code);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK(fixture.scripted.eventCount() < 64U);
    CHECK_EQ(
        0U,
        static_cast<uint8_t>(
            fixture.scripted.device().registerValue(
                max31865_cmd::REG_CONFIG) &
            (max31865_cmd::CONFIG_AUTO |
             max31865_cmd::CONFIG_ONE_SHOT |
             max31865_cmd::CONFIG_BIAS)));
    CHECK_EQ(0U, fixture.driver.health().trackedFailureCount);
    return true;
}

bool finalDeadlineCheckPreventsLateMillisecondSleep()
{
    uint32_t clockReadsThroughBeforeSleep = 0U;
    {
        DriverFixture reference;
        CHECK(reference.begin().ok());
        reference.scripted.resetLog();
        reference.scripted.setDelayAdvancesTime(false);
        const uint64_t startedUs = reference.scripted.timeUs();
        reference.scripted.setNowMsAdvanceUs(1000U);
        CHECK(reference.driver.startContinuous(500U).ok());

        const size_t sleepIndex = firstEventOfKind(
            reference.scripted,
            EventKind::SleepMs);
        CHECK(sleepIndex < reference.scripted.eventCount());
        const uint64_t elapsedToSleepUs =
            reference.scripted.event(sleepIndex).timeUs - startedUs;
        CHECK_EQ(0ULL, elapsedToSleepUs % 1000ULL);
        clockReadsThroughBeforeSleep = static_cast<uint32_t>(
            elapsedToSleepUs / 1000ULL);
        CHECK(clockReadsThroughBeforeSleep > 1U);
    }

    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    fixture.scripted.setDelayAdvancesTime(false);
    fixture.scripted.setNowMsAdvanceUs(1000U);
    const uint32_t requiredMs =
        (fixture.driver.biasSettleTimeUs() + 999U) / 1000U;
    CHECK(requiredMs > 0U);

    // The first remaining-budget observation leaves requiredMs + 1, while
    // the immediately following beforeSleep observation leaves exactly
    // requiredMs. The latter must prevent sleepMs from starting.
    const uint32_t timeoutMs =
        clockReadsThroughBeforeSleep + requiredMs - 1U;
    CHECK(!fixture.driver.startContinuous(timeoutMs).ok());
    CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::SleepMs));
    CHECK_EQ(1U, fixture.driver.health().operationTimeoutCount);
    return true;
}

bool finalDeadlineCheckPreventsLateSubmillisecondDelay()
{
    uint32_t clockReadsThroughBeforeDelay = 0U;
    {
        DriverFixture reference;
        CHECK(reference.begin().ok());
        CHECK(writeRawConfig(
            reference.scripted,
            static_cast<uint8_t>(
                max31865_cmd::CONFIG_BIAS |
                max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1)));
        reference.scripted.resetLog();
        reference.scripted.setDelayAdvancesTime(false);
        const uint64_t startedUs = reference.scripted.timeUs();
        reference.scripted.setNowMsAdvanceUs(1000U);
        CHECK(reference.driver.recover(500U).ok());

        size_t targetIndex = reference.scripted.eventCount();
        for (size_t index = 0U;
             index < reference.scripted.eventCount();
             ++index)
        {
            const TransportEvent &event = reference.scripted.event(index);
            if (event.kind == EventKind::DelayUs &&
                event.argument ==
                    max31865_cmd::MANUAL_FAULT_STEP1_TO_OPEN_US)
            {
                targetIndex = index;
                break;
            }
        }
        CHECK(targetIndex < reference.scripted.eventCount());
        const uint64_t elapsedToDelayUs =
            reference.scripted.event(targetIndex).timeUs - startedUs;
        CHECK_EQ(0ULL, elapsedToDelayUs % 1000ULL);
        clockReadsThroughBeforeDelay = static_cast<uint32_t>(
            elapsedToDelayUs / 1000ULL);
        CHECK(clockReadsThroughBeforeDelay > 1U);
    }

    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(writeRawConfig(
        fixture.scripted,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1)));
    fixture.scripted.resetLog();
    fixture.scripted.setDelayAdvancesTime(false);
    fixture.scripted.setNowMsAdvanceUs(1000U);

    // A sub-millisecond wait still consumes one rounded deadline millisecond.
    // Leave two milliseconds at the first check and one at beforeSleep.
    CHECK(!fixture.driver.recover(clockReadsThroughBeforeDelay).ok());
    for (size_t index = 0U; index < fixture.scripted.eventCount(); ++index)
    {
        const TransportEvent &event = fixture.scripted.event(index);
        CHECK(event.kind != EventKind::DelayUs ||
              event.argument !=
                  max31865_cmd::MANUAL_FAULT_STEP1_TO_OPEN_US);
    }
    CHECK_EQ(1U, fixture.driver.health().operationTimeoutCount);
    return true;
}

bool runtimeConfigurationIsTypedTransactionalAndStateGuarded()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    MAX31865DeviceConfig desired = max31865DefaultDeviceConfig();
    desired.wireMode = MAX31865WireMode::ThreeWire;
    desired.filter = MAX31865Filter::Hz50;
    desired.biasEnabled = true;
    desired.thresholds = {1000U, 25000U};
    CHECK(fixture.driver.applyConfiguration(desired, 100U).ok());
    CHECK(fixture.scripted.matchingEventCount(EventKind::Transfer) >= 5U);

    MAX31865Settings settings{};
    CHECK(fixture.driver.readConfiguration(settings, 100U).ok());
    CHECK_EQ(MAX31865WireMode::ThreeWire, settings.deviceConfig.wireMode);
    CHECK_EQ(MAX31865Filter::Hz50, settings.deviceConfig.filter);
    CHECK(settings.deviceConfig.biasEnabled);
    CHECK_EQ(1000U, settings.deviceConfig.thresholds.lowCode);
    CHECK_EQ(25000U, settings.deviceConfig.thresholds.highCode);
    CHECK_EQ(MAX31865ConversionMode::NormallyOff, settings.conversionMode);

    fixture.scripted.resetLog();
    desired.thresholds = {300U, 200U};
    CHECK_EQ(MAX31865Error::InvalidArgument,
             fixture.driver.applyConfiguration(desired, 100U).code);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(MAX31865DriverState::READY, fixture.driver.health().driverState);
    return true;
}

bool failedBeginRetainsBindingForExplicitRecovery()
{
    DriverFixture fixture;
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    CHECK_EQ(MAX31865Error::SpiTransferFailed, fixture.begin().code);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    MAX31865Health health = fixture.driver.health();
    CHECK_EQ(MAX31865DriverState::OFFLINE, health.driverState);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(1U, health.spiTransferFailureCount);

    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    CHECK(fixture.driver.recover(200U).ok());
    health = fixture.driver.health();
    CHECK_EQ(MAX31865State::Ready, health.state);
    CHECK_EQ(MAX31865DriverState::READY, health.driverState);
    CHECK_EQ(0U, health.consecutiveFailures);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(1U, health.trackedSuccessCount);
    CHECK(health.configurationKnown);
    return true;
}

bool failureStreakTransitionsDegradedOfflineAndBackToReady()
{
    DriverFixture fixture;
    fixture.config.offlineThreshold = 2U;
    CHECK(fixture.begin().ok());

    for (uint32_t attempt = 1U; attempt <= 2U; ++attempt) {
        fixture.scripted.clearFailures();
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::Transfer,
            1U,
            MAX31865Error::SpiTransferFailed)));
        uint8_t output = 0xA5U;
        CHECK_EQ(MAX31865Error::SpiTransferFailed,
                 fixture.driver.readRegister(
                     max31865_cmd::REG_CONFIG,
                     output,
                     20U)
                     .code);
        CHECK_EQ(0xA5U, output);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(attempt, health.trackedFailureCount);
        CHECK_EQ(static_cast<uint8_t>(attempt), health.consecutiveFailures);
        CHECK_EQ(attempt == 1U ? MAX31865DriverState::DEGRADED
                               : MAX31865DriverState::OFFLINE,
                 health.driverState);
    }

    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    uint8_t output = 0xFFU;
    CHECK(fixture.driver.readRegister(
              max31865_cmd::REG_CONFIG,
              output,
              20U)
              .ok());
    CHECK_EQ(0U, fixture.driver.health().consecutiveFailures);
    CHECK_EQ(MAX31865DriverState::READY,
             fixture.driver.health().driverState);
    return true;
}

bool probeIsHealthNeutralAndPreservesLastOperation()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const MAX31865Health before = fixture.driver.health();
    fixture.scripted.device().setRegisterReadOverride(
        max31865_cmd::REG_CONFIG,
        max31865_cmd::CONFIG_BIAS,
        1U);
    MAX31865DeviceInfo info{};
    CHECK_EQ(MAX31865Error::ProbeMismatch, fixture.driver.probe(info).code);
    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(before.trackedSuccessCount, after.trackedSuccessCount);
    CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
    CHECK_EQ(before.spiTransferFailureCount, after.spiTransferFailureCount);
    CHECK_EQ(before.lastOperation.code,
             fixture.driver.lastOperationStatus().code);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    return true;
}

bool oneTransactionHasCanonicalCsLockAndDelayOrdering()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    uint8_t output = 0xFFU;
    CHECK(fixture.driver.readRegister(
              max31865_cmd::REG_CONFIG,
              output,
              20U)
              .ok());
    CHECK_EQ(8U, fixture.scripted.eventCount());
    const EventKind expected[] = {
        EventKind::LockBus,
        EventKind::ChipSelectAssert,
        EventKind::DelayUs,
        EventKind::Transfer,
        EventKind::DelayUs,
        EventKind::ChipSelectDeassert,
        EventKind::DelayUs,
        EventKind::UnlockBus};
    for (size_t index = 0U; index < sizeof(expected) / sizeof(expected[0]);
         ++index) {
        CHECK_EQ(expected[index], fixture.scripted.event(index).kind);
    }
    CHECK_EQ(max31865_cmd::CS_SETUP_DELAY_US,
             fixture.scripted.event(2U).argument);
    CHECK_EQ(max31865_cmd::CS_HOLD_DELAY_US,
             fixture.scripted.event(4U).argument);
    CHECK_EQ(max31865_cmd::CS_INACTIVE_DELAY_US,
             fixture.scripted.event(6U).argument);
    CHECK_EQ(2U, fixture.scripted.event(3U).length);
    CHECK_EQ(max31865_cmd::REG_CONFIG,
             fixture.scripted.event(3U).tx[0]);
    CHECK_EQ(0U, fixture.scripted.event(3U).tx[1]);
    CHECK(!fixture.scripted.busLocked());
    CHECK(!fixture.scripted.device().chipSelected());
    return true;
}

bool remainingDeadlineIsPassedToCallbacks()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::LockBus,
        1U,
        4000U)));
    uint8_t output = 0U;
    CHECK(fixture.driver.readRegister(
              max31865_cmd::REG_CONFIG,
              output,
              10U)
              .ok());
    const size_t transferIndex = firstEventOfKind(
        fixture.scripted,
        EventKind::Transfer);
    CHECK(transferIndex < fixture.scripted.eventCount());
    CHECK_EQ(6U, fixture.scripted.event(transferIndex).argument);
    CHECK(fixture.scripted.timeUs() >= 4000U);
    CHECK(fixture.scripted.timeUs() < 5000U);
    return true;
}

bool deadlineExpiryAfterLockAlwaysUnlocksWithoutSelecting()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::LockBus,
        1U,
        10000U)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::OperationTimeout,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 10U)
                 .code);
    CHECK_EQ(0xA5U, output);
    CHECK_EQ(2U, fixture.scripted.eventCount());
    CHECK_EQ(EventKind::LockBus, fixture.scripted.event(0U).kind);
    CHECK_EQ(EventKind::UnlockBus, fixture.scripted.event(1U).kind);
    CHECK(!fixture.scripted.busLocked());
    CHECK_EQ(1U, fixture.driver.health().operationTimeoutCount);
    return true;
}

bool deadlineExpiryAfterTransferReportsTimeoutNotCleanupFailure()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::Transfer,
        1U,
        10000U)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::OperationTimeout,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 10U)
                 .code);
    CHECK_EQ(0xA5U, output);
    CHECK_EQ(1U,
             fixture.scripted.matchingEventCount(
                 EventKind::ChipSelectDeassert));
    CHECK_EQ(1U,
             fixture.scripted.matchingEventCount(EventKind::UnlockBus));
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());
    return true;
}

bool zeroTimeoutAllowsOneClockAdvancingImmediateFrame()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    fixture.scripted.setNowMsAdvanceUs(2000U);
    const uint64_t startUs = fixture.scripted.timeUs();

    uint8_t output = 0xA5U;
    CHECK(fixture.driver.readRegister(
              max31865_cmd::REG_CONFIG,
              output,
              0U)
              .ok());
    CHECK_EQ(
        fixture.scripted.device().registerValue(max31865_cmd::REG_CONFIG),
        output);
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());
    CHECK_EQ(1U,
             fixture.scripted.matchingEventCount(EventKind::LockBus));
    CHECK_EQ(1U,
             fixture.scripted.matchingEventCount(EventKind::Transfer));
    for (size_t index = 0U; index < fixture.scripted.eventCount(); ++index)
    {
        const TransportEvent &event = fixture.scripted.event(index);
        if (event.kind == EventKind::LockBus ||
            event.kind == EventKind::Transfer)
        {
            CHECK_EQ(0U, event.argument);
        }
    }
    // Only makeOperation() captures nowMs. Neither the zero-budget
    // remaining-time sentinel nor finishOperation() samples the clock.
    CHECK_EQ(
        startUs + 2000U + max31865_cmd::CS_SETUP_DELAY_US +
            max31865_cmd::CS_HOLD_DELAY_US +
            max31865_cmd::CS_INACTIVE_DELAY_US,
        fixture.scripted.timeUs());
    CHECK_EQ(0U, fixture.driver.health().operationTimeoutCount);
    return true;
}

bool zeroTimeoutReadSingleChecksOnceWithoutSleeping()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.resetLog();

    MAX31865Sample output{};
    memset(&output, 0xA5, sizeof(output));
    const MAX31865Sample before = output;
    const MAX31865Health healthBefore = fixture.driver.health();
    CHECK_EQ(
        MAX31865Error::NoData,
        fixture.driver.readSingle(output, 0U).code);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    CHECK_EQ(1U,
             fixture.scripted.matchingEventCount(EventKind::ReadDrdy));
    CHECK_EQ(0U,
             fixture.scripted.matchingEventCount(EventKind::SleepMs));
    CHECK_EQ(0U,
             fixture.scripted.matchingEventCount(EventKind::Transfer));
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());

    const MAX31865Health healthAfter = fixture.driver.health();
    CHECK_EQ(healthBefore.noDataCount + 1U, healthAfter.noDataCount);
    CHECK_EQ(healthBefore.trackedFailureCount,
             healthAfter.trackedFailureCount);
    CHECK_EQ(healthBefore.drdyTimeoutCount, healthAfter.drdyTimeoutCount);
    CHECK_EQ(healthBefore.operationTimeoutCount,
             healthAfter.operationTimeoutCount);
    CHECK(fixture.driver.stop(100U).ok());
    return true;
}

bool expiredOperationInvokesNoNewBusCallback()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    // Advance one millisecond on each deadline-clock observation. makeOperation
    // captures the first value; openSession's entry check observes expiry.
    fixture.scripted.setNowMsAdvanceUs(1000U);
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::OperationTimeout,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 1U)
                 .code);
    CHECK_EQ(0xA5U, output);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(1U, fixture.driver.health().operationTimeoutCount);
    return true;
}

bool deadlineExpiryAfterCsAssertionSkipsSetupAndTransferButCleansUp()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::ChipSelectAssert,
        1U,
        10000U)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::OperationTimeout,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 10U)
                 .code);
    CHECK_EQ(0xA5U, output);
    const EventKind expected[] = {
        EventKind::LockBus,
        EventKind::ChipSelectAssert,
        EventKind::DelayUs,
        EventKind::ChipSelectDeassert,
        EventKind::DelayUs,
        EventKind::UnlockBus};
    CHECK_EQ(sizeof(expected) / sizeof(expected[0]),
             fixture.scripted.eventCount());
    for (size_t index = 0U; index < sizeof(expected) / sizeof(expected[0]);
         ++index)
    {
        CHECK_EQ(expected[index], fixture.scripted.event(index).kind);
    }
    CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());
    return true;
}

bool cleanupPhaseDeadlineMatrixIsBoundedAndTransactional()
{
    struct Phase
    {
        EventKind kind;
        uint32_t ordinal;
    };
    const Phase phases[] = {
        {EventKind::DelayUs, 2U},
        {EventKind::ChipSelectDeassert, 1U},
        {EventKind::DelayUs, 3U},
        {EventKind::UnlockBus, 1U}};
    for (const Phase &phase : phases)
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeTimedSuccess(
            phase.kind,
            phase.ordinal,
            10000U)));
        uint8_t output = 0xA5U;
        CHECK_EQ(MAX31865Error::OperationTimeout,
                 fixture.driver.readRegister(
                     max31865_cmd::REG_CONFIG,
                     output,
                     10U)
                     .code);
        CHECK_EQ(0xA5U, output);
        CHECK_EQ(1U,
                 fixture.scripted.matchingEventCount(
                     EventKind::ChipSelectDeassert));
        CHECK_EQ(1U,
                 fixture.scripted.matchingEventCount(EventKind::UnlockBus));
        CHECK(!fixture.scripted.device().chipSelected());
        CHECK(!fixture.scripted.busLocked());
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(1U, health.operationTimeoutCount);
        CHECK_EQ(MAX31865State::Ready, health.state);
        CHECK(health.configurationKnown);
    }
    return true;
}

bool cleanupTimeoutTakesPrecedenceAndRetainsOriginCounters()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::DelayUs,
        2U,
        10000U)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::OperationTimeout,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 10U)
                 .code);
    CHECK_EQ(0xA5U, output);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.spiTransferFailureCount);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(MAX31865State::Ready, health.state);
    CHECK(health.configurationKnown);
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());
    return true;
}

bool cleanupFailureRetainsEarlierDeadlineObservation()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::DelayUs,
        2U,
        10000U)));
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::ChipSelectFailed)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::ChipSelectFailed,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 10U)
                 .code);
    CHECK_EQ(0xA5U, output);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.chipSelectFailureCount);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(MAX31865State::Fault, health.state);
    CHECK(fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());
    return true;
}

bool timedLockFailuresClassifyWholeOperationTimeout()
{
    const MAX31865Error failures[] = {
        MAX31865Error::BusLockTimeout,
        MAX31865Error::BusLockFailed};
    for (MAX31865Error failure : failures)
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::LockBus,
            1U,
            failure,
            FailureEffect::NotApplied,
            10000U)));
        uint8_t output = 0xA5U;
        CHECK_EQ(MAX31865Error::OperationTimeout,
                 fixture.driver.readRegister(
                     max31865_cmd::REG_CONFIG,
                     output,
                     10U)
                     .code);
        CHECK_EQ(0xA5U, output);
        CHECK_EQ(1U, fixture.scripted.eventCount());
        CHECK_EQ(EventKind::LockBus, fixture.scripted.event(0U).kind);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(failure == MAX31865Error::BusLockTimeout ? 1U : 0U,
                 health.busLockTimeoutCount);
        CHECK_EQ(failure == MAX31865Error::BusLockFailed ? 1U : 0U,
                 health.busLockFailureCount);
        CHECK_EQ(1U, health.operationTimeoutCount);
        CHECK_EQ(1U, health.trackedFailureCount);
        CHECK_EQ(MAX31865State::Ready, health.state);
        CHECK(health.configurationKnown);
    }
    return true;
}

bool timedSynchronizationLockFailureClassifiesOperationTimeout()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const MAX31865Health before = fixture.driver.health();
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::LockBus,
        1U,
        MAX31865Error::BusLockFailed,
        FailureEffect::NotApplied,
        100000U)));
    CHECK_EQ(MAX31865Error::OperationTimeout,
             fixture.driver.recover(100U).code);
    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(before.busLockFailureCount + 1U,
             after.busLockFailureCount);
    CHECK_EQ(before.operationTimeoutCount + 1U,
             after.operationTimeoutCount);
    CHECK_EQ(before.trackedFailureCount + 1U,
             after.trackedFailureCount);
    CHECK_EQ(MAX31865State::Ready, after.state);
    CHECK(after.configurationKnown);
    CHECK_EQ(1U, fixture.scripted.eventCount());
    CHECK(!fixture.scripted.busLocked());
    return true;
}

bool timedSynchronizationLockSuccessStillDeassertsUncertainChipSelect()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::ChipSelectFailed,
        FailureEffect::NotApplied)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::ChipSelectFailed,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 20U)
                 .code);
    CHECK_EQ(0xA5U, output);
    CHECK(fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());

    const MAX31865Health before = fixture.driver.health();
    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::LockBus,
        1U,
        10000U)));
    CHECK_EQ(MAX31865Error::OperationTimeout,
             fixture.driver.recover(10U).code);

    const EventKind expected[] = {
        EventKind::LockBus,
        EventKind::ChipSelectDeassert,
        EventKind::DelayUs,
        EventKind::UnlockBus};
    CHECK_EQ(sizeof(expected) / sizeof(expected[0]),
             fixture.scripted.eventCount());
    for (size_t index = 0U; index < sizeof(expected) / sizeof(expected[0]);
         ++index)
    {
        CHECK_EQ(expected[index], fixture.scripted.event(index).kind);
    }
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());

    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(before.chipSelectFailureCount,
             after.chipSelectFailureCount);
    CHECK_EQ(before.operationTimeoutCount + 1U,
             after.operationTimeoutCount);
    CHECK_EQ(before.trackedFailureCount + 1U,
             after.trackedFailureCount);
    return true;
}

bool timedSynchronizationDeassertFailureRetainsSafetyStatus()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::ChipSelectFailed,
        FailureEffect::NotApplied)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::ChipSelectFailed,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 20U)
                 .code);
    CHECK_EQ(0xA5U, output);
    CHECK(fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());

    const MAX31865Health before = fixture.driver.health();
    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::LockBus,
        1U,
        10000U)));
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::ChipSelectFailed,
        FailureEffect::NotApplied)));
    CHECK_EQ(MAX31865Error::ChipSelectFailed,
             fixture.driver.recover(10U).code);

    const EventKind expected[] = {
        EventKind::LockBus,
        EventKind::ChipSelectDeassert,
        EventKind::UnlockBus};
    CHECK_EQ(sizeof(expected) / sizeof(expected[0]),
             fixture.scripted.eventCount());
    for (size_t index = 0U; index < sizeof(expected) / sizeof(expected[0]);
         ++index)
    {
        CHECK_EQ(expected[index], fixture.scripted.event(index).kind);
    }
    CHECK(fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());

    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(before.chipSelectFailureCount + 1U,
             after.chipSelectFailureCount);
    CHECK_EQ(before.operationTimeoutCount + 1U,
             after.operationTimeoutCount);
    CHECK_EQ(before.trackedFailureCount + 1U,
             after.trackedFailureCount);
    CHECK_EQ(MAX31865State::Fault, after.state);
    CHECK(!after.configurationKnown);
    return true;
}

bool timedSafetyCleanupFailureRetainsDeadlineProvenance()
{
    for (uint8_t phase = 0U; phase < 2U; ++phase)
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::ChipSelectDeassert,
            1U,
            MAX31865Error::ChipSelectFailed,
            FailureEffect::NotApplied,
            phase == 0U ? 10000U : 0U)));
        if (phase == 1U)
        {
            CHECK(fixture.scripted.addFailure(makeTimedSuccess(
                EventKind::UnlockBus,
                1U,
                10000U)));
        }
        uint8_t output = 0xA5U;
        CHECK_EQ(MAX31865Error::ChipSelectFailed,
                 fixture.driver.readRegister(
                     max31865_cmd::REG_CONFIG,
                     output,
                     10U)
                     .code);
        CHECK_EQ(0xA5U, output);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(1U, health.chipSelectFailureCount);
        CHECK_EQ(1U, health.operationTimeoutCount);
        CHECK_EQ(1U, health.trackedFailureCount);
        CHECK_EQ(MAX31865State::Fault, health.state);
        CHECK(!health.configurationKnown);
        CHECK(fixture.scripted.device().chipSelected());
        CHECK(!fixture.scripted.busLocked());
    }
    return true;
}

bool failedAppliedCsAssertionIsDeassertedBeforeUnlock()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectAssert,
        1U,
        MAX31865Error::ChipSelectFailed,
        FailureEffect::Applied)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::ChipSelectFailed,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 20U)
                 .code);
    CHECK_EQ(0xA5U, output);
    const EventKind expected[] = {
        EventKind::LockBus,
        EventKind::ChipSelectAssert,
        EventKind::DelayUs,
        EventKind::ChipSelectDeassert,
        EventKind::DelayUs,
        EventKind::UnlockBus};
    CHECK_EQ(sizeof(expected) / sizeof(expected[0]),
             fixture.scripted.eventCount());
    for (size_t index = 0U; index < sizeof(expected) / sizeof(expected[0]);
         ++index)
    {
        CHECK_EQ(expected[index], fixture.scripted.event(index).kind);
    }
    CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.chipSelectFailureCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(MAX31865State::Fault, health.state);
    return true;
}

bool actionFreeCsAssertInvalidStateDoesNotCreateHiddenUncertainty()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectAssert,
        1U,
        MAX31865Error::InvalidState)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::InvalidState,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 20U)
                 .code);
    CHECK_EQ(0xA5U, output);

    fixture.scripted.resetLog();
    CHECK(fixture.driver.readRegister(
              max31865_cmd::REG_CONFIG,
              output,
              20U)
              .ok());
    CHECK_EQ(8U, fixture.scripted.eventCount());
    CHECK_EQ(EventKind::LockBus, fixture.scripted.event(0U).kind);
    CHECK_EQ(EventKind::ChipSelectAssert, fixture.scripted.event(1U).kind);
    CHECK_EQ(1U,
             fixture.scripted.matchingEventCount(EventKind::ChipSelectDeassert));
    CHECK_EQ(1U, fixture.scripted.matchingEventCount(EventKind::UnlockBus));
    return true;
}

bool baselineCsDeassertInvalidStateRemainsAnUntrackedPrecondition()
{
    {
        DriverFixture fixture;
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::ChipSelectDeassert,
            1U,
            MAX31865Error::InvalidState)));
        CHECK_EQ(MAX31865Error::InvalidState, fixture.begin().code);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(0U, health.chipSelectFailureCount);
        CHECK_EQ(0U, health.trackedFailureCount);
        CHECK_EQ(MAX31865State::Fault, health.state);
        CHECK_EQ(3U, fixture.scripted.eventCount());
        CHECK_EQ(EventKind::LockBus, fixture.scripted.event(0U).kind);
        CHECK_EQ(EventKind::ChipSelectDeassert,
                 fixture.scripted.event(1U).kind);
        CHECK_EQ(EventKind::UnlockBus, fixture.scripted.event(2U).kind);
        CHECK(!fixture.scripted.busLocked());
    }

    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        const MAX31865Health before = fixture.driver.health();
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::ChipSelectDeassert,
            1U,
            MAX31865Error::InvalidState)));
        CHECK_EQ(MAX31865Error::InvalidState,
                 fixture.driver.recover(100U).code);
        const MAX31865Health after = fixture.driver.health();
        CHECK_EQ(MAX31865State::Ready, after.state);
        CHECK(after.configurationKnown);
        CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
        CHECK_EQ(before.chipSelectFailureCount,
                 after.chipSelectFailureCount);
        CHECK_EQ(3U, fixture.scripted.eventCount());
        CHECK(!fixture.scripted.busLocked());
    }
    return true;
}

bool uncertainCsDeassertInvalidStateIsATrackedFramingFailure()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::ChipSelectFailed)));
    uint8_t output = 0xA5U;
    CHECK_EQ(MAX31865Error::ChipSelectFailed,
             fixture.driver.readRegister(
                 max31865_cmd::REG_CONFIG,
                 output,
                 20U)
                 .code);
    CHECK(fixture.scripted.device().chipSelected());
    const MAX31865Health before = fixture.driver.health();

    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::InvalidState,
        FailureEffect::NotApplied,
        100000U)));
    const MAX31865Status status = fixture.driver.recover(100U);
    CHECK_EQ(MAX31865Error::ChipSelectFailed, status.code);
    CHECK_EQ(static_cast<int32_t>(MAX31865Error::InvalidState),
             status.detail);
    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(before.chipSelectFailureCount + 1U,
             after.chipSelectFailureCount);
    CHECK_EQ(before.trackedFailureCount + 1U,
             after.trackedFailureCount);
    CHECK_EQ(before.operationTimeoutCount + 1U,
             after.operationTimeoutCount);
    CHECK_EQ(MAX31865State::Fault, after.state);
    CHECK(!after.configurationKnown);
    CHECK(fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());
    CHECK_EQ(3U, fixture.scripted.eventCount());
    return true;
}

bool callbackErrorsAreNormalizedByTransportRole()
{
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::LockBus,
            1U,
            MAX31865Error::SpiTransferFailed)));
        uint8_t output = 0U;
        const MAX31865Status status =
            fixture.driver.readRegister(0U, output, 10U);
        CHECK_EQ(MAX31865Error::BusLockFailed, status.code);
        CHECK_EQ(static_cast<int32_t>(MAX31865Error::SpiTransferFailed),
                 status.detail);
        CHECK(strcmp("bus lock callback failed", status.msg) == 0);
        CHECK_EQ(1U, fixture.driver.health().busLockFailureCount);
    }
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::Transfer,
            1U,
            MAX31865Error::GpioFailed)));
        uint8_t output = 0U;
        const MAX31865Status status =
            fixture.driver.readRegister(0U, output, 10U);
        CHECK_EQ(MAX31865Error::SpiTransferFailed, status.code);
        CHECK_EQ(static_cast<int32_t>(MAX31865Error::GpioFailed),
                 status.detail);
        CHECK(strcmp("SPI transfer callback failed", status.msg) == 0);
        CHECK_EQ(1U, fixture.driver.health().spiTransferFailureCount);
    }
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::ChipSelectAssert,
            1U,
            MAX31865Error::GpioFailed)));
        uint8_t output = 0U;
        const MAX31865Status status =
            fixture.driver.readRegister(0U, output, 10U);
        CHECK_EQ(MAX31865Error::ChipSelectFailed, status.code);
        CHECK_EQ(static_cast<int32_t>(MAX31865Error::GpioFailed),
                 status.detail);
        CHECK(strcmp("chip-select callback failed", status.msg) == 0);
        CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
        CHECK_EQ(1U, fixture.driver.health().chipSelectFailureCount);
    }
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        CHECK(fixture.driver.startContinuous(100U).ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::ReadDrdy,
            1U,
            MAX31865Error::SpiTransferFailed)));
        bool ready = true;
        const MAX31865Status status = fixture.driver.dataReady(ready);
        CHECK_EQ(MAX31865Error::GpioFailed, status.code);
        CHECK_EQ(static_cast<int32_t>(MAX31865Error::SpiTransferFailed),
                 status.detail);
        CHECK(strcmp("GPIO callback failed", status.msg) == 0);
        CHECK(ready);
        CHECK_EQ(1U, fixture.driver.health().gpioFailureCount);
    }
    return true;
}

bool nullCallbackMessagesReceiveStaticRoleFallbacks()
{
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        const FailureInjection failure{
            true,
            EventKind::LockBus,
            1U,
            {MAX31865Error::BusLockTimeout, nullptr, 17},
            FailureEffect::NotApplied,
            0U};
        CHECK(fixture.scripted.addFailure(failure));
        uint8_t output = 0xA5U;
        const MAX31865Status status =
            fixture.driver.readRegister(0U, output, 10U);
        CHECK_EQ(MAX31865Error::BusLockTimeout, status.code);
        CHECK(status.msg != nullptr);
        CHECK(strcmp("bus lock callback failed", status.msg) == 0);
        CHECK_EQ(17, status.detail);
        CHECK(fixture.driver.lastOperationStatus().msg != nullptr);
        CHECK_EQ(0xA5U, output);
    }
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        const FailureInjection failure{
            true,
            EventKind::Transfer,
            1U,
            {MAX31865Error::SpiTransferFailed, nullptr, 18},
            FailureEffect::NotApplied,
            0U};
        CHECK(fixture.scripted.addFailure(failure));
        uint8_t output = 0xA5U;
        const MAX31865Status status =
            fixture.driver.readRegister(0U, output, 10U);
        CHECK_EQ(MAX31865Error::SpiTransferFailed, status.code);
        CHECK(status.msg != nullptr);
        CHECK(strcmp("SPI transfer callback failed", status.msg) == 0);
        CHECK_EQ(18, status.detail);
        CHECK_EQ(0xA5U, output);
    }
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        const FailureInjection failure{
            true,
            EventKind::ChipSelectAssert,
            1U,
            {MAX31865Error::ChipSelectFailed, nullptr, 19},
            FailureEffect::NotApplied,
            0U};
        CHECK(fixture.scripted.addFailure(failure));
        uint8_t output = 0xA5U;
        const MAX31865Status status =
            fixture.driver.readRegister(0U, output, 10U);
        CHECK_EQ(MAX31865Error::ChipSelectFailed, status.code);
        CHECK(status.msg != nullptr);
        CHECK(strcmp("chip-select callback failed", status.msg) == 0);
        CHECK_EQ(19, status.detail);
        CHECK_EQ(0xA5U, output);
    }
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        CHECK(fixture.driver.startContinuous(100U).ok());
        fixture.scripted.resetLog();
        const FailureInjection failure{
            true,
            EventKind::ReadDrdy,
            1U,
            {MAX31865Error::GpioFailed, nullptr, 20},
            FailureEffect::NotApplied,
            0U};
        CHECK(fixture.scripted.addFailure(failure));
        bool ready = true;
        const MAX31865Status status = fixture.driver.dataReady(ready);
        CHECK_EQ(MAX31865Error::GpioFailed, status.code);
        CHECK(status.msg != nullptr);
        CHECK(strcmp("GPIO callback failed", status.msg) == 0);
        CHECK_EQ(20, status.detail);
        CHECK(ready);
    }
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        const FailureInjection failure{
            true,
            EventKind::LockBus,
            1U,
            {MAX31865Error::InvalidState, nullptr, 21},
            FailureEffect::NotApplied,
            0U};
        CHECK(fixture.scripted.addFailure(failure));
        uint8_t output = 0xA5U;
        const MAX31865Status status =
            fixture.driver.readRegister(0U, output, 10U);
        CHECK_EQ(MAX31865Error::InvalidState, status.code);
        CHECK(status.msg != nullptr);
        CHECK(strcmp("bus lock callback failed", status.msg) == 0);
        CHECK_EQ(21, status.detail);
        CHECK_EQ(0U, fixture.driver.health().trackedFailureCount);
        CHECK_EQ(0xA5U, output);
    }
    return true;
}

bool callbackInvalidStateIsPreservedAndHealthNeutral()
{
    const EventKind roles[] = {
        EventKind::LockBus,
        EventKind::ChipSelectAssert,
        EventKind::Transfer};
    for (EventKind role : roles)
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        const MAX31865Health before = fixture.driver.health();
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            role,
            1U,
            MAX31865Error::InvalidState)));
        uint8_t output = 0xA5U;
        CHECK_EQ(MAX31865Error::InvalidState,
                 fixture.driver.readRegister(0U, output, 10U).code);
        CHECK_EQ(0xA5U, output);
        const MAX31865Health after = fixture.driver.health();
        CHECK_EQ(MAX31865State::Ready, after.state);
        CHECK(after.configurationKnown);
        CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
        CHECK_EQ(before.busLockTimeoutCount, after.busLockTimeoutCount);
        CHECK_EQ(before.busLockFailureCount, after.busLockFailureCount);
        CHECK_EQ(before.spiTransferFailureCount,
                 after.spiTransferFailureCount);
        CHECK_EQ(before.chipSelectFailureCount, after.chipSelectFailureCount);
    }

    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.startContinuous(100U).ok());
    const MAX31865Health before = fixture.driver.health();
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ReadDrdy,
        1U,
        MAX31865Error::InvalidState)));
    bool ready = true;
    CHECK_EQ(MAX31865Error::InvalidState,
             fixture.driver.dataReady(ready).code);
    CHECK(ready);
    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(MAX31865State::Converting, after.state);
    CHECK(after.configurationKnown);
    CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
    CHECK_EQ(before.gpioFailureCount, after.gpioFailureCount);
    return true;
}

bool actionFreeRecoverRejectionPreservesKnownReadyState()
{
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        const MAX31865Health before = fixture.driver.health();
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::LockBus,
            1U,
            MAX31865Error::InvalidState)));

        CHECK_EQ(MAX31865Error::InvalidState,
                 fixture.driver.recover(100U).code);
        const MAX31865Health after = fixture.driver.health();
        CHECK_EQ(MAX31865State::Ready, after.state);
        CHECK(after.configurationKnown);
        CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
        CHECK_EQ(before.busLockTimeoutCount, after.busLockTimeoutCount);
        CHECK_EQ(before.busLockFailureCount, after.busLockFailureCount);
        CHECK_EQ(1U, fixture.scripted.eventCount());
    }

    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        const uint8_t expectedConfig =
            fixture.scripted.device().registerValue(
                max31865_cmd::REG_CONFIG);
        const MAX31865Health before = fixture.driver.health();
        fixture.scripted.resetLog();

        // recover() first synchronizes CS, then reads CONFIG while resolving
        // transient commands. Reject the following reset write without
        // transferring a byte; the verified baseline remains authoritative.
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::Transfer,
            2U,
            MAX31865Error::InvalidState)));
        CHECK_EQ(MAX31865Error::InvalidState,
                 fixture.driver.recover(100U).code);
        const MAX31865Health after = fixture.driver.health();
        CHECK_EQ(MAX31865State::Ready, after.state);
        CHECK(after.configurationKnown);
        CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
        CHECK_EQ(before.spiTransferFailureCount,
                 after.spiTransferFailureCount);
        CHECK_EQ(expectedConfig,
                 fixture.scripted.device().registerValue(
                     max31865_cmd::REG_CONFIG));
    }
    return true;
}

bool deassertInvalidStateIsATrackedFramingFailure()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::InvalidState)));
    uint8_t output = 0xA5U;
    const MAX31865Status status = fixture.driver.readRegister(
        max31865_cmd::REG_CONFIG, output, 20U);
    CHECK_EQ(MAX31865Error::ChipSelectFailed, status.code);
    CHECK_EQ(static_cast<int32_t>(MAX31865Error::InvalidState),
             status.detail);
    CHECK_EQ(0xA5U, output);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(MAX31865State::Fault, health.state);
    CHECK(!health.configurationKnown);
    CHECK_EQ(1U, health.chipSelectFailureCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK(fixture.scripted.device().chipSelected());

    const uint32_t trackedBefore = health.trackedFailureCount;
    const uint8_t consecutiveBefore = health.consecutiveFailures;
    const uint32_t lastErrorBefore = health.lastErrorMs;
    bool ready = true;
    MAX31865Sample sample{};
    CHECK_EQ(MAX31865Error::ConfigurationUnknown,
             fixture.driver.stop(20U).code);
    CHECK_EQ(MAX31865Error::ConfigurationUnknown,
             fixture.driver.dataReady(ready).code);
    CHECK(ready);
    CHECK_EQ(MAX31865Error::ConfigurationUnknown,
             fixture.driver.readSample(sample).code);
    const MAX31865Health afterPreconditions = fixture.driver.health();
    CHECK_EQ(trackedBefore, afterPreconditions.trackedFailureCount);
    CHECK_EQ(consecutiveBefore, afterPreconditions.consecutiveFailures);
    CHECK_EQ(lastErrorBefore, afterPreconditions.lastErrorMs);
    return true;
}

bool primaryAndDeassertFailureComposeRestoreFailure()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::ChipSelectFailed)));
    uint8_t output = 0xA5U;
    const MAX31865Status status = fixture.driver.readRegister(
        max31865_cmd::REG_CONFIG,
        output,
        20U);
    CHECK_EQ(MAX31865Error::RestoreFailed, status.code);
    const int32_t expectedDetail = static_cast<int32_t>(
        (static_cast<uint32_t>(MAX31865Error::SpiTransferFailed) << 8U) |
        static_cast<uint32_t>(MAX31865Error::ChipSelectFailed));
    CHECK_EQ(expectedDetail, status.detail);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    CHECK_EQ(MAX31865DriverState::OFFLINE,
             fixture.driver.health().driverState);
    CHECK(!fixture.scripted.busLocked());
    return true;
}

bool firstLockTimeoutPreservesKnownReadyStateForMutatingOperations()
{
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::LockBus,
            1U,
            MAX31865Error::BusLockTimeout)));
        CHECK_EQ(
            MAX31865Error::BusLockTimeout,
            fixture.driver.setBias(true, 100U).code);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(MAX31865State::Ready, health.state);
        CHECK(health.configurationKnown);
        CHECK_EQ(MAX31865DriverState::DEGRADED, health.driverState);
        CHECK_EQ(1U, health.consecutiveFailures);
        CHECK_EQ(1U, health.busLockTimeoutCount);
        CHECK_EQ(1U, health.trackedFailureCount);
        CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    }

    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        MAX31865DeviceConfig desired = max31865DefaultDeviceConfig();
        desired.wireMode = MAX31865WireMode::ThreeWire;
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::LockBus,
            1U,
            MAX31865Error::BusLockTimeout)));
        CHECK_EQ(
            MAX31865Error::BusLockTimeout,
            fixture.driver.applyConfiguration(desired, 100U).code);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(MAX31865State::Ready, health.state);
        CHECK(health.configurationKnown);
        CHECK_EQ(MAX31865DriverState::DEGRADED, health.driverState);
        CHECK_EQ(1U, health.busLockTimeoutCount);
        CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    }

    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::LockBus,
            1U,
            MAX31865Error::BusLockTimeout)));
        CHECK_EQ(
            MAX31865Error::BusLockTimeout,
            fixture.driver.restoreWritableDefaults(100U).code);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(MAX31865State::Ready, health.state);
        CHECK(health.configurationKnown);
        CHECK_EQ(MAX31865DriverState::DEGRADED, health.driverState);
        CHECK_EQ(1U, health.busLockTimeoutCount);
        CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    }

    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::LockBus,
            1U,
            MAX31865Error::BusLockTimeout)));
        CHECK_EQ(
            MAX31865Error::BusLockTimeout,
            fixture.driver.recover(100U).code);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(MAX31865State::Ready, health.state);
        CHECK(health.configurationKnown);
        CHECK_EQ(MAX31865DriverState::DEGRADED, health.driverState);
        CHECK_EQ(1U, health.busLockTimeoutCount);
        CHECK_EQ(0U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    }
    return true;
}

bool compositeOperationTimeoutStillIncrementsOriginCounter()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::Transfer,
        1U,
        10000U)));
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::ChipSelectFailed)));
    uint8_t output = 0xA5U;
    const MAX31865Status status = fixture.driver.readRegister(
        max31865_cmd::REG_CONFIG,
        output,
        10U);
    CHECK_EQ(MAX31865Error::RestoreFailed, status.code);
    const int32_t expectedDetail = static_cast<int32_t>(
        (static_cast<uint32_t>(MAX31865Error::OperationTimeout) << 8U) |
        static_cast<uint32_t>(MAX31865Error::ChipSelectFailed));
    CHECK_EQ(expectedDetail, status.detail);
    CHECK_EQ(0xA5U, output);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(MAX31865State::Fault, health.state);
    CHECK(!health.configurationKnown);
    return true;
}

bool zeroBudgetNestedCleanupDoesNotInventADeadline()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();

    // Continuous setup reaches its final CONFIG frame after fault clear,
    // VBIAS enable, and the stale-RTD flush. Fail that transfer, then make the
    // zero-budget idle restore spend a millisecond in its immediate CS callback
    // and fail its first deassert. Zero forbids callback-owned waiting but is
    // not a second elapsed-time deadline, so no timeout provenance is invented.
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        4U,
        MAX31865Error::SpiTransferFailed)));
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::ChipSelectAssert,
        5U,
        1000U)));
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        5U,
        MAX31865Error::ChipSelectFailed)));

    const MAX31865Status status = fixture.driver.startContinuous(100U);
    CHECK_EQ(MAX31865Error::RestoreFailed, status.code);
    const int32_t expectedDetail = static_cast<int32_t>(
        (static_cast<uint32_t>(MAX31865Error::SpiTransferFailed) << 8U) |
        static_cast<uint32_t>(MAX31865Error::ChipSelectFailed));
    CHECK_EQ(expectedDetail, status.detail);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(0U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(MAX31865State::Fault, health.state);
    CHECK(!health.configurationKnown);
    CHECK(fixture.scripted.device().chipSelected());
    return true;
}

bool healthNeutralPrimaryRetainsCleanupTimeoutObservation()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        4U,
        MAX31865Error::InvalidState)));

    const MAX31865Health before = fixture.driver.health();
    const uint32_t clockBaseline = fixture.scripted.nowMsCallCount();
    // Three complete setup frames, the bias-settle boundary, the wired DRDY
    // proof, and the action-free fourth transfer consume 57 clock reads. Make
    // the following required-cleanup boundary expire the original operation.
    fixture.scripted.setNowMsAdvanceUsOnCall(
        clockBaseline + 58U,
        100000U);

    const MAX31865Status status = fixture.driver.startContinuous(100U);
    CHECK_EQ(MAX31865Error::InvalidState, status.code);
    const MAX31865Health after = fixture.driver.health();
    CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
    CHECK_EQ(before.consecutiveFailures, after.consecutiveFailures);
    CHECK_EQ(before.lastErrorMs, after.lastErrorMs);
    CHECK_EQ(before.spiTransferFailureCount,
             after.spiTransferFailureCount);
    CHECK_EQ(before.operationTimeoutCount + 1U,
             after.operationTimeoutCount);
    CHECK_EQ(MAX31865State::Ready, after.state);
    CHECK(after.configurationKnown);
    CHECK_EQ(5U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    CHECK(!fixture.scripted.busLocked());
    CHECK(!fixture.scripted.device().chipSelected());
    return true;
}

bool zeroBudgetOneShotCleanupHasAFixedPassLimit()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();

    // Make the D5 transfer reach the device but fail at the callback boundary,
    // then consume the original operation budget. The driver must quarantine
    // the possibly-started one-shot with its internal no-wait cleanup context.
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        4U,
        MAX31865Error::SpiTransferFailed,
        FailureEffect::Applied,
        10000000U)));

    // Each apparent D2 observation clears on verification and then reappears.
    // Advancing past every newly-created conversion horizon reproduces the
    // adversarial sequence that used to keep timeoutMs==0 cleanup looping.
    fixture.scripted.device().setAlternatingRegisterReadOverride(
        max31865_cmd::REG_FAULT_STATUS,
        max31865_cmd::FAULT_OVER_UNDER_VOLTAGE,
        0U);
    fixture.scripted.setNowMsAdvanceUs(100000U);

    const MAX31865Status status =
        fixture.driver.triggerSingleConversion(10000U);
    CHECK_EQ(MAX31865Error::RestoreFailed, status.code);
    // The original D5 frame crossed its cleanup deadline, whose boundedness
    // result takes precedence while the role counter retains the SPI failure.
    // The outer safety stop then composes that primary timeout with exhaustion
    // of its own fixed no-wait pass budget.
    const int32_t expectedDetail = static_cast<int32_t>(
        (static_cast<uint32_t>(MAX31865Error::OperationTimeout) << 8U) |
        static_cast<uint32_t>(MAX31865Error::OperationTimeout));
    CHECK_EQ(expectedDetail, status.detail);

    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(MAX31865State::Fault, health.state);
    CHECK(!health.configurationKnown);
    CHECK_EQ(1U, health.spiTransferFailureCount);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK(fixture.scripted.eventCount() < 100U);
    CHECK(!fixture.scripted.busLocked());
    return true;
}

bool readbackPatternLockTimeoutSkipsUnneededRestore()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::LockBus,
        2U,
        MAX31865Error::BusLockTimeout)));

    uint8_t observed = 0xA5U;
    CHECK_EQ(
        MAX31865Error::BusLockTimeout,
        fixture.driver.registerReadbackTest(observed, 100U).code);
    CHECK_EQ(0xA5U, observed);
    CHECK_EQ(1U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    CHECK_EQ(2U, fixture.scripted.matchingEventCount(EventKind::LockBus));
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(MAX31865State::Ready, health.state);
    CHECK(health.configurationKnown);
    CHECK_EQ(MAX31865DriverState::DEGRADED, health.driverState);
    CHECK_EQ(1U, health.busLockTimeoutCount);
    return true;
}

bool verifiedReadbackRestoreRecoversPriorCsFailureState()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const uint8_t saved = fixture.scripted.device().registerValue(
        max31865_cmd::REG_LOW_FAULT_LSB);
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        2U,
        MAX31865Error::ChipSelectFailed)));

    uint8_t observed = 0xA5U;
    const MAX31865Status status = fixture.driver.registerReadbackTest(
        observed,
        100U);
    CHECK_EQ(MAX31865Error::ChipSelectFailed, status.code);
    CHECK_EQ(0xA5U, observed);
    CHECK_EQ(saved,
             fixture.scripted.device().registerValue(
                 max31865_cmd::REG_LOW_FAULT_LSB));
    CHECK_EQ(5U, fixture.scripted.matchingEventCount(EventKind::Transfer));
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(MAX31865State::Ready, health.state);
    CHECK_EQ(MAX31865DriverState::DEGRADED, health.driverState);
    CHECK(health.configurationKnown);
    CHECK_EQ(1U, health.chipSelectFailureCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.busLocked());
    return true;
}

bool moduloWraparoundDeadlinesRemainFinite()
{
    DriverFixture fixture;
    fixture.scripted.setTimeUs(
        (static_cast<uint64_t>(UINT32_MAX) - 1ULL) * 1000ULL);
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeTimedSuccess(
        EventKind::LockBus,
        1U,
        2000U)));
    uint8_t output = 0U;
    CHECK(fixture.driver.readRegister(
              max31865_cmd::REG_CONFIG,
              output,
              5U)
              .ok());
    const size_t transferIndex = firstEventOfKind(
        fixture.scripted,
        EventKind::Transfer);
    CHECK(transferIndex < fixture.scripted.eventCount());
    CHECK_EQ(3U, fixture.scripted.event(transferIndex).argument);
    return true;
}

bool beginAndRecoverResolvePreexistingTransientCommands()
{
    {
        DriverFixture fixture;
        CHECK(writeRawConfig(
            fixture.scripted,
            static_cast<uint8_t>(
                max31865_cmd::CONFIG_BIAS |
                max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1)));
        fixture.scripted.advanceTimeUs(
            max31865_cmd::MANUAL_FAULT_STEP1_TO_OPEN_US);
        CHECK_EQ(
            max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1,
            static_cast<uint8_t>(
                fixture.scripted.device().registerValue(
                    max31865_cmd::REG_CONFIG) &
                max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
        CHECK(fixture.begin().ok());
        CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
        CHECK_EQ(
            max31865_cmd::CONFIG_FAULT_CYCLE_NONE,
            static_cast<uint8_t>(
                fixture.scripted.device().registerValue(
                    max31865_cmd::REG_CONFIG) &
                max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    }

    {
        DriverFixture fixture;
        CHECK(fixture.scripted.device().enqueueSample(8000U));
        CHECK(writeRawConfig(
            fixture.scripted,
            static_cast<uint8_t>(
                max31865_cmd::CONFIG_BIAS |
                max31865_cmd::CONFIG_ONE_SHOT)));
        CHECK(fixture.scripted.device().oneShotActive());
        const uint64_t beginStartedUs = fixture.scripted.timeUs();
        CHECK(fixture.begin().ok());
        CHECK(fixture.scripted.timeUs() - beginStartedUs >=
              static_cast<uint64_t>(
                  max31865_cmd::SINGLE_CONVERSION_60HZ_MS) * 1000ULL);
        CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
        CHECK(!fixture.scripted.device().conversionRunning());
        CHECK(!fixture.scripted.device().dataReady());
    }

    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        CHECK(fixture.scripted.device().enqueueSample(8100U));
        CHECK(writeRawConfig(
            fixture.scripted,
            static_cast<uint8_t>(
                max31865_cmd::CONFIG_BIAS |
                max31865_cmd::CONFIG_ONE_SHOT)));
        CHECK(fixture.scripted.device().oneShotActive());
        const uint64_t recoverStartedUs = fixture.scripted.timeUs();
        CHECK(fixture.driver.recover(200U).ok());
        CHECK(fixture.scripted.timeUs() - recoverStartedUs >=
              static_cast<uint64_t>(
                  max31865_cmd::SINGLE_CONVERSION_60HZ_MS) * 1000ULL);
        CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
        CHECK(!fixture.scripted.device().conversionRunning());
        CHECK(!fixture.scripted.device().dataReady());
    }
    return true;
}

bool recoverNeverRestoresFreshnessAfterDiscardedOneShot()
{
    DriverFixture fixture(false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8200U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);
    fixture.scripted.resetLog();

    // recover(): synchronize is lock #1, elapsed FAULT_STATUS proof is #2,
    // RTD discard is #3, and the required idle CONFIG write is #4.
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::LockBus,
        4U,
        MAX31865Error::BusLockTimeout)));
    CHECK_EQ(MAX31865Error::BusLockTimeout,
             fixture.driver.recover(100U).code);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(MAX31865State::Fault, health.state);
    CHECK(!health.configurationKnown);
    CHECK(!fixture.scripted.device().dataReady());

    fixture.scripted.resetLog();
    MAX31865Sample output{};
    memset(&output, 0xA5, sizeof(output));
    const MAX31865Sample before = output;
    CHECK_EQ(MAX31865Error::ConfigurationUnknown,
             fixture.driver.poll(output).code);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"constructor/end zero I/O", constructorAndEndAreZeroIoAndFullyResetState},
        {"invalid begin is callback-free", invalidBeginConfigurationsPerformNoCallbacks},
        {"public preconditions are callback-free",
         publicPreconditionsAreCallbackFreeAndPreserveOutputs},
        {"power-ready failure has no protocol operation",
         powerReadyFailureDoesNotCreateAProtocolOperation},
        {"health timestamps use operation start",
         healthTimestampsUseTheCapturedOperationStart},
        {"begin applies full image", successfulBeginAppliesAndVerifiesCompleteImage},
        {"unused DRDY callback", unusedDrdyCallbackIsAcceptedWhenCapabilityIsFalse},
        {"cached no-conversion readiness is callback-free",
         cachedNoConversionReadinessIsCallbackFree},
        {"nonadvancing sleep", nonadvancingSleepFailsFinitelyAndRestoresIdleState},
        {"late millisecond sleep is skipped",
         finalDeadlineCheckPreventsLateMillisecondSleep},
        {"late sub-millisecond delay is skipped",
         finalDeadlineCheckPreventsLateSubmillisecondDelay},
        {"typed transactional configuration",
         runtimeConfigurationIsTypedTransactionalAndStateGuarded},
        {"recover after failed begin", failedBeginRetainsBindingForExplicitRecovery},
        {"health failure streak", failureStreakTransitionsDegradedOfflineAndBackToReady},
        {"probe health-neutral status", probeIsHealthNeutralAndPreservesLastOperation},
        {"canonical transaction transcript",
         oneTransactionHasCanonicalCsLockAndDelayOrdering},
        {"remaining deadline", remainingDeadlineIsPassedToCallbacks},
        {"lock deadline cleanup", deadlineExpiryAfterLockAlwaysUnlocksWithoutSelecting},
        {"transfer deadline classification",
         deadlineExpiryAfterTransferReportsTimeoutNotCleanupFailure},
        {"zero-timeout immediate frame",
         zeroTimeoutAllowsOneClockAdvancingImmediateFrame},
        {"zero-timeout readSingle checks once",
         zeroTimeoutReadSingleChecksOnceWithoutSleeping},
        {"expired operation performs no bus callback",
         expiredOperationInvokesNoNewBusCallback},
        {"post-assert deadline cutoff",
         deadlineExpiryAfterCsAssertionSkipsSetupAndTransferButCleansUp},
        {"cleanup phase deadline matrix",
         cleanupPhaseDeadlineMatrixIsBoundedAndTransactional},
        {"cleanup timeout precedence",
         cleanupTimeoutTakesPrecedenceAndRetainsOriginCounters},
        {"cleanup failure timeout provenance",
         cleanupFailureRetainsEarlierDeadlineObservation},
        {"timed lock failure timeout classification",
         timedLockFailuresClassifyWholeOperationTimeout},
        {"timed synchronization lock failure",
         timedSynchronizationLockFailureClassifiesOperationTimeout},
        {"timed synchronization lock success deasserts uncertain CS",
         timedSynchronizationLockSuccessStillDeassertsUncertainChipSelect},
        {"timed synchronization deassert failure retains safety status",
         timedSynchronizationDeassertFailureRetainsSafetyStatus},
        {"timed safety cleanup provenance",
         timedSafetyCleanupFailureRetainsDeadlineProvenance},
        {"applied CS assertion failure cleanup",
         failedAppliedCsAssertionIsDeassertedBeforeUnlock},
        {"CS assertion InvalidState remains action-free",
         actionFreeCsAssertInvalidStateDoesNotCreateHiddenUncertainty},
        {"baseline CS deassert InvalidState is untracked",
         baselineCsDeassertInvalidStateRemainsAnUntrackedPrecondition},
        {"uncertain CS deassert InvalidState is framing failure",
         uncertainCsDeassertInvalidStateIsATrackedFramingFailure},
        {"callback status normalization", callbackErrorsAreNormalizedByTransportRole},
        {"null callback message fallbacks",
         nullCallbackMessagesReceiveStaticRoleFallbacks},
        {"callback InvalidState is health-neutral",
         callbackInvalidStateIsPreservedAndHealthNeutral},
        {"action-free recover rejection preserves Ready",
         actionFreeRecoverRejectionPreservesKnownReadyState},
        {"deassert InvalidState is tracked framing failure",
         deassertInvalidStateIsATrackedFramingFailure},
        {"combined primary/cleanup failure",
         primaryAndDeassertFailureComposeRestoreFailure},
        {"first lock timeout preserves known Ready mutation state",
         firstLockTimeoutPreservesKnownReadyStateForMutatingOperations},
        {"composite timeout increments originating counter",
         compositeOperationTimeoutStillIncrementsOriginCounter},
        {"zero-budget nested cleanup has no elapsed deadline",
         zeroBudgetNestedCleanupDoesNotInventADeadline},
        {"health-neutral primary retains cleanup timeout provenance",
         healthNeutralPrimaryRetainsCleanupTimeoutObservation},
        {"zero-budget one-shot cleanup has a fixed pass limit",
         zeroBudgetOneShotCleanupHasAFixedPassLimit},
        {"readback pre-write lock timeout preserves Ready",
         readbackPatternLockTimeoutSkipsUnneededRestore},
        {"verified readback restore recovers CS-failure lifecycle",
         verifiedReadbackRestoreRecoversPriorCsFailureState},
        {"deadline wraparound", moduloWraparoundDeadlinesRemainFinite},
        {"begin/recover resolve transient command state",
         beginAndRecoverResolvePreexistingTransientCommands},
        {"recover never restores discarded one-shot freshness",
         recoverNeverRestoresFreshnessAfterDiscardedOneShot}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
