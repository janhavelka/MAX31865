#include "support/ScriptedMax31865Transport.h"

#include <stdlib.h>
#include <string.h>

namespace max31865_test {
namespace {

MAX31865Status scriptedArgumentError()
{
    return MAX31865Status::Error(
        MAX31865Error::InvalidArgument,
        "Invalid scripted transport argument");
}

} // namespace

ScriptedMax31865Transport::ScriptedMax31865Transport()
    : _eventCount(0U),
      _failureCount(0U),
      _timeUs(0U),
      _sleepAdvancesTime(true),
      _delayAdvancesTime(true),
      _nowMsAdvanceUs(0U),
      _nowMsAdvanceCallOrdinal(0U),
      _nowMsScheduledAdvanceUs(0U),
      _nowMsCallCount(0U),
      _busLocked(false)
{
    memset(_events, 0, sizeof(_events));
    memset(_matchingCounts, 0, sizeof(_matchingCounts));
    memset(_failures, 0, sizeof(_failures));
}

Max31865DeviceModel &ScriptedMax31865Transport::device()
{
    return _device;
}

const Max31865DeviceModel &ScriptedMax31865Transport::device() const
{
    return _device;
}

void ScriptedMax31865Transport::resetLog()
{
    memset(_events, 0, sizeof(_events));
    memset(_matchingCounts, 0, sizeof(_matchingCounts));
    _eventCount = 0U;
}

void ScriptedMax31865Transport::clearFailures()
{
    memset(_failures, 0, sizeof(_failures));
    _failureCount = 0U;
}

bool ScriptedMax31865Transport::addFailure(
    const FailureInjection &failure)
{
    if (_failureCount >= TRANSPORT_FAILURE_CAPACITY ||
        failure.kind == EventKind::Count ||
        failure.matchingOrdinal == 0U) {
        return false;
    }
    _failures[_failureCount] = failure;
    ++_failureCount;
    return true;
}

void ScriptedMax31865Transport::setTimeUs(uint64_t timeUs)
{
    if (timeUs >= _timeUs) {
        _device.advanceTimeUs(timeUs - _timeUs);
    } else {
        _device.powerOnReset();
        _device.advanceTimeUs(timeUs);
    }
    _timeUs = timeUs;
}

void ScriptedMax31865Transport::advanceTimeUs(uint64_t elapsedUs)
{
    if (UINT64_MAX - _timeUs < elapsedUs) {
        elapsedUs = UINT64_MAX - _timeUs;
    }
    _timeUs += elapsedUs;
    _device.advanceTimeUs(elapsedUs);
}

void ScriptedMax31865Transport::advanceTimeMs(uint32_t elapsedMs)
{
    advanceTimeUs(static_cast<uint64_t>(elapsedMs) * 1000ULL);
}

void ScriptedMax31865Transport::setSleepAdvancesTime(bool enabled)
{
    _sleepAdvancesTime = enabled;
}

void ScriptedMax31865Transport::setDelayAdvancesTime(bool enabled)
{
    _delayAdvancesTime = enabled;
}

void ScriptedMax31865Transport::setNowMsAdvanceUs(
    uint64_t elapsedUsPerRead)
{
    _nowMsAdvanceUs = elapsedUsPerRead;
}

void ScriptedMax31865Transport::setNowMsAdvanceUsOnCall(
    uint32_t callOrdinal,
    uint64_t elapsedUs)
{
    _nowMsAdvanceCallOrdinal = callOrdinal;
    _nowMsScheduledAdvanceUs = elapsedUs;
}

size_t ScriptedMax31865Transport::eventCount() const
{
    return _eventCount;
}

const TransportEvent &ScriptedMax31865Transport::event(size_t index) const
{
    if (index >= _eventCount) {
        abort();
    }
    return _events[index];
}

uint32_t ScriptedMax31865Transport::matchingEventCount(EventKind kind) const
{
    if (kind == EventKind::Count) {
        return 0U;
    }
    return _matchingCounts[kindIndex(kind)];
}

bool ScriptedMax31865Transport::busLocked() const
{
    return _busLocked;
}

uint64_t ScriptedMax31865Transport::timeUs() const
{
    return _timeUs;
}

uint32_t ScriptedMax31865Transport::nowMsCallCount() const
{
    return _nowMsCallCount;
}

MAX31865Status ScriptedMax31865Transport::lockBus(uint32_t timeoutMs)
{
    TransportEvent &entry = recordEvent(EventKind::LockBus);
    entry.argument = timeoutMs;
    FailureInjection *const failure = takeFailure(
        EventKind::LockBus,
        entry.matchingOrdinal);
    if (failure != nullptr && !failure->status.ok()) {
        advanceTimeUs(failure->callbackElapsedUs);
        entry.result = failure->status.code;
        return failure->status;
    }
    if (_busLocked) {
        const MAX31865Status result = MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "Scripted bus is already locked");
        entry.result = result.code;
        return result;
    }
    _busLocked = true;
    if (failure != nullptr) {
        advanceTimeUs(failure->callbackElapsedUs);
    }
    entry.result = MAX31865Error::Ok;
    return MAX31865Status::Ok();
}

void ScriptedMax31865Transport::unlockBus()
{
    TransportEvent &entry = recordEvent(EventKind::UnlockBus);
    FailureInjection *const failure = takeFailure(
        EventKind::UnlockBus,
        entry.matchingOrdinal);
    _busLocked = false;
    if (failure != nullptr) {
        advanceTimeUs(failure->callbackElapsedUs);
        entry.result = failure->status.code;
    }
}

MAX31865Status ScriptedMax31865Transport::setChipSelect(bool asserted)
{
    const EventKind kind = asserted
        ? EventKind::ChipSelectAssert
        : EventKind::ChipSelectDeassert;
    TransportEvent &entry = recordEvent(kind);
    entry.level = asserted;
    FailureInjection *const failure = takeFailure(kind, entry.matchingOrdinal);
    const bool apply = failure == nullptr || failure->status.ok() ||
        failure->effect == FailureEffect::Applied;

    MAX31865Status result = MAX31865Status::Ok();
    if (apply) {
        result = _device.setChipSelect(asserted);
    }
    if (failure != nullptr) {
        advanceTimeUs(failure->callbackElapsedUs);
        if (!failure->status.ok()) {
            entry.result = failure->status.code;
            return failure->status;
        }
    }
    entry.result = result.code;
    return result;
}

MAX31865Status ScriptedMax31865Transport::transfer(
    const uint8_t *tx,
    uint8_t *rx,
    size_t length,
    uint32_t timeoutMs)
{
    TransportEvent &entry = recordEvent(EventKind::Transfer);
    entry.argument = timeoutMs;
    FailureInjection *const failure = takeFailure(
        EventKind::Transfer,
        entry.matchingOrdinal);

    if (tx == nullptr || rx == nullptr || length == 0U ||
        length > max31865_cmd::MAX_FRAME_BYTES) {
        if (failure != nullptr) {
            advanceTimeUs(failure->callbackElapsedUs);
        }
        const MAX31865Status result =
            failure != nullptr && !failure->status.ok()
            ? failure->status
            : scriptedArgumentError();
        entry.result = result.code;
        return result;
    }

    entry.length = static_cast<uint8_t>(length);
    memcpy(entry.tx, tx, length);
    const bool apply = failure == nullptr || failure->status.ok() ||
        failure->effect == FailureEffect::Applied;
    MAX31865Status result = MAX31865Status::Ok();
    if (apply) {
        result = _device.transfer(tx, rx, length);
        if (result.ok()) {
            memcpy(entry.rx, rx, length);
        }
    }
    if (failure != nullptr) {
        advanceTimeUs(failure->callbackElapsedUs);
        if (!failure->status.ok()) {
            entry.result = failure->status.code;
            return failure->status;
        }
    }
    entry.result = result.code;
    return result;
}

MAX31865Status ScriptedMax31865Transport::readDrdy(bool &level)
{
    TransportEvent &entry = recordEvent(EventKind::ReadDrdy);
    FailureInjection *const failure = takeFailure(
        EventKind::ReadDrdy,
        entry.matchingOrdinal);
    const bool apply = failure == nullptr || failure->status.ok() ||
        failure->effect == FailureEffect::Applied;
    bool candidate = level;
    MAX31865Status result = MAX31865Status::Ok();
    if (apply) {
        result = _device.readDrdy(candidate);
    }
    if (failure != nullptr) {
        advanceTimeUs(failure->callbackElapsedUs);
        if (!failure->status.ok()) {
            entry.result = failure->status.code;
            return failure->status;
        }
    }
    if (result.ok()) {
        level = candidate;
        entry.level = level;
    }
    entry.result = result.code;
    return result;
}

uint32_t ScriptedMax31865Transport::nowMs()
{
    if (_nowMsCallCount < UINT32_MAX) {
        ++_nowMsCallCount;
    }
    advanceTimeUs(_nowMsAdvanceUs);
    if (_nowMsAdvanceCallOrdinal != 0U &&
        _nowMsCallCount == _nowMsAdvanceCallOrdinal) {
        advanceTimeUs(_nowMsScheduledAdvanceUs);
    }
    return static_cast<uint32_t>(_timeUs / 1000ULL);
}

uint32_t ScriptedMax31865Transport::nowUs() const
{
    return static_cast<uint32_t>(_timeUs);
}

void ScriptedMax31865Transport::sleepMs(uint32_t ms)
{
    TransportEvent &entry = recordEvent(EventKind::SleepMs);
    entry.argument = ms;
    FailureInjection *const failure = takeFailure(
        EventKind::SleepMs,
        entry.matchingOrdinal);
    const bool apply = failure == nullptr || failure->status.ok() ||
        failure->effect == FailureEffect::Applied;
    if (apply && _sleepAdvancesTime) {
        advanceTimeMs(ms);
    }
    if (failure != nullptr) {
        advanceTimeUs(failure->callbackElapsedUs);
        entry.result = failure->status.code;
    }
}

void ScriptedMax31865Transport::delayUs(uint32_t us)
{
    TransportEvent &entry = recordEvent(EventKind::DelayUs);
    entry.argument = us;
    FailureInjection *const failure = takeFailure(
        EventKind::DelayUs,
        entry.matchingOrdinal);
    const bool apply = failure == nullptr || failure->status.ok() ||
        failure->effect == FailureEffect::Applied;
    if (apply && _delayAdvancesTime) {
        advanceTimeUs(us);
    }
    if (failure != nullptr) {
        advanceTimeUs(failure->callbackElapsedUs);
        entry.result = failure->status.code;
    }
}

MAX31865Transport ScriptedMax31865Transport::makeTransport(
    bool hasDrdy,
    bool includeBusLock,
    bool includeNowUs)
{
    MAX31865Transport transport = {};
    transport.user = this;
    transport.capabilities = {hasDrdy};
    transport.lockBus = includeBusLock ? lockBusThunk : nullptr;
    transport.unlockBus = includeBusLock ? unlockBusThunk : nullptr;
    transport.setChipSelect = setChipSelectThunk;
    transport.transfer = transferThunk;
    transport.readPin = hasDrdy ? readPinThunk : nullptr;
    transport.nowMs = nowMsThunk;
    transport.nowUs = includeNowUs ? nowUsThunk : nullptr;
    transport.sleepMs = sleepMsThunk;
    transport.delayUs = delayUsThunk;
    return transport;
}

TransportEvent &ScriptedMax31865Transport::recordEvent(EventKind kind)
{
    if (_eventCount >= TRANSPORT_EVENT_CAPACITY || kind == EventKind::Count) {
        abort();
    }
    TransportEvent &entry = _events[_eventCount];
    memset(&entry, 0, sizeof(entry));
    entry.kind = kind;
    entry.ordinal = static_cast<uint32_t>(_eventCount + 1U);
    entry.timeUs = _timeUs;
    const size_t index = kindIndex(kind);
    _matchingCounts[index] = _matchingCounts[index] == UINT32_MAX
        ? UINT32_MAX
        : _matchingCounts[index] + 1U;
    entry.matchingOrdinal = _matchingCounts[index];
    entry.result = MAX31865Error::Ok;
    ++_eventCount;
    return entry;
}

FailureInjection *ScriptedMax31865Transport::takeFailure(
    EventKind kind,
    uint32_t matchingOrdinal)
{
    for (size_t index = 0U; index < _failureCount; ++index) {
        FailureInjection &failure = _failures[index];
        if (failure.enabled && failure.kind == kind &&
            failure.matchingOrdinal == matchingOrdinal) {
            failure.enabled = false;
            return &failure;
        }
    }
    return nullptr;
}

size_t ScriptedMax31865Transport::kindIndex(EventKind kind)
{
    return static_cast<size_t>(kind);
}

MAX31865Status ScriptedMax31865Transport::lockBusThunk(
    void *user,
    uint32_t timeoutMs)
{
    return static_cast<ScriptedMax31865Transport *>(user)->lockBus(timeoutMs);
}

void ScriptedMax31865Transport::unlockBusThunk(void *user)
{
    static_cast<ScriptedMax31865Transport *>(user)->unlockBus();
}

MAX31865Status ScriptedMax31865Transport::setChipSelectThunk(
    void *user,
    bool asserted)
{
    return static_cast<ScriptedMax31865Transport *>(user)->setChipSelect(
        asserted);
}

MAX31865Status ScriptedMax31865Transport::transferThunk(
    void *user,
    const uint8_t *tx,
    uint8_t *rx,
    size_t length,
    uint32_t timeoutMs)
{
    return static_cast<ScriptedMax31865Transport *>(user)->transfer(
        tx,
        rx,
        length,
        timeoutMs);
}

MAX31865Status ScriptedMax31865Transport::readPinThunk(
    void *user,
    MAX31865Pin pin,
    bool *level)
{
    if (pin != MAX31865Pin::DRDY || level == nullptr) {
        return scriptedArgumentError();
    }
    return static_cast<ScriptedMax31865Transport *>(user)->readDrdy(*level);
}

uint32_t ScriptedMax31865Transport::nowMsThunk(void *user)
{
    return static_cast<ScriptedMax31865Transport *>(user)->nowMs();
}

uint32_t ScriptedMax31865Transport::nowUsThunk(void *user)
{
    return static_cast<ScriptedMax31865Transport *>(user)->nowUs();
}

void ScriptedMax31865Transport::sleepMsThunk(void *user, uint32_t ms)
{
    static_cast<ScriptedMax31865Transport *>(user)->sleepMs(ms);
}

void ScriptedMax31865Transport::delayUsThunk(void *user, uint32_t us)
{
    static_cast<ScriptedMax31865Transport *>(user)->delayUs(us);
}

FailureInjection makeFailure(
    EventKind kind,
    uint32_t matchingOrdinal,
    MAX31865Error error,
    FailureEffect effect,
    uint64_t callbackElapsedUs,
    int32_t detail)
{
    const FailureInjection result = {
        true,
        kind,
        matchingOrdinal,
        MAX31865Status::Error(error, "Scripted failure", detail),
        effect,
        callbackElapsedUs};
    return result;
}

FailureInjection makeTimedSuccess(
    EventKind kind,
    uint32_t matchingOrdinal,
    uint64_t callbackElapsedUs)
{
    const FailureInjection result = {
        true,
        kind,
        matchingOrdinal,
        MAX31865Status::Ok(),
        FailureEffect::Applied,
        callbackElapsedUs};
    return result;
}

} // namespace max31865_test
