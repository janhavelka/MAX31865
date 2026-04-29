#pragma once

#include <Arduino.h>

namespace cli_shell {

inline bool readLine(String& outLine) {
  static String buffer;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if ((c == '\r') || (c == '\n')) {
      if (buffer.length() == 0U) {
        continue;
      }
      outLine = buffer;
      outLine.trim();
      buffer = "";
      return true;
    }
    buffer += c;
  }
  return false;
}

}  // namespace cli_shell
