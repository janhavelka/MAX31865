#pragma once

#include <cstdint>

#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1

class SerialClass {
public:
  void begin(unsigned long) {}
  void println(const char*) {}
  void println() {}
  void print(const char*) {}
  template <typename... Args>
  void printf(const char*, Args...) {}
  int available() { return 0; }
  int read() { return -1; }
};

extern SerialClass Serial;

inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}
inline void yield() {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; }
