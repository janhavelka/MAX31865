#pragma once

#include <Arduino.h>

#include "CliStyle.h"

namespace transport_adapter {

struct SpiPins {
  int sck;
  int miso;
  int mosi;
  int cs;
  int drdy;
  int reset;
  int start;
  int pwdn;
};

inline void printPins(const SpiPins& pins) {
  Serial.println("=== SPI Wiring ===");
  Serial.printf("  SCK=%d MISO=%d MOSI=%d CS=%d\n",
                pins.sck, pins.miso, pins.mosi, pins.cs);
  Serial.printf("  DRDY=%s%d%s RESET=%s%d%s START=%s%d%s PWDN=%s%d%s\n",
                (pins.drdy >= 0) ? cli::kGreen : cli::kYellow, pins.drdy, cli::kReset,
                (pins.reset >= 0) ? cli::kGreen : cli::kYellow, pins.reset, cli::kReset,
                (pins.start >= 0) ? cli::kGreen : cli::kYellow, pins.start, cli::kReset,
                (pins.pwdn >= 0) ? cli::kGreen : cli::kYellow, pins.pwdn, cli::kReset);
}

}  // namespace transport_adapter
