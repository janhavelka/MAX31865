/// @file Config.h
/// @brief Configuration types for MAX31865 driver.
#pragma once

#include <cstddef>
#include <cstdint>

#include "MAX31865/Status.h"

namespace MAX31865 {

/// SPI transaction callback.
///
/// The callback must perform one complete MAX31865 transaction, including CS
/// low/high framing, and transfer exactly len bytes full-duplex.
using SpiTransferFn = Status (*)(const uint8_t* txData, uint8_t* rxData,
                                 size_t len, uint32_t timeoutMs, void* user);

/// Optional monotonic millisecond source.
using NowMsFn = uint32_t (*)(void* user);

/// Optional blocking millisecond delay hook.
using DelayMsFn = void (*)(uint32_t ms, void* user);

/// Optional blocking microsecond delay hook.
using DelayUsFn = void (*)(uint32_t us, void* user);

/// Optional cooperative yield hook for blocking waits.
using YieldFn = void (*)(void* user);

/// Optional GPIO read hook, used for DRDY when available.
using GpioReadFn = bool (*)(int pin, void* user);

/// RTD wire topology. Hardware distinguishes only 3-wire vs 2/4-wire.
enum class WireMode : uint8_t {
  TWO_WIRE = 2,
  THREE_WIRE = 3,
  FOUR_WIRE = 4
};

/// Digital notch filter selection.
enum class Filter : uint8_t {
  HZ_60 = 0,
  HZ_50 = 1
};

/// Stable conversion mode.
enum class ConversionMode : uint8_t {
  NORMALLY_OFF = 0,
  AUTO = 1
};

/// Callendar-Van Dusen coefficients for RTD temperature conversion.
struct RtdCoefficients {
  float a = 3.90830e-3f;
  float b = -5.77500e-7f;
  float c = -4.18301e-12f;
};

/// Driver configuration.
struct Config {
  // === SPI transport (required) ===
  SpiTransferFn spiTransfer = nullptr;
  void* spiUser = nullptr;
  uint32_t spiTimeoutMs = 50;

  // === Timing hooks (optional) ===
  NowMsFn nowMs = nullptr;
  DelayMsFn delayMs = nullptr;
  DelayUsFn delayUs = nullptr;
  YieldFn cooperativeYield = nullptr;
  void* timeUser = nullptr;

  // === Optional DRDY hook ===
  GpioReadFn gpioRead = nullptr;
  void* gpioUser = nullptr;
  int drdyPin = -1;

  // === Sensor and board values ===
  float referenceResistorOhms = 400.0f;
  float rtdNominalOhms = 100.0f;
  RtdCoefficients coefficients{};
  uint32_t inputFilterTimeConstantUs = 0;

  // === Initial MAX31865 configuration ===
  WireMode wireMode = WireMode::FOUR_WIRE;
  Filter filter = Filter::HZ_60;
  bool enableBias = false;
  bool autoConvert = false;

  // === One-shot policy ===
  bool clearFaultsBeforeOneShot = true;
  bool disableBiasAfterOneShot = true;

  // === Health tracking ===
  uint8_t offlineThreshold = 5;
};

}  // namespace MAX31865
