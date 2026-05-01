/**
 * @file Config.h
 * @brief Configuration types for the MAX31865 driver.
 */

#pragma once

#include <SPI.h>
#include <stdint.h>

#ifndef MAX31865_DEFAULT_SPI_HZ
/// Default SPI clock used when MAX31865BeginConfig::spiHz is zero.
#define MAX31865_DEFAULT_SPI_HZ 1000000U
#endif

#ifndef MAX31865_DEFAULT_OFFLINE_THRESHOLD
/// Consecutive tracked failures before healthState() becomes OFFLINE.
#define MAX31865_DEFAULT_OFFLINE_THRESHOLD 5
#endif

#ifndef MAX31865_SPI_LOCK_TIMEOUT_MS
/// Default timeout for serializing SPI transactions on ESP32.
#define MAX31865_SPI_LOCK_TIMEOUT_MS 50
#endif

/**
 * @brief RTD wiring compensation mode.
 *
 * The MAX31865 register has one explicit bit for 3-wire mode. Two-wire and
 * four-wire operation both use the cleared register bit; the driver keeps the
 * requested 2-wire/4-wire distinction for diagnostics and UI reporting.
 */
enum class MAX31865WireMode : uint8_t {
    TwoWire = 2,   ///< 2-wire RTD connection; application compensates lead resistance.
    ThreeWire = 3, ///< 3-wire lead compensation enabled in the MAX31865.
    FourWire = 4   ///< 4-wire Kelvin RTD connection.
};

/**
 * @brief Digital notch-filter selection.
 */
enum class MAX31865Filter : uint8_t {
    Hz60 = 0, ///< 60 Hz rejection, shorter conversion time.
    Hz50 = 1  ///< 50 Hz rejection, longer conversion time.
};

/**
 * @brief Physical MCU pin assignment for one MAX31865 device.
 */
typedef struct MAX31865Pins {
    int sck;  ///< SPI SCLK pin.
    int miso; ///< SPI MISO pin connected to MAX31865 SDO.
    int mosi; ///< SPI MOSI pin connected to MAX31865 SDI.
    int cs;   ///< MAX31865 /CS pin. Required.
    int drdy; ///< Optional MAX31865 /DRDY pin, or -1 when not wired.
} MAX31865Pins;

/**
 * @brief Callendar-Van Dusen coefficients used by resistanceToTemperature().
 */
typedef struct MAX31865RtdCoefficients {
    float a; ///< Linear coefficient, normally 3.90830e-3 for IEC 60751 platinum.
    float b; ///< Quadratic coefficient, normally -5.77500e-7.
    float c; ///< Negative-temperature coefficient, normally -4.18301e-12.
} MAX31865RtdCoefficients;

/**
 * @brief begin() configuration bundle.
 */
typedef struct MAX31865BeginConfig {
    SPIClass* spi;                   ///< SPI bus object. Required.
    MAX31865Pins pins;               ///< Physical wiring.
    uint32_t spiHz;                  ///< SPI clock in Hz. Zero selects MAX31865_DEFAULT_SPI_HZ.
    bool verifyProbe;                ///< Run probe() after initial register configuration.
    float referenceResistorOhms;     ///< Precision reference resistor value used by the board.
    float rtdNominalOhms;            ///< Nominal RTD value at 0 C, for example 100 or 1000.
    uint32_t inputFilterTimeConstantUs; ///< External input-filter RC time constant in microseconds.
    MAX31865WireMode wireMode;       ///< RTD wiring mode.
    MAX31865Filter filter;           ///< Digital notch-filter selection.
    bool useCustomCoefficients;      ///< Use coefficients instead of IEC 60751 defaults when true.
    MAX31865RtdCoefficients coefficients; ///< Optional custom RTD coefficients.
} MAX31865BeginConfig;

/**
 * @brief Fault thresholds expressed as 15-bit RTD ADC codes.
 */
typedef struct MAX31865FaultThresholds {
    uint16_t low_code;  ///< Low fault threshold, 0..32767.
    uint16_t high_code; ///< High fault threshold, 0..32767.
} MAX31865FaultThresholds;

/**
 * @brief Snapshot of current configuration decoded from registers and cached settings.
 */
typedef struct MAX31865Settings {
    uint8_t config_register;                ///< Raw CONFIG register.
    MAX31865WireMode wire_mode;             ///< Decoded/cached wire mode.
    MAX31865Filter filter;                  ///< Decoded filter mode.
    bool bias_enabled;                      ///< VBIAS bit state.
    bool auto_convert;                      ///< Auto-conversion bit state.
    bool one_shot;                          ///< One-shot bit state at snapshot time.
    uint8_t fault_cycle;                    ///< Raw fault-cycle field from CONFIG[3:2].
    MAX31865FaultThresholds thresholds;     ///< Decoded threshold ADC codes.
    float low_threshold_ohms;               ///< Low threshold converted to ohms.
    float high_threshold_ohms;              ///< High threshold converted to ohms.
    float low_threshold_c;                  ///< Low threshold converted to Celsius.
    float high_threshold_c;                 ///< High threshold converted to Celsius.
} MAX31865Settings;
