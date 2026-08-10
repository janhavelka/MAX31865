/**
 * @file Config.h
 * @brief Framework-neutral MAX31865 configuration and diagnostics types.
 */

#pragma once

#include <stdint.h>

#include "MAX31865/CommandTable.h"
#include "MAX31865/Transport.h"

/** @brief RTD wiring compensation selection. */
enum class MAX31865WireMode : uint8_t
{
    TwoWire = 2, ///< 2-wire connection; lead resistance is application-owned.
    ThreeWire = 3, ///< Enable MAX31865 three-wire lead compensation.
    FourWire = 4 ///< 4-wire Kelvin connection.
};

/** @brief Digital sinc-notch selection. */
enum class MAX31865Filter : uint8_t
{
    Hz60 = 0, ///< Reject 60 Hz and harmonics.
    Hz50 = max31865_cmd::CONFIG_FILTER_50HZ ///< Reject 50 Hz and harmonics.
};

/** @brief Current ADC conversion mode decoded from CONFIG D6. */
enum class MAX31865ConversionMode : uint8_t
{
    NormallyOff = 0, ///< ADC normally off; one-shot commands remain available.
    Continuous = max31865_cmd::CONFIG_AUTO ///< Automatic continuous conversion.
};

/** @brief CONFIG D3:D2 fault-cycle state/command. */
enum class MAX31865FaultCycle : uint8_t
{
    None = max31865_cmd::CONFIG_FAULT_CYCLE_NONE,
    Automatic = max31865_cmd::CONFIG_FAULT_CYCLE_AUTO,
    ManualStep1 = max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1,
    ManualStep2 = max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2
};

/** @brief Sentinel accepted for an optional Arduino backend pin. */
static constexpr int32_t MAX31865_PIN_UNUSED = -1;

/**
 * @brief Arduino-backend wiring supplied and initialized by the application.
 *
 * SCK, MISO, MOSI, and chipSelect are required. dataReady is optional. The
 * core driver itself has no physical-pin concept and consumes only transport
 * callbacks.
 */
struct MAX31865Pins
{
    int32_t sck = MAX31865_PIN_UNUSED;
    int32_t miso = MAX31865_PIN_UNUSED;
    int32_t mosi = MAX31865_PIN_UNUSED;
    int32_t chipSelect = MAX31865_PIN_UNUSED;
    int32_t dataReady = MAX31865_PIN_UNUSED;
};

/** @brief Callendar-Van Dusen coefficients for a platinum RTD curve. */
struct MAX31865RtdCoefficients
{
    float a; ///< Linear coefficient.
    float b; ///< Quadratic coefficient.
    float c; ///< Negative-temperature coefficient.
};

/** @brief RTD/reference scaling and finite conversion-domain configuration. */
struct MAX31865RtdConfig
{
    float referenceResistorOhms; ///< Measured external RREF value.
    float nominalResistanceOhms; ///< RTD resistance at 0 degrees Celsius.
    MAX31865RtdCoefficients coefficients; ///< Callendar-Van Dusen coefficients.
    uint32_t inputFilterTimeConstantUs; ///< External RTDIN RC time constant.
    float minimumTemperatureC; ///< Lowest accepted inversion temperature.
    float maximumTemperatureC; ///< Highest accepted conversion temperature.
};

/** @brief Low/high fault thresholds represented as 15-bit ADC codes. */
struct MAX31865FaultThresholds
{
    uint16_t lowCode; ///< Low threshold in 0..32767.
    uint16_t highCode; ///< High threshold in 0..32767.
};

/** @brief Complete persistent writable device configuration. */
struct MAX31865DeviceConfig
{
    MAX31865WireMode wireMode; ///< Two-, three-, or four-wire selection.
    MAX31865Filter filter; ///< 50/60 Hz notch selection.
    bool biasEnabled; ///< Desired VBIAS state while no conversion is active.
    MAX31865FaultThresholds thresholds; ///< Persistent raw fault thresholds.
};

/** @brief Borrowed transport, startup image, scaling, and bounded defaults. */
struct MAX31865BeginConfig
{
    MAX31865Transport transport; ///< Borrowed callbacks copied by begin().
    MAX31865DeviceConfig initialDeviceConfig; ///< Desired verified startup image.
    MAX31865RtdConfig rtd; ///< Local conversion/scaling settings.
    /**
     * Optional begin-only pre-protocol delay. This delay runs before the
     * default I/O deadline starts, so it is additional to begin()'s bounded
     * protocol work.
     */
    uint32_t powerReadyDelayMs;
    uint32_t defaultOperationTimeoutMs; ///< Nonzero default I/O deadline.
    uint8_t offlineThreshold; ///< Nonzero tracked-failure threshold.
};

/** @return IEC 60751 coefficients A=3.9083e-3, B=-5.775e-7, C=-4.183e-12. */
MAX31865RtdCoefficients max31865Iec60751Coefficients();
/** @return Fully initialized default PT100/400-ohm RTD configuration. */
MAX31865RtdConfig max31865DefaultRtdConfig();
/** @return Reset-aligned persistent device configuration. */
MAX31865DeviceConfig max31865DefaultDeviceConfig();
/** @return Fully initialized unbound begin configuration. */
MAX31865BeginConfig max31865DefaultBeginConfig();

/** @brief Raw writable image and decoded values observed by probe(). */
struct MAX31865DeviceInfo
{
    uint8_t rawConfig; ///< Raw CONFIG byte.
    uint16_t rawHighThresholdRegister; ///< Unshifted high-threshold register pair.
    uint16_t rawLowThresholdRegister; ///< Unshifted low-threshold register pair.
    uint8_t rawFaultStatus; ///< Raw FAULT_STATUS byte.
    bool configurationMatches; ///< Persistent fields match the desired image.
};

/** @brief Complete live register/settings snapshot. */
struct MAX31865Settings
{
    uint8_t rawConfig; ///< Raw CONFIG byte.
    MAX31865DeviceConfig deviceConfig; ///< Decoded persistent configuration.
    MAX31865ConversionMode conversionMode; ///< CONFIG automatic/normal mode.
    bool oneShotActive; ///< Transient CONFIG D5 observation.
    MAX31865FaultCycle faultCycle; ///< Transient CONFIG D3:D2 observation.
    float lowThresholdOhms; ///< Low threshold converted through RTD scaling.
    float highThresholdOhms; ///< High threshold converted through RTD scaling.
    float lowThresholdC; ///< Low threshold converted to Celsius.
    float highThresholdC; ///< High threshold converted to Celsius.
    bool lowThresholdTemperatureValid; ///< lowThresholdC is inside the RTD domain.
    bool highThresholdTemperatureValid; ///< highThresholdC is inside the RTD domain.
};

#if !defined(DOXYGEN)
static_assert(
    static_cast<uint8_t>(MAX31865Filter::Hz50) ==
        max31865_cmd::CONFIG_FILTER_50HZ,
    "MAX31865Filter mapping");
static_assert(
    static_cast<uint8_t>(MAX31865ConversionMode::Continuous) ==
        max31865_cmd::CONFIG_AUTO,
    "MAX31865ConversionMode mapping");
static_assert(
    static_cast<uint8_t>(MAX31865FaultCycle::ManualStep2) ==
        max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2,
    "MAX31865FaultCycle mapping");
#endif
