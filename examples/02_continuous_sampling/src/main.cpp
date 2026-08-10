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
bool sampling = false;
}

void setup()
{
    board::beginSerial();
    Serial.println("MAX31865 continuous sampling; loop() owns scheduling.");

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
    if (!example_status::report("begin", status)) return;

    status = rtd.startContinuous(board::OPERATION_TIMEOUT_MS);
    sampling = example_status::report("start continuous", status);
}

void loop()
{
    if (!sampling)
    {
        delay(100U);
        return;
    }

    rtd.tick(millis());
    MAX31865Sample sample{};
    const MAX31865Status status = rtd.poll(sample);
    if (status.code == MAX31865Error::NoData)
    {
        delay(1U);
        return;
    }
    if (!example_status::report("poll", status))
    {
        // Retry/recovery policy belongs to the application. This example stops
        // acquisition after one terminal error and reports bounded cleanup.
        const MAX31865Status stopStatus =
            rtd.stop(board::OPERATION_TIMEOUT_MS);
        (void)example_status::report("stop after error", stopStatus);
        sampling = false;
        return;
    }

    example_status::printSample(sample);
}
