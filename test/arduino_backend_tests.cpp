#include "MAX31865/ArduinoBackend.h"
#include "MAX31865/CommandTable.h"
#include "MAX31865/MAX31865.h"
#include "support/Max31865DeviceModel.h"
#include "support/TestHarness.h"

#include <stddef.h>
#include <stdint.h>

#include <cstring>
#include <type_traits>
#include <vector>

using namespace max31865_test;

namespace {

enum class EventKind : uint8_t
{
    PinMode,
    DigitalWrite,
    DigitalRead,
    BeginTransaction,
    EndTransaction,
    Transfer,
    DelayMs,
    DelayUs,
    ArbiterLock,
    ArbiterUnlock
};

struct Event
{
    EventKind kind;
    int32_t first;
    uint32_t second;
    const void *object;
};

struct BackendSnapshot
{
    SPIClass *spi;
    uint32_t settingsClockHz;
    uint8_t settingsBitOrder;
    uint8_t settingsMode;
    uint32_t spiClockHz;
    MAX31865Pins pins;
    void *arbiterUser;
    MAX31865Status (*arbiterLock)(void *, uint32_t);
    void (*arbiterUnlock)(void *);
    bool pinsConfigured;
};

std::vector<Event> gEvents;
uint32_t gTimeMs = 0U;
uint32_t gTimeUs = 0U;
bool gDrdyLevel = true;
MAX31865Status gArbiterStatus = MAX31865Status::Ok();
Max31865DeviceModel gDevice;
bool gUseDeviceModel = false;
bool gModelCallbackFailed = false;

MAX31865Pins validPins()
{
    return MAX31865Pins{1, 8, 2, 3, 4};
}

MAX31865Pins replacementPins()
{
    return MAX31865Pins{9, 10, 11, 12, MAX31865_PIN_UNUSED};
}

void resetFixture()
{
    gEvents.clear();
    gTimeMs = 0U;
    gTimeUs = 0U;
    gDrdyLevel = true;
    gArbiterStatus = MAX31865Status::Ok();
    gDevice.powerOnReset();
    gUseDeviceModel = false;
    gModelCallbackFailed = false;
}

BackendSnapshot snapshot(const MAX31865ArduinoBackend &backend)
{
    return BackendSnapshot{
        backend.spi,
        backend.settings.clockHz,
        backend.settings.bitOrder,
        backend.settings.dataMode,
        backend.spiClockHz,
        backend.pins,
        backend.arbiter.user,
        backend.arbiter.lock,
        backend.arbiter.unlock,
        backend.pinsConfigured};
}

bool samePins(const MAX31865Pins &left, const MAX31865Pins &right)
{
    return left.sck == right.sck && left.miso == right.miso &&
           left.mosi == right.mosi &&
           left.chipSelect == right.chipSelect &&
           left.dataReady == right.dataReady;
}

bool sameSnapshot(
    const BackendSnapshot &left,
    const BackendSnapshot &right)
{
    return left.spi == right.spi &&
           left.settingsClockHz == right.settingsClockHz &&
           left.settingsBitOrder == right.settingsBitOrder &&
           left.settingsMode == right.settingsMode &&
           left.spiClockHz == right.spiClockHz &&
           samePins(left.pins, right.pins) &&
           left.arbiterUser == right.arbiterUser &&
           left.arbiterLock == right.arbiterLock &&
           left.arbiterUnlock == right.arbiterUnlock &&
           left.pinsConfigured == right.pinsConfigured;
}

MAX31865Status arbiterLock(void *user, uint32_t timeoutMs)
{
    gEvents.push_back(
        {EventKind::ArbiterLock, 0, timeoutMs, user});
    return gArbiterStatus;
}

void arbiterUnlock(void *user)
{
    gEvents.push_back({EventKind::ArbiterUnlock, 0, 0U, user});
}

bool invalidBindPreservesState(
    MAX31865ArduinoBackend &backend,
    SPIClass &spi,
    uint32_t clockHz,
    const MAX31865Pins &pins,
    const MAX31865ArduinoBusArbiter *arbiter = nullptr)
{
    const BackendSnapshot before = snapshot(backend);
    gEvents.clear();
    const MAX31865Status status = max31865ArduinoBackendBind(
        backend,
        spi,
        SPISettings(clockHz == 0U ? 1000000U : clockHz,
                    MSBFIRST,
                    SPI_MODE1),
        clockHz,
        pins,
        arbiter);
    return status.code == MAX31865Error::InvalidArgument &&
           gEvents.empty() && sameSnapshot(before, snapshot(backend));
}

bool constructorAndBindingAreTransactional()
{
    static_assert(
        !std::is_copy_constructible<MAX31865ArduinoBackend>::value,
        "backend must not copy");
    static_assert(
        !std::is_copy_assignable<MAX31865ArduinoBackend>::value,
        "backend must not copy-assign");
    static_assert(
        !std::is_move_constructible<MAX31865ArduinoBackend>::value,
        "backend must not move");
    static_assert(
        !std::is_move_assignable<MAX31865ArduinoBackend>::value,
        "backend must not move-assign");

    resetFixture();
    SPIClass spi;
    SPIClass replacementSpi;
    MAX31865ArduinoBackend backend;
    CHECK_EQ(nullptr, backend.spi);
    CHECK_EQ(0U, backend.spiClockHz);
    CHECK(samePins(backend.pins, MAX31865Pins{}));
    CHECK_EQ(nullptr, backend.arbiter.user);
    CHECK_EQ(nullptr, backend.arbiter.lock);
    CHECK_EQ(nullptr, backend.arbiter.unlock);
    CHECK(!backend.pinsConfigured);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        max31865ArduinoConfigureControlPins(backend).code);
    CHECK(gEvents.empty());

    int arbiterContext = 42;
    MAX31865ArduinoBusArbiter arbiter{
        &arbiterContext, arbiterLock, arbiterUnlock};
    const MAX31865Pins initialPins = validPins();
    CHECK(max31865ArduinoBackendBind(
              backend,
              spi,
              SPISettings(4000000U, MSBFIRST, SPI_MODE3),
              4000000U,
              initialPins,
              &arbiter)
              .ok());
    CHECK(gEvents.empty());
    CHECK_EQ(&spi, backend.spi);
    CHECK_EQ(4000000U, backend.spiClockHz);
    CHECK_EQ(4000000U, backend.settings.clockHz);
    CHECK_EQ(SPI_MODE3, backend.settings.dataMode);
    CHECK(samePins(initialPins, backend.pins));
    CHECK_EQ(&arbiterContext, backend.arbiter.user);
    CHECK_EQ(arbiterLock, backend.arbiter.lock);
    CHECK_EQ(arbiterUnlock, backend.arbiter.unlock);
    CHECK(!backend.pinsConfigured);

    CHECK(invalidBindPreservesState(
        backend, replacementSpi, 0U, replacementPins()));
    CHECK(invalidBindPreservesState(
        backend,
        replacementSpi,
        max31865_cmd::SPI_MAX_HZ + 1U,
        replacementPins()));

    using PinMember = int32_t MAX31865Pins::*;
    const PinMember required[] = {
        &MAX31865Pins::sck,
        &MAX31865Pins::miso,
        &MAX31865Pins::mosi,
        &MAX31865Pins::chipSelect};
    for (const PinMember member : required)
    {
        MAX31865Pins candidate = validPins();
        candidate.*member = MAX31865_PIN_UNUSED;
        CHECK(invalidBindPreservesState(
            backend, replacementSpi, 1000000U, candidate));
        candidate = validPins();
        candidate.*member = -2;
        CHECK(invalidBindPreservesState(
            backend, replacementSpi, 1000000U, candidate));
    }

    MAX31865Pins candidate = validPins();
    candidate.dataReady = -2;
    CHECK(invalidBindPreservesState(
        backend, replacementSpi, 1000000U, candidate));
    candidate = validPins();
    candidate.dataReady = candidate.chipSelect;
    CHECK(invalidBindPreservesState(
        backend, replacementSpi, 1000000U, candidate));
    candidate = validPins();
    candidate.sck = GPIO_PIN_COUNT;
    CHECK(invalidBindPreservesState(
        backend, replacementSpi, 1000000U, candidate));
    candidate = validPins();
    candidate.miso = 22;
    CHECK(invalidBindPreservesState(
        backend, replacementSpi, 1000000U, candidate));

#if defined(CONFIG_IDF_TARGET_ESP32S2)
    candidate = validPins();
    candidate.sck = 46;
    CHECK(invalidBindPreservesState(
        backend, replacementSpi, 1000000U, candidate));
    candidate = validPins();
    candidate.miso = 46;
    CHECK(max31865ArduinoBackendBind(
              backend,
              replacementSpi,
              SPISettings(),
              1000000U,
              candidate)
              .ok());
    CHECK(gEvents.empty());
#else
    candidate = validPins();
    candidate.sck = 48;
    CHECK(max31865ArduinoBackendBind(
              backend,
              replacementSpi,
              SPISettings(),
              1000000U,
              candidate)
              .ok());
    CHECK(gEvents.empty());
#endif

    const MAX31865ArduinoBusArbiter incomplete[] = {
        {nullptr, arbiterLock, nullptr},
        {nullptr, nullptr, arbiterUnlock},
        {nullptr, nullptr, nullptr}};
    for (const MAX31865ArduinoBusArbiter &entry : incomplete)
    {
        CHECK(invalidBindPreservesState(
            backend,
            replacementSpi,
            1000000U,
            validPins(),
            &entry));
    }
    return true;
}

bool configurationIsExplicitAndGlitchSafe()
{
    resetFixture();
    SPIClass spi;
    MAX31865ArduinoBackend backend;
    CHECK(max31865ArduinoBackendBind(
              backend,
              spi,
              SPISettings(5000000U, MSBFIRST, SPI_MODE1),
              5000000U,
              validPins())
              .ok());
    MAX31865Transport transport = max31865ArduinoTransport(backend);
    uint8_t tx[2] = {0x00U, 0xA5U};
    uint8_t rx[2] = {0x11U, 0x22U};
    bool level = false;
    CHECK_EQ(
        MAX31865Error::InvalidState,
        transport.lockBus(transport.user, 7U).code);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        transport.setChipSelect(transport.user, true).code);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        transport.transfer(transport.user, tx, rx, 2U, 7U).code);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        transport.readPin(transport.user, MAX31865Pin::DRDY, &level).code);
    transport.unlockBus(transport.user);
    CHECK(gEvents.empty());
    CHECK_EQ(0x11U, rx[0]);
    CHECK_EQ(0x22U, rx[1]);
    CHECK(!level);

    CHECK(max31865ArduinoConfigureControlPins(backend).ok());
    CHECK(backend.pinsConfigured);
    CHECK_EQ(3U, gEvents.size());
    CHECK_EQ(EventKind::DigitalWrite, gEvents[0].kind);
    CHECK_EQ(validPins().chipSelect, gEvents[0].first);
    CHECK_EQ(static_cast<uint32_t>(HIGH), gEvents[0].second);
    CHECK_EQ(EventKind::PinMode, gEvents[1].kind);
    CHECK_EQ(validPins().chipSelect, gEvents[1].first);
    CHECK_EQ(static_cast<uint32_t>(OUTPUT), gEvents[1].second);
    CHECK_EQ(EventKind::PinMode, gEvents[2].kind);
    CHECK_EQ(validPins().dataReady, gEvents[2].first);
    CHECK_EQ(static_cast<uint32_t>(INPUT), gEvents[2].second);
    for (const Event &event : gEvents)
    {
        CHECK(event.first != validPins().sck);
        CHECK(event.first != validPins().miso);
        CHECK(event.first != validPins().mosi);
    }

    resetFixture();
    MAX31865ArduinoBackend noDrdy;
    const MAX31865Pins pins = replacementPins();
    CHECK(max31865ArduinoBackendBind(
              noDrdy, spi, SPISettings(), 1000000U, pins)
              .ok());
    CHECK(max31865ArduinoConfigureControlPins(noDrdy).ok());
    CHECK_EQ(2U, gEvents.size());
    for (const Event &event : gEvents)
    {
        CHECK(event.first != MAX31865_PIN_UNUSED);
    }
    const MAX31865Transport noDrdyTransport =
        max31865ArduinoTransport(noDrdy);
    CHECK(!noDrdyTransport.capabilities.hasDrdy);
    CHECK_EQ(
        MAX31865Error::GpioFailed,
        noDrdyTransport
            .readPin(noDrdyTransport.user, MAX31865Pin::DRDY, &level)
            .code);
    return true;
}

bool transportPublicationAndCallbacksAreExact()
{
    resetFixture();
    SPIClass spi;
    MAX31865ArduinoBackend backend;
    CHECK(max31865ArduinoBackendBind(
              backend,
              spi,
              SPISettings(4000000U, MSBFIRST, SPI_MODE3),
              4000000U,
              validPins())
              .ok());
    CHECK(max31865ArduinoConfigureControlPins(backend).ok());
    gEvents.clear();
    const MAX31865Transport transport = max31865ArduinoTransport(backend);
    CHECK_EQ(&backend, transport.user);
    CHECK(transport.capabilities.hasDrdy);
    CHECK(transport.lockBus != nullptr);
    CHECK(transport.unlockBus != nullptr);
    CHECK(transport.setChipSelect != nullptr);
    CHECK(transport.transfer != nullptr);
    CHECK(transport.readPin != nullptr);
    CHECK(transport.nowMs != nullptr);
    CHECK(transport.nowUs != nullptr);
    CHECK(transport.sleepMs != nullptr);
    CHECK(transport.delayUs != nullptr);

    uint8_t tx[3] = {0x80U, 0x12U, 0x34U};
    uint8_t rx[3] = {0U, 0U, 0U};
    CHECK(transport.lockBus(transport.user, 23U).ok());
    CHECK(transport.setChipSelect(transport.user, true).ok());
    CHECK(transport.transfer(transport.user, tx, rx, 3U, 19U).ok());
    CHECK(transport.setChipSelect(transport.user, false).ok());
    transport.unlockBus(transport.user);
    const EventKind expected[] = {
        EventKind::BeginTransaction,
        EventKind::DigitalWrite,
        EventKind::Transfer,
        EventKind::DigitalWrite,
        EventKind::EndTransaction};
    CHECK_EQ(sizeof(expected) / sizeof(expected[0]), gEvents.size());
    for (size_t index = 0U; index < gEvents.size(); ++index)
    {
        CHECK_EQ(expected[index], gEvents[index].kind);
    }
    CHECK_EQ(&spi, gEvents[0].object);
    CHECK_EQ(4000000, gEvents[0].first);
    CHECK_EQ(static_cast<uint32_t>(SPI_MODE3), gEvents[0].second);
    CHECK_EQ(static_cast<uint32_t>(LOW), gEvents[1].second);
    CHECK_EQ(3U, gEvents[2].second);
    CHECK_EQ(static_cast<uint32_t>(HIGH), gEvents[3].second);
    CHECK_EQ(static_cast<uint8_t>(tx[0] ^ 0xA5U), rx[0]);
    CHECK_EQ(static_cast<uint8_t>(tx[1] ^ 0xA5U), rx[1]);
    CHECK_EQ(static_cast<uint8_t>(tx[2] ^ 0xA5U), rx[2]);

    gEvents.clear();
    const uint8_t preserved[3] = {rx[0], rx[1], rx[2]};
    CHECK_EQ(
        MAX31865Error::SpiTransferFailed,
        transport.transfer(transport.user, nullptr, rx, 3U, 1U).code);
    CHECK_EQ(
        MAX31865Error::SpiTransferFailed,
        transport.transfer(transport.user, tx, nullptr, 3U, 1U).code);
    CHECK_EQ(
        MAX31865Error::SpiTransferFailed,
        transport.transfer(transport.user, tx, rx, 0U, 1U).code);
    CHECK_EQ(
        MAX31865Error::SpiTransferFailed,
        transport
            .transfer(
                transport.user,
                tx,
                rx,
                max31865_cmd::MAX_FRAME_BYTES + 1U,
                1U)
            .code);
    CHECK(gEvents.empty());
    CHECK(std::memcmp(rx, preserved, sizeof(preserved)) == 0);

    gDrdyLevel = false;
    bool level = true;
    CHECK(transport.readPin(transport.user, MAX31865Pin::DRDY, &level).ok());
    CHECK(!level);
    CHECK_EQ(EventKind::DigitalRead, gEvents.back().kind);
    const size_t eventCount = gEvents.size();
    level = true;
    CHECK_EQ(
        MAX31865Error::GpioFailed,
        transport.readPin(transport.user, MAX31865Pin::DRDY, nullptr).code);
    CHECK_EQ(eventCount, gEvents.size());
    CHECK_EQ(
        MAX31865Error::GpioFailed,
        transport
            .readPin(
                transport.user,
                static_cast<MAX31865Pin>(99U),
                &level)
            .code);
    CHECK(level);
    CHECK_EQ(eventCount, gEvents.size());

    gTimeMs = 17U;
    gTimeUs = 17000U;
    CHECK_EQ(17U, transport.nowMs(transport.user));
    CHECK_EQ(17000U, transport.nowUs(transport.user));
    transport.sleepMs(transport.user, 3U);
    transport.delayUs(transport.user, 250U);
    CHECK_EQ(20U, transport.nowMs(transport.user));
    CHECK_EQ(20250U, transport.nowUs(transport.user));
    CHECK_EQ(EventKind::DelayMs, gEvents[gEvents.size() - 2U].kind);
    CHECK_EQ(EventKind::DelayUs, gEvents.back().kind);
    return true;
}

bool arbiterOrderingAndStatusMappingAreBounded()
{
    resetFixture();
    SPIClass spi;
    MAX31865ArduinoBackend backend;
    int arbiterContext = 7;
    MAX31865ArduinoBusArbiter arbiter{
        &arbiterContext, arbiterLock, arbiterUnlock};
    CHECK(max31865ArduinoBackendBind(
              backend,
              spi,
              SPISettings(2000000U, MSBFIRST, SPI_MODE1),
              2000000U,
              validPins(),
              &arbiter)
              .ok());
    arbiter.user = nullptr;
    arbiter.lock = nullptr;
    arbiter.unlock = nullptr;
    CHECK_EQ(&arbiterContext, backend.arbiter.user);
    CHECK_EQ(arbiterLock, backend.arbiter.lock);
    CHECK_EQ(arbiterUnlock, backend.arbiter.unlock);
    CHECK(max31865ArduinoConfigureControlPins(backend).ok());
    const MAX31865Transport transport = max31865ArduinoTransport(backend);

    gEvents.clear();
    gArbiterStatus = MAX31865Status::Error(
        MAX31865Error::BusLockTimeout, "owner timeout", 91);
    MAX31865Status status = transport.lockBus(transport.user, 17U);
    CHECK_EQ(MAX31865Error::BusLockTimeout, status.code);
    CHECK_EQ(91, status.detail);
    CHECK_EQ(1U, gEvents.size());
    CHECK_EQ(EventKind::ArbiterLock, gEvents[0].kind);
    CHECK_EQ(17U, gEvents[0].second);
    CHECK_EQ(&arbiterContext, gEvents[0].object);

    gEvents.clear();
    gArbiterStatus = MAX31865Status::Error(
        MAX31865Error::BusLockFailed, "owner failed", 92);
    status = transport.lockBus(transport.user, 11U);
    CHECK_EQ(MAX31865Error::BusLockFailed, status.code);
    CHECK_CSTR_EQ("owner failed", status.msg);
    CHECK_EQ(92, status.detail);
    CHECK_EQ(1U, gEvents.size());

    gEvents.clear();
    gArbiterStatus = MAX31865Status::Error(
        MAX31865Error::InvalidState, "native invalid", 93);
    status = transport.lockBus(transport.user, 9U);
    CHECK_EQ(MAX31865Error::BusLockFailed, status.code);
    CHECK_CSTR_EQ("Arduino bus arbiter returned invalid status", status.msg);
    CHECK_EQ(static_cast<int32_t>(MAX31865Error::InvalidState), status.detail);
    CHECK_EQ(1U, gEvents.size());

    gEvents.clear();
    gArbiterStatus = MAX31865Status::Ok();
    CHECK(transport.lockBus(transport.user, 5U).ok());
    transport.unlockBus(transport.user);
    const EventKind order[] = {
        EventKind::ArbiterLock,
        EventKind::BeginTransaction,
        EventKind::EndTransaction,
        EventKind::ArbiterUnlock};
    CHECK_EQ(sizeof(order) / sizeof(order[0]), gEvents.size());
    for (size_t index = 0U; index < gEvents.size(); ++index)
    {
        CHECK_EQ(order[index], gEvents[index].kind);
    }
    CHECK_EQ(&arbiterContext, gEvents.back().object);
    return true;
}

bool rebindReplacesOnlyAfterCompleteValidation()
{
    resetFixture();
    SPIClass firstSpi;
    SPIClass secondSpi;
    MAX31865ArduinoBackend backend;
    CHECK(max31865ArduinoBackendBind(
              backend,
              firstSpi,
              SPISettings(4000000U, MSBFIRST, SPI_MODE1),
              4000000U,
              validPins())
              .ok());
    CHECK(max31865ArduinoConfigureControlPins(backend).ok());
    CHECK(backend.pinsConfigured);

    MAX31865Pins invalid = replacementPins();
    invalid.chipSelect = GPIO_PIN_COUNT;
    CHECK(invalidBindPreservesState(
        backend, secondSpi, 2000000U, invalid));
    CHECK(backend.pinsConfigured);

    gEvents.clear();
    const MAX31865Pins replacement = replacementPins();
    CHECK(max31865ArduinoBackendBind(
              backend,
              secondSpi,
              SPISettings(2000000U, MSBFIRST, SPI_MODE3),
              2000000U,
              replacement)
              .ok());
    CHECK(gEvents.empty());
    CHECK_EQ(&secondSpi, backend.spi);
    CHECK_EQ(2000000U, backend.spiClockHz);
    CHECK_EQ(2000000U, backend.settings.clockHz);
    CHECK_EQ(SPI_MODE3, backend.settings.dataMode);
    CHECK(samePins(replacement, backend.pins));
    CHECK_EQ(nullptr, backend.arbiter.user);
    CHECK_EQ(nullptr, backend.arbiter.lock);
    CHECK_EQ(nullptr, backend.arbiter.unlock);
    CHECK(!backend.pinsConfigured);

    const MAX31865Transport rebound = max31865ArduinoTransport(backend);
    CHECK_EQ(&backend, rebound.user);
    CHECK(!rebound.capabilities.hasDrdy);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        rebound.lockBus(rebound.user, 1U).code);
    CHECK(gEvents.empty());
    CHECK(max31865ArduinoConfigureControlPins(backend).ok());
    CHECK_EQ(2U, gEvents.size());
    CHECK_EQ(replacement.chipSelect, gEvents[0].first);
    CHECK_EQ(replacement.chipSelect, gEvents[1].first);

    gEvents.clear();
    CHECK(max31865ArduinoBackendBind(
              backend,
              secondSpi,
              SPISettings(2000000U, MSBFIRST, SPI_MODE3),
              2000000U,
              replacement)
              .ok());
    CHECK(gEvents.empty());
    CHECK(!backend.pinsConfigured);
    return true;
}

bool nullContextsFailWithoutBackendIo()
{
    resetFixture();
    SPIClass spi;
    MAX31865ArduinoBackend backend;
    CHECK(max31865ArduinoBackendBind(
              backend,
              spi,
              SPISettings(1000000U, MSBFIRST, SPI_MODE1),
              1000000U,
              validPins())
              .ok());
    CHECK(max31865ArduinoConfigureControlPins(backend).ok());
    const MAX31865Transport transport = max31865ArduinoTransport(backend);
    gEvents.clear();

    uint8_t tx[2] = {0x00U, 0x00U};
    uint8_t rx[2] = {0xA5U, 0x5AU};
    bool level = false;
    CHECK_EQ(
        MAX31865Error::InvalidState,
        transport.lockBus(nullptr, 1U).code);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        transport.setChipSelect(nullptr, true).code);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        transport.transfer(nullptr, tx, rx, sizeof(tx), 1U).code);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        transport.readPin(nullptr, MAX31865Pin::DRDY, &level).code);
    transport.unlockBus(nullptr);
    CHECK(gEvents.empty());
    CHECK_EQ(0xA5U, rx[0]);
    CHECK_EQ(0x5AU, rx[1]);
    CHECK(!level);
    return true;
}

bool sharedHostUsesPerDeviceSettingsAndOneArbiter()
{
    resetFixture();
    SPIClass sharedSpi;
    int arbiterContext = 37;
    MAX31865ArduinoBusArbiter arbiter{
        &arbiterContext, arbiterLock, arbiterUnlock};

    MAX31865ArduinoBackend first;
    MAX31865ArduinoBackend second;
    MAX31865Pins secondPins = validPins();
    secondPins.chipSelect = 9;
    secondPins.dataReady = 10;
    CHECK(max31865ArduinoBackendBind(
              first,
              sharedSpi,
              SPISettings(1000000U, MSBFIRST, SPI_MODE1),
              1000000U,
              validPins(),
              &arbiter)
              .ok());
    CHECK(max31865ArduinoBackendBind(
              second,
              sharedSpi,
              SPISettings(4000000U, MSBFIRST, SPI_MODE3),
              4000000U,
              secondPins,
              &arbiter)
              .ok());
    CHECK(max31865ArduinoConfigureControlPins(first).ok());
    CHECK(max31865ArduinoConfigureControlPins(second).ok());

    const MAX31865Transport transports[] = {
        max31865ArduinoTransport(first),
        max31865ArduinoTransport(second)};
    const int32_t chipSelects[] = {
        validPins().chipSelect,
        secondPins.chipSelect};
    const uint32_t clocks[] = {1000000U, 4000000U};
    const uint8_t modes[] = {SPI_MODE1, SPI_MODE3};
    gEvents.clear();
    for (size_t device = 0U; device < 2U; ++device)
    {
        uint8_t tx[2] = {0x00U, static_cast<uint8_t>(device)};
        uint8_t rx[2] = {};
        const MAX31865Transport &transport = transports[device];
        CHECK(transport.lockBus(transport.user, 25U).ok());
        CHECK(transport.setChipSelect(transport.user, true).ok());
        CHECK(transport
                  .transfer(
                      transport.user, tx, rx, sizeof(tx), 25U)
                  .ok());
        CHECK(transport.setChipSelect(transport.user, false).ok());
        transport.unlockBus(transport.user);
    }

    CHECK_EQ(14U, gEvents.size());
    for (size_t device = 0U; device < 2U; ++device)
    {
        const size_t base = device * 7U;
        CHECK_EQ(EventKind::ArbiterLock, gEvents[base].kind);
        CHECK_EQ(&arbiterContext, gEvents[base].object);
        CHECK_EQ(EventKind::BeginTransaction, gEvents[base + 1U].kind);
        CHECK_EQ(&sharedSpi, gEvents[base + 1U].object);
        CHECK_EQ(
            static_cast<int32_t>(clocks[device]),
            gEvents[base + 1U].first);
        CHECK_EQ(
            static_cast<uint32_t>(modes[device]),
            gEvents[base + 1U].second);
        CHECK_EQ(EventKind::DigitalWrite, gEvents[base + 2U].kind);
        CHECK_EQ(chipSelects[device], gEvents[base + 2U].first);
        CHECK_EQ(static_cast<uint32_t>(LOW), gEvents[base + 2U].second);
        CHECK_EQ(EventKind::Transfer, gEvents[base + 3U].kind);
        CHECK_EQ(EventKind::DigitalWrite, gEvents[base + 4U].kind);
        CHECK_EQ(chipSelects[device], gEvents[base + 4U].first);
        CHECK_EQ(static_cast<uint32_t>(HIGH), gEvents[base + 4U].second);
        CHECK_EQ(EventKind::EndTransaction, gEvents[base + 5U].kind);
        CHECK_EQ(&sharedSpi, gEvents[base + 5U].object);
        CHECK_EQ(EventKind::ArbiterUnlock, gEvents[base + 6U].kind);
        CHECK_EQ(&arbiterContext, gEvents[base + 6U].object);
    }
    return true;
}

bool productionDriverLifecycleTraversesArduinoBackend()
{
    resetFixture();
    gUseDeviceModel = true;
    SPIClass spi;
    MAX31865ArduinoBackend backend;
    CHECK(max31865ArduinoBackendBind(
              backend,
              spi,
              SPISettings(1000000U, MSBFIRST, SPI_MODE1),
              1000000U,
              validPins())
              .ok());
    CHECK(max31865ArduinoConfigureControlPins(backend).ok());
    gEvents.clear();

    {
        MAX31865 device;
        MAX31865BeginConfig config = max31865DefaultBeginConfig();
        config.transport = max31865ArduinoTransport(backend);
        CHECK(device.begin(config).ok());
        CHECK_EQ(MAX31865State::Ready, device.state());
        CHECK(!gModelCallbackFailed);

        CHECK(gDevice.enqueueSample(8192U));
        CHECK(device.startContinuous(100U).ok());
        delay(70U);
        MAX31865Sample sample{};
        CHECK(device.poll(sample, 100U).ok());
        CHECK_EQ(8192U, sample.rawCode);
        CHECK_NEAR(100.0F, sample.resistanceOhms, 0.001F);
        CHECK(device.stop(100U).ok());
        CHECK(!gDevice.conversionRunning());
        CHECK(!gModelCallbackFailed);

        bool transactionOpen = false;
        bool selected = false;
        for (const Event &event : gEvents)
        {
            if (event.kind == EventKind::BeginTransaction)
            {
                CHECK(!transactionOpen);
                CHECK(!selected);
                transactionOpen = true;
            }
            else if (event.kind == EventKind::DigitalWrite &&
                     event.first == validPins().chipSelect)
            {
                if (event.second == static_cast<uint32_t>(LOW))
                {
                    CHECK(transactionOpen);
                    CHECK(!selected);
                    selected = true;
                }
                else
                {
                    CHECK(transactionOpen);
                    // Recovery/begin may first prove CS inactive while it is
                    // already high; ordinary frames arrive here selected.
                    selected = false;
                }
            }
            else if (event.kind == EventKind::Transfer)
            {
                CHECK(transactionOpen);
                CHECK(selected);
            }
            else if (event.kind == EventKind::EndTransaction)
            {
                CHECK(transactionOpen);
                CHECK(!selected);
                transactionOpen = false;
            }
        }
        CHECK(!transactionOpen);
        CHECK(!selected);

        const size_t beforeEnd = gEvents.size();
        device.end();
        CHECK_EQ(beforeEnd, gEvents.size());
    }
    CHECK(!gModelCallbackFailed);
    return true;
}

} // namespace

void pinMode(int pin, int mode)
{
    gEvents.push_back(
        {EventKind::PinMode, pin, static_cast<uint32_t>(mode), nullptr});
}

void digitalWrite(int pin, int level)
{
    gEvents.push_back(
        {EventKind::DigitalWrite,
         pin,
         static_cast<uint32_t>(level),
         nullptr});
    if (gUseDeviceModel && pin == validPins().chipSelect &&
        !gDevice.setChipSelect(level == LOW).ok())
    {
        gModelCallbackFailed = true;
    }
}

int digitalRead(int pin)
{
    gEvents.push_back({EventKind::DigitalRead, pin, 0U, nullptr});
    if (gUseDeviceModel && pin == validPins().dataReady)
    {
        bool level = true;
        if (!gDevice.readDrdy(level).ok())
        {
            gModelCallbackFailed = true;
        }
        return level ? HIGH : LOW;
    }
    return gDrdyLevel ? HIGH : LOW;
}

uint32_t millis()
{
    return gTimeMs;
}

uint32_t micros()
{
    return gTimeUs;
}

void delay(uint32_t milliseconds)
{
    gEvents.push_back({EventKind::DelayMs, 0, milliseconds, nullptr});
    gTimeMs += milliseconds;
    gTimeUs += milliseconds * 1000U;
    if (gUseDeviceModel)
    {
        gDevice.advanceTimeMs(milliseconds);
    }
}

void delayMicroseconds(uint32_t microseconds)
{
    gEvents.push_back({EventKind::DelayUs, 0, microseconds, nullptr});
    gTimeUs += microseconds;
    gTimeMs = gTimeUs / 1000U;
    if (gUseDeviceModel)
    {
        gDevice.advanceTimeUs(microseconds);
    }
}

void fakeArduinoBeginTransaction(
    const void *spi,
    uint32_t clock,
    uint8_t,
    uint8_t mode)
{
    gEvents.push_back(
        {EventKind::BeginTransaction,
         static_cast<int32_t>(clock),
         static_cast<uint32_t>(mode),
         spi});
}

void fakeArduinoEndTransaction(const void *spi)
{
    gEvents.push_back({EventKind::EndTransaction, 0, 0U, spi});
}

void fakeArduinoTransfer(
    const void *spi,
    const uint8_t *tx,
    uint8_t *rx,
    uint32_t length)
{
    gEvents.push_back({EventKind::Transfer, 0, length, spi});
    if (gUseDeviceModel)
    {
        if (!gDevice.transfer(tx, rx, length).ok())
        {
            gModelCallbackFailed = true;
            std::memset(rx, 0, length);
        }
        return;
    }
    for (uint32_t index = 0U; index < length; ++index)
    {
        rx[index] = static_cast<uint8_t>(tx[index] ^ 0xA5U);
    }
}

int main()
{
    const TestCase tests[] = {
        {"constructor and bind/rebind validation are transactional",
         constructorAndBindingAreTransactional},
        {"control-pin configuration is explicit and glitch-safe",
         configurationIsExplicitAndGlitchSafe},
        {"transport publication and callbacks are exact",
         transportPublicationAndCallbacksAreExact},
        {"arbiter ordering and status mapping are bounded",
         arbiterOrderingAndStatusMappingAreBounded},
        {"valid rebind replaces state only after validation",
         rebindReplacesOnlyAfterCompleteValidation},
        {"null callback contexts fail without backend I/O",
         nullContextsFailWithoutBackendIo},
        {"shared host applies per-device settings under one arbiter",
         sharedHostUsesPerDeviceSettingsAndOneArbiter},
        {"production driver lifecycle traverses Arduino backend",
         productionDriverLifecycleTraversesArduinoBackend}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
