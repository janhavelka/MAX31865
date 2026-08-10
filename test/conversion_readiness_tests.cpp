#include "support/DriverFixture.h"
#include "support/TestHarness.h"

#include <string.h>

using namespace max31865_test;

namespace {

bool triggerPollAndMetadataConsumeExactlyOneOneShot()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8192U));
    fixture.scripted.resetLog();
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());
    CHECK(fixture.scripted.device().oneShotActive());
    CHECK_EQ(1U, fixture.scripted.device().rtdReadAcknowledgeCount());

    bool ready = true;
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(!ready);
    MAX31865Sample untouched{};
    memset(&untouched, 0xA5, sizeof(untouched));
    const MAX31865Sample before = untouched;
    CHECK_EQ(MAX31865Error::NoData, fixture.driver.poll(untouched).code);
    CHECK(memcmp(&untouched, &before, sizeof(untouched)) == 0);
    CHECK_EQ(1U, fixture.driver.health().noDataCount);

    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(ready);
    MAX31865ReadOptions options{};
    options.hasReadyTimestamp = true;
    options.readyTimestampUs = 4242U;
    options.channelId = 7U;
    MAX31865Sample sample{};
    CHECK(fixture.driver.poll(sample, &options).ok());
    CHECK_EQ(0x4000U, sample.rawRegister);
    CHECK_EQ(8192U, sample.rawCode);
    CHECK_EQ(1U, sample.sampleCounter);
    CHECK_NEAR(100.0F, sample.resistanceOhms, 0.001F);
    CHECK_NEAR(0.0F, sample.temperatureC, 0.01F);
    CHECK_EQ(7U, sample.channelId);
    CHECK_EQ(4242U, sample.readyTimestampUs);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_DATA_VALID) != 0U);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_READ_TIMESTAMP) != 0U);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_READY_TIMESTAMP) != 0U);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.sampleFrameAttemptCount);
    CHECK_EQ(1U, health.sampleFrameSuccessCount);
    CHECK_EQ(0U, health.sampleFrameFailureCount);
    CHECK(health.hasLastSampleTimestamp);
    CHECK_EQ(sample.readTimestampUs, health.lastSampleTimestampUs);

    // Consuming a split one-shot restores the desired idle image before Ready,
    // so configurationKnown and probe() remain truthful.
    CHECK_EQ(0U, static_cast<uint8_t>(
                     fixture.scripted.device().registerValue(
                         max31865_cmd::REG_CONFIG) &
                     max31865_cmd::CONFIG_BIAS));
    MAX31865DeviceInfo info{};
    CHECK(fixture.driver.probe(info).ok());
    CHECK(info.configurationMatches);
    CHECK(fixture.driver.stop(100U).ok());
    CHECK_EQ(0U, static_cast<uint8_t>(
                     fixture.scripted.device().registerValue(
                         max31865_cmd::REG_CONFIG) &
                     max31865_cmd::CONFIG_BIAS));
    return true;
}

bool immediateReadIsRejectedOnlyDuringArmedOneShot()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.device().forceReadySample(10000U);
    MAX31865Sample sample{};
    CHECK(fixture.driver.readSample(sample).ok());
    CHECK_EQ(10000U, sample.rawCode);
    CHECK_EQ(1U, sample.sampleCounter);

    CHECK(fixture.scripted.device().enqueueSample(11000U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.resetLog();
    const uint32_t clockCalls = fixture.scripted.nowMsCallCount();
    memset(&sample, 0xA5, sizeof(sample));
    const MAX31865Sample before = sample;
    CHECK_EQ(MAX31865Error::InvalidState,
             fixture.driver.readSample(sample).code);
    CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(clockCalls, fixture.scripted.nowMsCallCount());
    CHECK(fixture.driver.stop(100U).ok());
    return true;
}

bool readSingleWaitIsBoundedAndTimeoutLeavesConversionStoppable()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(9000U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    MAX31865Sample sample{};
    memset(&sample, 0x5A, sizeof(sample));
    const MAX31865Sample before = sample;
    const uint64_t startUs = fixture.scripted.timeUs();
    CHECK_EQ(MAX31865Error::DrdyTimeout,
             fixture.driver.readSingle(sample, 10U).code);
    CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
    CHECK(fixture.scripted.timeUs() - startUs <= 10000U);
    CHECK_EQ(1U, fixture.driver.health().drdyTimeoutCount);
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());
    CHECK(fixture.driver.stop(100U).ok());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    return true;
}

bool compositeOneShotRestoresIdleBiasAndCommitsMetadata()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8192U));
    MAX31865ReadOptions options{true, 5000U, 2U};
    MAX31865Sample sample{};
    CHECK(fixture.driver.readOneShot(sample, 100U, &options).ok());
    CHECK_EQ(8192U, sample.rawCode);
    CHECK_EQ(2U, sample.channelId);
    CHECK_EQ(5000U, sample.readyTimestampUs);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK_EQ(0U, static_cast<uint8_t>(
                     fixture.scripted.device().registerValue(
                         max31865_cmd::REG_CONFIG) &
                     (max31865_cmd::CONFIG_AUTO |
                      max31865_cmd::CONFIG_ONE_SHOT |
                      max31865_cmd::CONFIG_BIAS)));
    CHECK(!fixture.scripted.device().conversionRunning());
    return true;
}

bool continuousDrdySamplingRemainsArmedAcrossReads()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(7000U));
    CHECK(fixture.scripted.device().enqueueSample(7100U));
    CHECK(fixture.driver.startContinuous(100U).ok());
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());
    CHECK(fixture.scripted.device().continuousConversion());

    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    bool ready = false;
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(ready);
    MAX31865Sample first{};
    CHECK(fixture.driver.poll(first).ok());
    CHECK_EQ(7000U, first.rawCode);
    CHECK_EQ(1U, first.sampleCounter);
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());

    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(!ready);
    fixture.scripted.advanceTimeMs(
        max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS);
    MAX31865Sample second{};
    CHECK(fixture.driver.poll(second).ok());
    CHECK_EQ(7100U, second.rawCode);
    CHECK_EQ(2U, second.sampleCounter);
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());
    CHECK(fixture.driver.stop(20U).ok());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK(!fixture.scripted.device().conversionRunning());
    return true;
}

bool elapsedReadinessWorksWithoutDrdyAndTickPerformsNoCallbacks()
{
    DriverFixture fixture(false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8500U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    const uint32_t startMs = fixture.scripted.nowMs();
    fixture.scripted.resetLog();
    bool ready = true;
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(!ready);
    CHECK_EQ(0U, fixture.scripted.eventCount());

    fixture.driver.tick(startMs +
                        max31865_cmd::SINGLE_CONVERSION_60HZ_MS - 1U);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(!ready);
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);
    fixture.driver.tick(
        startMs + max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(ready);
    MAX31865Sample sample{};
    CHECK(fixture.driver.poll(sample).ok());
    CHECK_EQ(8500U, sample.rawCode);
    return true;
}

bool continuousTickAccountsForAllElapsedOverruns()
{
    DriverFixture fixture(false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8000U));
    CHECK(fixture.driver.startContinuous(100U).ok());
    const uint32_t startMs = fixture.scripted.nowMs();
    fixture.scripted.resetLog();
    fixture.driver.tick(
        startMs + max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);
    CHECK_EQ(0U, fixture.driver.health().overrunCount);
    fixture.driver.tick(
        startMs + max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U +
        (3U * max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS));
    CHECK_EQ(3U, fixture.driver.health().overrunCount);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK(fixture.driver.stop(20U).ok());
    return true;
}

bool lateElapsedReadPreservesContinuousCadence()
{
    DriverFixture fixture(false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8000U));
    CHECK(fixture.scripted.device().enqueueSample(8100U));
    CHECK(fixture.scripted.device().enqueueSample(8200U));
    CHECK(fixture.scripted.device().enqueueSample(8300U));
    CHECK(fixture.driver.startContinuous(100U).ok());
    fixture.scripted.advanceTimeMs(100U);
    MAX31865Sample sample{};
    CHECK(fixture.driver.poll(sample).ok());
    CHECK_EQ(2U, fixture.driver.health().overrunCount);

    bool ready = true;
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(!ready);
    fixture.scripted.advanceTimeMs(10U);
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(ready);
    CHECK(fixture.driver.stop(20U).ok());
    return true;
}

bool gpioFailurePreservesReadinessOutputAndIsTracked()
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
    CHECK_EQ(MAX31865Error::GpioFailed, fixture.driver.dataReady(ready).code);
    CHECK(ready);
    CHECK_EQ(1U, fixture.driver.health().gpioFailureCount);
    CHECK_EQ(MAX31865DriverState::DEGRADED,
             fixture.driver.health().driverState);

    fixture.scripted.clearFailures();
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK_EQ(MAX31865DriverState::DEGRADED,
             fixture.driver.health().driverState);
    CHECK(fixture.driver.stop(20U).ok());
    CHECK_EQ(MAX31865DriverState::READY,
             fixture.driver.health().driverState);
    return true;
}

bool timedGpioFailureRetainsFailureAndDeadlineProvenance()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.startContinuous(100U).ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ReadDrdy,
        1U,
        MAX31865Error::GpioFailed,
        FailureEffect::NotApplied,
        10000U)));

    bool ready = true;
    CHECK_EQ(MAX31865Error::OperationTimeout,
             fixture.driver.dataReady(ready, 10U).code);
    CHECK(ready);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.gpioFailureCount);
    CHECK_EQ(1U, health.operationTimeoutCount);
    CHECK_EQ(1U, health.trackedFailureCount);
    CHECK_EQ(MAX31865State::Converting, health.state);
    return true;
}

bool sampleTransferFailurePreservesOutputAndCountsDrop()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8192U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    MAX31865Sample sample{};
    memset(&sample, 0xA5, sizeof(sample));
    const MAX31865Sample before = sample;
    CHECK_EQ(MAX31865Error::SpiTransferFailed,
             fixture.driver.poll(sample).code);
    CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.sampleFrameAttemptCount);
    CHECK_EQ(0U, health.sampleFrameSuccessCount);
    CHECK_EQ(1U, health.sampleFrameFailureCount);
    CHECK_EQ(1U, health.droppedSampleCount);
    CHECK_EQ(1U, health.spiTransferFailureCount);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    CHECK(fixture.driver.recover(100U).ok());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    return true;
}

bool rtdReadDeassertFailureInvalidatesContinuousAndOneShotState()
{
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        CHECK(fixture.scripted.device().enqueueSample(8192U));
        CHECK(fixture.driver.startContinuous(100U).ok());
        fixture.scripted.advanceTimeMs(
            max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::ChipSelectDeassert,
            1U,
            MAX31865Error::ChipSelectFailed)));
        MAX31865Sample sample{};
        memset(&sample, 0xA5, sizeof(sample));
        const MAX31865Sample before = sample;
        CHECK_EQ(
            MAX31865Error::ChipSelectFailed,
            fixture.driver.poll(sample).code);
        CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(MAX31865State::Fault, health.state);
        CHECK_EQ(MAX31865DriverState::OFFLINE, health.driverState);
        CHECK(!health.configurationKnown);
        CHECK(fixture.scripted.device().chipSelected());
    }

    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        CHECK(fixture.scripted.device().enqueueSample(9000U));
        CHECK(fixture.driver.triggerSingleConversion(100U).ok());
        fixture.scripted.advanceTimeMs(
            max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::ChipSelectDeassert,
            1U,
            MAX31865Error::ChipSelectFailed)));
        MAX31865Sample sample{};
        memset(&sample, 0x5A, sizeof(sample));
        const MAX31865Sample before = sample;
        CHECK_EQ(
            MAX31865Error::ChipSelectFailed,
            fixture.driver.poll(sample).code);
        CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
        const MAX31865Health health = fixture.driver.health();
        CHECK_EQ(MAX31865State::Fault, health.state);
        CHECK_EQ(MAX31865DriverState::OFFLINE, health.driverState);
        CHECK(!health.configurationKnown);
        CHECK(!fixture.scripted.device().chipSelected());
    }
    return true;
}

bool cleanupResynchronizesAmbiguousOneShotBeforeRecovery()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        4U,
        MAX31865Error::ChipSelectFailed)));

    const MAX31865Status trigger =
        fixture.driver.triggerSingleConversion(100U);
    CHECK_EQ(MAX31865Error::ChipSelectFailed, trigger.code);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    CHECK(!fixture.driver.health().configurationKnown);
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.device().oneShotActive());
    CHECK(!fixture.scripted.device().conversionRunning());

    // Cleanup first proves CS high, which commits the transferred D5 frame,
    // then uses the remaining whole-operation budget to wait out and drain the
    // quarantined result. Recovery only has to restore configuration proof.
    fixture.scripted.clearFailures();
    fixture.scripted.resetLog();
    const uint64_t recoverStartedUs = fixture.scripted.timeUs();
    CHECK(fixture.driver.recover(200U).ok());
    const uint64_t recoverElapsedUs =
        fixture.scripted.timeUs() - recoverStartedUs;
    CHECK(recoverElapsedUs < 10000ULL);
    CHECK_EQ(EventKind::LockBus, fixture.scripted.event(0U).kind);
    CHECK_EQ(EventKind::ChipSelectDeassert, fixture.scripted.event(1U).kind);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK(fixture.driver.health().configurationKnown);
    CHECK(!fixture.scripted.device().chipSelected());
    CHECK(!fixture.scripted.device().oneShotActive());
    CHECK(!fixture.scripted.device().conversionRunning());
    CHECK_EQ(
        0U,
        static_cast<uint8_t>(
            fixture.scripted.device().registerValue(
                max31865_cmd::REG_CONFIG) &
            (max31865_cmd::CONFIG_ONE_SHOT |
             max31865_cmd::CONFIG_AUTO)));
    return true;
}

bool validOneShotFrameDroppedWhenIdleRestoreFails()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8192U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        2U,
        MAX31865Error::ChipSelectFailed)));

    MAX31865Sample sample{};
    memset(&sample, 0xA5, sizeof(sample));
    const MAX31865Sample before = sample;
    CHECK_EQ(MAX31865Error::ChipSelectFailed,
             fixture.driver.poll(sample).code);
    CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
    MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.sampleFrameSuccessCount);
    CHECK_EQ(0U, health.sampleFrameFailureCount);
    CHECK_EQ(1U, health.droppedSampleCount);
    CHECK(!health.hasLastSampleTimestamp);
    CHECK_EQ(MAX31865State::Fault, health.state);

    fixture.scripted.clearFailures();
    CHECK(fixture.driver.recover(100U).ok());
    CHECK(fixture.scripted.device().enqueueSample(9000U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    CHECK(fixture.driver.poll(sample).ok());
    CHECK_EQ(9000U, sample.rawCode);
    CHECK_EQ(2U, sample.sampleCounter);
    health = fixture.driver.health();
    CHECK_EQ(2U, health.sampleFrameSuccessCount);
    CHECK_EQ(1U, health.droppedSampleCount);
    CHECK(health.hasLastSampleTimestamp);
    CHECK_EQ(sample.readTimestampUs, health.lastSampleTimestampUs);
    return true;
}

bool lateOneShotIdleRestoreSuppressesOutputAndTimestamp()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8192U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);

    // With delayUs excluded from the millisecond clock, readSingle performs
    // seventeen deadline reads after its start timestamp through the final RTD
    // consumption check. Advancing 1 ms per clock read makes the next check --
    // the required idle-restore boundary -- reach this 18 ms deadline exactly.
    fixture.scripted.setDelayAdvancesTime(false);
    fixture.scripted.setNowMsAdvanceUs(1000U);
    MAX31865Sample sample{};
    memset(&sample, 0xA5, sizeof(sample));
    const MAX31865Sample before = sample;
    const MAX31865Health beforeHealth = fixture.driver.health();

    const MAX31865Status status = fixture.driver.readSingle(sample, 18U);
    CHECK_EQ(MAX31865Error::OperationTimeout, status.code);
    CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(beforeHealth.sampleFrameSuccessCount + 1U,
             health.sampleFrameSuccessCount);
    CHECK_EQ(beforeHealth.droppedSampleCount + 1U,
             health.droppedSampleCount);
    CHECK_EQ(beforeHealth.operationTimeoutCount + 1U,
             health.operationTimeoutCount);
    CHECK_EQ(beforeHealth.trackedFailureCount + 1U,
             health.trackedFailureCount);
    CHECK_EQ(beforeHealth.hasLastSampleTimestamp,
             health.hasLastSampleTimestamp);
    CHECK_EQ(beforeHealth.lastSampleTimestampUs,
             health.lastSampleTimestampUs);
    CHECK_EQ(MAX31865State::Ready, health.state);
    CHECK(health.configurationKnown);
    CHECK_EQ(0U, static_cast<uint8_t>(
                     fixture.scripted.device().registerValue(
                         max31865_cmd::REG_CONFIG) &
                     max31865_cmd::CONFIG_BIAS));
    return true;
}

bool faultedFrameCommitsOnlyFrameAndFaultFields()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    const uint8_t faults = static_cast<uint8_t>(
        max31865_cmd::FAULT_HIGH_THRESHOLD |
        max31865_cmd::FAULT_REFIN_LOW);
    CHECK(fixture.scripted.device().enqueueSample(9000U, faults));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    MAX31865Sample sample{};
    const MAX31865Status status = fixture.driver.poll(sample);
    CHECK_EQ(MAX31865Error::DeviceFault, status.code);
    CHECK_EQ(faults, static_cast<uint8_t>(status.detail));
    CHECK_EQ(9000U, sample.rawCode);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_FAULT_STATUS) != 0U);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_DATA_VALID) == 0U);
    CHECK_EQ(faults, sample.faultStatus.raw);
    CHECK(sample.faultStatus.highThreshold);
    CHECK(sample.faultStatus.refinLow);
    CHECK_EQ(0U, sample.sampleCounter);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.sampleFrameFailureCount);
    CHECK_EQ(1U, health.droppedSampleCount);
    CHECK_EQ(1U, health.faultObservationCount);
    CHECK_EQ(1U, health.thresholdFaultObservationCount);
    CHECK_EQ(1U, health.referenceFaultObservationCount);
    CHECK(health.hasLastFaultStatus);
    CHECK_EQ(faults, health.lastFaultStatus);
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK(fixture.driver.stop(20U).ok());
    return true;
}

bool immediateContinuousReadDoesNotRelatchConsumedElapsedSample()
{
    DriverFixture fixture(false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(8000U));
    CHECK(fixture.scripted.device().enqueueSample(8100U));
    CHECK(fixture.scripted.device().enqueueSample(8200U));
    CHECK(fixture.scripted.device().enqueueSample(8300U));
    CHECK(fixture.driver.startContinuous(100U).ok());
    const uint32_t startMs = fixture.scripted.nowMs();

    // Skip several device cadences without first calling tick()/dataReady().
    fixture.scripted.advanceTimeMs(100U);
    MAX31865Sample sample{};
    CHECK(fixture.driver.readSample(sample).ok());
    CHECK_EQ(8200U, sample.rawCode);
    CHECK_EQ(2U, fixture.driver.health().overrunCount);

    MAX31865Sample untouched{};
    memset(&untouched, 0xA5, sizeof(untouched));
    const MAX31865Sample before = untouched;
    CHECK_EQ(MAX31865Error::NoData, fixture.driver.poll(untouched).code);
    CHECK(memcmp(&untouched, &before, sizeof(untouched)) == 0);

    // Driver cadence is first guarded horizon (56 ms) plus 18 ms periods:
    // after consuming at t=100 ms, the next phase deadline is t=110 ms.
    fixture.scripted.advanceTimeMs(9U);
    bool ready = true;
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(!ready);
    fixture.scripted.advanceTimeMs(1U);
    CHECK_EQ(startMs + 110U, fixture.scripted.nowMs());
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(ready);
    CHECK(fixture.driver.stop(20U).ok());
    return true;
}

bool d2HaltsNoDrdyOneShotUntilClearAndFreshHorizon()
{
    DriverFixture fixture(false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(9000U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.device().setFaultInputs(
        max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);

    bool ready = true;
    const MAX31865Status readyStatus = fixture.driver.dataReady(ready);
    CHECK_EQ(MAX31865Error::DeviceFault, readyStatus.code);
    CHECK_EQ(max31865_cmd::FAULT_OVER_UNDER_VOLTAGE,
             static_cast<uint8_t>(readyStatus.detail));
    CHECK(ready); // output is transactional on failure
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());
    CHECK_EQ(0U, fixture.scripted.device().conversionCount());

    CHECK_EQ(MAX31865Error::DeviceFault,
             fixture.driver.stop(20U).code);
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());
    CHECK_EQ(0U, fixture.scripted.device().conversionCount());

    fixture.scripted.device().setFaultInputs(0U);
    const uint64_t stopStartedUs = fixture.scripted.timeUs();
    CHECK(fixture.driver.stop(100U).ok());
    CHECK(fixture.scripted.timeUs() - stopStartedUs >=
          static_cast<uint64_t>(
              max31865_cmd::SINGLE_CONVERSION_60HZ_MS) * 1000ULL);
    CHECK_EQ(1U, fixture.scripted.device().conversionCount());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK(!fixture.scripted.device().oneShotActive());
    CHECK_EQ(0U, static_cast<uint8_t>(
                     fixture.scripted.device().registerValue(
                         max31865_cmd::REG_CONFIG) &
                     (max31865_cmd::CONFIG_BIAS |
                      max31865_cmd::CONFIG_AUTO |
                      max31865_cmd::CONFIG_ONE_SHOT)));
    fixture.scripted.advanceTimeMs(100U);
    CHECK_EQ(1U, fixture.scripted.device().conversionCount());
    return true;
}

bool d2HaltsWiredOneShotButStopRemainsRecoverable()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(9100U));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.device().setFaultInputs(
        max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);

    bool ready = false;
    CHECK_EQ(MAX31865Error::DeviceFault,
             fixture.driver.dataReady(ready).code);
    CHECK(!ready);
    CHECK_EQ(MAX31865Error::DeviceFault,
             fixture.driver.stop(20U).code);
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());

    fixture.scripted.device().setFaultInputs(0U);
    const uint64_t stopStartedUs = fixture.scripted.timeUs();
    CHECK(fixture.driver.stop(100U).ok());
    CHECK(fixture.scripted.timeUs() - stopStartedUs >=
          static_cast<uint64_t>(
              max31865_cmd::SINGLE_CONVERSION_60HZ_MS) * 1000ULL);
    CHECK_EQ(1U, fixture.scripted.device().conversionCount());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK(!fixture.scripted.device().dataReady());
    return true;
}

bool d2ObservationKeepsNoDrdyContinuousOverrunsHalted()
{
    DriverFixture fixture(false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.startContinuous(100U).ok());
    const uint32_t startMs = fixture.scripted.nowMs();
    fixture.scripted.device().setFaultInputs(
        max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);
    fixture.driver.tick(fixture.scripted.nowMs());

    bool ready = false;
    CHECK_EQ(MAX31865Error::DeviceFault,
             fixture.driver.dataReady(ready).code);
    const uint32_t before = fixture.driver.health().overrunCount;
    fixture.driver.tick(startMs + 1000000U);
    CHECK_EQ(before, fixture.driver.health().overrunCount);

    MAX31865Sample sample{};
    CHECK_EQ(MAX31865Error::DeviceFault,
             fixture.driver.readSample(sample).code);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U);
    CHECK(sample.faultStatus.overUnderVoltage);
    fixture.driver.tick(startMs + 2000000U);
    CHECK_EQ(before, fixture.driver.health().overrunCount);

    CHECK(fixture.driver.stop(20U).ok());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    return true;
}

bool faultBitObservationSurvivesFaultStatusTransferFailure()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    CHECK(fixture.scripted.device().enqueueSample(
        9000U, max31865_cmd::FAULT_HIGH_THRESHOLD));
    CHECK(fixture.driver.triggerSingleConversion(100U).ok());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        2U,
        MAX31865Error::SpiTransferFailed)));

    MAX31865Sample output{};
    memset(&output, 0xA5, sizeof(output));
    const MAX31865Sample before = output;
    CHECK_EQ(MAX31865Error::SpiTransferFailed,
             fixture.driver.poll(output).code);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    const MAX31865Health health = fixture.driver.health();
    CHECK_EQ(1U, health.faultObservationCount);
    CHECK_EQ(0U, health.thresholdFaultObservationCount);
    CHECK(!health.hasLastFaultStatus);
    CHECK_EQ(1U, health.sampleFrameFailureCount);
    CHECK_EQ(1U, health.droppedSampleCount);
    CHECK_EQ(MAX31865State::Ready, health.state);
    return true;
}

bool uncertainOneShotTransferFailureWithHighDrdyCanRecover()
{
    DriverFixture fixture;
    CHECK(fixture.begin().ok());
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        4U,
        MAX31865Error::SpiTransferFailed)));
    CHECK_EQ(MAX31865Error::SpiTransferFailed,
             fixture.driver.triggerSingleConversion(100U).code);
    CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
    CHECK(!fixture.scripted.device().oneShotActive());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);
    fixture.scripted.clearFailures();
    CHECK(fixture.driver.recover(100U).ok());
    CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
    CHECK(!fixture.scripted.device().dataReady());
    CHECK(!fixture.scripted.device().conversionRunning());
    return true;
}

bool failedOneShotDiscardClearsFreshnessAndRecoveryRestoresIdle()
{
    for (uint8_t mode = 0U; mode < 2U; ++mode)
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        CHECK(fixture.scripted.device().enqueueSample(9200U));
        CHECK(fixture.driver.triggerSingleConversion(100U).ok());
        fixture.scripted.advanceTimeMs(
            max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            mode == 0U
                ? EventKind::Transfer
                : EventKind::ChipSelectDeassert,
            1U,
            mode == 0U
                ? MAX31865Error::SpiTransferFailed
                : MAX31865Error::ChipSelectFailed)));

        const MAX31865Status stopped = fixture.driver.stop(20U);
        CHECK_EQ(
            mode == 0U
                ? MAX31865Error::SpiTransferFailed
                : MAX31865Error::ChipSelectFailed,
            stopped.code);
        CHECK_EQ(MAX31865State::Fault, fixture.driver.state());
        fixture.scripted.clearFailures();
        CHECK(fixture.driver.recover(100U).ok());
        CHECK_EQ(MAX31865State::Ready, fixture.driver.state());
        CHECK(!fixture.scripted.device().chipSelected());
        CHECK(!fixture.scripted.device().conversionRunning());
        CHECK_EQ(0U, static_cast<uint8_t>(
                         fixture.scripted.device().registerValue(
                             max31865_cmd::REG_CONFIG) &
                         (max31865_cmd::CONFIG_BIAS |
                          max31865_cmd::CONFIG_AUTO |
                          max31865_cmd::CONFIG_ONE_SHOT)));
    }
    return true;
}

bool elapsedReadinessGuardAndHalfRangeServiceAreExplicit()
{
    {
        DriverFixture fixture(false);
        CHECK(fixture.begin().ok());
        CHECK(fixture.scripted.device().enqueueSample(8000U));
        CHECK(fixture.driver.triggerSingleConversion(100U).ok());
        fixture.scripted.advanceTimeMs(
            max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
        bool ready = true;
        CHECK(fixture.driver.dataReady(ready).ok());
        CHECK(!ready);
        fixture.scripted.advanceTimeMs(1U);
        CHECK(fixture.driver.dataReady(ready).ok());
        CHECK(ready);
        CHECK(fixture.driver.stop(20U).ok());
    }

    {
        DriverFixture fixture(false);
        CHECK(fixture.begin().ok());
        CHECK(fixture.driver.startContinuous(100U).ok());
        fixture.scripted.advanceTimeMs(
            static_cast<uint32_t>(INT32_MAX) - 1U);
        fixture.driver.tick(fixture.scripted.nowMs());
        bool ready = false;
        CHECK(fixture.driver.dataReady(ready).ok());
        CHECK(ready);
        CHECK(fixture.driver.stop(20U).ok());
    }
    return true;
}

bool elapsedReadinessSpiProofRecoversTransportHealth()
{
    DriverFixture fixture(false);
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.startContinuous(100U).ok());
    fixture.scripted.advanceTimeMs(
        max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);
    fixture.scripted.resetLog();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));

    bool ready = false;
    CHECK_EQ(MAX31865Error::SpiTransferFailed,
             fixture.driver.dataReady(ready).code);
    CHECK_EQ(1U, fixture.driver.health().consecutiveFailures);
    CHECK_EQ(MAX31865DriverState::DEGRADED,
             fixture.driver.health().driverState);

    fixture.scripted.clearFailures();
    CHECK(fixture.driver.dataReady(ready).ok());
    CHECK(ready);
    CHECK_EQ(0U, fixture.driver.health().consecutiveFailures);
    CHECK_EQ(MAX31865DriverState::READY,
             fixture.driver.health().driverState);
    CHECK(fixture.driver.stop(20U).ok());
    return true;
}

bool spiBackedNoDataClearsFailureStreakWithoutBecomingFailure()
{
    for (uint8_t useReadSingle = 0U; useReadSingle < 2U; ++useReadSingle)
    {
        DriverFixture fixture;
        CHECK(fixture.begin().ok());
        CHECK(fixture.driver.startContinuous(100U).ok());
        fixture.scripted.device().setFaultInputs(
            max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
        fixture.scripted.advanceTimeMs(
            max31865_cmd::SINGLE_CONVERSION_60HZ_MS + 1U);

        // Establish one tracked transport failure while leaving conversion
        // state usable, then let the next readiness check complete a real SPI
        // FAULT_STATUS proof whose terminal availability result is NoData.
        fixture.scripted.resetLog();
        CHECK(fixture.scripted.addFailure(makeFailure(
            EventKind::ReadDrdy,
            1U,
            MAX31865Error::GpioFailed)));
        bool ready = true;
        CHECK_EQ(MAX31865Error::GpioFailed,
                 fixture.driver.dataReady(ready).code);
        const MAX31865Health before = fixture.driver.health();
        CHECK_EQ(1U, before.consecutiveFailures);

        fixture.scripted.clearFailures();
        fixture.scripted.resetLog();
        fixture.scripted.device().setRegisterReadOverride(
            max31865_cmd::REG_FAULT_STATUS,
            0U);
        MAX31865Sample sample{};
        memset(&sample, 0xA5, sizeof(sample));
        const MAX31865Sample sampleBefore = sample;
        const MAX31865Status status = useReadSingle == 0U
            ? fixture.driver.poll(sample)
            : fixture.driver.readSingle(sample, 0U);
        CHECK_EQ(MAX31865Error::NoData, status.code);
        CHECK(memcmp(&sample, &sampleBefore, sizeof(sample)) == 0);
        CHECK_EQ(1U, fixture.scripted.matchingEventCount(EventKind::Transfer));

        const MAX31865Health after = fixture.driver.health();
        CHECK_EQ(before.trackedSuccessCount + 1U,
                 after.trackedSuccessCount);
        CHECK_EQ(before.trackedFailureCount, after.trackedFailureCount);
        CHECK_EQ(0U, after.consecutiveFailures);
        CHECK_EQ(MAX31865DriverState::READY, after.driverState);
        CHECK_EQ(before.noDataCount + 1U, after.noDataCount);
        CHECK_EQ(before.operationTimeoutCount, after.operationTimeoutCount);
        CHECK_EQ(before.drdyTimeoutCount, after.drdyTimeoutCount);
        CHECK_EQ(MAX31865State::Converting, after.state);
        CHECK(after.configurationKnown);
        CHECK_EQ(MAX31865Error::NoData, after.lastOperation.code);
        CHECK(!fixture.scripted.busLocked());
    }
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"trigger/poll metadata", triggerPollAndMetadataConsumeExactlyOneOneShot},
        {"immediate read guard", immediateReadIsRejectedOnlyDuringArmedOneShot},
        {"bounded readSingle timeout",
         readSingleWaitIsBoundedAndTimeoutLeavesConversionStoppable},
        {"composite one-shot cleanup",
         compositeOneShotRestoresIdleBiasAndCommitsMetadata},
        {"continuous DRDY sampling",
         continuousDrdySamplingRemainsArmedAcrossReads},
        {"elapsed readiness and zero-I/O tick",
         elapsedReadinessWorksWithoutDrdyAndTickPerformsNoCallbacks},
        {"continuous tick overruns", continuousTickAccountsForAllElapsedOverruns},
        {"late elapsed cadence", lateElapsedReadPreservesContinuousCadence},
        {"GPIO failure", gpioFailurePreservesReadinessOutputAndIsTracked},
        {"timed GPIO failure provenance",
         timedGpioFailureRetainsFailureAndDeadlineProvenance},
        {"sample transfer failure", sampleTransferFailurePreservesOutputAndCountsDrop},
        {"RTD read deassert failure invalidates state",
         rtdReadDeassertFailureInvalidatesContinuousAndOneShotState},
        {"cleanup resynchronizes ambiguous one-shot before recovery",
         cleanupResynchronizesAmbiguousOneShotBeforeRecovery},
        {"valid one-shot frame dropped on restore failure",
         validOneShotFrameDroppedWhenIdleRestoreFails},
        {"late one-shot idle restore is transactional",
         lateOneShotIdleRestoreSuppressesOutputAndTimestamp},
        {"faulted sample validity", faultedFrameCommitsOnlyFrameAndFaultFields},
        {"immediate elapsed read consumes exactly once",
         immediateContinuousReadDoesNotRelatchConsumedElapsedSample},
        {"no-DRDY D2 one-shot quarantine",
         d2HaltsNoDrdyOneShotUntilClearAndFreshHorizon},
        {"wired D2 one-shot recovery",
         d2HaltsWiredOneShotButStopRemainsRecoverable},
        {"D2 halts elapsed continuous overrun estimates",
         d2ObservationKeepsNoDrdyContinuousOverrunsHalted},
        {"RTD fault evidence survives status-read failure",
         faultBitObservationSurvivesFaultStatusTransferFailure},
        {"uncertain one-shot transfer recovers with high DRDY",
         uncertainOneShotTransferFailureWithHighDrdyCanRecover},
        {"failed one-shot discard remains recoverable",
         failedOneShotDiscardClearsFreshnessAndRecoveryRestoresIdle},
        {"elapsed guard and half-range service",
         elapsedReadinessGuardAndHalfRangeServiceAreExplicit},
        {"elapsed readiness SPI proof restores health",
         elapsedReadinessSpiProofRecoversTransportHealth},
        {"SPI-backed NoData restores health",
         spiBackedNoDataClearsFailureStreakWithoutBecomingFailure}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
