/**
 * @file CommandTable.h
 * @brief Authoritative MAX31865 register, field, protocol, and timing constants.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Low-level values from MAX31865 data sheet Rev. 3.
 *
 * Application code normally uses the typed driver API. These constants remain
 * public for diagnostics, independent protocol tests, and register displays.
 */
namespace max31865_cmd
{
/** Address-byte mask for a register read. */
static constexpr uint8_t READ_MASK = 0x7FU;
/** Address-byte bit selecting a register write. */
static constexpr uint8_t WRITE_BIT = 0x80U;
/** Highest supported serial-clock frequency. */
static constexpr uint32_t SPI_MAX_HZ = 5000000UL;
/** Number of documented byte registers. */
static constexpr size_t NUM_REGISTERS = 8U;
/** Longest address-plus-register-range SPI transfer. */
static constexpr size_t MAX_FRAME_BYTES = NUM_REGISTERS + 1U;

/** Configuration register address. */
static constexpr uint8_t REG_CONFIG = 0x00U;
/** RTD resistance data MSB address. */
static constexpr uint8_t REG_RTD_MSB = 0x01U;
/** RTD resistance data LSB/fault address. */
static constexpr uint8_t REG_RTD_LSB = 0x02U;
/** High fault-threshold MSB address. */
static constexpr uint8_t REG_HIGH_FAULT_MSB = 0x03U;
/** High fault-threshold LSB address. */
static constexpr uint8_t REG_HIGH_FAULT_LSB = 0x04U;
/** Low fault-threshold MSB address. */
static constexpr uint8_t REG_LOW_FAULT_MSB = 0x05U;
/** Low fault-threshold LSB address. */
static constexpr uint8_t REG_LOW_FAULT_LSB = 0x06U;
/** Latched fault-status register address. */
static constexpr uint8_t REG_FAULT_STATUS = 0x07U;
/** Last documented register address. */
static constexpr uint8_t REG_LAST = REG_FAULT_STATUS;

/** Configuration-register power-on value. */
static constexpr uint8_t CONFIG_RESET = 0x00U;
/** RTD MSB power-on value. */
static constexpr uint8_t RTD_MSB_RESET = 0x00U;
/** RTD LSB power-on value. */
static constexpr uint8_t RTD_LSB_RESET = 0x00U;
/** High fault-threshold MSB power-on value. */
static constexpr uint8_t HIGH_FAULT_MSB_RESET = 0xFFU;
/** High fault-threshold LSB power-on value. */
static constexpr uint8_t HIGH_FAULT_LSB_RESET = 0xFFU;
/** Low fault-threshold MSB power-on value. */
static constexpr uint8_t LOW_FAULT_MSB_RESET = 0x00U;
/** Low fault-threshold LSB power-on value. */
static constexpr uint8_t LOW_FAULT_LSB_RESET = 0x00U;
/** Fault-status power-on value. */
static constexpr uint8_t FAULT_STATUS_RESET = 0x00U;

/** CONFIG D7: enable VBIAS. */
static constexpr uint8_t CONFIG_BIAS = 0x80U;
/** CONFIG D6: automatic continuous conversion. */
static constexpr uint8_t CONFIG_AUTO = 0x40U;
/** CONFIG D5: trigger a one-shot conversion; self-clearing. */
static constexpr uint8_t CONFIG_ONE_SHOT = 0x20U;
/** CONFIG D4: enable three-wire compensation. */
static constexpr uint8_t CONFIG_3WIRE = 0x10U;
/** CONFIG D3:D2: fault-detection-cycle field. */
static constexpr uint8_t CONFIG_FAULT_CYCLE_MASK = 0x0CU;
/** CONFIG D3:D2: fault detection finished/no action. */
static constexpr uint8_t CONFIG_FAULT_CYCLE_NONE = 0x00U;
/** CONFIG D3:D2: automatic fault-detection cycle. */
static constexpr uint8_t CONFIG_FAULT_CYCLE_AUTO = 0x04U;
/** CONFIG D3:D2: manual fault-detection cycle 1. */
static constexpr uint8_t CONFIG_FAULT_CYCLE_MANUAL_1 = 0x08U;
/** CONFIG D3:D2: manual fault-detection cycle 2. */
static constexpr uint8_t CONFIG_FAULT_CYCLE_MANUAL_2 = 0x0CU;
/** CONFIG D1: clear latched fault status; self-clearing. */
static constexpr uint8_t CONFIG_FAULT_CLEAR = 0x02U;
/** CONFIG D0: select the 50 Hz notch; clear selects 60 Hz. */
static constexpr uint8_t CONFIG_FILTER_50HZ = 0x01U;
/** Persistent, readback-stable CONFIG fields. */
static constexpr uint8_t CONFIG_PERSISTENT_MASK =
    CONFIG_BIAS | CONFIG_AUTO | CONFIG_3WIRE | CONFIG_FILTER_50HZ;
/** Self-clearing or transient CONFIG command fields. */
static constexpr uint8_t CONFIG_COMMAND_MASK =
    CONFIG_ONE_SHOT | CONFIG_FAULT_CYCLE_MASK | CONFIG_FAULT_CLEAR;

/** Highest valid 15-bit RTD/threshold code. */
static constexpr uint16_t ADC_CODE_MAX = 0x7FFFU;
/** Denominator used by the RTD/reference resistance ratio. */
static constexpr uint32_t ADC_FULL_SCALE = 32768UL;
/** RTD data-register D0 fault indicator. */
static constexpr uint16_t RTD_FAULT_BIT = 0x0001U;
/** Threshold-register LSB bits with defined readback behavior. */
static constexpr uint8_t THRESHOLD_LSB_DEFINED_MASK = 0xFEU;

/** FAULT_STATUS D7: RTD high-threshold fault. */
static constexpr uint8_t FAULT_HIGH_THRESHOLD = 0x80U;
/** FAULT_STATUS D6: RTD low-threshold fault. */
static constexpr uint8_t FAULT_LOW_THRESHOLD = 0x40U;
/** FAULT_STATUS D5: REFIN- greater than 0.85 x VBIAS. */
static constexpr uint8_t FAULT_REFIN_HIGH = 0x20U;
/** FAULT_STATUS D4: REFIN- low while FORCE- is open. */
static constexpr uint8_t FAULT_REFIN_LOW = 0x10U;
/** FAULT_STATUS D3: RTDIN- low while FORCE- is open. */
static constexpr uint8_t FAULT_RTDIN_LOW = 0x08U;
/** FAULT_STATUS D2: protected-input over/undervoltage. */
static constexpr uint8_t FAULT_OVER_UNDER_VOLTAGE = 0x04U;
/** Mask of every documented fault-status bit; D1:D0 are don't-care. */
static constexpr uint8_t FAULT_DEFINED_MASK = 0xFCU;

/** Minimum recommended external reference resistance. */
static constexpr float REFERENCE_RESISTOR_MIN_OHMS = 350.0F;
/** Maximum recommended external reference resistance. */
static constexpr float REFERENCE_RESISTOR_MAX_OHMS = 10000.0F;
/** Minimum platinum RTD nominal resistance stated by the data sheet. */
static constexpr float RTD_NOMINAL_MIN_OHMS = 100.0F;
/** Maximum platinum RTD nominal resistance stated by the data sheet. */
static constexpr float RTD_NOMINAL_MAX_OHMS = 1000.0F;

/** Maximum one-shot conversion time with the 60 Hz notch. */
static constexpr uint32_t SINGLE_CONVERSION_60HZ_MS = 55U;
/** Maximum one-shot conversion time with the 50 Hz notch. */
static constexpr uint32_t SINGLE_CONVERSION_50HZ_MS = 66U;
/** Maximum continuous conversion period with the 60 Hz notch. */
static constexpr uint32_t CONTINUOUS_CONVERSION_60HZ_MS = 18U;
/** Maximum continuous conversion period with the 50 Hz notch. */
static constexpr uint32_t CONTINUOUS_CONVERSION_50HZ_MS = 21U;
/** Maximum automatic fault-detection-cycle duration. */
static constexpr uint32_t AUTO_FAULT_DETECTION_MAX_US = 600U;
/** Duration of one documented internal manual-fault comparison phase. */
static constexpr uint32_t MANUAL_FAULT_PHASE_US = 100U;
/** Two internal phases before FORCE- is open after manual step 1. */
static constexpr uint32_t MANUAL_FAULT_STEP1_TO_OPEN_US = 200U;
/** Two internal comparison phases performed by manual step 2. */
static constexpr uint32_t MANUAL_FAULT_STEP2_MAX_US = 200U;
/** Automatic fault timing supports at most this external RC time constant. */
static constexpr uint32_t AUTO_FAULT_MAX_RC_US = 100U;
/** External-settling multiplier required between manual fault phases. */
static constexpr uint32_t MANUAL_FAULT_SETTLE_MULTIPLIER = 5U;
/** Additional one-shot settling delay after 10.5 external RC constants. */
static constexpr uint32_t BIAS_SETTLE_EXTRA_US = 1000U;
/** Numerator for the exact 10.5 RC-time-constant settling multiplier. */
static constexpr uint32_t BIAS_SETTLE_MULTIPLIER_NUMERATOR = 21U;
/** Denominator for the exact 10.5 RC-time-constant settling multiplier. */
static constexpr uint32_t BIAS_SETTLE_MULTIPLIER_DENOMINATOR = 2U;

/** Conservative CS-to-SCLK setup delay (data-sheet minimum 400 ns). */
static constexpr uint32_t CS_SETUP_DELAY_US = 1U;
/** Conservative final-SCLK-to-CS hold delay (data-sheet minimum 100 ns). */
static constexpr uint32_t CS_HOLD_DELAY_US = 1U;
/** Conservative CS-high inactive interval (data-sheet minimum 400 ns). */
static constexpr uint32_t CS_INACTIVE_DELAY_US = 1U;
} // namespace max31865_cmd
