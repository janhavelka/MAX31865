#include <Arduino.h>
#include <SPI.h>

#include "MAX31865/ArduinoBackend.h"
#include "MAX31865/MAX31865.h"
#include "common/BoardConfig.h"
#include "common/ExampleStatus.h"

namespace
{
MAX31865ArduinoBackend backend;
MAX31865 rtd;
bool initialized = false;
uint32_t nextReadMs = 0U;
}

void setup()
{
    board::beginSerial();
    Serial.println("MAX31865 bounded one-shot sampling.");

    const MAX31865Pins pins = board::pins();
    SPI.begin(pins.sck, pins.miso, pins.mosi, MAX31865_PIN_UNUSED);
    const SPISettings settings(board::SPI_CLOCK_HZ, MSBFIRST, SPI_MODE1);
    MAX31865Status status = max31865ArduinoBackendBind(
        backend,
        SPI,
        settings,
        board::SPI_CLOCK_HZ,
        pins);
    if (!example_status::report("backend bind", status)) return;
    status = max31865ArduinoConfigureControlPins(backend);
    if (!example_status::report("control pins", status)) return;

    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = max31865ArduinoTransport(backend);
    config.defaultOperationTimeoutMs = board::OPERATION_TIMEOUT_MS;
    config.initialDeviceConfig.wireMode = board::WIRE_MODE;
    config.initialDeviceConfig.filter = board::FILTER;
    config.rtd.referenceResistorOhms = board::REFERENCE_RESISTOR_OHMS;
    config.rtd.nominalResistanceOhms = board::RTD_NOMINAL_OHMS;
    config.rtd.inputFilterTimeConstantUs = board::INPUT_FILTER_RC_US;
    status = rtd.begin(config);
    initialized = example_status::report("begin", status);
    nextReadMs = millis();
}

void loop()
{
    if (!initialized)
    {
        delay(100U);
        return;
    }

    const uint32_t nowMs = millis();
    rtd.tick(nowMs);
    if (static_cast<int32_t>(nowMs - nextReadMs) < 0)
    {
        delay(1U);
        return;
    }
    nextReadMs = nowMs + 1000U;

    MAX31865Sample sample{};
    const MAX31865Status status = rtd.readOneShot(
        sample,
        board::OPERATION_TIMEOUT_MS);
    if (example_status::report("read one shot", status))
    {
        example_status::printSample(sample);
    }
    else
    {
        // A timeout can leave the committed one-shot armed. Do not blindly
        // retrigger; let the application choose an explicit stop/recovery path.
        initialized = false;
    }
}
