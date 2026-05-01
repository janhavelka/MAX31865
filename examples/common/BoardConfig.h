/**
 * @file BoardConfig.h
 * @brief Example board defaults for MAX31865 bringup CLI.
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "MAX31865/MAX31865.h"

namespace board {

static constexpr int PIN_SCK = 12;
static constexpr int PIN_MISO = 13;
static constexpr int PIN_MOSI = 11;
static constexpr int PIN_CS = 10;
static constexpr int PIN_DRDY = -1;

static constexpr uint32_t SERIAL_BAUD = 115200U;
static constexpr uint32_t SERIAL_WAIT_MS = 3000U;

inline MAX31865Pins rtdPins() {
  MAX31865Pins pins;
  pins.sck = PIN_SCK;
  pins.miso = PIN_MISO;
  pins.mosi = PIN_MOSI;
  pins.cs = PIN_CS;
  pins.drdy = PIN_DRDY;
  return pins;
}

inline void initSerial() {
  Serial.begin(SERIAL_BAUD);
  const uint32_t start = millis();
  while (!Serial && ((millis() - start) < SERIAL_WAIT_MS)) {
    delay(10);
  }
}

}  // namespace board
