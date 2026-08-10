/**
 * @file MAX31865ArduinoBackend.cpp
 * @brief Arduino implementation of the borrowed MAX31865 transport adapter.
 */

#include "MAX31865/ArduinoBackend.h"
#include "MAX31865/CommandTable.h"

#if defined(ARDUINO)

namespace
{
MAX31865Status invalidState(const char *message)
{
    return MAX31865Status::Error(MAX31865Error::InvalidState, message);
}

MAX31865Status invalidPin(const char *message, int32_t pin)
{
    return MAX31865Status::Error(
        MAX31865Error::InvalidArgument,
        message,
        pin);
}

bool isInputCapable(int32_t pin)
{
    return (pin >= 0) &&
           (pin < static_cast<int32_t>(SOC_GPIO_PIN_COUNT)) &&
           digitalPinIsValid(pin);
}

bool isOutputCapable(int32_t pin)
{
    return (pin >= 0) &&
           (pin < static_cast<int32_t>(SOC_GPIO_PIN_COUNT)) &&
           digitalPinCanOutput(pin);
}

MAX31865Status validatePins(const MAX31865Pins &pins)
{
    const int32_t values[] = {
        pins.sck,
        pins.miso,
        pins.mosi,
        pins.chipSelect,
        pins.dataReady};

    if ((pins.sck == MAX31865_PIN_UNUSED) ||
        (pins.miso == MAX31865_PIN_UNUSED) ||
        (pins.mosi == MAX31865_PIN_UNUSED) ||
        (pins.chipSelect == MAX31865_PIN_UNUSED))
    {
        return invalidPin(
            "required Arduino pin is unused",
            MAX31865_PIN_UNUSED);
    }

    for (size_t index = 0U; index < (sizeof(values) / sizeof(values[0])); ++index)
    {
        if (values[index] < MAX31865_PIN_UNUSED)
        {
            return invalidPin("invalid negative Arduino pin", values[index]);
        }
        if ((values[index] != MAX31865_PIN_UNUSED) &&
            !isInputCapable(values[index]))
        {
            return invalidPin(
                "Arduino pin is outside target GPIO range",
                values[index]);
        }
        if (values[index] == MAX31865_PIN_UNUSED)
        {
            continue;
        }
        for (size_t other = index + 1U;
             other < (sizeof(values) / sizeof(values[0]));
             ++other)
        {
            if (values[index] == values[other])
            {
                return invalidPin("duplicate Arduino pin role", values[index]);
            }
        }
    }

    const int32_t outputs[] = {
        pins.sck,
        pins.mosi,
        pins.chipSelect};
    for (size_t index = 0U; index < (sizeof(outputs) / sizeof(outputs[0])); ++index)
    {
        if (!isOutputCapable(outputs[index]))
        {
            return invalidPin(
                "Arduino pin is not output-capable",
                outputs[index]);
        }
    }

    return MAX31865Status::Ok();
}

MAX31865Status validateConfigured(const MAX31865ArduinoBackend *backend)
{
    if ((backend == nullptr) || (backend->spi == nullptr) ||
        !backend->pinsConfigured || backend->spiClockHz == 0U ||
        backend->spiClockHz > max31865_cmd::SPI_MAX_HZ)
    {
        return invalidState("Arduino control pins are not configured");
    }
    return MAX31865Status::Ok();
}

MAX31865Status lockBus(void *user, uint32_t timeoutMs)
{
    MAX31865ArduinoBackend *backend =
        static_cast<MAX31865ArduinoBackend *>(user);
    MAX31865Status status = validateConfigured(backend);
    if (!status.ok())
    {
        return status;
    }

    if (backend->arbiter.lock != nullptr)
    {
        status = backend->arbiter.lock(backend->arbiter.user, timeoutMs);
        if (!status.ok())
        {
            if ((status.code == MAX31865Error::BusLockTimeout) ||
                (status.code == MAX31865Error::BusLockFailed))
            {
                return status;
            }
            return MAX31865Status::Error(
                MAX31865Error::BusLockFailed,
                "Arduino bus arbiter returned invalid status",
                static_cast<int32_t>(status.code));
        }
    }

    backend->spi->beginTransaction(backend->settings);
    return MAX31865Status::Ok();
}

void unlockBus(void *user)
{
    MAX31865ArduinoBackend *backend =
        static_cast<MAX31865ArduinoBackend *>(user);
    if ((backend == nullptr) || (backend->spi == nullptr) ||
        !backend->pinsConfigured)
    {
        return;
    }

    backend->spi->endTransaction();
    if (backend->arbiter.unlock != nullptr)
    {
        backend->arbiter.unlock(backend->arbiter.user);
    }
}

MAX31865Status setChipSelect(void *user, bool asserted)
{
    MAX31865ArduinoBackend *backend =
        static_cast<MAX31865ArduinoBackend *>(user);
    const MAX31865Status status = validateConfigured(backend);
    if (!status.ok())
    {
        return status;
    }

    digitalWrite(backend->pins.chipSelect, asserted ? LOW : HIGH);
    return MAX31865Status::Ok();
}

MAX31865Status transfer(
    void *user,
    const uint8_t *tx,
    uint8_t *rx,
    size_t length,
    uint32_t)
{
    MAX31865ArduinoBackend *backend =
        static_cast<MAX31865ArduinoBackend *>(user);
    const MAX31865Status status = validateConfigured(backend);
    if (!status.ok())
    {
        return status;
    }
    if ((tx == nullptr) || (rx == nullptr) || (length == 0U) ||
        (length > max31865_cmd::MAX_FRAME_BYTES))
    {
        return MAX31865Status::Error(
            MAX31865Error::SpiTransferFailed,
            "invalid Arduino SPI transfer");
    }

    // Arduino SPI exposes neither a wire-error result nor cancellation. The
    // fixed frame bound and application-selected clock bound normal wire time.
    backend->spi->transferBytes(tx, rx, static_cast<uint32_t>(length));
    return MAX31865Status::Ok();
}

MAX31865Status readPin(void *user, MAX31865Pin role, bool *level)
{
    const MAX31865ArduinoBackend *backend =
        static_cast<const MAX31865ArduinoBackend *>(user);
    const MAX31865Status status = validateConfigured(backend);
    if (!status.ok())
    {
        return status;
    }
    if (level == nullptr)
    {
        return MAX31865Status::Error(
            MAX31865Error::GpioFailed,
            "Arduino GPIO input pointer is null",
            static_cast<int32_t>(role));
    }
    if (role != MAX31865Pin::DRDY)
    {
        return MAX31865Status::Error(
            MAX31865Error::GpioFailed,
            "Arduino GPIO read role is unsupported",
            static_cast<int32_t>(role));
    }
    if (backend->pins.dataReady == MAX31865_PIN_UNUSED)
    {
        return MAX31865Status::Error(
            MAX31865Error::GpioFailed,
            "Arduino input pin is unavailable",
            static_cast<int32_t>(role));
    }

    const bool observed = digitalRead(backend->pins.dataReady) != LOW;
    *level = observed;
    return MAX31865Status::Ok();
}

uint32_t nowMs(void *)
{
    return millis();
}

uint32_t nowUs(void *)
{
    return micros();
}

void sleepMs(void *, uint32_t milliseconds)
{
    delay(milliseconds);
}

void delayUs(void *, uint32_t microseconds)
{
    delayMicroseconds(microseconds);
}
} // namespace

MAX31865ArduinoBackend::MAX31865ArduinoBackend()
    : spi(nullptr),
      settings(),
      spiClockHz(0U),
      pins{},
      arbiter{nullptr, nullptr, nullptr},
      pinsConfigured(false)
{
}

MAX31865Status max31865ArduinoBackendBind(
    MAX31865ArduinoBackend &backend,
    SPIClass &spi,
    const SPISettings &settings,
    uint32_t spiClockHz,
    const MAX31865Pins &pins,
    const MAX31865ArduinoBusArbiter *arbiter)
{
    if (spiClockHz == 0U || spiClockHz > max31865_cmd::SPI_MAX_HZ)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "Arduino SPI clock must be within the MAX31865 limit",
            spiClockHz > static_cast<uint32_t>(INT32_MAX)
                ? INT32_MAX
                : static_cast<int32_t>(spiClockHz));
    }
    const MAX31865Status pinStatus = validatePins(pins);
    if (!pinStatus.ok())
    {
        return pinStatus;
    }
    if ((arbiter != nullptr) &&
        ((arbiter->lock == nullptr) || (arbiter->unlock == nullptr)))
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "Arduino arbiter requires lock and unlock callbacks");
    }

    const MAX31865ArduinoBusArbiter storedArbiter =
        (arbiter == nullptr)
            ? MAX31865ArduinoBusArbiter{nullptr, nullptr, nullptr}
            : *arbiter;

    backend.spi = &spi;
    backend.settings = settings;
    backend.spiClockHz = spiClockHz;
    backend.pins = pins;
    backend.arbiter = storedArbiter;
    backend.pinsConfigured = false;
    return MAX31865Status::Ok();
}

MAX31865Status max31865ArduinoConfigureControlPins(
    MAX31865ArduinoBackend &backend)
{
    if (backend.spi == nullptr)
    {
        return invalidState("Arduino backend is not bound");
    }

    // Preload the inactive latch before enabling the output driver. CS rising
    // edges commit commands, so avoiding a low glitch is protocol-significant.
    digitalWrite(backend.pins.chipSelect, HIGH);
    pinMode(backend.pins.chipSelect, OUTPUT);
    if (backend.pins.dataReady != MAX31865_PIN_UNUSED)
    {
        pinMode(backend.pins.dataReady, INPUT);
    }

    backend.pinsConfigured = true;
    return MAX31865Status::Ok();
}

MAX31865Transport max31865ArduinoTransport(
    MAX31865ArduinoBackend &backend)
{
    MAX31865Transport transport{};
    transport.user = &backend;
    transport.capabilities = {
        backend.pins.dataReady != MAX31865_PIN_UNUSED};
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

#endif // defined(ARDUINO)
