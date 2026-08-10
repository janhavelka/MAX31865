#include <Arduino.h>
#include <SPI.h>

#include <cstdio>

#include "MAX31865/ArduinoBackend.h"
#include "MAX31865/MAX31865.h"
#include "DiagnosticConfig.h"
#include "Max31865Cli.h"
#include "Max31865CliShell.h"

namespace
{
MAX31865ArduinoBackend backend;
MAX31865 device;
MAX31865BeginConfig beginConfig;
max31865_cli::TransportObserver transportObserver;
max31865_cli::BoundedSerialShell shell;
const MAX31865Pins pins = diagnostic_config::pins();
bool transportReady = false;

void reportSetup(const char *operation, const MAX31865Status &status)
{
    char line[192]{};
    std::snprintf(
        line,
        sizeof(line),
        "%s: code=%u name=%s detail=%ld msg=%s\n",
        operation,
        static_cast<unsigned>(status.code),
        max31865ErrorName(status.code),
        static_cast<long>(status.detail),
        status.msg == nullptr ? "" : status.msg);
    Serial.print(line);
}

void writeSerial(void *, const char *data, size_t length)
{
    if (data != nullptr && length > 0U)
    {
        Serial.write(reinterpret_cast<const uint8_t *>(data), length);
    }
}

uint32_t platformNowMs(void *)
{
    return millis();
}

uint32_t freeHeapBytes(void *)
{
    return ESP.getFreeHeap();
}
} // namespace

void setup()
{
    diagnostic_config::beginSerial();
    Serial.println(
        "MAX31865 diagnostic CLI; real hardware required, no simulated data.");

    SPI.begin(pins.sck, pins.miso, pins.mosi, MAX31865_PIN_UNUSED);
    const SPISettings settings(
        diagnostic_config::SPI_CLOCK_HZ,
        MSBFIRST,
        SPI_MODE1);

    MAX31865Status status = max31865ArduinoBackendBind(
        backend,
        SPI,
        settings,
        diagnostic_config::SPI_CLOCK_HZ,
        pins);
    reportSetup("backend_bind", status);
    if (status.ok())
    {
        status = max31865ArduinoConfigureControlPins(backend);
        reportSetup("control_pins", status);
        transportReady = status.ok();
    }

    beginConfig = max31865DefaultBeginConfig();
    beginConfig.transport = max31865_cli::observeTransport(
        transportObserver,
        max31865ArduinoTransport(backend));
    beginConfig.defaultOperationTimeoutMs =
        diagnostic_config::OPERATION_TIMEOUT_MS;
    beginConfig.initialDeviceConfig.wireMode =
        diagnostic_config::WIRE_MODE;
    beginConfig.initialDeviceConfig.filter =
        diagnostic_config::FILTER;
    beginConfig.rtd.referenceResistorOhms =
        diagnostic_config::REFERENCE_RESISTOR_OHMS;
    beginConfig.rtd.nominalResistanceOhms =
        diagnostic_config::RTD_NOMINAL_OHMS;
    beginConfig.rtd.inputFilterTimeConstantUs =
        diagnostic_config::INPUT_FILTER_RC_US;

    const max31865_cli::Context context{
        &device,
        &beginConfig,
        &pins,
        &transportObserver,
        diagnostic_config::SPI_CLOCK_HZ,
        {nullptr, writeSerial, platformNowMs, freeHeapBytes},
        transportReady};
    max31865_cli::bind(context);
    max31865_cli::start();
}

void loop()
{
    char line[192]{};
    const max31865_cli::BoundedSerialShell::Result result = shell.poll(
        Serial,
        line,
        sizeof(line));
    if (result == max31865_cli::BoundedSerialShell::Result::Line)
    {
        max31865_cli::processLine(line);
        max31865_cli::printPrompt();
    }
    else if (result == max31865_cli::BoundedSerialShell::Result::Overflow)
    {
        max31865_cli::processInputOverflow();
        max31865_cli::printPrompt();
    }
    max31865_cli::service();
}
