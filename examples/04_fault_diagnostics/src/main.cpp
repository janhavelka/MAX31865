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
}

void setup()
{
    board::beginSerial();
    Serial.println("MAX31865 threshold and fault-cycle diagnostics.");

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

    status = rtd.setFaultThresholdsTemperature(
        -50.0F,
        200.0F,
        board::OPERATION_TIMEOUT_MS);
    if (!example_status::report("set -50..200 C thresholds", status)) return;

    float lowC = 0.0F;
    float highC = 0.0F;
    status = rtd.readFaultThresholdsTemperature(
        lowC,
        highC,
        board::OPERATION_TIMEOUT_MS);
    if (example_status::report("read thresholds", status))
    {
        Serial.print("decoded thresholds: ");
        Serial.print(lowC, 3);
        Serial.print(" C .. ");
        Serial.print(highC, 3);
        Serial.println(" C");
    }

    MAX31865FaultStatus fault{};
    status = rtd.runManualFaultDetection(
        fault,
        board::OPERATION_TIMEOUT_MS);
    (void)example_status::report("manual fault cycle", status);
    if (status.ok() || status.code == MAX31865Error::DeviceFault)
    {
        example_status::printFault(fault);
    }

    if (board::INPUT_FILTER_RC_US <=
        max31865_cmd::AUTO_FAULT_MAX_RC_US)
    {
        fault = MAX31865FaultStatus{};
        status = rtd.runAutomaticFaultDetection(
            fault,
            board::OPERATION_TIMEOUT_MS);
        (void)example_status::report("automatic fault cycle", status);
        if (status.ok() || status.code == MAX31865Error::DeviceFault)
        {
            example_status::printFault(fault);
        }
    }
    else
    {
        Serial.println(
            "Automatic cycle skipped: fitted external RC exceeds 100 us; "
            "the data sheet requires manual timing.");
    }

    example_status::printHealth(rtd.health());
}

void loop()
{
    delay(1000U);
}
