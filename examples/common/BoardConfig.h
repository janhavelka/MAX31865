#pragma once

#include <Arduino.h>

#include "MAX31865/Config.h"

namespace board
{
// Example-only wiring. Review these pins against the actual ESP32-S2/S3 board
// and MAX31865 breakout before applying power.
static constexpr int32_t PIN_SCK = 12;
static constexpr int32_t PIN_MISO = 13;
static constexpr int32_t PIN_MOSI = 11;
static constexpr int32_t PIN_CS = 10;
static constexpr int32_t PIN_DRDY = MAX31865_PIN_UNUSED;

static constexpr uint32_t SERIAL_BAUD = 115200U;
static constexpr uint32_t SERIAL_WAIT_MS = 3000U;
static constexpr uint32_t SPI_CLOCK_HZ = 1000000U;
static constexpr uint32_t OPERATION_TIMEOUT_MS = 250U;

// These are measurement-board values, not library defaults. Enter the actual
// RTD wiring and mains-notch choice plus the measured fitted components.
static constexpr MAX31865WireMode WIRE_MODE = MAX31865WireMode::FourWire;
static constexpr MAX31865Filter FILTER = MAX31865Filter::Hz60;
static constexpr float REFERENCE_RESISTOR_OHMS = 400.0F;
static constexpr float RTD_NOMINAL_OHMS = 100.0F;
static constexpr uint32_t INPUT_FILTER_RC_US = 1000U;

static constexpr MAX31865Pins pins()
{
    return MAX31865Pins{
        PIN_SCK,
        PIN_MISO,
        PIN_MOSI,
        PIN_CS,
        PIN_DRDY};
}

inline void beginSerial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t startedMs = millis();
    while (!Serial &&
           static_cast<uint32_t>(millis() - startedMs) < SERIAL_WAIT_MS)
    {
        delay(10U);
    }
}
} // namespace board
