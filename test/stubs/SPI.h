#pragma once

#include <stdint.h>

static constexpr uint8_t MSBFIRST = 1;
static constexpr uint8_t SPI_MODE1 = 1;

class SPISettings {
public:
    SPISettings(uint32_t = 1000000U, uint8_t = MSBFIRST, uint8_t = SPI_MODE1) {}
};

class SPIClass {
public:
    void begin(int = -1, int = -1, int = -1, int = -1) {}
    void beginTransaction(const SPISettings&) {}
    uint8_t transfer(uint8_t value) { return value; }
    void endTransaction() {}
};
