/**
 * @file CommandTable.h
 * @brief MAX31865 register constants and low-level protocol values.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Low-level MAX31865 register, bit-mask, and timing constants.
 *
 * These constants are public so diagnostics and bringup tools can print and
 * edit registers without duplicating magic numbers. Application code normally
 * uses the MAX31865 class methods instead.
 */
namespace max31865_cmd {

/// Mask applied to the address byte for register reads.
static constexpr uint8_t READ_MASK = 0x7F;
/// Bit added to the address byte for register writes.
static constexpr uint8_t WRITE_BIT = 0x80;
/// Datasheet maximum SPI clock.
static constexpr uint32_t SPI_MAX_HZ = 5000000UL;
/// Recommended minimum reference resistor from the operating-condition table.
static constexpr float REFERENCE_RESISTOR_MIN_OHMS = 350.0f;
/// Recommended maximum reference resistor from the operating-condition table.
static constexpr float REFERENCE_RESISTOR_MAX_OHMS = 10000.0f;

/// Configuration register address.
static constexpr uint8_t REG_CONFIG = 0x00;
/// RTD data MSB register address.
static constexpr uint8_t REG_RTD_MSB = 0x01;
/// RTD data LSB register address.
static constexpr uint8_t REG_RTD_LSB = 0x02;
/// High fault threshold MSB register address.
static constexpr uint8_t REG_HIGH_FAULT_MSB = 0x03;
/// High fault threshold LSB register address.
static constexpr uint8_t REG_HIGH_FAULT_LSB = 0x04;
/// Low fault threshold MSB register address.
static constexpr uint8_t REG_LOW_FAULT_MSB = 0x05;
/// Low fault threshold LSB register address.
static constexpr uint8_t REG_LOW_FAULT_LSB = 0x06;
/// Fault status register address.
static constexpr uint8_t REG_FAULT_STATUS = 0x07;
/// Last documented register address.
static constexpr uint8_t REG_LAST = REG_FAULT_STATUS;

/// CONFIG register power-on value.
static constexpr uint8_t CONFIG_RESET = 0x00;
/// High fault threshold MSB power-on value.
static constexpr uint8_t HIGH_FAULT_MSB_RESET = 0xFF;
/// High fault threshold LSB power-on value.
static constexpr uint8_t HIGH_FAULT_LSB_RESET = 0xFF;
/// Low fault threshold MSB power-on value.
static constexpr uint8_t LOW_FAULT_MSB_RESET = 0x00;
/// Low fault threshold LSB power-on value.
static constexpr uint8_t LOW_FAULT_LSB_RESET = 0x00;

/// CONFIG bit: enable VBIAS.
static constexpr uint8_t CONFIG_BIAS = 0x80;
/// CONFIG bit: enable automatic conversion.
static constexpr uint8_t CONFIG_AUTO = 0x40;
/// CONFIG bit: start one-shot conversion.
static constexpr uint8_t CONFIG_ONE_SHOT = 0x20;
/// CONFIG bit: enable 3-wire RTD compensation.
static constexpr uint8_t CONFIG_3WIRE = 0x10;
/// CONFIG mask: fault-detection cycle field.
static constexpr uint8_t CONFIG_FAULT_CYCLE_MASK = 0x0C;
/// CONFIG fault-cycle field: no active cycle.
static constexpr uint8_t CONFIG_FAULT_CYCLE_NONE = 0x00;
/// CONFIG fault-cycle field: automatic fault detection.
static constexpr uint8_t CONFIG_FAULT_CYCLE_AUTO = 0x04;
/// CONFIG fault-cycle field: manual fault detection step 1.
static constexpr uint8_t CONFIG_FAULT_CYCLE_MANUAL_1 = 0x08;
/// CONFIG fault-cycle field: manual fault detection step 2.
static constexpr uint8_t CONFIG_FAULT_CYCLE_MANUAL_2 = 0x0C;
/// CONFIG bit: clear latched fault status.
static constexpr uint8_t CONFIG_FAULT_CLEAR = 0x02;
/// CONFIG bit: select 50 Hz notch filter when set; 60 Hz when clear.
static constexpr uint8_t CONFIG_FILTER_50HZ = 0x01;

/// Maximum 15-bit RTD ADC code.
static constexpr uint16_t ADC_CODE_MAX = 0x7FFF;
/// RTD ADC full-scale denominator.
static constexpr uint32_t ADC_FULL_SCALE = 32768UL;
/// Raw RTD register bit that indicates a fault.
static constexpr uint16_t RTD_FAULT_BIT = 0x0001;

/// Fault status bit: high threshold.
static constexpr uint8_t FAULT_HIGH_THRESHOLD = 0x80;
/// Fault status bit: low threshold.
static constexpr uint8_t FAULT_LOW_THRESHOLD = 0x40;
/// Fault status bit: REFIN- high.
static constexpr uint8_t FAULT_REFIN_HIGH = 0x20;
/// Fault status bit: REFIN- low.
static constexpr uint8_t FAULT_REFIN_LOW = 0x10;
/// Fault status bit: RTDIN- low.
static constexpr uint8_t FAULT_RTDIN_LOW = 0x08;
/// Fault status bit: overvoltage/undervoltage.
static constexpr uint8_t FAULT_OVER_UNDER_VOLTAGE = 0x04;
/// Mask of all documented fault status bits.
static constexpr uint8_t FAULT_DEFINED_MASK = 0xFC;

/// Maximum 60 Hz one-shot conversion time, milliseconds.
static constexpr uint32_t SINGLE_CONVERSION_60HZ_MS = 55;
/// Maximum 50 Hz one-shot conversion time, milliseconds.
static constexpr uint32_t SINGLE_CONVERSION_50HZ_MS = 66;
/// Continuous conversion cadence at 60 Hz filter, milliseconds.
static constexpr uint32_t CONTINUOUS_CONVERSION_60HZ_MS = 18;
/// Continuous conversion cadence at 50 Hz filter, milliseconds.
static constexpr uint32_t CONTINUOUS_CONVERSION_50HZ_MS = 21;
/// Maximum automatic fault-detection cycle time, microseconds.
static constexpr uint32_t AUTO_FAULT_DETECTION_MAX_US = 600;
/// Conservative delay used between manual fault-cycle steps, microseconds.
static constexpr uint32_t MANUAL_FAULT_STEP_SETTLE_US = 100;
/// Extra fixed VBIAS settle delay after input-filter settling, microseconds.
static constexpr uint32_t BIAS_SETTLE_EXTRA_US = 1000;
/// Datasheet input-filter settling multiplier after enabling VBIAS.
static constexpr float BIAS_SETTLE_TIME_CONSTANTS = 10.5f;

}  // namespace max31865_cmd
