#pragma once

#include <Arduino.h>

#include "CliStyle.h"

namespace bus_diag {

inline void printDrdyLevel(int drdyPin) {
  if (drdyPin < 0) {
    Serial.printf("DRDY: %snot connected%s\n", cli::kYellow, cli::kReset);
    return;
  }
  const bool high = (digitalRead(drdyPin) != LOW);
  Serial.printf("DRDY pin %d level: %s%s%s\n",
                drdyPin,
                high ? cli::kYellow : cli::kGreen,
                high ? "HIGH" : "LOW",
                cli::kReset);
}

inline void printControlLevel(const char* name, int pin, bool activeLow) {
  if (pin < 0) {
    Serial.printf("%s: %snot connected%s\n", name, cli::kYellow, cli::kReset);
    return;
  }
  const bool high = (digitalRead(pin) != LOW);
  const bool asserted = activeLow ? !high : high;
  Serial.printf("%s pin %d: level=%s asserted=%s%s%s\n",
                name,
                pin,
                high ? "HIGH" : "LOW",
                cli::yesNoColor(asserted),
                asserted ? "yes" : "no",
                cli::kReset);
}

}  // namespace bus_diag
