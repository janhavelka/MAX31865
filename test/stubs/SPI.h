#pragma once

#include <cstdint>

#define MSBFIRST 1
#define SPI_MODE1 1

class SPISettings {
public:
  SPISettings(uint32_t = 1000000UL, uint8_t = MSBFIRST, uint8_t = SPI_MODE1) {}
};

class SPIClass {
public:
  void begin(int = -1, int = -1, int = -1, int = -1) {}
  void beginTransaction(const SPISettings&) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t value) { return value; }
};

extern SPIClass SPI;
