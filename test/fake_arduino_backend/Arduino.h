#pragma once

#include <stdint.h>

static constexpr uint8_t LOW = 0U;
static constexpr uint8_t HIGH = 1U;
static constexpr uint8_t INPUT = 0U;
static constexpr uint8_t OUTPUT = 1U;

#if defined(CONFIG_IDF_TARGET_ESP32S2)
static constexpr int GPIO_PIN_COUNT = 47;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
static constexpr int GPIO_PIN_COUNT = 49;
#else
#error Select one fake ESP32-S2/S3 target
#endif

static constexpr int SOC_GPIO_PIN_COUNT = GPIO_PIN_COUNT;

inline bool digitalPinIsValid(int pin)
{
    return (pin >= 0) && (pin < GPIO_PIN_COUNT) &&
           !((pin >= 22) && (pin <= 25));
}

inline bool digitalPinCanOutput(int pin)
{
#if defined(CONFIG_IDF_TARGET_ESP32S2)
    return digitalPinIsValid(pin) && (pin != 46);
#else
    return digitalPinIsValid(pin);
#endif
}

void pinMode(int pin, int mode);
void digitalWrite(int pin, int level);
int digitalRead(int pin);
uint32_t millis();
uint32_t micros();
void delay(uint32_t milliseconds);
void delayMicroseconds(uint32_t microseconds);
