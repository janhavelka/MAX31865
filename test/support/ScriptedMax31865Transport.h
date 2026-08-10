#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MAX31865/Transport.h"
#include "support/Max31865DeviceModel.h"

namespace max31865_test {

static constexpr size_t TRANSPORT_EVENT_CAPACITY = 1024U;
static constexpr size_t TRANSPORT_FAILURE_CAPACITY = 32U;

enum class EventKind : uint8_t {
    LockBus = 0,
    UnlockBus,
    ChipSelectAssert,
    ChipSelectDeassert,
    Transfer,
    ReadDrdy,
    SleepMs,
    DelayUs,
    Count
};

struct TransportEvent {
    EventKind kind;
    uint32_t ordinal;
    uint32_t matchingOrdinal;
    uint64_t timeUs;
    uint32_t argument;
    bool level;
    uint8_t tx[max31865_cmd::MAX_FRAME_BYTES];
    uint8_t rx[max31865_cmd::MAX_FRAME_BYTES];
    uint8_t length;
    MAX31865Error result;
};

enum class FailureEffect : uint8_t {
    NotApplied = 0,
    Applied
};

struct FailureInjection {
    bool enabled;
    EventKind kind;
    uint32_t matchingOrdinal;
    MAX31865Status status;
    FailureEffect effect;
    uint64_t callbackElapsedUs;
};

/** Deterministic callback table, event transcript, and failure injector. */
class ScriptedMax31865Transport final {
public:
    ScriptedMax31865Transport();

    Max31865DeviceModel &device();
    const Max31865DeviceModel &device() const;

    void resetLog();
    void clearFailures();
    bool addFailure(const FailureInjection &failure);

    void setTimeUs(uint64_t timeUs);
    void advanceTimeUs(uint64_t elapsedUs);
    void advanceTimeMs(uint32_t elapsedMs);
    void setSleepAdvancesTime(bool enabled);
    void setDelayAdvancesTime(bool enabled);
    void setNowMsAdvanceUs(uint64_t elapsedUsPerRead);
    void setNowMsAdvanceUsOnCall(
        uint32_t callOrdinal,
        uint64_t elapsedUs);

    size_t eventCount() const;
    const TransportEvent &event(size_t index) const;
    uint32_t matchingEventCount(EventKind kind) const;
    bool busLocked() const;
    uint64_t timeUs() const;
    uint32_t nowMsCallCount() const;

    MAX31865Status lockBus(uint32_t timeoutMs);
    void unlockBus();
    MAX31865Status setChipSelect(bool asserted);
    MAX31865Status transfer(
        const uint8_t *tx,
        uint8_t *rx,
        size_t length,
        uint32_t timeoutMs);
    MAX31865Status readDrdy(bool &level);
    uint32_t nowMs();
    uint32_t nowUs() const;
    void sleepMs(uint32_t ms);
    void delayUs(uint32_t us);

    MAX31865Transport makeTransport(
        bool hasDrdy = true,
        bool includeBusLock = true,
        bool includeNowUs = true);

private:
    TransportEvent &recordEvent(EventKind kind);
    FailureInjection *takeFailure(
        EventKind kind,
        uint32_t matchingOrdinal);
    static size_t kindIndex(EventKind kind);

    static MAX31865Status lockBusThunk(void *user, uint32_t timeoutMs);
    static void unlockBusThunk(void *user);
    static MAX31865Status setChipSelectThunk(void *user, bool asserted);
    static MAX31865Status transferThunk(
        void *user,
        const uint8_t *tx,
        uint8_t *rx,
        size_t length,
        uint32_t timeoutMs);
    static MAX31865Status readPinThunk(
        void *user,
        MAX31865Pin pin,
        bool *level);
    static uint32_t nowMsThunk(void *user);
    static uint32_t nowUsThunk(void *user);
    static void sleepMsThunk(void *user, uint32_t ms);
    static void delayUsThunk(void *user, uint32_t us);

    Max31865DeviceModel _device;
    TransportEvent _events[TRANSPORT_EVENT_CAPACITY];
    size_t _eventCount;
    uint32_t _matchingCounts[static_cast<size_t>(EventKind::Count)];
    FailureInjection _failures[TRANSPORT_FAILURE_CAPACITY];
    size_t _failureCount;
    uint64_t _timeUs;
    bool _sleepAdvancesTime;
    bool _delayAdvancesTime;
    uint64_t _nowMsAdvanceUs;
    uint32_t _nowMsAdvanceCallOrdinal;
    uint64_t _nowMsScheduledAdvanceUs;
    uint32_t _nowMsCallCount;
    bool _busLocked;
};

FailureInjection makeFailure(
    EventKind kind,
    uint32_t matchingOrdinal,
    MAX31865Error error,
    FailureEffect effect = FailureEffect::NotApplied,
    uint64_t callbackElapsedUs = 0U,
    int32_t detail = 0);

FailureInjection makeTimedSuccess(
    EventKind kind,
    uint32_t matchingOrdinal,
    uint64_t callbackElapsedUs);

} // namespace max31865_test
