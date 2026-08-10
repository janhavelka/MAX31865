#include "MAX31865/MAX31865.h"
#include "support/ScriptedMax31865Transport.h"
#include "support/TestHarness.h"

#include <climits>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <type_traits>

using namespace max31865_test;

struct FirmwareRtdSample
{
    uint16_t rawCode;
    uint32_t sequence;
    uint32_t timestampUs;
    float resistanceOhms;
    float temperatureC;
    uint8_t channelId;
    uint8_t qualityFlags;
};

static_assert(
    std::is_standard_layout<FirmwareRtdSample>::value,
    "firmware DTO must be standard-layout");
static_assert(
    std::is_trivially_copyable<FirmwareRtdSample>::value,
    "firmware DTO must be trivially copyable");
static_assert(
    sizeof(FirmwareRtdSample) <= 32U,
    "firmware DTO must remain bounded");
static_assert(
    std::is_same<decltype(FirmwareRtdSample::rawCode), uint16_t>::value,
    "raw code type changed");

namespace {

static constexpr uint32_t FIRMWARE_OWNER_CALL_BUDGET_MS = 20U;
static constexpr uint32_t FIRMWARE_OWNER_NATIVE_TOLERANCE_MS = 50U;
static constexpr uint8_t FIRMWARE_SAMPLE_QUALITY_VALID = 1U << 0;

enum class FirmwareRtdEvent : uint8_t
{
    Published = 0,
    NoData,
    DeviceFault,
    BusLockTimeout,
    OperationTimeout,
    DegradedHealth,
    OfflineHealth,
    FaultState,
    DriverError,
    RequestExpired,
    RecoveryNotAuthorized,
    InstanceBusy
};

struct FirmwareRtdResult
{
    FirmwareRtdEvent event;
    int32_t detail;
};

struct FirmwareRequestDeadline
{
    uint32_t startedAtMs;
    uint32_t timeoutMs;
};

struct FirmwareSamplePublisher
{
    FirmwareRtdSample last{};
    uint32_t publishCount = 0U;

    void publish(const FirmwareRtdSample &sample)
    {
        last = sample;
        ++publishCount;
    }
};

enum class FirmwareSpiTopology : uint8_t
{
    DedicatedHost = 0,
    SharedHostApplicationArbiter
};

bool sharedHostMultipleContextsAreAdmissible(
    bool everyBusClientUsesTheSameArbiter)
{
    return everyBusClientUsesTheSameArbiter;
}

uint32_t elapsedMs(uint32_t startedAtMs, uint32_t nowMs)
{
    return nowMs - startedAtMs;
}

uint32_t remainingMs(
    const FirmwareRequestDeadline &deadline,
    uint32_t nowMs)
{
    const uint32_t elapsed = elapsedMs(deadline.startedAtMs, nowMs);
    return elapsed >= deadline.timeoutMs
        ? 0U
        : deadline.timeoutMs - elapsed;
}

bool expired(
    const FirmwareRequestDeadline &deadline,
    uint32_t nowMs)
{
    return elapsedMs(deadline.startedAtMs, nowMs) >= deadline.timeoutMs;
}

uint32_t clampTimeout(uint32_t requestedMs, uint32_t remaining)
{
    uint32_t clamped = requestedMs;
    if (clamped > static_cast<uint32_t>(INT32_MAX))
    {
        clamped = static_cast<uint32_t>(INT32_MAX);
    }
    return clamped > remaining ? remaining : clamped;
}

enum class FirmwareLockEvent : uint8_t
{
    InstanceLock = 0,
    BusLock,
    BusUnlock,
    InstanceUnlock,
    OtherPeripheralLock,
    OtherPeripheralUnlock
};

struct FirmwareLockState
{
    std::timed_mutex instanceMutex;
    std::timed_mutex busMutex;
    std::atomic<bool> instanceLocked{false};
    std::atomic<bool> missingInstanceLock{false};
    std::atomic<uint32_t> busLockAttemptCount{0U};
    std::atomic<uint32_t> lastBusTimeoutMs{0U};
    mutable std::mutex transcriptMutex;
    FirmwareLockEvent events[512]{};
    size_t eventCount = 0U;

    void record(FirmwareLockEvent event)
    {
        const std::lock_guard<std::mutex> guard(transcriptMutex);
        if (eventCount < (sizeof(events) / sizeof(events[0])))
        {
            events[eventCount] = event;
            ++eventCount;
        }
    }

    void clearEvents()
    {
        const std::lock_guard<std::mutex> guard(transcriptMutex);
        eventCount = 0U;
        missingInstanceLock.store(false);
        busLockAttemptCount.store(0U);
        lastBusTimeoutMs.store(0U);
    }
};

class FirmwareInstanceGuard final
{
public:
    explicit FirmwareInstanceGuard(FirmwareLockState &locks)
        : _locks(locks), _held(_locks.instanceMutex.try_lock())
    {
        if (_held)
        {
            _locks.instanceLocked.store(true);
            _locks.record(FirmwareLockEvent::InstanceLock);
        }
    }

    ~FirmwareInstanceGuard()
    {
        if (_held)
        {
            _locks.record(FirmwareLockEvent::InstanceUnlock);
            _locks.instanceLocked.store(false);
            _locks.instanceMutex.unlock();
        }
    }

    FirmwareInstanceGuard(const FirmwareInstanceGuard &) = delete;
    FirmwareInstanceGuard &operator=(const FirmwareInstanceGuard &) = delete;

    bool held() const { return _held; }

private:
    FirmwareLockState &_locks;
    bool _held;
};

class FirmwareTransportHarness final
{
public:
    FirmwareTransportHarness(
        ScriptedMax31865Transport &script,
        FirmwareLockState &locks,
        FirmwareSpiTopology topology)
        : _script(script),
          _locks(locks),
          _topology(topology),
          _inner(script.makeTransport(false, false, true))
    {
    }

    MAX31865Transport transport()
    {
        MAX31865Transport value = _inner;
        value.user = this;
        const bool shared =
            _topology == FirmwareSpiTopology::SharedHostApplicationArbiter;
        value.lockBus = shared ? lockBusThunk : nullptr;
        value.unlockBus = shared ? unlockBusThunk : nullptr;
        value.setChipSelect = setChipSelectThunk;
        value.transfer = transferThunk;
        value.readPin = readPinThunk;
        value.nowMs = nowMsThunk;
        value.nowUs = nowUsThunk;
        value.sleepMs = sleepMsThunk;
        value.delayUs = delayUsThunk;
        return value;
    }

    uint32_t nowMs() const { return _script.nowMs(); }

    void advanceTimeMs(uint32_t amount) { _script.advanceTimeMs(amount); }

    void setPostDriverCallDelayMs(uint32_t amount)
    {
        _postDriverCallDelayMs = amount;
    }

    void applyPostDriverCallDelay()
    {
        _script.advanceTimeMs(_postDriverCallDelayMs);
        _postDriverCallDelayMs = 0U;
    }

private:
    static FirmwareTransportHarness &self(void *user)
    {
        return *static_cast<FirmwareTransportHarness *>(user);
    }

    static MAX31865Status lockBusThunk(void *user, uint32_t timeoutMs)
    {
        FirmwareTransportHarness &harness = self(user);
        harness._locks.busLockAttemptCount.fetch_add(1U);
        harness._locks.lastBusTimeoutMs.store(timeoutMs);
        harness._locks.record(FirmwareLockEvent::BusLock);
        if (!harness._locks.instanceLocked.load())
        {
            harness._locks.missingInstanceLock.store(true);
        }
        const std::chrono::steady_clock::time_point started =
            std::chrono::steady_clock::now();
        if (!harness._locks.busMutex.try_lock_for(
                std::chrono::milliseconds(timeoutMs)))
        {
            const std::chrono::steady_clock::duration requested =
                std::chrono::milliseconds(timeoutMs);
            const std::chrono::steady_clock::duration waited =
                std::chrono::steady_clock::now() - started;
            if (waited < requested)
            {
                std::this_thread::sleep_for(requested - waited);
            }
            harness._script.advanceTimeMs(timeoutMs);
            return MAX31865Status::Error(
                MAX31865Error::BusLockTimeout,
                "application shared-bus arbiter timeout",
                81);
        }
        return MAX31865Status::Ok();
    }

    static void unlockBusThunk(void *user)
    {
        FirmwareTransportHarness &harness = self(user);
        harness._locks.record(FirmwareLockEvent::BusUnlock);
        harness._locks.busMutex.unlock();
    }

    static MAX31865Status setChipSelectThunk(void *user, bool asserted)
    {
        FirmwareTransportHarness &harness = self(user);
        return harness._inner.setChipSelect(
            harness._inner.user, asserted);
    }

    static MAX31865Status transferThunk(
        void *user,
        const uint8_t *tx,
        uint8_t *rx,
        size_t length,
        uint32_t timeoutMs)
    {
        FirmwareTransportHarness &harness = self(user);
        return harness._inner.transfer(
            harness._inner.user, tx, rx, length, timeoutMs);
    }

    static MAX31865Status readPinThunk(
        void *user,
        MAX31865Pin pin,
        bool *level)
    {
        FirmwareTransportHarness &harness = self(user);
        return harness._inner.readPin(harness._inner.user, pin, level);
    }

    static uint32_t nowMsThunk(void *user)
    {
        FirmwareTransportHarness &harness = self(user);
        return harness._inner.nowMs(harness._inner.user);
    }

    static uint32_t nowUsThunk(void *user)
    {
        FirmwareTransportHarness &harness = self(user);
        return harness._inner.nowUs(harness._inner.user);
    }

    static void sleepMsThunk(void *user, uint32_t amount)
    {
        FirmwareTransportHarness &harness = self(user);
        harness._inner.sleepMs(harness._inner.user, amount);
    }

    static void delayUsThunk(void *user, uint32_t amount)
    {
        FirmwareTransportHarness &harness = self(user);
        harness._inner.delayUs(harness._inner.user, amount);
    }

    ScriptedMax31865Transport &_script;
    FirmwareLockState &_locks;
    FirmwareSpiTopology _topology;
    MAX31865Transport _inner;
    uint32_t _postDriverCallDelayMs = 0U;
};

class FirmwareOtherPeripheralHold final
{
public:
    explicit FirmwareOtherPeripheralHold(FirmwareLockState &locks)
        : _locks(locks),
          _thread([this]() {
              _acquired.store(_locks.busMutex.try_lock());
              if (_acquired.load())
              {
                  _locks.record(FirmwareLockEvent::OtherPeripheralLock);
              }
              _started.store(true);
              while (_acquired.load() && !_release.load())
              {
                  std::this_thread::yield();
              }
              if (_acquired.load())
              {
                  _locks.record(FirmwareLockEvent::OtherPeripheralUnlock);
                  _locks.busMutex.unlock();
              }
          })
    {
    }

    ~FirmwareOtherPeripheralHold() { release(); }

    FirmwareOtherPeripheralHold(const FirmwareOtherPeripheralHold &) = delete;
    FirmwareOtherPeripheralHold &operator=(
        const FirmwareOtherPeripheralHold &) = delete;

    bool waitUntilStarted()
    {
        const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (!_started.load() &&
               std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }
        return _started.load();
    }

    bool acquired() const { return _acquired.load(); }

    void release()
    {
        _release.store(true);
        if (_thread.joinable())
        {
            _thread.join();
        }
    }

private:
    FirmwareLockState &_locks;
    std::atomic<bool> _started{false};
    std::atomic<bool> _acquired{false};
    std::atomic<bool> _release{false};
    std::thread _thread;
};

class FirmwareInstanceHold final
{
public:
    explicit FirmwareInstanceHold(FirmwareLockState &locks)
        : _locks(locks),
          _thread([this]() {
              FirmwareInstanceGuard guard(_locks);
              _held.store(guard.held());
              _started.store(true);
              while (guard.held() && !_release.load())
              {
                  std::this_thread::yield();
              }
          })
    {
    }

    ~FirmwareInstanceHold() { release(); }

    bool waitUntilStarted()
    {
        const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (!_started.load() &&
               std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }
        return _started.load();
    }

    bool held() const { return _held.load(); }

    void release()
    {
        _release.store(true);
        if (_thread.joinable())
        {
            _thread.join();
        }
    }

private:
    FirmwareLockState &_locks;
    std::atomic<bool> _started{false};
    std::atomic<bool> _held{false};
    std::atomic<bool> _release{false};
    std::thread _thread;
};

class FirmwareRtdOwner;

class FirmwareRtdInterface final
{
public:
    FirmwareRtdResult begin();
    FirmwareRtdResult startContinuous(
        uint32_t requestedTimeoutMs,
        const FirmwareRequestDeadline &outer);
    FirmwareRtdResult stop(
        uint32_t requestedTimeoutMs,
        const FirmwareRequestDeadline &outer);
    FirmwareRtdResult pollAndPublish(
        const FirmwareRequestDeadline &outer,
        FirmwareSamplePublisher &publisher);
    FirmwareRtdResult recoverIfAuthorized(
        bool applicationAuthorized,
        uint32_t requestedTimeoutMs,
        const FirmwareRequestDeadline &outer);
    FirmwareRtdEvent classifyHealth();

private:
    friend class FirmwareRtdOwner;
    explicit FirmwareRtdInterface(FirmwareRtdOwner &owner) : _owner(owner) {}
    FirmwareRtdOwner &_owner;
};

class FirmwareRtdOwner final
{
public:
    FirmwareRtdOwner(
        MAX31865 &device,
        FirmwareTransportHarness &transport,
        FirmwareLockState &locks)
        : _device(device),
          _transport(transport),
          _locks(locks),
          _interface(*this)
    {
    }

    FirmwareRtdInterface &applicationInterface() { return _interface; }

    FirmwareRtdResult begin()
    {
        FirmwareInstanceGuard guard(_locks);
        if (!guard.held())
        {
            return {FirmwareRtdEvent::InstanceBusy, 0};
        }
        MAX31865BeginConfig config = max31865DefaultBeginConfig();
        config.transport = _transport.transport();
        config.powerReadyDelayMs = 0U;
        config.defaultOperationTimeoutMs = FIRMWARE_OWNER_CALL_BUDGET_MS;
        config.offlineThreshold = 3U;
        return mapStatus(_device.begin(config));
    }

    FirmwareRtdResult startContinuous(
        uint32_t requestedTimeoutMs,
        const FirmwareRequestDeadline &outer)
    {
        return callWithDeadline(
            requestedTimeoutMs,
            outer,
            [this](uint32_t timeoutMs) {
                return _device.startContinuous(timeoutMs);
            });
    }

    FirmwareRtdResult stop(
        uint32_t requestedTimeoutMs,
        const FirmwareRequestDeadline &outer)
    {
        return callWithDeadline(
            requestedTimeoutMs,
            outer,
            [this](uint32_t timeoutMs) {
                return _device.stop(timeoutMs);
            });
    }

    FirmwareRtdResult pollAndPublish(
        const FirmwareRequestDeadline &outer,
        FirmwareSamplePublisher &publisher)
    {
        const uint32_t remaining = remainingMs(outer, _transport.nowMs());
        if (remaining < _device.defaultOperationTimeoutMs())
        {
            return {FirmwareRtdEvent::RequestExpired, 0};
        }
        FirmwareInstanceGuard guard(_locks);
        if (!guard.held())
        {
            return {FirmwareRtdEvent::InstanceBusy, 0};
        }
        MAX31865Sample sample{};
        const MAX31865Status status = _device.poll(sample);
        _transport.applyPostDriverCallDelay();
        if (expired(outer, _transport.nowMs()))
        {
            return {FirmwareRtdEvent::RequestExpired, 0};
        }
        if (!status.ok())
        {
            return mapStatus(status);
        }
        if ((sample.flags & MAX31865_SAMPLE_FLAG_DATA_VALID) == 0U)
        {
            return {FirmwareRtdEvent::DriverError, sample.flags};
        }
        const FirmwareRtdSample published = {
            sample.rawCode,
            sample.sampleCounter,
            sample.readTimestampUs,
            sample.resistanceOhms,
            sample.temperatureC,
            sample.channelId,
            FIRMWARE_SAMPLE_QUALITY_VALID};
        publisher.publish(published);
        return {FirmwareRtdEvent::Published, 0};
    }

    FirmwareRtdResult recoverIfAuthorized(
        bool applicationAuthorized,
        uint32_t requestedTimeoutMs,
        const FirmwareRequestDeadline &outer)
    {
        if (!applicationAuthorized)
        {
            return {FirmwareRtdEvent::RecoveryNotAuthorized, 0};
        }
        return callWithDeadline(
            requestedTimeoutMs,
            outer,
            [this](uint32_t timeoutMs) {
                return _device.recover(timeoutMs);
            });
    }

    FirmwareRtdEvent classifyHealth()
    {
        FirmwareInstanceGuard guard(_locks);
        if (!guard.held())
        {
            return FirmwareRtdEvent::InstanceBusy;
        }
        const MAX31865Health health = _device.health();
        if (health.state == MAX31865State::Fault)
        {
            return FirmwareRtdEvent::FaultState;
        }
        if (health.driverState == MAX31865DriverState::OFFLINE)
        {
            return FirmwareRtdEvent::OfflineHealth;
        }
        if (health.driverState == MAX31865DriverState::DEGRADED)
        {
            return FirmwareRtdEvent::DegradedHealth;
        }
        return FirmwareRtdEvent::Published;
    }

private:
    template <typename Callable>
    FirmwareRtdResult callWithDeadline(
        uint32_t requestedTimeoutMs,
        const FirmwareRequestDeadline &outer,
        Callable callable)
    {
        const uint32_t remaining = remainingMs(outer, _transport.nowMs());
        const uint32_t timeout = clampTimeout(requestedTimeoutMs, remaining);
        if (timeout == 0U)
        {
            return {FirmwareRtdEvent::RequestExpired, 0};
        }
        FirmwareInstanceGuard guard(_locks);
        if (!guard.held())
        {
            return {FirmwareRtdEvent::InstanceBusy, 0};
        }
        const MAX31865Status status = callable(timeout);
        _transport.applyPostDriverCallDelay();
        if (expired(outer, _transport.nowMs()))
        {
            return {FirmwareRtdEvent::RequestExpired, status.detail};
        }
        return mapStatus(status);
    }

    FirmwareRtdResult mapStatus(const MAX31865Status &status) const
    {
        if (_device.state() == MAX31865State::Fault)
        {
            return {FirmwareRtdEvent::FaultState, status.detail};
        }
        switch (status.code)
        {
            case MAX31865Error::Ok:
                return {FirmwareRtdEvent::Published, status.detail};
            case MAX31865Error::NoData:
                return {FirmwareRtdEvent::NoData, status.detail};
            case MAX31865Error::DeviceFault:
                return {FirmwareRtdEvent::DeviceFault, status.detail};
            case MAX31865Error::BusLockTimeout:
                return {FirmwareRtdEvent::BusLockTimeout, status.detail};
            case MAX31865Error::OperationTimeout:
                return {FirmwareRtdEvent::OperationTimeout, status.detail};
            default:
                return {FirmwareRtdEvent::DriverError, status.detail};
        }
    }

    MAX31865 &_device;
    FirmwareTransportHarness &_transport;
    FirmwareLockState &_locks;
    FirmwareRtdInterface _interface;
};

FirmwareRtdResult FirmwareRtdInterface::begin()
{
    return _owner.begin();
}

FirmwareRtdResult FirmwareRtdInterface::startContinuous(
    uint32_t requestedTimeoutMs,
    const FirmwareRequestDeadline &outer)
{
    return _owner.startContinuous(requestedTimeoutMs, outer);
}

FirmwareRtdResult FirmwareRtdInterface::stop(
    uint32_t requestedTimeoutMs,
    const FirmwareRequestDeadline &outer)
{
    return _owner.stop(requestedTimeoutMs, outer);
}

FirmwareRtdResult FirmwareRtdInterface::pollAndPublish(
    const FirmwareRequestDeadline &outer,
    FirmwareSamplePublisher &publisher)
{
    return _owner.pollAndPublish(outer, publisher);
}

FirmwareRtdResult FirmwareRtdInterface::recoverIfAuthorized(
    bool applicationAuthorized,
    uint32_t requestedTimeoutMs,
    const FirmwareRequestDeadline &outer)
{
    return _owner.recoverIfAuthorized(
        applicationAuthorized, requestedTimeoutMs, outer);
}

FirmwareRtdEvent FirmwareRtdInterface::classifyHealth()
{
    return _owner.classifyHealth();
}

FirmwareRequestDeadline deadlineFromNow(
    const FirmwareTransportHarness &transport,
    uint32_t timeoutMs)
{
    return FirmwareRequestDeadline{transport.nowMs(), timeoutMs};
}

bool exactDirectOwnerCallOrder(const FirmwareLockState &locks)
{
    return locks.eventCount == 2U &&
           locks.events[0] == FirmwareLockEvent::InstanceLock &&
           locks.events[1] == FirmwareLockEvent::InstanceUnlock &&
           locks.busLockAttemptCount.load() == 0U;
}

bool exactSharedArbiterCallOrder(const FirmwareLockState &locks)
{
    if (locks.eventCount < 4U ||
        locks.events[0] != FirmwareLockEvent::InstanceLock ||
        locks.events[locks.eventCount - 1U] !=
            FirmwareLockEvent::InstanceUnlock)
    {
        return false;
    }
    size_t index = 1U;
    while (index + 1U < locks.eventCount)
    {
        if (locks.events[index] != FirmwareLockEvent::BusLock ||
            locks.events[index + 1U] != FirmwareLockEvent::BusUnlock)
        {
            return false;
        }
        index += 2U;
    }
    return index == locks.eventCount - 1U &&
           !locks.missingInstanceLock.load();
}

bool dtoDedicatedAndPublicationBoundary()
{
    CHECK(!sharedHostMultipleContextsAreAdmissible(false));
    CHECK(sharedHostMultipleContextsAreAdmissible(true));
    ScriptedMax31865Transport script;
    FirmwareLockState locks;
    FirmwareTransportHarness transport(
        script, locks, FirmwareSpiTopology::DedicatedHost);
    MAX31865 device;
    FirmwareRtdOwner owner(device, transport, locks);
    FirmwareRtdInterface &application = owner.applicationInterface();

    CHECK_EQ(FirmwareRtdEvent::Published, application.begin().event);
    locks.clearEvents();
    CHECK_EQ(
        FirmwareRtdEvent::Published,
        application
            .startContinuous(20U, deadlineFromNow(transport, 100U))
            .event);
    CHECK(exactDirectOwnerCallOrder(locks));
    CHECK(script.device().enqueueSample(8192U));
    FirmwareSamplePublisher publisher;
    locks.clearEvents();
    CHECK_EQ(
        FirmwareRtdEvent::NoData,
        application
            .pollAndPublish(
                deadlineFromNow(transport, 100U), publisher)
            .event);
    CHECK_EQ(0U, publisher.publishCount);
    CHECK(exactDirectOwnerCallOrder(locks));

    transport.advanceTimeMs(60U);
    locks.clearEvents();
    CHECK_EQ(
        FirmwareRtdEvent::Published,
        application
            .pollAndPublish(
                deadlineFromNow(transport, 100U), publisher)
            .event);
    CHECK_EQ(1U, publisher.publishCount);
    CHECK_EQ(8192U, publisher.last.rawCode);
    CHECK_EQ(FIRMWARE_SAMPLE_QUALITY_VALID, publisher.last.qualityFlags);
    CHECK_NEAR(100.0F, publisher.last.resistanceOhms, 0.001F);
    CHECK(exactDirectOwnerCallOrder(locks));
    locks.clearEvents();
    CHECK_EQ(
        FirmwareRtdEvent::Published,
        application
            .stop(20U, deadlineFromNow(transport, 100U))
            .event);
    CHECK(exactDirectOwnerCallOrder(locks));

    CHECK_EQ(
        FirmwareRtdEvent::Published,
        application
            .startContinuous(20U, deadlineFromNow(transport, 100U))
            .event);
    CHECK(script.device().enqueueSample(
        8200U, max31865_cmd::FAULT_HIGH_THRESHOLD));
    transport.advanceTimeMs(60U);
    CHECK_EQ(
        FirmwareRtdEvent::DeviceFault,
        application
            .pollAndPublish(
                deadlineFromNow(transport, 100U), publisher)
            .event);
    CHECK_EQ(1U, publisher.publishCount);
    CHECK_EQ(
        FirmwareRtdEvent::Published,
        application
            .stop(20U, deadlineFromNow(transport, 100U))
            .event);
    return true;
}

bool sharedArbiterDeadlineAndHealthBoundary()
{
    ScriptedMax31865Transport script;
    FirmwareLockState locks;
    FirmwareTransportHarness transport(
        script,
        locks,
        FirmwareSpiTopology::SharedHostApplicationArbiter);
    MAX31865 device;
    FirmwareRtdOwner owner(device, transport, locks);
    FirmwareRtdInterface &application = owner.applicationInterface();
    CHECK_EQ(FirmwareRtdEvent::Published, application.begin().event);
    CHECK(exactSharedArbiterCallOrder(locks));

    FirmwareOtherPeripheralHold other(locks);
    CHECK(other.waitUntilStarted());
    CHECK(other.acquired());
    locks.clearEvents();
    const std::chrono::steady_clock::time_point started =
        std::chrono::steady_clock::now();
    const FirmwareRtdResult first = application.startContinuous(
        FIRMWARE_OWNER_CALL_BUDGET_MS,
        deadlineFromNow(transport, 100U));
    const auto nativeElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count();
    CHECK_EQ(FirmwareRtdEvent::OperationTimeout, first.event);
    CHECK(nativeElapsed >= FIRMWARE_OWNER_CALL_BUDGET_MS);
    CHECK(nativeElapsed <=
          FIRMWARE_OWNER_CALL_BUDGET_MS +
              FIRMWARE_OWNER_NATIVE_TOLERANCE_MS);
    CHECK_EQ(1U, locks.busLockAttemptCount.load());
    CHECK_EQ(
        FIRMWARE_OWNER_CALL_BUDGET_MS,
        locks.lastBusTimeoutMs.load());
    CHECK_EQ(1U, device.health().busLockTimeoutCount);
    CHECK_EQ(1U, device.health().operationTimeoutCount);
    CHECK_EQ(
        FirmwareRtdEvent::DegradedHealth,
        application.classifyHealth());

    for (uint32_t attempt = 1U; attempt < 3U; ++attempt)
    {
        CHECK_EQ(
            FirmwareRtdEvent::OperationTimeout,
            application
                .startContinuous(
                    FIRMWARE_OWNER_CALL_BUDGET_MS,
                    deadlineFromNow(transport, 100U))
                .event);
    }
    CHECK_EQ(3U, device.health().busLockTimeoutCount);
    CHECK_EQ(3U, device.health().operationTimeoutCount);
    CHECK_EQ(
        FirmwareRtdEvent::OfflineHealth,
        application.classifyHealth());
    other.release();
    return true;
}

bool outerDeadlineAndPublicationSuppressionBoundary()
{
    ScriptedMax31865Transport script;
    FirmwareLockState locks;
    FirmwareTransportHarness transport(
        script,
        locks,
        FirmwareSpiTopology::SharedHostApplicationArbiter);
    MAX31865 device;
    FirmwareRtdOwner owner(device, transport, locks);
    FirmwareRtdInterface &application = owner.applicationInterface();
    CHECK_EQ(FirmwareRtdEvent::Published, application.begin().event);

    locks.clearEvents();
    const FirmwareRequestDeadline alreadyExpired = {
        transport.nowMs() - 5U, 5U};
    CHECK_EQ(
        FirmwareRtdEvent::RequestExpired,
        application.startContinuous(100U, alreadyExpired).event);
    CHECK_EQ(0U, locks.busLockAttemptCount.load());

    FirmwareOtherPeripheralHold other(locks);
    CHECK(other.waitUntilStarted());
    CHECK(other.acquired());
    locks.clearEvents();
    const uint32_t clampStarted = transport.nowMs();
    CHECK_EQ(
        FirmwareRtdEvent::RequestExpired,
        application
            .startContinuous(100U, {clampStarted, 7U})
            .event);
    CHECK_EQ(7U, locks.lastBusTimeoutMs.load());
    other.release();

    CHECK_EQ(
        FirmwareRtdEvent::Published,
        application
            .startContinuous(20U, deadlineFromNow(transport, 100U))
            .event);
    CHECK(script.device().enqueueSample(9000U));
    transport.advanceTimeMs(60U);
    FirmwareSamplePublisher publisher;
    const uint32_t publishStarted = transport.nowMs();
    transport.setPostDriverCallDelayMs(25U);
    CHECK_EQ(
        FirmwareRtdEvent::RequestExpired,
        application
            .pollAndPublish({publishStarted, 25U}, publisher)
            .event);
    CHECK_EQ(0U, publisher.publishCount);

    locks.clearEvents();
    CHECK_EQ(
        FirmwareRtdEvent::RequestExpired,
        application
            .pollAndPublish(
                deadlineFromNow(transport, 19U), publisher)
            .event);
    CHECK_EQ(0U, locks.busLockAttemptCount.load());
    CHECK_EQ(
        1U,
        remainingMs({0xFFFFFFFCU, 8U}, 3U));
    CHECK(expired({0xFFFFFFFCU, 8U}, 4U));
    CHECK_EQ(
        static_cast<uint32_t>(INT32_MAX),
        clampTimeout(UINT32_MAX, UINT32_MAX));
    return true;
}

bool instanceFaultAndExplicitRecoveryBoundary()
{
    ScriptedMax31865Transport script;
    FirmwareLockState locks;
    FirmwareTransportHarness transport(
        script,
        locks,
        FirmwareSpiTopology::SharedHostApplicationArbiter);
    MAX31865 device;
    FirmwareRtdOwner owner(device, transport, locks);
    FirmwareRtdInterface &application = owner.applicationInterface();
    CHECK_EQ(FirmwareRtdEvent::Published, application.begin().event);

    FirmwareInstanceHold instanceHold(locks);
    CHECK(instanceHold.waitUntilStarted());
    CHECK(instanceHold.held());
    locks.clearEvents();
    CHECK_EQ(
        FirmwareRtdEvent::InstanceBusy,
        application
            .startContinuous(20U, deadlineFromNow(transport, 100U))
            .event);
    CHECK_EQ(0U, locks.busLockAttemptCount.load());
    CHECK_EQ(
        FirmwareRtdEvent::InstanceBusy,
        application.classifyHealth());
    instanceHold.release();

    CHECK_EQ(
        FirmwareRtdEvent::Published,
        application
            .startContinuous(20U, deadlineFromNow(transport, 100U))
            .event);
    CHECK(script.device().enqueueSample(10000U));
    transport.advanceTimeMs(60U);
    script.resetLog();
    CHECK(script.addFailure(makeFailure(
        EventKind::ChipSelectDeassert,
        1U,
        MAX31865Error::ChipSelectFailed,
        FailureEffect::Applied)));
    FirmwareSamplePublisher publisher;
    CHECK_EQ(
        FirmwareRtdEvent::FaultState,
        application
            .pollAndPublish(
                deadlineFromNow(transport, 100U), publisher)
            .event);
    CHECK_EQ(0U, publisher.publishCount);
    CHECK_EQ(FirmwareRtdEvent::FaultState, application.classifyHealth());

    locks.clearEvents();
    CHECK_EQ(
        FirmwareRtdEvent::RecoveryNotAuthorized,
        application
            .recoverIfAuthorized(
                false,
                500U,
                deadlineFromNow(transport, 500U))
            .event);
    CHECK_EQ(0U, locks.busLockAttemptCount.load());
    CHECK_EQ(
        FirmwareRtdEvent::Published,
        application
            .recoverIfAuthorized(
                true,
                500U,
                deadlineFromNow(transport, 500U))
            .event);
    CHECK_EQ(MAX31865State::Ready, device.state());
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"external DTO, dedicated owner, and publication boundary",
         dtoDedicatedAndPublicationBoundary},
        {"shared arbiter deadline and health boundary",
         sharedArbiterDeadlineAndHealthBoundary},
        {"outer deadline and publication suppression boundary",
         outerDeadlineAndPublicationSuppressionBoundary},
        {"instance serialization, fault, and explicit recovery boundary",
         instanceFaultAndExplicitRecoveryBoundary}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
