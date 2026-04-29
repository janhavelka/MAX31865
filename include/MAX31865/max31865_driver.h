/**
 * @file max31865_driver.h
 * @brief MAX31865 register constants and low-level protocol values.
 */

#pragma once

#include <stdint.h>

namespace max31865_cmd {

static constexpr uint8_t READ_MASK = 0x7F;
static constexpr uint8_t WRITE_BIT = 0x80;
static constexpr uint32_t SPI_MAX_HZ = 5000000UL;

static constexpr uint8_t REG_CONFIG = 0x00;
static constexpr uint8_t REG_RTD_MSB = 0x01;
static constexpr uint8_t REG_RTD_LSB = 0x02;
static constexpr uint8_t REG_HIGH_FAULT_MSB = 0x03;
static constexpr uint8_t REG_HIGH_FAULT_LSB = 0x04;
static constexpr uint8_t REG_LOW_FAULT_MSB = 0x05;
static constexpr uint8_t REG_LOW_FAULT_LSB = 0x06;
static constexpr uint8_t REG_FAULT_STATUS = 0x07;
static constexpr uint8_t REG_LAST = REG_FAULT_STATUS;

static constexpr uint8_t CONFIG_RESET = 0x00;
static constexpr uint8_t HIGH_FAULT_MSB_RESET = 0xFF;
static constexpr uint8_t HIGH_FAULT_LSB_RESET = 0xFF;
static constexpr uint8_t LOW_FAULT_MSB_RESET = 0x00;
static constexpr uint8_t LOW_FAULT_LSB_RESET = 0x00;

static constexpr uint8_t CONFIG_BIAS = 0x80;
static constexpr uint8_t CONFIG_AUTO = 0x40;
static constexpr uint8_t CONFIG_ONE_SHOT = 0x20;
static constexpr uint8_t CONFIG_3WIRE = 0x10;
static constexpr uint8_t CONFIG_FAULT_CYCLE_MASK = 0x0C;
static constexpr uint8_t CONFIG_FAULT_CYCLE_NONE = 0x00;
static constexpr uint8_t CONFIG_FAULT_CYCLE_AUTO = 0x04;
static constexpr uint8_t CONFIG_FAULT_CYCLE_MANUAL_1 = 0x08;
static constexpr uint8_t CONFIG_FAULT_CYCLE_MANUAL_2 = 0x0C;
static constexpr uint8_t CONFIG_FAULT_CLEAR = 0x02;
static constexpr uint8_t CONFIG_FILTER_50HZ = 0x01;

static constexpr uint16_t ADC_CODE_MAX = 0x7FFF;
static constexpr uint32_t ADC_FULL_SCALE = 32768UL;
static constexpr uint16_t RTD_FAULT_BIT = 0x0001;

static constexpr uint8_t FAULT_HIGH_THRESHOLD = 0x80;
static constexpr uint8_t FAULT_LOW_THRESHOLD = 0x40;
static constexpr uint8_t FAULT_REFIN_HIGH = 0x20;
static constexpr uint8_t FAULT_REFIN_LOW = 0x10;
static constexpr uint8_t FAULT_RTDIN_LOW = 0x08;
static constexpr uint8_t FAULT_OVER_UNDER_VOLTAGE = 0x04;
static constexpr uint8_t FAULT_DEFINED_MASK = 0xFC;

static constexpr uint32_t SINGLE_CONVERSION_60HZ_MS = 55;
static constexpr uint32_t SINGLE_CONVERSION_50HZ_MS = 66;
static constexpr uint32_t CONTINUOUS_CONVERSION_60HZ_MS = 18;
static constexpr uint32_t CONTINUOUS_CONVERSION_50HZ_MS = 21;
static constexpr uint32_t AUTO_FAULT_DETECTION_MAX_US = 600;
static constexpr uint32_t MANUAL_FAULT_STEP_SETTLE_US = 100;
static constexpr uint32_t BIAS_SETTLE_EXTRA_US = 1000;
static constexpr float BIAS_SETTLE_TIME_CONSTANTS = 10.5f;

}  // namespace max31865_cmd
