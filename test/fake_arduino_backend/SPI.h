#pragma once

#include <stdint.h>

static constexpr uint8_t MSBFIRST = 1U;
static constexpr uint8_t SPI_MODE1 = 1U;
static constexpr uint8_t SPI_MODE3 = 3U;

struct SPISettings
{
    uint32_t clockHz;
    uint8_t bitOrder;
    uint8_t dataMode;

    SPISettings(
        uint32_t clock = 1000000U,
        uint8_t order = MSBFIRST,
        uint8_t mode = SPI_MODE1)
        : clockHz(clock), bitOrder(order), dataMode(mode)
    {
    }
};

void fakeArduinoBeginTransaction(
    const void *spi,
    uint32_t clock,
    uint8_t bitOrder,
    uint8_t mode);
void fakeArduinoEndTransaction(const void *spi);
void fakeArduinoTransfer(
    const void *spi,
    const uint8_t *tx,
    uint8_t *rx,
    uint32_t length);

class SPIClass
{
public:
    void beginTransaction(const SPISettings &settings)
    {
        fakeArduinoBeginTransaction(
            this,
            settings.clockHz,
            settings.bitOrder,
            settings.dataMode);
    }

    void endTransaction()
    {
        fakeArduinoEndTransaction(this);
    }

    void transferBytes(
        const uint8_t *tx,
        uint8_t *rx,
        uint32_t length)
    {
        fakeArduinoTransfer(this, tx, rx, length);
    }
};
