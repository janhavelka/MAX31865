#include <Arduino.h>
#include <SPI.h>

#include <cstdio>

#include "MAX31865/ArduinoBackend.h"
#include "MAX31865/MAX31865.h"
#include "common/BoardConfig.h"
#include "common/ExampleStatus.h"

namespace
{
MAX31865ArduinoBackend backend;
MAX31865 rtd;
bool initialized = false;
}

void setup()
{
    board::beginSerial();
    Serial.println("MAX31865 basic bringup; review example-only wiring first.");

    const MAX31865Pins pins = board::pins();
    SPI.begin(pins.sck, pins.miso, pins.mosi, MAX31865_PIN_UNUSED);

    const SPISettings settings(
        board::SPI_CLOCK_HZ,
        MSBFIRST,
        SPI_MODE1);
    MAX31865Status status = max31865ArduinoBackendBind(
        backend,
        SPI,
        settings,
        board::SPI_CLOCK_HZ,
        pins);
    if (!example_status::report("backend bind", status))
    {
        return;
    }
    status = max31865ArduinoConfigureControlPins(backend);
    if (!example_status::report("control pins", status))
    {
        return;
    }

    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = max31865ArduinoTransport(backend);
    config.defaultOperationTimeoutMs = board::OPERATION_TIMEOUT_MS;
    config.initialDeviceConfig.wireMode = board::WIRE_MODE;
    config.initialDeviceConfig.filter = board::FILTER;
    config.rtd.referenceResistorOhms = board::REFERENCE_RESISTOR_OHMS;
    config.rtd.nominalResistanceOhms = board::RTD_NOMINAL_OHMS;
    config.rtd.inputFilterTimeConstantUs = board::INPUT_FILTER_RC_US;
    status = rtd.begin(config);
    if (!example_status::report("begin", status))
    {
        return;
    }

    MAX31865DeviceInfo info{};
    status = rtd.probe(info);
    if (example_status::report("read-only probe", status))
    {
        char line[128];
        std::snprintf(
            line,
            sizeof(line),
            "CONFIG=0x%02X high=0x%04X low=0x%04X fault=0x%02X matches=%u\n",
            static_cast<unsigned>(info.rawConfig),
            static_cast<unsigned>(info.rawHighThresholdRegister),
            static_cast<unsigned>(info.rawLowThresholdRegister),
            static_cast<unsigned>(info.rawFaultStatus),
            info.configurationMatches ? 1U : 0U);
        Serial.print(line);
    }

    MAX31865Settings observed{};
    status = rtd.readConfiguration(observed, board::OPERATION_TIMEOUT_MS);
    if (example_status::report("read configuration", status))
    {
        Serial.print("wire mode=");
        Serial.print(static_cast<unsigned>(observed.deviceConfig.wireMode));
        Serial.print(" filter=");
        Serial.print(observed.deviceConfig.filter == MAX31865Filter::Hz50 ? "50 Hz" : "60 Hz");
        Serial.print(" bias=");
        Serial.println(observed.deviceConfig.biasEnabled ? "on" : "off");
    }

    example_status::printHealth(rtd.health());
    initialized = true;
}

void loop()
{
    if (!initialized)
    {
        delay(250U);
        return;
    }
    rtd.tick(millis());
    delay(1000U);
}
