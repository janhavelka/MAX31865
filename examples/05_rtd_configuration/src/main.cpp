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
}

void setup()
{
    board::beginSerial();
    Serial.println("MAX31865 RTD/reference configuration and conversion helpers.");
    Serial.println(
        "BoardConfig.h must match wiring, notch, RREF, RTD, and RC.");

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

    MAX31865RtdConfig rtdConfig = max31865DefaultRtdConfig();
    rtdConfig.referenceResistorOhms = board::REFERENCE_RESISTOR_OHMS;
    rtdConfig.nominalResistanceOhms = board::RTD_NOMINAL_OHMS;
    rtdConfig.inputFilterTimeConstantUs = board::INPUT_FILTER_RC_US;
    rtdConfig.coefficients = max31865Iec60751Coefficients();
    rtdConfig.minimumTemperatureC = -200.0F;
    rtdConfig.maximumTemperatureC = 850.0F;

    MAX31865BeginConfig config = max31865DefaultBeginConfig();
    config.transport = max31865ArduinoTransport(backend);
    config.defaultOperationTimeoutMs = board::OPERATION_TIMEOUT_MS;
    config.rtd = rtdConfig;
    config.initialDeviceConfig.wireMode = board::WIRE_MODE;
    config.initialDeviceConfig.filter = board::FILTER;
    status = rtd.begin(config);
    if (!example_status::report("begin", status)) return;

    float resistance = 0.0F;
    status = rtd.temperatureToResistance(100.0F, resistance);
    if (example_status::report("100 C to resistance", status))
    {
        Serial.print("100 C resistance: ");
        Serial.print(resistance, 6);
        Serial.println(" ohm");
    }

    uint16_t code = 0U;
    status = rtd.temperatureToCode(100.0F, code);
    if (example_status::report("100 C to ADC code", status))
    {
        Serial.print("100 C code: ");
        Serial.println(static_cast<unsigned>(code));
    }

    initialized = true;
}

void loop()
{
    if (!initialized)
    {
        delay(100U);
        return;
    }

    MAX31865Sample sample{};
    const MAX31865Status status = rtd.readOneShot(
        sample,
        board::OPERATION_TIMEOUT_MS);
    if (example_status::report("configured RTD read", status))
    {
        example_status::printSample(sample);
    }
    else
    {
        initialized = false;
    }
    delay(1000U);
}
