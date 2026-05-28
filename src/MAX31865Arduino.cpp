#include "MAX31865/MAX31865.h"

#if MAX31865_HAS_ARDUINO_BACKEND

#include <Arduino.h>
#include <SPI.h>

bool MAX31865::begin(SPIClass& spi, const MAX31865BeginConfig& config) {
    MAX31865BeginConfig arduinoConfig = config;
    arduinoConfig.transport = MAX31865TransportConfig{};
    return beginInternal(arduinoConfig, &spi);
}

bool MAX31865::begin(SPIClass& spi,
                     int sckPin,
                     int misoPin,
                     int mosiPin,
                     int csPin,
                     int drdyPin,
                     uint32_t spiHz) {
    MAX31865BeginConfig config{};
    config.pins = {sckPin, misoPin, mosiPin, csPin, drdyPin};
    config.spiHz = spiHz;
    config.verifyProbe = true;
    config.referenceResistorOhms = 400.0f;
    config.rtdNominalOhms = 100.0f;
    config.inputFilterTimeConstantUs = 0;
    config.wireMode = MAX31865WireMode::FourWire;
    config.filter = MAX31865Filter::Hz60;
    return beginInternal(config, &spi);
}

bool MAX31865::beginArduinoBackend(const MAX31865BeginConfig& config, void* arduinoSpi) {
    if (arduinoSpi == nullptr) {
        setFault(MAX31865Error::InvalidConfig);
        return false;
    }

    SPIClass* spi = static_cast<SPIClass*>(arduinoSpi);
    spi->begin(config.pins.sck, config.pins.miso, config.pins.mosi, config.pins.cs);
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    if (_drdyPin >= 0) {
        pinMode(_drdyPin, INPUT);
    }
    return true;
}

bool MAX31865::transferArduinoBackend(const uint8_t* tx, uint8_t* rx, size_t len) {
    if (_arduinoSpi == nullptr || _csPin < 0) {
        return false;
    }

    SPIClass* spi = static_cast<SPIClass*>(_arduinoSpi);
    SPISettings settings(_spiHz, MSBFIRST, SPI_MODE1);
    spi->beginTransaction(settings);
    digitalWrite(_csPin, LOW);
    for (size_t i = 0; i < len; ++i) {
        rx[i] = spi->transfer(tx[i]);
    }
    digitalWrite(_csPin, HIGH);
    spi->endTransaction();
    return true;
}

uint32_t MAX31865::arduinoNowMs() const {
    return millis();
}

void MAX31865::arduinoDelayMs(uint32_t ms) const {
    delay(ms);
}

void MAX31865::arduinoDelayUs(uint32_t us) const {
    delayMicroseconds(us);
}

void MAX31865::arduinoYield() const {
    yield();
}

bool MAX31865::arduinoReadDrdyReady() const {
    return _drdyPin >= 0 && digitalRead(_drdyPin) == LOW;
}

#endif  // MAX31865_HAS_ARDUINO_BACKEND
