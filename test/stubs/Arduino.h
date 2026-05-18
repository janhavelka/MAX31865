#pragma once

#include <math.h>
#include <stdint.h>

static constexpr uint8_t LOW = 0;
static constexpr uint8_t HIGH = 1;
static constexpr uint8_t INPUT = 0;
static constexpr uint8_t OUTPUT = 1;

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline uint32_t millis() { return 0U; }
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}
inline void yield() {}
