#pragma once

#include <Arduino.h>

namespace build_config {

inline void printBanner(const char* name) {
  Serial.println();
  Serial.println(name);
  Serial.print("Version: ");
  Serial.println(MAX31865::VERSION_FULL);
}

}  // namespace build_config
