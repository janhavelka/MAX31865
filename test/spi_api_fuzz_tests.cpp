#include "support/DriverFixture.h"
#include "support/TestHarness.h"

#include <string.h>

using namespace max31865_test;

namespace {

struct SlowTimestampTransport
{
    ScriptedMax31865Transport scripted;
    MAX31865Transport downstream;
    uint64_t nowUsElapsedUs;
    uint64_t nowMsElapsedUs;
    uint32_t nowMsCallCount;
    uint32_t slowNowMsOrdinal;
    bool delayOnCommittedContinuousStart;
    bool delayOnCommittedOneShotStart;
    bool sawContinuous;
    bool sawOneShot;
    uint32_t continuousClockCount;
    uint32_t oneShotClockCount;

    SlowTimestampTransport()
        : scripted(),
          downstream(scripted.makeTransport(true, true, true)),
          nowUsElapsedUs(0U),
          nowMsElapsedUs(0U),
          nowMsCallCount(0U),
          slowNowMsOrdinal(0U),
          delayOnCommittedContinuousStart(false),
          delayOnCommittedOneShotStart(false),
          sawContinuous(false),
          sawOneShot(false),
          continuousClockCount(0U),
          oneShotClockCount(0U)
    {
    }

    static SlowTimestampTransport *self(void *user)
    {
        return static_cast<SlowTimestampTransport *>(user);
    }

    static MAX31865Status lockBus(void *user, uint32_t timeoutMs)
    {
        SlowTimestampTransport *const transport = self(user);
        return transport->downstream.lockBus(
            transport->downstream.user, timeoutMs);
    }

    static void unlockBus(void *user)
    {
        SlowTimestampTransport *const transport = self(user);
        transport->downstream.unlockBus(transport->downstream.user);
    }

    static MAX31865Status setChipSelect(void *user, bool asserted)
    {
        SlowTimestampTransport *const transport = self(user);
        return transport->downstream.setChipSelect(
            transport->downstream.user, asserted);
    }

    static MAX31865Status transfer(
        void *user,
        const uint8_t *tx,
        uint8_t *rx,
        size_t length,
        uint32_t timeoutMs)
    {
        SlowTimestampTransport *const transport = self(user);
        return transport->downstream.transfer(
            transport->downstream.user, tx, rx, length, timeoutMs);
    }

    static MAX31865Status readPin(
        void *user,
        MAX31865Pin pin,
        bool *level)
    {
        SlowTimestampTransport *const transport = self(user);
        return transport->downstream.readPin(
            transport->downstream.user, pin, level);
    }

    static uint32_t nowMs(void *user)
    {
        SlowTimestampTransport *const transport = self(user);
        ++transport->nowMsCallCount;
        const bool continuous =
            transport->scripted.device().continuousConversion();
        const bool oneShot = transport->scripted.device().oneShotActive();
        if (continuous && !transport->sawContinuous)
        {
            transport->sawContinuous = true;
            transport->continuousClockCount = 0U;
        }
        if (oneShot && !transport->sawOneShot)
        {
            transport->sawOneShot = true;
            transport->oneShotClockCount = 0U;
        }
        if (continuous)
        {
            ++transport->continuousClockCount;
        }
        if (oneShot)
        {
            ++transport->oneShotClockCount;
        }
        const bool slowOrdinal = transport->slowNowMsOrdinal != 0U &&
            transport->nowMsCallCount == transport->slowNowMsOrdinal;
        const bool slowContinuousStart =
            transport->delayOnCommittedContinuousStart &&
            transport->continuousClockCount == 5U;
        const bool slowOneShotStart =
            transport->delayOnCommittedOneShotStart &&
            transport->oneShotClockCount == 5U;
        if (slowOrdinal || slowContinuousStart || slowOneShotStart)
        {
            transport->scripted.advanceTimeUs(transport->nowMsElapsedUs);
            transport->delayOnCommittedContinuousStart = false;
            transport->delayOnCommittedOneShotStart = false;
        }
        return transport->downstream.nowMs(transport->downstream.user);
    }

    static uint32_t nowUs(void *user)
    {
        SlowTimestampTransport *const transport = self(user);
        transport->scripted.advanceTimeUs(transport->nowUsElapsedUs);
        return transport->downstream.nowUs(transport->downstream.user);
    }

    static void sleepMs(void *user, uint32_t delayMs)
    {
        SlowTimestampTransport *const transport = self(user);
        transport->downstream.sleepMs(
            transport->downstream.user, delayMs);
    }

    static void delayUs(void *user, uint32_t delay)
    {
        SlowTimestampTransport *const transport = self(user);
        transport->downstream.delayUs(transport->downstream.user, delay);
    }

    MAX31865Transport makeTransport()
    {
        MAX31865Transport transport = downstream;
        transport.user = this;
        transport.lockBus = lockBus;
        transport.unlockBus = unlockBus;
        transport.setChipSelect = setChipSelect;
        transport.transfer = transfer;
        transport.readPin = readPin;
        transport.nowMs = nowMs;
        transport.nowUs = nowUs;
        transport.sleepMs = sleepMs;
        transport.delayUs = delayUs;
        return transport;
    }

    void resetNowMsScript()
    {
        nowMsCallCount = 0U;
        slowNowMsOrdinal = 0U;
        delayOnCommittedContinuousStart = false;
        delayOnCommittedOneShotStart = false;
        sawContinuous = scripted.device().continuousConversion();
        sawOneShot = scripted.device().oneShotActive();
        continuousClockCount = 0U;
        oneShotClockCount = 0U;
    }
};

bool elapsedReadinessCannotSucceedPastItsDeadline()
{
    DriverFixture fixture(false, true, true);
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.startContinuous(100U).ok());
    fixture.scripted.resetLog();
    fixture.scripted.setNowMsAdvanceUs(2000U);

    bool ready = true;
    const MAX31865Status status = fixture.driver.dataReady(ready, 1U);
    CHECK_EQ(MAX31865Error::OperationTimeout, status.code);
    CHECK(ready);
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(1U, fixture.driver.health().operationTimeoutCount);
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());
    return true;
}

bool expiredDrdyCheckDoesNotInvokeTheGpioCallback()
{
    DriverFixture fixture(true, true, true);
    CHECK(fixture.begin().ok());
    CHECK(fixture.driver.startContinuous(100U).ok());
    fixture.scripted.resetLog();
    fixture.scripted.setNowMsAdvanceUs(2000U);

    bool ready = true;
    const MAX31865Status status = fixture.driver.dataReady(ready, 1U);
    CHECK_EQ(MAX31865Error::OperationTimeout, status.code);
    CHECK(ready);
    CHECK_EQ(0U,
             fixture.scripted.matchingEventCount(EventKind::ReadDrdy));
    CHECK_EQ(1U, fixture.driver.health().operationTimeoutCount);
    CHECK_EQ(MAX31865State::Converting, fixture.driver.state());
    return true;
}

bool slowOptionalTimestampCannotPublishAFramePastTheDeadline()
{
    SlowTimestampTransport transport;
    MAX31865 driver;
    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = transport.makeTransport();
    CHECK(driver.begin(config).ok());

    transport.scripted.device().forceReadySample(8192U);
    transport.scripted.resetLog();
    transport.nowUsElapsedUs = 2000U;
    MAX31865Sample output{};
    memset(&output, 0xA5, sizeof(output));
    const MAX31865Sample sentinel = output;
    const MAX31865Health before = driver.health();

    const MAX31865Status status = driver.readSample(output, 1U);
    CHECK_EQ(MAX31865Error::OperationTimeout, status.code);
    CHECK_EQ(0, memcmp(&sentinel, &output, sizeof(output)));
    const MAX31865Health after = driver.health();
    CHECK_EQ(before.sampleFrameAttemptCount + 1U,
             after.sampleFrameAttemptCount);
    CHECK_EQ(before.sampleFrameFailureCount + 1U,
             after.sampleFrameFailureCount);
    CHECK_EQ(before.sampleFrameSuccessCount,
             after.sampleFrameSuccessCount);
    CHECK_EQ(before.droppedSampleCount + 1U,
             after.droppedSampleCount);
    CHECK_EQ(before.operationTimeoutCount + 1U,
             after.operationTimeoutCount);
    CHECK(!transport.scripted.busLocked());
    CHECK(!transport.scripted.device().chipSelected());
    return true;
}

bool lateOneShotTimestampStillRestoresTheIdleImage()
{
    SlowTimestampTransport transport;
    MAX31865 driver;
    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = transport.makeTransport();
    CHECK(driver.begin(config).ok());
    CHECK(driver.triggerSingleConversion(100U).ok());
    transport.scripted.advanceTimeMs(70U);
    transport.scripted.resetLog();
    transport.nowUsElapsedUs = 2000U;
    MAX31865Sample output{};
    memset(&output, 0xA5, sizeof(output));
    const MAX31865Sample sentinel = output;
    const MAX31865Health before = driver.health();

    CHECK_EQ(MAX31865Error::OperationTimeout,
             driver.poll(output, 1U).code);
    CHECK_EQ(0, memcmp(&sentinel, &output, sizeof(output)));
    const MAX31865Health after = driver.health();
    CHECK_EQ(before.sampleFrameAttemptCount + 1U,
             after.sampleFrameAttemptCount);
    CHECK_EQ(before.sampleFrameFailureCount + 1U,
             after.sampleFrameFailureCount);
    CHECK_EQ(before.droppedSampleCount + 1U,
             after.droppedSampleCount);
    CHECK_EQ(MAX31865State::Ready, driver.state());
    CHECK(after.configurationKnown);
    CHECK(!transport.scripted.device().oneShotActive());
    CHECK(!transport.scripted.busLocked());
    CHECK(!transport.scripted.device().chipSelected());
    return true;
}

bool wiredHighReadinessClockCannotCrossTheDeadlineUnnoticed()
{
    SlowTimestampTransport transport;
    MAX31865 driver;
    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = transport.makeTransport();
    CHECK(driver.begin(config).ok());
    CHECK(driver.startContinuous(100U).ok());
    transport.scripted.resetLog();
    transport.resetNowMsScript();
    transport.nowMsElapsedUs = 2000U;
    transport.slowNowMsOrdinal = 4U;

    bool ready = true;
    CHECK_EQ(MAX31865Error::OperationTimeout,
             driver.dataReady(ready, 1U).code);
    CHECK(ready);
    CHECK_EQ(1U,
             transport.scripted.matchingEventCount(EventKind::ReadDrdy));
    CHECK_EQ(MAX31865State::Converting, driver.state());
    return true;
}

bool committedContinuousStartCannotReportSuccessAfterDeadline()
{
    SlowTimestampTransport transport;
    MAX31865 driver;
    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = transport.makeTransport();
    CHECK(driver.begin(config).ok());
    transport.resetNowMsScript();
    transport.nowMsElapsedUs = 25000U;
    transport.delayOnCommittedContinuousStart = true;

    const MAX31865Status status = driver.startContinuous(20U);
    CHECK_EQ(MAX31865Error::OperationTimeout, status.code);
    CHECK(!transport.scripted.busLocked());
    CHECK(!transport.scripted.device().chipSelected());
    CHECK_EQ(MAX31865State::Ready, driver.state());
    CHECK(!transport.scripted.device().continuousConversion());
    CHECK(driver.health().configurationKnown);
    return true;
}

bool committedOneShotStartIsQuarantinedAfterDeadline()
{
    SlowTimestampTransport transport;
    MAX31865 driver;
    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = transport.makeTransport();
    CHECK(driver.begin(config).ok());
    transport.resetNowMsScript();
    transport.nowMsElapsedUs = 25000U;
    transport.delayOnCommittedOneShotStart = true;

    const MAX31865Status status = driver.triggerSingleConversion(20U);
    CHECK_EQ(MAX31865Error::RestoreFailed, status.code);
    CHECK_EQ(MAX31865State::Fault, driver.state());
    CHECK(!driver.health().configurationKnown);
    CHECK(transport.scripted.device().oneShotActive());
    CHECK(!transport.scripted.busLocked());
    CHECK(!transport.scripted.device().chipSelected());
    CHECK_EQ(1U, driver.health().operationTimeoutCount);
    transport.scripted.advanceTimeMs(70U);
    CHECK(driver.recover(200U).ok());
    CHECK_EQ(MAX31865State::Ready, driver.state());
    CHECK(driver.health().configurationKnown);
    CHECK(!transport.scripted.device().oneShotActive());
    return true;
}

bool continuousConsumptionClockCannotPublishPastTheDeadline()
{
    SlowTimestampTransport transport;
    MAX31865 driver;
    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = transport.makeTransport();
    config.transport.nowUs = nullptr;
    CHECK(driver.begin(config).ok());
    CHECK(driver.startContinuous(100U).ok());
    transport.scripted.device().forceReadySample(8192U);
    transport.scripted.resetLog();
    transport.resetNowMsScript();
    transport.nowMsElapsedUs = 2000U;
    transport.slowNowMsOrdinal = 15U;
    MAX31865Sample output{};
    memset(&output, 0xA5, sizeof(output));
    const MAX31865Sample sentinel = output;
    const MAX31865Health before = driver.health();

    CHECK_EQ(MAX31865Error::OperationTimeout,
             driver.readSample(output, 1U).code);
    CHECK_EQ(0, memcmp(&sentinel, &output, sizeof(output)));
    const MAX31865Health after = driver.health();
    CHECK_EQ(before.sampleFrameAttemptCount + 1U,
             after.sampleFrameAttemptCount);
    CHECK_EQ(before.sampleFrameFailureCount + 1U,
             after.sampleFrameFailureCount);
    CHECK_EQ(before.sampleFrameSuccessCount,
             after.sampleFrameSuccessCount);
    CHECK_EQ(before.droppedSampleCount + 1U,
             after.droppedSampleCount);
    CHECK_EQ(MAX31865State::Converting, driver.state());
    CHECK(!transport.scripted.busLocked());
    CHECK(!transport.scripted.device().chipSelected());
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"elapsed readiness deadline",
         elapsedReadinessCannotSucceedPastItsDeadline},
        {"DRDY pre-deadline cutoff",
         expiredDrdyCheckDoesNotInvokeTheGpioCallback},
        {"optional timestamp deadline",
         slowOptionalTimestampCannotPublishAFramePastTheDeadline},
        {"one-shot timestamp cleanup",
         lateOneShotTimestampStillRestoresTheIdleImage},
        {"wired readiness horizon clock",
         wiredHighReadinessClockCannotCrossTheDeadlineUnnoticed},
        {"committed continuous start clock",
         committedContinuousStartCannotReportSuccessAfterDeadline},
        {"committed one-shot start clock",
         committedOneShotStartIsQuarantinedAfterDeadline},
        {"continuous consumption clock",
         continuousConsumptionClockCannotPublishPastTheDeadline}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
