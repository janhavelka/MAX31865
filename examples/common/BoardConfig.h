#pragma once

namespace board {

struct SpiPins {
  int sck;
  int miso;
  int mosi;
  int cs;
  int drdy;
};

inline SpiPins max31865Pins() {
  return {
      12,  // SCK
      13,  // MISO / SDO
      11,  // MOSI / SDI
      10,  // CS
      -1   // DRDY, set to a GPIO if wired
  };
}

}  // namespace board
