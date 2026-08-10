#include "MAX31865/MAX31865.h"

#include <limits.h>
#include <math.h>
#include <string.h>

struct MAX31865BusSession
{
    bool locked;
    bool selected;
};

struct MAX31865OperationContext
{
    uint32_t startMs;
    uint32_t timeoutMs;
    bool lastTransferAttempted;
    bool lastTransferApplied;
    bool lastFrameCommitted;
    bool lastDeassertUnknown;
    bool anyWriteMayHaveApplied;
    bool unsafeStateObserved;
    bool sawDrdyTimeout;
    bool sawOperationTimeout;
};

enum class MAX31865TrackingOutcome : uint8_t
{
    Untracked = 0,
    Success,
    Failure
};

namespace
{
static constexpr uint32_t kDefaultOperationTimeoutMs = 250U;
static constexpr uint8_t kDefaultOfflineThreshold = 5U;
static constexpr uint32_t kDefaultInputFilterTimeConstantUs = 100U;
static constexpr uint32_t kDeadlineClockQuantizationGuardMs = 1U;
// timeoutMs == 0 is the internal no-wait cleanup sentinel. It deliberately
// permits fixed SPI frames, but it must never turn stopInternal()'s state
// resolution loop into an unbounded retry when callbacks keep changing the
// apparent one-shot/fault state. Two passes cover the immediate finite chain:
// one can resolve uncertain-command readiness and the second can discard that
// proven result before the final idle CONFIG write outside the loop.
static constexpr uint8_t kNoWaitStopPassLimit = 2U;
static constexpr uint8_t kPersistentObservedMask =
    static_cast<uint8_t>((1U << max31865_cmd::REG_CONFIG) |
                         (1U << max31865_cmd::REG_HIGH_FAULT_MSB) |
                         (1U << max31865_cmd::REG_HIGH_FAULT_LSB) |
                         (1U << max31865_cmd::REG_LOW_FAULT_MSB) |
                         (1U << max31865_cmd::REG_LOW_FAULT_LSB));

void saturatingIncrement(uint32_t &value)
{
    if (value < UINT32_MAX)
    {
        ++value;
    }
}

void saturatingAdd(uint32_t &value, uint32_t amount)
{
    const uint32_t available = UINT32_MAX - value;
    value += (amount > available) ? available : amount;
}

uint32_t elapsedMs(uint32_t now, uint32_t start)
{
    return now - start;
}

bool timeReached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

bool validWireMode(MAX31865WireMode mode)
{
    return mode == MAX31865WireMode::TwoWire ||
           mode == MAX31865WireMode::ThreeWire ||
           mode == MAX31865WireMode::FourWire;
}

bool validFilter(MAX31865Filter filter)
{
    return filter == MAX31865Filter::Hz60 ||
           filter == MAX31865Filter::Hz50;
}

bool validFaultCycleBits(uint8_t bits)
{
    return bits == max31865_cmd::CONFIG_FAULT_CYCLE_NONE ||
           bits == max31865_cmd::CONFIG_FAULT_CYCLE_AUTO ||
           bits == max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1 ||
           bits == max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2;
}

uint8_t persistentConfigByte(
    const MAX31865DeviceConfig &config,
    bool bias,
    bool continuous)
{
    uint8_t value = 0U;
    if (bias || continuous)
    {
        value |= max31865_cmd::CONFIG_BIAS;
    }
    if (continuous)
    {
        value |= max31865_cmd::CONFIG_AUTO;
    }
    if (config.wireMode == MAX31865WireMode::ThreeWire)
    {
        value |= max31865_cmd::CONFIG_3WIRE;
    }
    if (config.filter == MAX31865Filter::Hz50)
    {
        value |= max31865_cmd::CONFIG_FILTER_50HZ;
    }
    return value;
}

uint16_t joinedRegister(const uint8_t bytes[2])
{
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[0]) << 8U) |
        static_cast<uint16_t>(bytes[1]));
}

bool persistentByteMatches(uint8_t address, uint8_t actual, uint8_t expected)
{
    const uint8_t mask = max31865RegisterVerifyMask(address);
    return (actual & mask) == (expected & mask);
}

MAX31865Status timeoutStatus(
    MAX31865Error code,
    MAX31865OperationContext &operation)
{
    if (code == MAX31865Error::DrdyTimeout)
    {
        operation.sawDrdyTimeout = true;
        return MAX31865Status::Error(code, "conversion readiness timed out");
    }
    operation.sawOperationTimeout = true;
    return MAX31865Status::Error(
        MAX31865Error::OperationTimeout,
        "operation deadline expired");
}

void mergeTimeoutObservations(
    MAX31865OperationContext &destination,
    const MAX31865OperationContext &source)
{
    destination.sawDrdyTimeout =
        destination.sawDrdyTimeout || source.sawDrdyTimeout;
    destination.sawOperationTimeout =
        destination.sawOperationTimeout || source.sawOperationTimeout;
}

MAX31865Status normalizeLockStatus(const MAX31865Status &status)
{
    if (status.ok())
    {
        return MAX31865Status::Ok();
    }
    if (status.code == MAX31865Error::InvalidState ||
        status.code == MAX31865Error::BusLockTimeout ||
        status.code == MAX31865Error::BusLockFailed)
    {
        return MAX31865Status::Error(
            status.code,
            status.msg != nullptr ? status.msg : "bus lock callback failed",
            status.detail);
    }
    return MAX31865Status::Error(
        MAX31865Error::BusLockFailed,
        "bus lock callback failed",
        static_cast<int32_t>(status.code));
}

MAX31865Status normalizeChipSelectStatus(const MAX31865Status &status)
{
    if (status.ok())
    {
        return MAX31865Status::Ok();
    }
    if (status.code == MAX31865Error::InvalidState ||
        status.code == MAX31865Error::ChipSelectFailed)
    {
        return MAX31865Status::Error(
            status.code,
            status.msg != nullptr
                ? status.msg
                : "chip-select callback failed",
            status.detail);
    }
    return MAX31865Status::Error(
        MAX31865Error::ChipSelectFailed,
        "chip-select callback failed",
        static_cast<int32_t>(status.code));
}

MAX31865Status normalizeRequiredDeassertStatus(
    const MAX31865Status &status)
{
    const MAX31865Status normalized = normalizeChipSelectStatus(status);
    if (normalized.code != MAX31865Error::InvalidState)
    {
        return normalized;
    }
    // Once a frame may be selected, failure to prove CS high is a framing
    // failure even when the backend describes its own precondition as
    // InvalidState. Keeping it health-neutral would hide an unsafe bus state.
    return MAX31865Status::Error(
        MAX31865Error::ChipSelectFailed,
        status.msg != nullptr
            ? status.msg
            : "chip-select deassert precondition failed",
        static_cast<int32_t>(MAX31865Error::InvalidState));
}

MAX31865Status normalizeTransferStatus(const MAX31865Status &status)
{
    if (status.ok())
    {
        return MAX31865Status::Ok();
    }
    if (status.code == MAX31865Error::InvalidState ||
        status.code == MAX31865Error::SpiTransferFailed)
    {
        return MAX31865Status::Error(
            status.code,
            status.msg != nullptr ? status.msg : "SPI transfer callback failed",
            status.detail);
    }
    return MAX31865Status::Error(
        MAX31865Error::SpiTransferFailed,
        "SPI transfer callback failed",
        static_cast<int32_t>(status.code));
}

MAX31865Status normalizeGpioStatus(const MAX31865Status &status)
{
    if (status.ok())
    {
        return MAX31865Status::Ok();
    }
    if (status.code == MAX31865Error::InvalidState ||
        status.code == MAX31865Error::GpioFailed)
    {
        return MAX31865Status::Error(
            status.code,
            status.msg != nullptr ? status.msg : "GPIO callback failed",
            status.detail);
    }
    return MAX31865Status::Error(
        MAX31865Error::GpioFailed,
        "GPIO callback failed",
        static_cast<int32_t>(status.code));
}

int32_t combinedFailureDetail(
    MAX31865Error primary,
    MAX31865Error cleanup)
{
    return static_cast<int32_t>(
        (static_cast<uint32_t>(primary) << 8U) |
        static_cast<uint32_t>(cleanup));
}

bool statusContainsTimeout(
    const MAX31865Status &status,
    MAX31865Error timeoutCode)
{
    if (status.code == timeoutCode)
    {
        return true;
    }
    if (status.code != MAX31865Error::RestoreFailed || status.detail < 0)
    {
        return false;
    }

    const uint32_t detail = static_cast<uint32_t>(status.detail);
    const MAX31865Error primary = static_cast<MAX31865Error>(
        (detail >> 8U) & 0xFFU);
    const MAX31865Error cleanup = static_cast<MAX31865Error>(detail & 0xFFU);
    return primary == timeoutCode || cleanup == timeoutCode;
}

double resistanceAtTemperature(
    const MAX31865RtdConfig &config,
    double temperatureC)
{
    const double t = temperatureC;
    const double a = static_cast<double>(config.coefficients.a);
    const double b = static_cast<double>(config.coefficients.b);
    const double c = temperatureC < 0.0
        ? static_cast<double>(config.coefficients.c)
        : 0.0;
    const double ratio = 1.0 + (a * t) + (b * t * t) +
                         (c * (t - 100.0) * t * t * t);
    return static_cast<double>(config.nominalResistanceOhms) * ratio;
}

double resistanceDerivativeAtTemperature(
    const MAX31865RtdConfig &config,
    double temperatureC)
{
    const double a = static_cast<double>(config.coefficients.a);
    const double b = static_cast<double>(config.coefficients.b);
    const double c = static_cast<double>(config.coefficients.c);
    const double t = temperatureC;
    const double ratioDerivative = temperatureC < 0.0
        ? a + (2.0 * b * t) + (c * t * t * ((4.0 * t) - 300.0))
        : a + (2.0 * b * t);
    return static_cast<double>(config.nominalResistanceOhms) * ratioDerivative;
}

bool nonnegativeDerivativeAt(
    const MAX31865RtdConfig &config,
    double temperatureC)
{
    const double derivative = resistanceDerivativeAtTemperature(
        config, temperatureC);
    return isfinite(derivative) != 0 && derivative >= 0.0;
}

bool negativeCurveSegmentIsStrictlyIncreasing(
    const MAX31865RtdConfig &config,
    double low,
    double high)
{
    if (!nonnegativeDerivativeAt(config, low) ||
        !nonnegativeDerivativeAt(config, high))
    {
        return false;
    }

    const double b = static_cast<double>(config.coefficients.b);
    const double c = static_cast<double>(config.coefficients.c);
    if (c == 0.0)
    {
        // The resistance derivative is linear; its minimum is at an endpoint.
        return true;
    }

    // For T<0, dR/dT is cubic. Its extrema are the real roots of
    // 12*C*T^2 - 600*C*T + 2*B, so endpoints plus these roots prove the
    // derivative positive over the complete interval (not merely a grid).
    const double qa = 12.0 * c;
    const double qb = -600.0 * c;
    const double qc = 2.0 * b;
    const double discriminant = (qb * qb) - (4.0 * qa * qc);
    if (!isfinite(discriminant))
    {
        return false;
    }
    if (discriminant < 0.0)
    {
        return true;
    }

    const double rootTerm = sqrt(discriminant);
    // Use the cancellation-resistant quadratic form. Extreme but finite custom
    // coefficients can otherwise move the small critical root far enough to
    // miss a narrow negative-derivative interval.
    const double q = -0.5 * (qb + copysign(rootTerm, qb));
    const double repeatedRoot = -qb / (2.0 * qa);
    const double roots[2] = {
        q == 0.0 ? repeatedRoot : q / qa,
        q == 0.0 ? repeatedRoot : qc / q};
    for (double root : roots)
    {
        if (root > low && root < high &&
            !nonnegativeDerivativeAt(config, root))
        {
            return false;
        }
    }
    return true;
}

bool curveIsStrictlyIncreasing(const MAX31865RtdConfig &config)
{
    const double minimum = static_cast<double>(config.minimumTemperatureC);
    const double maximum = static_cast<double>(config.maximumTemperatureC);
    if (minimum < 0.0)
    {
        const double negativeMaximum = maximum < 0.0 ? maximum : 0.0;
        if (negativeMaximum > minimum &&
            config.coefficients.a == 0.0F &&
            config.coefficients.b == 0.0F &&
            config.coefficients.c == 0.0F)
        {
            return false;
        }
        if (!negativeCurveSegmentIsStrictlyIncreasing(
                config, minimum, negativeMaximum))
        {
            return false;
        }
    }
    if (maximum >= 0.0)
    {
        const double positiveMinimum = minimum > 0.0 ? minimum : 0.0;
        if (maximum > positiveMinimum &&
            config.coefficients.a == 0.0F &&
            config.coefficients.b == 0.0F)
        {
            return false;
        }
        if (!nonnegativeDerivativeAt(config, positiveMinimum) ||
            !nonnegativeDerivativeAt(config, maximum))
        {
            return false;
        }
    }
    return true;
}

bool finiteFloat(float value)
{
    return isfinite(static_cast<double>(value)) != 0;
}

uint32_t biasSettleUsFor(const MAX31865RtdConfig &config)
{
    const uint64_t scaled =
        (static_cast<uint64_t>(config.inputFilterTimeConstantUs) *
             max31865_cmd::BIAS_SETTLE_MULTIPLIER_NUMERATOR +
         max31865_cmd::BIAS_SETTLE_MULTIPLIER_DENOMINATOR - 1U) /
        max31865_cmd::BIAS_SETTLE_MULTIPLIER_DENOMINATOR;
    const uint64_t total = scaled + max31865_cmd::BIAS_SETTLE_EXTRA_US;
    return total > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(total);
}

uint32_t postFaultSettleUsFor(const MAX31865RtdConfig &config)
{
    const uint64_t total =
        static_cast<uint64_t>(config.inputFilterTimeConstantUs) *
            max31865_cmd::MANUAL_FAULT_SETTLE_MULTIPLIER +
        max31865_cmd::BIAS_SETTLE_EXTRA_US;
    return total > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(total);
}
} // namespace

MAX31865RtdCoefficients max31865Iec60751Coefficients()
{
    return MAX31865RtdCoefficients{
        3.90830e-3F,
        -5.77500e-7F,
        -4.18301e-12F};
}

MAX31865RtdConfig max31865DefaultRtdConfig()
{
    return MAX31865RtdConfig{
        400.0F,
        100.0F,
        max31865Iec60751Coefficients(),
        kDefaultInputFilterTimeConstantUs,
        -200.0F,
        850.0F};
}

MAX31865DeviceConfig max31865DefaultDeviceConfig()
{
    return MAX31865DeviceConfig{
        MAX31865WireMode::FourWire,
        MAX31865Filter::Hz60,
        false,
        MAX31865FaultThresholds{0U, max31865_cmd::ADC_CODE_MAX}};
}

MAX31865BeginConfig max31865DefaultBeginConfig()
{
    MAX31865BeginConfig config{};
    config.transport = MAX31865Transport{};
    config.initialDeviceConfig = max31865DefaultDeviceConfig();
    config.rtd = max31865DefaultRtdConfig();
    config.powerReadyDelayMs = 0U;
    config.defaultOperationTimeoutMs = kDefaultOperationTimeoutMs;
    config.offlineThreshold = kDefaultOfflineThreshold;
    return config;
}

const char *max31865StateName(MAX31865State state)
{
    switch (state)
    {
        case MAX31865State::Uninitialized: return "Uninitialized";
        case MAX31865State::Ready: return "Ready";
        case MAX31865State::Converting: return "Converting";
        case MAX31865State::Fault: return "Fault";
        default: return "Unknown";
    }
}

const char *max31865DriverStateName(MAX31865DriverState state)
{
    switch (state)
    {
        case MAX31865DriverState::UNINIT: return "UNINIT";
        case MAX31865DriverState::READY: return "READY";
        case MAX31865DriverState::DEGRADED: return "DEGRADED";
        case MAX31865DriverState::OFFLINE: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

const char *max31865ErrorName(MAX31865Error error)
{
    switch (error)
    {
        case MAX31865Error::Ok: return "Ok";
        case MAX31865Error::NotInitialized: return "NotInitialized";
        case MAX31865Error::InvalidArgument: return "InvalidArgument";
        case MAX31865Error::InvalidState: return "InvalidState";
        case MAX31865Error::Busy: return "Busy";
        case MAX31865Error::UnsupportedCommand: return "UnsupportedCommand";
        case MAX31865Error::NoData: return "NoData";
        case MAX31865Error::TimingUnavailable: return "TimingUnavailable";
        case MAX31865Error::RegisterAddressInvalid: return "RegisterAddressInvalid";
        case MAX31865Error::BusLockTimeout: return "BusLockTimeout";
        case MAX31865Error::BusLockFailed: return "BusLockFailed";
        case MAX31865Error::SpiTransferFailed: return "SpiTransferFailed";
        case MAX31865Error::ChipSelectFailed: return "ChipSelectFailed";
        case MAX31865Error::GpioFailed: return "GpioFailed";
        case MAX31865Error::OperationTimeout: return "OperationTimeout";
        case MAX31865Error::DrdyTimeout: return "DrdyTimeout";
        case MAX31865Error::RegisterVerifyFailed: return "RegisterVerifyFailed";
        case MAX31865Error::ProbeMismatch: return "ProbeMismatch";
        case MAX31865Error::ConfigurationUnknown: return "ConfigurationUnknown";
        case MAX31865Error::DeviceFault: return "DeviceFault";
        case MAX31865Error::RestoreFailed: return "RestoreFailed";
        case MAX31865Error::ConversionOutOfRange: return "ConversionOutOfRange";
        default: return "Unknown";
    }
}

MAX31865::MAX31865()
{
    resetLocalState();
}

MAX31865::~MAX31865()
{
    // Borrowed callbacks may already be dead. Destruction is intentionally inert.
}

void MAX31865::resetLocalState()
{
    _transport = MAX31865Transport{};
    _desiredConfig = max31865DefaultDeviceConfig();
    _rtdConfig = max31865DefaultRtdConfig();
    memset(_observedRegisters, 0, sizeof(_observedRegisters));
    _observedValidMask = 0U;
    _configurationKnown = false;
    _continuous = false;
    _oneShotArmed = false;
    _oneShotCommitPending = false;
    _oneShotCommitUncertain = false;
    _freshResultArmed = false;
    _readyLatched = false;
    _timedReadinessHalted = false;
    _chipSelectUncertain = false;
    _conversionStartMs = 0U;
    _nextReadyMs = 0U;
    _defaultOperationTimeoutMs = kDefaultOperationTimeoutMs;
    _state = MAX31865State::Uninitialized;
    _lastOperation = MAX31865Status::Ok();
    _offlineThreshold = kDefaultOfflineThreshold;
    _consecutiveFailures = 0U;
    _sampleCounter = 0U;

    _trackedSuccessCount = 0U;
    _trackedFailureCount = 0U;
    _sampleFrameAttemptCount = 0U;
    _sampleFrameSuccessCount = 0U;
    _sampleFrameFailureCount = 0U;
    _noDataCount = 0U;
    _droppedSampleCount = 0U;
    _overrunCount = 0U;
    _busLockTimeoutCount = 0U;
    _busLockFailureCount = 0U;
    _spiTransferFailureCount = 0U;
    _chipSelectFailureCount = 0U;
    _gpioFailureCount = 0U;
    _drdyTimeoutCount = 0U;
    _operationTimeoutCount = 0U;
    _faultObservationCount = 0U;
    _thresholdFaultObservationCount = 0U;
    _referenceFaultObservationCount = 0U;
    _voltageFaultObservationCount = 0U;

    _hasLastFaultStatus = false;
    _lastFaultStatus = 0U;
    _hasLastOkMs = false;
    _hasLastErrorMs = false;
    _hasLastSampleTimestamp = false;
    _lastOkMs = 0U;
    _lastErrorMs = 0U;
    _lastSampleTimestampUs = 0U;
}

MAX31865Status MAX31865::validateTransport(
    const MAX31865Transport &transport) const
{
    if (transport.transfer == nullptr ||
        transport.setChipSelect == nullptr ||
        transport.nowMs == nullptr ||
        transport.sleepMs == nullptr ||
        transport.delayUs == nullptr)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "transport is missing a required callback");
    }
    if ((transport.lockBus == nullptr) != (transport.unlockBus == nullptr))
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "bus lock and unlock callbacks must be paired");
    }
    if (transport.capabilities.hasDrdy && transport.readPin == nullptr)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "DRDY capability requires a pin-read callback");
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::validateDeviceConfig(
    const MAX31865DeviceConfig &config) const
{
    if (!validWireMode(config.wireMode) || !validFilter(config.filter))
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid wire mode or filter");
    }
    if (config.thresholds.lowCode > max31865_cmd::ADC_CODE_MAX ||
        config.thresholds.highCode > max31865_cmd::ADC_CODE_MAX ||
        config.thresholds.lowCode > config.thresholds.highCode)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid fault thresholds");
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::validateRtdConfig(
    const MAX31865RtdConfig &config) const
{
    if (!finiteFloat(config.referenceResistorOhms) ||
        config.referenceResistorOhms <
            max31865_cmd::REFERENCE_RESISTOR_MIN_OHMS ||
        config.referenceResistorOhms >
            max31865_cmd::REFERENCE_RESISTOR_MAX_OHMS ||
        !finiteFloat(config.nominalResistanceOhms) ||
        config.nominalResistanceOhms <= 0.0F)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid RTD/reference resistance");
    }
    if (!finiteFloat(config.coefficients.a) ||
        !finiteFloat(config.coefficients.b) ||
        !finiteFloat(config.coefficients.c) ||
        !finiteFloat(config.minimumTemperatureC) ||
        !finiteFloat(config.maximumTemperatureC) ||
        config.minimumTemperatureC < -273.15F ||
        config.minimumTemperatureC >= config.maximumTemperatureC)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid RTD coefficients or temperature domain");
    }
    if (!curveIsStrictlyIncreasing(config))
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "RTD curve is not monotonic over the complete domain");
    }

    const uint64_t biasDelay =
        (static_cast<uint64_t>(config.inputFilterTimeConstantUs) * 21U + 1U) /
            2U +
        max31865_cmd::BIAS_SETTLE_EXTRA_US;
    const uint64_t manualDelay =
        static_cast<uint64_t>(config.inputFilterTimeConstantUs) *
            max31865_cmd::MANUAL_FAULT_SETTLE_MULTIPLIER +
        max31865_cmd::MANUAL_FAULT_STEP1_TO_OPEN_US;
    if (biasDelay > UINT32_MAX || manualDelay > UINT32_MAX)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "RTD filter settling duration is too large");
    }

    double previous = resistanceAtTemperature(
        config, static_cast<double>(config.minimumTemperatureC));
    if (!isfinite(previous) || previous < 0.0)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "RTD curve has invalid minimum resistance");
    }
    for (uint8_t index = 1U; index <= 64U; ++index)
    {
        const double fraction = static_cast<double>(index) / 64.0;
        const double temperature =
            static_cast<double>(config.minimumTemperatureC) +
            (static_cast<double>(config.maximumTemperatureC) -
             static_cast<double>(config.minimumTemperatureC)) *
                fraction;
        const double resistance = resistanceAtTemperature(config, temperature);
        if (!isfinite(resistance) || resistance <= previous)
        {
            return MAX31865Status::Error(
                MAX31865Error::InvalidArgument,
                "RTD curve is not finite and strictly increasing");
        }
        previous = resistance;
    }
    const double maximumRepresentable =
        static_cast<double>(config.referenceResistorOhms) *
        static_cast<double>(max31865_cmd::ADC_CODE_MAX) /
        static_cast<double>(max31865_cmd::ADC_FULL_SCALE);
    if (previous > maximumRepresentable)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "RTD range exceeds reference-resistor full scale");
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::validateTimeout(
    uint32_t timeoutMs,
    bool allowZero) const
{
    if ((!allowZero && timeoutMs == 0U) || timeoutMs > INT32_MAX)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid operation timeout",
            static_cast<int32_t>(timeoutMs));
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::requireReady() const
{
    if (_state == MAX31865State::Uninitialized)
    {
        return MAX31865Status::Error(
            MAX31865Error::NotInitialized,
            "driver is not initialized");
    }
    if (_state != MAX31865State::Ready || !_configurationKnown)
    {
        return MAX31865Status::Error(
            _state == MAX31865State::Converting
                ? MAX31865Error::Busy
                : MAX31865Error::InvalidState,
            "operation requires Ready with known configuration");
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::requireOperational() const
{
    if (_state == MAX31865State::Uninitialized)
    {
        return MAX31865Status::Error(
            MAX31865Error::NotInitialized,
            "driver is not initialized");
    }
    if (_state == MAX31865State::Fault || !_configurationKnown)
    {
        return MAX31865Status::Error(
            MAX31865Error::ConfigurationUnknown,
            "device configuration is not trusted");
    }
    return MAX31865Status::Ok();
}

MAX31865OperationContext MAX31865::makeOperation(uint32_t timeoutMs) const
{
    return MAX31865OperationContext{
        _transport.nowMs != nullptr ? _transport.nowMs(_transport.user) : 0U,
        timeoutMs,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false};
}

bool MAX31865::deadlineExpired(
    const MAX31865OperationContext &operation) const
{
    if (operation.timeoutMs == 0U)
    {
        // Zero is the explicit immediate-attempt sentinel. It removes all
        // callback-owned waiting (remaining budget is also zero), but the
        // fixed SPI frame and mandatory CS timing may still complete even if
        // the millisecond clock advances while those callbacks run.
        return false;
    }
    const uint32_t elapsed = elapsedMs(
        _transport.nowMs(_transport.user), operation.startMs);
    return elapsed >= operation.timeoutMs;
}

uint32_t MAX31865::deadlineRemainingMs(
    const MAX31865OperationContext &operation) const
{
    if (operation.timeoutMs == 0U)
    {
        return 0U;
    }
    const uint32_t elapsed = elapsedMs(
        _transport.nowMs(_transport.user), operation.startMs);
    if (elapsed >= operation.timeoutMs)
    {
        return 0U;
    }
    return operation.timeoutMs - elapsed;
}

MAX31865Status MAX31865::sleepWithinDeadline(
    uint32_t delayUs,
    MAX31865OperationContext &operation,
    MAX31865Error timeoutCode)
{
    if (delayUs == 0U)
    {
        return MAX31865Status::Ok();
    }
    const uint32_t requiredMs = delayUs / 1000U +
        ((delayUs % 1000U) == 0U ? 0U : 1U);
    const uint32_t remaining = deadlineRemainingMs(operation);
    if (operation.timeoutMs == 0U || requiredMs >= remaining)
    {
        return timeoutStatus(timeoutCode, operation);
    }

    const uint32_t wholeMs = delayUs / 1000U;
    const uint32_t remainderUs = delayUs % 1000U;
    const uint32_t beforeSleep = _transport.nowMs(_transport.user);
    const uint32_t elapsedBeforeSleep = elapsedMs(
        beforeSleep, operation.startMs);
    const uint32_t remainingBeforeSleep =
        elapsedBeforeSleep >= operation.timeoutMs
            ? 0U
            : operation.timeoutMs - elapsedBeforeSleep;
    if (requiredMs >= remainingBeforeSleep)
    {
        // deadlineRemainingMs() and this final callback-boundary observation
        // are distinct clock reads. Do not start callback-owned waiting when
        // the latter has consumed the last rounded millisecond in which the
        // complete requested delay could finish.
        return timeoutStatus(timeoutCode, operation);
    }
    if (wholeMs > 0U)
    {
        _transport.sleepMs(_transport.user, wholeMs);
    }
    if (remainderUs > 0U)
    {
        _transport.delayUs(_transport.user, remainderUs);
    }
    const uint32_t afterSleep = _transport.nowMs(_transport.user);
    if (wholeMs > 0U && elapsedMs(afterSleep, beforeSleep) < wholeMs)
    {
        return MAX31865Status::Error(
            MAX31865Error::TimingUnavailable,
            "sleep callback did not advance the deadline clock");
    }
    return deadlineExpired(operation)
        ? timeoutStatus(timeoutCode, operation)
        : MAX31865Status::Ok();
}

MAX31865Status MAX31865::synchronizeChipSelect(
    MAX31865OperationContext &operation)
{
    if (deadlineExpired(operation))
    {
        return timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }
    const bool framingWasUncertain = _chipSelectUncertain;
    bool locked = false;
    if (_transport.lockBus != nullptr)
    {
        MAX31865Status status = normalizeLockStatus(
            _transport.lockBus(
                _transport.user, deadlineRemainingMs(operation)));
        if (!status.ok())
        {
            if (status.code == MAX31865Error::BusLockTimeout)
            {
                saturatingIncrement(_busLockTimeoutCount);
            }
            else if (status.code == MAX31865Error::BusLockFailed)
            {
                saturatingIncrement(_busLockFailureCount);
            }
            return deadlineExpired(operation)
                ? timeoutStatus(
                      MAX31865Error::OperationTimeout, operation)
                : status;
        }
        locked = true;
        if (deadlineExpired(operation) && !framingWasUncertain)
        {
            _transport.unlockBus(_transport.user);
            return timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
    }

    const MAX31865Status deassert =
        _transport.setChipSelect(_transport.user, false);
    MAX31865Status status = framingWasUncertain
        ? normalizeRequiredDeassertStatus(deassert)
        : normalizeChipSelectStatus(deassert);
    if (!status.ok())
    {
        if (status.code == MAX31865Error::ChipSelectFailed)
        {
            _chipSelectUncertain = true;
            saturatingIncrement(_chipSelectFailureCount);
            _state = MAX31865State::Fault;
            invalidateObservedState();
        }
        if (deadlineExpired(operation))
        {
            if (status.code == MAX31865Error::ChipSelectFailed)
            {
                (void)timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
            else
            {
                status = timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
        }
    }
    else
    {
        _chipSelectUncertain = false;
        if (_oneShotArmed && _oneShotCommitPending)
        {
            // This is the first proven rising edge after an ambiguous D5 frame.
            // The transfer may or may not have delivered the command, so retain
            // the uncertainty but restart the conservative completion horizon.
            _conversionStartMs = _transport.nowMs(_transport.user);
            _nextReadyMs = _conversionStartMs + singleConversionTimeMs() +
                kDeadlineClockQuantizationGuardMs;
            _oneShotCommitPending = false;
            _timedReadinessHalted = false;
        }
        _transport.delayUs(
            _transport.user, max31865_cmd::CS_INACTIVE_DELAY_US);
        if (deadlineExpired(operation))
        {
            status = timeoutStatus(MAX31865Error::OperationTimeout, operation);
        }
    }
    if (locked)
    {
        _transport.unlockBus(_transport.user);
        if (deadlineExpired(operation))
        {
            if (status.code == MAX31865Error::ChipSelectFailed)
            {
                (void)timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
            else
            {
                status = timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
        }
    }
    return status;
}

MAX31865Status MAX31865::openSession(
    MAX31865BusSession &session,
    MAX31865OperationContext &operation)
{
    session = MAX31865BusSession{false, false};
    if (deadlineExpired(operation))
    {
        return timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }
    if (_transport.lockBus != nullptr)
    {
        MAX31865Status status = normalizeLockStatus(
            _transport.lockBus(
                _transport.user, deadlineRemainingMs(operation)));
        if (!status.ok())
        {
            if (status.code == MAX31865Error::BusLockTimeout)
            {
                saturatingIncrement(_busLockTimeoutCount);
            }
            else if (status.code == MAX31865Error::BusLockFailed)
            {
                saturatingIncrement(_busLockFailureCount);
            }
            return deadlineExpired(operation)
                ? timeoutStatus(
                      MAX31865Error::OperationTimeout, operation)
                : status;
        }
        session.locked = true;
    }

    if (deadlineExpired(operation))
    {
        return timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }

    MAX31865Status status = normalizeChipSelectStatus(
        _transport.setChipSelect(_transport.user, true));
    if (!status.ok())
    {
        if (status.code == MAX31865Error::ChipSelectFailed)
        {
            // A failed GPIO callback does not prove whether active-low CS was
            // applied. Keep the bus lock and let closeSession() make the
            // required deassertion attempt before any other bus owner runs.
            session.selected = true;
            _chipSelectUncertain = true;
            saturatingIncrement(_chipSelectFailureCount);
            _state = MAX31865State::Fault;
            invalidateObservedState();
        }
        else
        {
            // InvalidState is contractually action-free, so the previously
            // proven inactive CS level remains authoritative.
            _chipSelectUncertain = false;
        }
        return deadlineExpired(operation)
            ? timeoutStatus(MAX31865Error::OperationTimeout, operation)
            : status;
    }
    session.selected = true;
    _chipSelectUncertain = false;
    if (deadlineExpired(operation))
    {
        return timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }
    _transport.delayUs(_transport.user, max31865_cmd::CS_SETUP_DELAY_US);
    if (deadlineExpired(operation))
    {
        return timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::transferInSession(
    MAX31865BusSession &session,
    const uint8_t *tx,
    uint8_t *rx,
    size_t length,
    MAX31865OperationContext &operation)
{
    if (!session.selected || tx == nullptr || rx == nullptr || length == 0U ||
        length > max31865_cmd::MAX_FRAME_BYTES)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid in-session transfer");
    }
    if (deadlineExpired(operation))
    {
        return timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }
    MAX31865Status status = normalizeTransferStatus(
        _transport.transfer(
            _transport.user,
            tx,
            rx,
            length,
            deadlineRemainingMs(operation)));
    operation.lastTransferAttempted =
        status.code != MAX31865Error::InvalidState;
    if (!status.ok())
    {
        if (status.code == MAX31865Error::SpiTransferFailed)
        {
            saturatingIncrement(_spiTransferFailureCount);
        }
        return status;
    }
    operation.lastTransferApplied = true;
    return deadlineExpired(operation)
        ? timeoutStatus(MAX31865Error::OperationTimeout, operation)
        : MAX31865Status::Ok();
}

MAX31865Status MAX31865::closeSession(
    MAX31865BusSession &session,
    MAX31865OperationContext &operation)
{
    MAX31865Status status = MAX31865Status::Ok();
    if (session.selected)
    {
        _transport.delayUs(_transport.user, max31865_cmd::CS_HOLD_DELAY_US);
        if (deadlineExpired(operation))
        {
            status = timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        const MAX31865Status deassert = normalizeRequiredDeassertStatus(
            _transport.setChipSelect(_transport.user, false));
        if (!deassert.ok())
        {
            status = deassert;
            _chipSelectUncertain = true;
            if (status.code == MAX31865Error::ChipSelectFailed)
            {
                saturatingIncrement(_chipSelectFailureCount);
            }
            _state = MAX31865State::Fault;
            invalidateObservedState();
            if (deadlineExpired(operation))
            {
                // The safety/framing failure remains terminal, while timeout
                // provenance records that its callback also exhausted the
                // whole-operation budget.
                (void)timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
        }
        else
        {
            _chipSelectUncertain = false;
            session.selected = false;
            if (deadlineExpired(operation))
            {
                status = timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
            _transport.delayUs(
                _transport.user, max31865_cmd::CS_INACTIVE_DELAY_US);
            if (deadlineExpired(operation))
            {
                status = timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
        }
    }
    if (session.locked)
    {
        _transport.unlockBus(_transport.user);
        session.locked = false;
        if (deadlineExpired(operation))
        {
            if (status.code == MAX31865Error::ChipSelectFailed)
            {
                (void)timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
            else
            {
                status = timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
        }
    }
    return status;
}

MAX31865Status MAX31865::transact(
    const uint8_t *tx,
    uint8_t *rx,
    size_t length,
    MAX31865OperationContext &operation)
{
    operation.lastTransferAttempted = false;
    operation.lastTransferApplied = false;
    operation.lastFrameCommitted = false;
    operation.lastDeassertUnknown = false;
    if (_chipSelectUncertain)
    {
        const MAX31865Status synchronization =
            synchronizeChipSelect(operation);
        if (!synchronization.ok())
        {
            return synchronization;
        }
    }
    MAX31865BusSession session{};
    MAX31865Status primary = openSession(session, operation);
    if (primary.ok())
    {
        primary = transferInSession(session, tx, rx, length, operation);
    }
    const MAX31865Status cleanup = closeSession(session, operation);
    if (operation.lastTransferApplied && !session.selected)
    {
        operation.lastFrameCommitted = true;
    }
    else if (operation.lastTransferAttempted && session.selected)
    {
        operation.lastDeassertUnknown = true;
    }
    if (cleanup.code == MAX31865Error::OperationTimeout)
    {
        // Once cleanup crosses the whole-operation deadline, that boundedness
        // result takes precedence while the originating role counter retains
        // any earlier callback failure. A successfully deasserted frame may
        // still be committed and is tracked above.
        return cleanup;
    }
    if (!cleanup.ok())
    {
        return primary.ok()
            ? cleanup
            : MAX31865Status::Error(
                  MAX31865Error::RestoreFailed,
                  "SPI operation and chip-select cleanup both failed",
                  combinedFailureDetail(primary.code, cleanup.code));
    }
    if (primary.ok() && deadlineExpired(operation))
    {
        return timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }
    return primary;
}

MAX31865Status MAX31865::readRegistersInternal(
    uint8_t startAddress,
    uint8_t *out,
    size_t length,
    MAX31865OperationContext &operation)
{
    uint8_t tx[max31865_cmd::MAX_FRAME_BYTES]{};
    uint8_t rx[max31865_cmd::MAX_FRAME_BYTES]{};
    size_t frameLength = 0U;
    MAX31865Status status = max31865EncodeReadFrame(
        startAddress, length, tx, sizeof(tx), frameLength);
    if (!status.ok())
    {
        return status;
    }
    status = transact(tx, rx, frameLength, operation);
    if (!status.ok())
    {
        return status;
    }
    memcpy(out, &rx[1], length);
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::writeRegistersInternal(
    uint8_t startAddress,
    const uint8_t *values,
    size_t length,
    MAX31865OperationContext &operation)
{
    uint8_t tx[max31865_cmd::MAX_FRAME_BYTES]{};
    uint8_t rx[max31865_cmd::MAX_FRAME_BYTES]{};
    size_t frameLength = 0U;
    MAX31865Status status = max31865EncodeWriteFrame(
        startAddress, values, length, tx, sizeof(tx), frameLength);
    if (!status.ok())
    {
        return status;
    }
    status = transact(tx, rx, frameLength, operation);
    if (status.ok() || operation.lastTransferAttempted)
    {
        operation.anyWriteMayHaveApplied = true;
    }
    return status;
}

MAX31865Status MAX31865::readPersistentImage(
    uint8_t &config,
    uint8_t thresholds[4],
    MAX31865OperationContext &operation)
{
    uint8_t candidateConfig = 0U;
    uint8_t candidateThresholds[4]{};
    MAX31865Status status = readRegistersInternal(
        max31865_cmd::REG_CONFIG, &candidateConfig, 1U, operation);
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_HIGH_FAULT_MSB,
            candidateThresholds,
            sizeof(candidateThresholds),
            operation);
    }
    if (!status.ok())
    {
        return status;
    }
    config = candidateConfig;
    memcpy(thresholds, candidateThresholds, sizeof(candidateThresholds));
    return MAX31865Status::Ok();
}

void MAX31865::invalidateObservedState()
{
    _configurationKnown = false;
    _observedValidMask = 0U;
}

MAX31865Status MAX31865::verifyDesiredImage(
    MAX31865OperationContext &operation)
{
    uint8_t expectedThresholds[4]{};
    MAX31865Status status = max31865EncodeThreshold(
        _desiredConfig.thresholds.highCode, &expectedThresholds[0]);
    if (status.ok())
    {
        status = max31865EncodeThreshold(
            _desiredConfig.thresholds.lowCode, &expectedThresholds[2]);
    }
    if (!status.ok())
    {
        return status;
    }

    uint8_t actualConfig = 0U;
    uint8_t actualThresholds[4]{};
    status = readPersistentImage(actualConfig, actualThresholds, operation);
    if (!status.ok())
    {
        return status;
    }

    const uint8_t expectedConfig = persistentConfigByte(
        _desiredConfig, _desiredConfig.biasEnabled, false);
    if (!persistentByteMatches(
            max31865_cmd::REG_CONFIG, actualConfig, expectedConfig))
    {
        return MAX31865Status::Error(
            MAX31865Error::RegisterVerifyFailed,
            "CONFIG readback mismatch",
            static_cast<int32_t>(
                (static_cast<uint32_t>(max31865_cmd::REG_CONFIG) << 16U) |
                (static_cast<uint32_t>(expectedConfig) << 8U) |
                actualConfig));
    }
    for (size_t index = 0U; index < sizeof(actualThresholds); ++index)
    {
        const uint8_t address = static_cast<uint8_t>(
            max31865_cmd::REG_HIGH_FAULT_MSB + index);
        if (!persistentByteMatches(
                address, actualThresholds[index], expectedThresholds[index]))
        {
            return MAX31865Status::Error(
                MAX31865Error::RegisterVerifyFailed,
                "fault-threshold readback mismatch",
                static_cast<int32_t>(
                    (static_cast<uint32_t>(address) << 16U) |
                    (static_cast<uint32_t>(expectedThresholds[index]) << 8U) |
                    actualThresholds[index]));
        }
    }

    _observedRegisters[max31865_cmd::REG_CONFIG] = actualConfig;
    memcpy(
        &_observedRegisters[max31865_cmd::REG_HIGH_FAULT_MSB],
        actualThresholds,
        sizeof(actualThresholds));
    _observedValidMask = static_cast<uint8_t>(
        _observedValidMask | kPersistentObservedMask);
    _configurationKnown = true;
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::applyConfigurationInternal(
    const MAX31865DeviceConfig &config,
    bool commitDesired,
    MAX31865OperationContext &operation)
{
    MAX31865Status status = validateDeviceConfig(config);
    if (!status.ok())
    {
        return status;
    }

    uint8_t thresholds[4]{};
    status = max31865EncodeThreshold(
        config.thresholds.highCode, &thresholds[0]);
    if (status.ok())
    {
        status = max31865EncodeThreshold(
            config.thresholds.lowCode, &thresholds[2]);
    }
    if (!status.ok())
    {
        return status;
    }

    // Enter a proven normally-off, bias-off state before changing thresholds.
    const uint8_t safeConfig = persistentConfigByte(config, false, false);
    bool mutationApplied = false;
    status = writeRegistersInternal(
        max31865_cmd::REG_CONFIG, &safeConfig, 1U, operation);
    mutationApplied = status.ok();
    if (status.ok())
    {
        status = writeRegistersInternal(
            max31865_cmd::REG_HIGH_FAULT_MSB,
            thresholds,
            sizeof(thresholds),
            operation);
        mutationApplied = mutationApplied || status.ok();
    }
    const uint8_t finalConfig = persistentConfigByte(
        config, config.biasEnabled, false);
    if (status.ok())
    {
        status = writeRegistersInternal(
            max31865_cmd::REG_CONFIG, &finalConfig, 1U, operation);
        mutationApplied = mutationApplied || status.ok();
    }
    if (!status.ok())
    {
        if (mutationApplied || operation.lastTransferAttempted ||
            status.code == MAX31865Error::ChipSelectFailed ||
            status.code == MAX31865Error::RestoreFailed)
        {
            _state = MAX31865State::Fault;
            invalidateObservedState();
        }
        return status;
    }

    const MAX31865DeviceConfig previousDesired = _desiredConfig;
    _desiredConfig = config;
    status = verifyDesiredImage(operation);
    if (!status.ok())
    {
        _desiredConfig = previousDesired;
        _state = MAX31865State::Fault;
        invalidateObservedState();
        return status;
    }
    if (!commitDesired)
    {
        // The caller already installed this desired image before entry.
        _desiredConfig = config;
    }

    _continuous = false;
    _oneShotArmed = false;
    _oneShotCommitPending = false;
    _oneShotCommitUncertain = false;
    _freshResultArmed = false;
    _readyLatched = false;
    _timedReadinessHalted = false;
    _state = MAX31865State::Ready;
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::writeRuntimeConfig(
    bool bias,
    bool continuous,
    uint8_t commandBits,
    MAX31865OperationContext &operation)
{
    const uint8_t allowedCommands =
        max31865_cmd::CONFIG_ONE_SHOT |
        max31865_cmd::CONFIG_FAULT_CYCLE_MASK |
        max31865_cmd::CONFIG_FAULT_CLEAR;
    if ((commandBits & static_cast<uint8_t>(~allowedCommands)) != 0U ||
        !validFaultCycleBits(
            commandBits & max31865_cmd::CONFIG_FAULT_CYCLE_MASK) ||
        ((commandBits & max31865_cmd::CONFIG_ONE_SHOT) != 0U &&
         (commandBits & max31865_cmd::CONFIG_FAULT_CYCLE_MASK) != 0U) ||
        ((commandBits & max31865_cmd::CONFIG_FAULT_CLEAR) != 0U &&
         (commandBits & (max31865_cmd::CONFIG_ONE_SHOT |
                         max31865_cmd::CONFIG_FAULT_CYCLE_MASK)) != 0U))
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid CONFIG command-bit combination");
    }

    const uint8_t value = static_cast<uint8_t>(
        persistentConfigByte(_desiredConfig, bias, continuous) | commandBits);
    MAX31865Status status = writeRegistersInternal(
        max31865_cmd::REG_CONFIG, &value, 1U, operation);
    if (!status.ok())
    {
        if (operation.lastTransferAttempted ||
            status.code == MAX31865Error::ChipSelectFailed ||
            status.code == MAX31865Error::RestoreFailed)
        {
            _state = MAX31865State::Fault;
            invalidateObservedState();
        }
        return status;
    }
    _observedRegisters[max31865_cmd::REG_CONFIG] =
        persistentConfigByte(_desiredConfig, bias, continuous);
    _observedValidMask = static_cast<uint8_t>(
        _observedValidMask | (1U << max31865_cmd::REG_CONFIG));
    return MAX31865Status::Ok();
}

bool MAX31865::isTrackedFailure(MAX31865Error error) const
{
    switch (error)
    {
        case MAX31865Error::BusLockTimeout:
        case MAX31865Error::BusLockFailed:
        case MAX31865Error::SpiTransferFailed:
        case MAX31865Error::ChipSelectFailed:
        case MAX31865Error::GpioFailed:
        case MAX31865Error::OperationTimeout:
        case MAX31865Error::DrdyTimeout:
        case MAX31865Error::RegisterVerifyFailed:
        case MAX31865Error::ProbeMismatch:
        case MAX31865Error::DeviceFault:
        case MAX31865Error::RestoreFailed:
            return true;
        default:
            return false;
    }
}

MAX31865TrackingOutcome MAX31865::trackingFor(
    const MAX31865Status &status,
    bool trackedSuccess) const
{
    if (status.ok())
    {
        return trackedSuccess
            ? MAX31865TrackingOutcome::Success
            : MAX31865TrackingOutcome::Untracked;
    }
    return isTrackedFailure(status.code)
        ? MAX31865TrackingOutcome::Failure
        : MAX31865TrackingOutcome::Untracked;
}

MAX31865Status MAX31865::finishOperation(
    const MAX31865Status &status,
    MAX31865TrackingOutcome tracking,
    const MAX31865OperationContext &operation)
{
    _lastOperation = status;
    // Timeout counters describe originating deadline observations, not only
    // the terminal health classification. A required cleanup can cross the
    // deadline while an earlier, health-neutral callback precondition remains
    // the terminal result. Preserve that provenance exactly once, as the
    // other role-specific transport counters do.
    if (operation.sawDrdyTimeout ||
        statusContainsTimeout(status, MAX31865Error::DrdyTimeout))
    {
        saturatingIncrement(_drdyTimeoutCount);
    }
    if (operation.sawOperationTimeout ||
        statusContainsTimeout(status, MAX31865Error::OperationTimeout))
    {
        saturatingIncrement(_operationTimeoutCount);
    }
    if (tracking == MAX31865TrackingOutcome::Success)
    {
        _consecutiveFailures = 0U;
        saturatingIncrement(_trackedSuccessCount);
        _hasLastOkMs = true;
        _lastOkMs = operation.startMs;
    }
    else if (tracking == MAX31865TrackingOutcome::Failure)
    {
        if (_consecutiveFailures < UINT8_MAX)
        {
            ++_consecutiveFailures;
        }
        saturatingIncrement(_trackedFailureCount);
        _hasLastErrorMs = true;
        _lastErrorMs = operation.startMs;
    }
    return status;
}

MAX31865DriverState MAX31865::derivedDriverState() const
{
    if (_state == MAX31865State::Uninitialized)
    {
        return MAX31865DriverState::UNINIT;
    }
    if (_state == MAX31865State::Fault ||
        _consecutiveFailures >= _offlineThreshold)
    {
        return MAX31865DriverState::OFFLINE;
    }
    return _consecutiveFailures == 0U
        ? MAX31865DriverState::READY
        : MAX31865DriverState::DEGRADED;
}

MAX31865Status MAX31865::begin(const MAX31865BeginConfig &config)
{
    if (_state != MAX31865State::Uninitialized)
    {
        const MAX31865Status status = MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "begin is legal only while Uninitialized");
        _lastOperation = status;
        return status;
    }

    MAX31865Status status = validateTransport(config.transport);
    if (status.ok())
    {
        status = validateDeviceConfig(config.initialDeviceConfig);
    }
    if (status.ok())
    {
        status = validateRtdConfig(config.rtd);
    }
    if (status.ok())
    {
        status = validateTimeout(config.defaultOperationTimeoutMs, false);
    }
    if (status.ok() &&
        (config.offlineThreshold == 0U ||
         config.powerReadyDelayMs > INT32_MAX))
    {
        status = MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid begin timeout or offline threshold");
    }
    if (!status.ok())
    {
        _lastOperation = status;
        return status;
    }

    _transport = config.transport;
    _desiredConfig = config.initialDeviceConfig;
    _rtdConfig = config.rtd;
    _defaultOperationTimeoutMs = config.defaultOperationTimeoutMs;
    _offlineThreshold = config.offlineThreshold;
    _state = MAX31865State::Fault;
    invalidateObservedState();

    MAX31865OperationContext operation{};
    if (config.powerReadyDelayMs > 0U)
    {
        const uint32_t beforeSleep =
            _transport.nowMs(_transport.user);
        operation.startMs = beforeSleep;
        operation.timeoutMs = _defaultOperationTimeoutMs;
        _transport.sleepMs(_transport.user, config.powerReadyDelayMs);
        const uint32_t afterSleep =
            _transport.nowMs(_transport.user);
        if (elapsedMs(afterSleep, beforeSleep) < config.powerReadyDelayMs)
        {
            status = MAX31865Status::Error(
                MAX31865Error::TimingUnavailable,
                "power-ready sleep did not advance the monotonic clock");
        }
    }
    if (status.ok())
    {
        operation = makeOperation(_defaultOperationTimeoutMs);
        status = synchronizeChipSelect(operation);
    }
    if (status.ok())
    {
        // The device may not be at POR (for example after an application
        // rebind). Resolve command-state bits before applying a persistent
        // image; writing zeros does not terminate manual step 1 or prove an
        // already-triggered one-shot has completed.
        status = resolveFaultCycleInternal(operation);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(
            _desiredConfig, false, operation);
    }
    if (status.ok())
    {
        status = clearFaultsInternal(operation);
    }
    if (status.ok())
    {
        status = verifyDesiredImage(operation);
    }
    if (status.ok())
    {
        _state = MAX31865State::Ready;
        _configurationKnown = true;
    }
    else
    {
        _state = MAX31865State::Fault;
        invalidateObservedState();
    }
    return finishOperation(
        status, trackingFor(status, true), operation);
}

void MAX31865::end()
{
    resetLocalState();
}

MAX31865State MAX31865::state() const
{
    return _state;
}

MAX31865Status MAX31865::lastOperationStatus() const
{
    return _lastOperation;
}

uint32_t MAX31865::defaultOperationTimeoutMs() const
{
    return _defaultOperationTimeoutMs;
}

MAX31865Health MAX31865::health() const
{
    const MAX31865DriverState driverState = derivedDriverState();
    return MAX31865Health{
        _state,
        driverState,
        _lastOperation,
        driverState == MAX31865DriverState::READY ||
            driverState == MAX31865DriverState::DEGRADED,
        _configurationKnown,
        _hasLastFaultStatus,
        _lastFaultStatus,
        _offlineThreshold,
        _consecutiveFailures,
        _trackedSuccessCount,
        _trackedFailureCount,
        _sampleFrameAttemptCount,
        _sampleFrameSuccessCount,
        _sampleFrameFailureCount,
        _noDataCount,
        _droppedSampleCount,
        _overrunCount,
        _busLockTimeoutCount,
        _busLockFailureCount,
        _spiTransferFailureCount,
        _chipSelectFailureCount,
        _gpioFailureCount,
        _drdyTimeoutCount,
        _operationTimeoutCount,
        _faultObservationCount,
        _thresholdFaultObservationCount,
        _referenceFaultObservationCount,
        _voltageFaultObservationCount,
        _hasLastOkMs,
        _hasLastErrorMs,
        _hasLastSampleTimestamp,
        _lastOkMs,
        _lastErrorMs,
        _lastSampleTimestampUs};
}

void MAX31865::clearLifetimeCounters()
{
    _trackedSuccessCount = 0U;
    _trackedFailureCount = 0U;
    _sampleFrameAttemptCount = 0U;
    _sampleFrameSuccessCount = 0U;
    _sampleFrameFailureCount = 0U;
    _noDataCount = 0U;
    _droppedSampleCount = 0U;
    _overrunCount = 0U;
    _busLockTimeoutCount = 0U;
    _busLockFailureCount = 0U;
    _spiTransferFailureCount = 0U;
    _chipSelectFailureCount = 0U;
    _gpioFailureCount = 0U;
    _drdyTimeoutCount = 0U;
    _operationTimeoutCount = 0U;
    _faultObservationCount = 0U;
    _thresholdFaultObservationCount = 0U;
    _referenceFaultObservationCount = 0U;
    _voltageFaultObservationCount = 0U;
}

MAX31865Status MAX31865::setOfflineThreshold(uint8_t threshold)
{
    const MAX31865Status status = threshold == 0U
        ? MAX31865Status::Error(
              MAX31865Error::InvalidArgument,
              "offline threshold must be nonzero")
        : MAX31865Status::Ok();
    if (status.ok())
    {
        _offlineThreshold = threshold;
    }
    _lastOperation = status;
    return status;
}

void MAX31865::updateTimedReadiness(uint32_t nowMs)
{
    if (_timedReadinessHalted || !_freshResultArmed ||
        (!_continuous && !_oneShotArmed) ||
        !timeReached(nowMs, _nextReadyMs))
    {
        return;
    }
    if (!_continuous)
    {
        _readyLatched = true;
        return;
    }

    const uint32_t period = continuousConversionTimeMs();
    const uint32_t completions = 1U +
        elapsedMs(nowMs, _nextReadyMs) / period;
    const uint32_t overruns = _readyLatched
        ? completions
        : completions - 1U;
    saturatingAdd(_overrunCount, overruns);
    _readyLatched = true;
    _nextReadyMs += completions * period;
}

void MAX31865::tick(uint32_t nowMs)
{
    if (_state == MAX31865State::Converting &&
        !_transport.capabilities.hasDrdy)
    {
        updateTimedReadiness(nowMs);
    }
}

MAX31865Status MAX31865::probe(MAX31865DeviceInfo &out)
{
    return probe(out, _defaultOperationTimeoutMs);
}

MAX31865Status MAX31865::probe(
    MAX31865DeviceInfo &out,
    uint32_t timeoutMs)
{
    if (_state == MAX31865State::Uninitialized)
    {
        return MAX31865Status::Error(
            MAX31865Error::NotInitialized,
            "driver is not initialized");
    }
    if (_state == MAX31865State::Converting)
    {
        return MAX31865Status::Error(
            MAX31865Error::Busy,
            "probe is not legal during conversion");
    }
    const MAX31865Status timeoutStatusValue =
        validateTimeout(timeoutMs, false);
    if (!timeoutStatusValue.ok())
    {
        return timeoutStatusValue;
    }
    MAX31865OperationContext operation = makeOperation(timeoutMs);

    const uint32_t savedLockTimeouts = _busLockTimeoutCount;
    const uint32_t savedLockFailures = _busLockFailureCount;
    const uint32_t savedTransfers = _spiTransferFailureCount;
    const uint32_t savedChipSelect = _chipSelectFailureCount;
    uint8_t config = 0U;
    uint8_t thresholds[4]{};
    uint8_t fault = 0U;
    MAX31865Status status = readPersistentImage(
        config, thresholds, operation);
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_FAULT_STATUS, &fault, 1U, operation);
    }

    _busLockTimeoutCount = savedLockTimeouts;
    _busLockFailureCount = savedLockFailures;
    _spiTransferFailureCount = savedTransfers;
    _chipSelectFailureCount = savedChipSelect;

    if (!status.ok())
    {
        if (status.code == MAX31865Error::ChipSelectFailed ||
            status.code == MAX31865Error::RestoreFailed)
        {
            _state = MAX31865State::Fault;
            invalidateObservedState();
        }
        return status;
    }

    uint8_t expectedThresholds[4]{};
    (void)max31865EncodeThreshold(
        _desiredConfig.thresholds.highCode, &expectedThresholds[0]);
    (void)max31865EncodeThreshold(
        _desiredConfig.thresholds.lowCode, &expectedThresholds[2]);
    const uint8_t expectedConfig = persistentConfigByte(
        _desiredConfig, _desiredConfig.biasEnabled, false);
    bool matches = persistentByteMatches(
        max31865_cmd::REG_CONFIG, config, expectedConfig);
    for (size_t index = 0U; index < sizeof(thresholds); ++index)
    {
        matches = matches && persistentByteMatches(
            static_cast<uint8_t>(
                max31865_cmd::REG_HIGH_FAULT_MSB + index),
            thresholds[index],
            expectedThresholds[index]);
    }

    const MAX31865DeviceInfo candidate{
        config,
        joinedRegister(&thresholds[0]),
        joinedRegister(&thresholds[2]),
        fault,
        matches};
    out = candidate;
    if (!matches)
    {
        _state = MAX31865State::Fault;
        invalidateObservedState();
        const MAX31865Status mismatch = MAX31865Status::Error(
            MAX31865Error::ProbeMismatch,
            "probe image differs from desired configuration",
            config);
        return mismatch;
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::recover(uint32_t timeoutMs)
{
    if (_state == MAX31865State::Uninitialized)
    {
        const MAX31865Status status = MAX31865Status::Error(
            MAX31865Error::NotInitialized,
            "driver is not initialized");
        _lastOperation = status;
        return status;
    }
    MAX31865Status status = validateTimeout(timeoutMs, false);
    MAX31865OperationContext operation{};
    if (!status.ok())
    {
        return finishOperation(
            status, MAX31865TrackingOutcome::Untracked, operation);
    }
    operation = makeOperation(timeoutMs);

    const MAX31865State baselineState = _state;
    const bool baselineConfigurationKnown = _configurationKnown;
    const bool baselineContinuous = _continuous;
    const bool baselineOneShotArmed = _oneShotArmed;
    const bool baselineOneShotCommitPending = _oneShotCommitPending;
    const bool baselineOneShotCommitUncertain = _oneShotCommitUncertain;
    const bool baselineFreshResultArmed = _freshResultArmed;
    const bool baselineReadyLatched = _readyLatched;
    const bool baselineTimedReadinessHalted = _timedReadinessHalted;
    const uint32_t baselineConversionStartMs = _conversionStartMs;
    const uint32_t baselineNextReadyMs = _nextReadyMs;
    const uint8_t baselineObservedValidMask = _observedValidMask;
    uint8_t baselineObserved[max31865_cmd::NUM_REGISTERS]{};
    memcpy(baselineObserved, _observedRegisters, sizeof(baselineObserved));

    status = synchronizeChipSelect(operation);
    if (status.ok() && _oneShotArmed)
    {
        status = stopInternal(operation);
    }
    if (status.ok())
    {
        status = resolveFaultCycleInternal(operation);
    }
    const uint8_t resetConfig = max31865_cmd::CONFIG_RESET;
    if (status.ok())
    {
        status = writeRegistersInternal(
            max31865_cmd::REG_CONFIG, &resetConfig, 1U, operation);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(
            _desiredConfig, false, operation);
    }
    if (status.ok())
    {
        status = clearFaultsInternal(operation);
    }
    if (status.ok())
    {
        _state = MAX31865State::Ready;
        _continuous = false;
        _oneShotArmed = false;
        _oneShotCommitPending = false;
        _oneShotCommitUncertain = false;
        _freshResultArmed = false;
        _readyLatched = false;
        _timedReadinessHalted = false;
        _configurationKnown = true;
    }
    else
    {
        // InvalidState is an action-free transport precondition by contract.
        // It can therefore retain the verified baseline under the same
        // mutation/uncertainty guards as a failure before any bus action.
        const bool transportOnlyFailure =
            status.code == MAX31865Error::InvalidState ||
            status.code == MAX31865Error::BusLockTimeout ||
            status.code == MAX31865Error::BusLockFailed ||
            status.code == MAX31865Error::SpiTransferFailed ||
            status.code == MAX31865Error::OperationTimeout ||
            status.code == MAX31865Error::TimingUnavailable;
        const bool stateProvablyUnchanged = transportOnlyFailure &&
            !operation.anyWriteMayHaveApplied &&
            !baselineOneShotCommitPending &&
            !operation.lastDeassertUnknown &&
            !operation.unsafeStateObserved;
        if (stateProvablyUnchanged)
        {
            _state = baselineState;
            _configurationKnown = baselineConfigurationKnown;
            _continuous = baselineContinuous;
            _oneShotArmed = baselineOneShotArmed;
            _oneShotCommitPending = baselineOneShotCommitPending;
            _oneShotCommitUncertain = baselineOneShotCommitUncertain;
            _freshResultArmed = baselineFreshResultArmed;
            _readyLatched = baselineReadyLatched;
            _timedReadinessHalted = baselineTimedReadinessHalted;
            _conversionStartMs = baselineConversionStartMs;
            _nextReadyMs = baselineNextReadyMs;
            _observedValidMask = baselineObservedValidMask;
            memcpy(
                _observedRegisters,
                baselineObserved,
                sizeof(baselineObserved));
        }
        else
        {
            _state = MAX31865State::Fault;
            invalidateObservedState();
        }
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::applyConfiguration(
    const MAX31865DeviceConfig &config,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    if (status.ok())
    {
        status = validateDeviceConfig(config);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(config, true, operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::readConfiguration(
    MAX31865Settings &out,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    uint8_t config = 0U;
    uint8_t thresholds[4]{};
    if (status.ok())
    {
        status = readPersistentImage(config, thresholds, operation);
    }
    if (!status.ok())
    {
        return finishOperation(status, trackingFor(status, true), operation);
    }

    uint16_t highCode = 0U;
    uint16_t lowCode = 0U;
    status = max31865DecodeThreshold(&thresholds[0], highCode);
    if (status.ok())
    {
        status = max31865DecodeThreshold(&thresholds[2], lowCode);
    }
    if (!status.ok())
    {
        return finishOperation(
            status, MAX31865TrackingOutcome::Untracked, operation);
    }

    MAX31865Settings candidate{};
    candidate.rawConfig = config;
    candidate.deviceConfig = _desiredConfig;
    candidate.deviceConfig.wireMode =
        (config & max31865_cmd::CONFIG_3WIRE) != 0U
            ? MAX31865WireMode::ThreeWire
            : (_desiredConfig.wireMode == MAX31865WireMode::TwoWire
                   ? MAX31865WireMode::TwoWire
                   : MAX31865WireMode::FourWire);
    candidate.deviceConfig.filter =
        (config & max31865_cmd::CONFIG_FILTER_50HZ) != 0U
            ? MAX31865Filter::Hz50
            : MAX31865Filter::Hz60;
    candidate.deviceConfig.biasEnabled =
        (config & max31865_cmd::CONFIG_BIAS) != 0U;
    candidate.deviceConfig.thresholds =
        MAX31865FaultThresholds{lowCode, highCode};
    candidate.conversionMode =
        (config & max31865_cmd::CONFIG_AUTO) != 0U
            ? MAX31865ConversionMode::Continuous
            : MAX31865ConversionMode::NormallyOff;
    candidate.oneShotActive =
        (config & max31865_cmd::CONFIG_ONE_SHOT) != 0U;
    candidate.faultCycle = static_cast<MAX31865FaultCycle>(
        config & max31865_cmd::CONFIG_FAULT_CYCLE_MASK);

    status = codeToResistance(lowCode, candidate.lowThresholdOhms);
    if (status.ok())
    {
        status = codeToResistance(highCode, candidate.highThresholdOhms);
    }
    if (!status.ok())
    {
        return finishOperation(
            status, MAX31865TrackingOutcome::Untracked, operation);
    }

    MAX31865Status lowTemperature = faultThresholdCodeToTemperature(
        lowCode, candidate.lowThresholdC);
    MAX31865Status highTemperature = faultThresholdCodeToTemperature(
        highCode, candidate.highThresholdC);
    candidate.lowThresholdTemperatureValid = lowTemperature.ok();
    candidate.highThresholdTemperatureValid = highTemperature.ok();
    if (!candidate.lowThresholdTemperatureValid)
    {
        candidate.lowThresholdC = NAN;
    }
    if (!candidate.highThresholdTemperatureValid)
    {
        candidate.highThresholdC = NAN;
    }
    out = candidate;
    return finishOperation(
        MAX31865Status::Ok(), MAX31865TrackingOutcome::Success, operation);
}

MAX31865Status MAX31865::configureMeasurement(
    MAX31865WireMode wireMode,
    MAX31865Filter filter,
    uint32_t timeoutMs)
{
    MAX31865DeviceConfig candidate = _desiredConfig;
    candidate.wireMode = wireMode;
    candidate.filter = filter;
    return applyConfiguration(candidate, timeoutMs);
}

MAX31865Status MAX31865::setBias(bool enabled, uint32_t timeoutMs)
{
    MAX31865DeviceConfig candidate = _desiredConfig;
    candidate.biasEnabled = enabled;
    return applyConfiguration(candidate, timeoutMs);
}

MAX31865Status MAX31865::setWireMode(
    MAX31865WireMode mode,
    uint32_t timeoutMs)
{
    MAX31865DeviceConfig candidate = _desiredConfig;
    candidate.wireMode = mode;
    return applyConfiguration(candidate, timeoutMs);
}

MAX31865Status MAX31865::setFilter(
    MAX31865Filter filter,
    uint32_t timeoutMs)
{
    MAX31865DeviceConfig candidate = _desiredConfig;
    candidate.filter = filter;
    return applyConfiguration(candidate, timeoutMs);
}

MAX31865Status MAX31865::setRtdConfig(const MAX31865RtdConfig &config)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateRtdConfig(config);
    }
    if (status.ok())
    {
        _rtdConfig = config;
    }
    _lastOperation = status;
    return status;
}

MAX31865RtdConfig MAX31865::rtdConfig() const
{
    return _rtdConfig;
}

MAX31865Status MAX31865::clearFaultsInternal(
    MAX31865OperationContext &operation)
{
    MAX31865Status status = writeRuntimeConfig(
        _desiredConfig.biasEnabled,
        false,
        max31865_cmd::CONFIG_FAULT_CLEAR,
        operation);
    if (status.ok())
    {
        // A persistent D2 condition can reassert immediately, so zero is not
        // claimed until a later FAULT_STATUS read proves it.
        _hasLastFaultStatus = false;
    }
    return status;
}

void MAX31865::recordFaultObservation(
    const MAX31865FaultStatus &fault)
{
    if (!fault.any())
    {
        return;
    }
    saturatingIncrement(_faultObservationCount);
    if (fault.highThreshold || fault.lowThreshold)
    {
        saturatingIncrement(_thresholdFaultObservationCount);
    }
    if (fault.refinHigh || fault.refinLow || fault.rtdinLow)
    {
        saturatingIncrement(_referenceFaultObservationCount);
    }
    if (fault.overUnderVoltage)
    {
        saturatingIncrement(_voltageFaultObservationCount);
    }
}

MAX31865Status MAX31865::readFaultStatusInternal(
    MAX31865FaultStatus &out,
    MAX31865OperationContext &operation,
    bool countObservation)
{
    uint8_t raw = 0U;
    MAX31865Status status = readRegistersInternal(
        max31865_cmd::REG_FAULT_STATUS, &raw, 1U, operation);
    if (!status.ok())
    {
        return status;
    }
    const MAX31865FaultStatus candidate = decodeFaultStatus(raw);
    _hasLastFaultStatus = true;
    _lastFaultStatus = candidate.raw;
    if (countObservation)
    {
        recordFaultObservation(candidate);
    }
    out = candidate;
    return candidate.any()
        ? MAX31865Status::Error(
              MAX31865Error::DeviceFault,
              "MAX31865 fault status is asserted",
              candidate.raw)
        : MAX31865Status::Ok();
}

MAX31865Status MAX31865::waitForFaultCycle(
    MAX31865OperationContext &operation)
{
    for (;;)
    {
        uint8_t config = 0U;
        MAX31865Status status = readRegistersInternal(
            max31865_cmd::REG_CONFIG, &config, 1U, operation);
        if (!status.ok())
        {
            return status;
        }
        if ((config & max31865_cmd::CONFIG_FAULT_CYCLE_MASK) ==
            max31865_cmd::CONFIG_FAULT_CYCLE_NONE)
        {
            return MAX31865Status::Ok();
        }
        status = sleepWithinDeadline(
            1000U, operation, MAX31865Error::OperationTimeout);
        if (!status.ok())
        {
            return status;
        }
    }
}

MAX31865Status MAX31865::resolveFaultCycleInternal(
    MAX31865OperationContext &operation)
{
    uint8_t config = 0U;
    MAX31865Status status = readRegistersInternal(
        max31865_cmd::REG_CONFIG, &config, 1U, operation);
    if (!status.ok())
    {
        return status;
    }

    if ((config & max31865_cmd::CONFIG_ONE_SHOT) != 0U &&
        !_oneShotArmed)
    {
        operation.unsafeStateObserved = true;
        // A pre-existing D5 is not cancellable authoritatively. Discard any
        // already-buffered result first, then quarantine the command as
        // uncertain for a fresh full horizon. This is safe whether D5 clears at
        // command acceptance or at conversion completion.
        uint8_t discarded[2]{};
        status = readRegistersInternal(
            max31865_cmd::REG_RTD_MSB,
            discarded,
            sizeof(discarded),
            operation);
        if (!status.ok())
        {
            return status;
        }
        _continuous = false;
        _oneShotArmed = true;
        _oneShotCommitPending = false;
        _oneShotCommitUncertain = true;
        _freshResultArmed = true;
        _readyLatched = false;
        _timedReadinessHalted = false;
        _conversionStartMs = _transport.nowMs(_transport.user);
        _nextReadyMs = _conversionStartMs + singleConversionTimeMs() +
            kDeadlineClockQuantizationGuardMs;
        status = stopInternal(operation);
        if (!status.ok())
        {
            return status;
        }
        status = readRegistersInternal(
            max31865_cmd::REG_CONFIG, &config, 1U, operation);
        if (!status.ok())
        {
            return status;
        }
    }

    uint8_t cycle = static_cast<uint8_t>(
        config & max31865_cmd::CONFIG_FAULT_CYCLE_MASK);
    const bool expectedBias = _continuous || _oneShotArmed ||
        _desiredConfig.biasEnabled;
    const uint8_t expectedConfig = persistentConfigByte(
        _desiredConfig, expectedBias, _continuous);
    if (cycle != max31865_cmd::CONFIG_FAULT_CYCLE_NONE ||
        (config & max31865_cmd::CONFIG_ONE_SHOT) != 0U ||
        !persistentByteMatches(
            max31865_cmd::REG_CONFIG, config, expectedConfig))
    {
        operation.unsafeStateObserved = true;
    }
    if (cycle == max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1)
    {
        // Step 1 leaves FORCE- open and D3:D2=10 until the host explicitly
        // issues step 2. Starting the waits at observation time is conservative
        // even when some or all of the required interval already elapsed.
        status = sleepWithinDeadline(
            max31865_cmd::MANUAL_FAULT_STEP1_TO_OPEN_US,
            operation,
            MAX31865Error::OperationTimeout);
        if (status.ok())
        {
            const uint64_t settle =
                static_cast<uint64_t>(
                    _rtdConfig.inputFilterTimeConstantUs) *
                max31865_cmd::MANUAL_FAULT_SETTLE_MULTIPLIER;
            status = sleepWithinDeadline(
                static_cast<uint32_t>(settle),
                operation,
                MAX31865Error::OperationTimeout);
        }
        if (status.ok())
        {
            status = writeRuntimeConfig(
                true,
                false,
                max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2,
                operation);
        }
        if (status.ok())
        {
            status = sleepWithinDeadline(
                max31865_cmd::MANUAL_FAULT_STEP2_MAX_US,
                operation,
                MAX31865Error::OperationTimeout);
        }
        cycle = max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2;
    }

    if (status.ok() && cycle != max31865_cmd::CONFIG_FAULT_CYCLE_NONE)
    {
        status = waitForFaultCycle(operation);
    }
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_CONFIG, &config, 1U, operation);
    }
    if (status.ok() &&
        (config & max31865_cmd::CONFIG_FAULT_CYCLE_MASK) !=
            max31865_cmd::CONFIG_FAULT_CYCLE_NONE)
    {
        status = MAX31865Status::Error(
            MAX31865Error::RegisterVerifyFailed,
            "fault-cycle state did not return to idle",
            config);
    }
    return status;
}

MAX31865Status MAX31865::flushRtdReadyState(
    MAX31865OperationContext &operation)
{
    uint8_t discarded[2]{};
    MAX31865Status status = readRegistersInternal(
        max31865_cmd::REG_RTD_MSB,
        discarded,
        sizeof(discarded),
        operation);
    if (status.ok() &&
        (discarded[1] & max31865_cmd::RTD_FAULT_BIT) != 0U)
    {
        // The just-cleared latch can reassert immediately while a persistent
        // voltage condition is present. Starting D5/D6 in that state would
        // create a conversion which the data sheet says the ADC will halt.
        MAX31865FaultStatus fault{};
        status = readFaultStatusInternal(fault, operation, true);
        if (!status.ok() && status.code != MAX31865Error::DeviceFault)
        {
            // D0 is already authoritative evidence of a faulted RTD frame even
            // when the category register cannot be read.
            saturatingIncrement(_faultObservationCount);
            return status;
        }
        if (!fault.any())
        {
            saturatingIncrement(_faultObservationCount);
            return MAX31865Status::Error(
                MAX31865Error::DeviceFault,
                "RTD flush reports a fault with no documented status bit");
        }
        return status;
    }
    if (status.ok() && _transport.capabilities.hasDrdy)
    {
        bool level = false;
        if (deadlineExpired(operation))
        {
            status = timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        else
        {
            status = normalizeGpioStatus(
                _transport.readPin(
                    _transport.user, MAX31865Pin::DRDY, &level));
        }
        if (!status.ok())
        {
            if (status.code == MAX31865Error::GpioFailed)
            {
                saturatingIncrement(_gpioFailureCount);
            }
        }
        else if (!level)
        {
            saturatingIncrement(_gpioFailureCount);
            status = MAX31865Status::Error(
                MAX31865Error::GpioFailed,
                "DRDY remained low after RTD acknowledgement");
        }
        if (deadlineExpired(operation))
        {
            status = timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
    }
    return status;
}

MAX31865Status MAX31865::dataReadyInternal(
    bool &out,
    MAX31865OperationContext &operation)
{
    if (_oneShotArmed &&
        (_oneShotCommitPending || _oneShotCommitUncertain))
    {
        out = false;
        return MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "uncertain one-shot command requires stop or recover");
    }
    if (!_freshResultArmed ||
        (_state != MAX31865State::Converting && !_oneShotArmed))
    {
        out = false;
        return MAX31865Status::Ok();
    }

    if (_transport.capabilities.hasDrdy)
    {
        bool level = true;
        MAX31865Status status = deadlineExpired(operation)
            ? timeoutStatus(MAX31865Error::OperationTimeout, operation)
            : normalizeGpioStatus(
                  _transport.readPin(
                      _transport.user, MAX31865Pin::DRDY, &level));
        if (!status.ok())
        {
            if (status.code == MAX31865Error::GpioFailed)
            {
                saturatingIncrement(_gpioFailureCount);
            }
        }
        if (deadlineExpired(operation))
        {
            return timeoutStatus(MAX31865Error::OperationTimeout, operation);
        }
        if (!status.ok())
        {
            return status;
        }
        out = !level;
        uint32_t readinessNowMs = 0U;
        if (!out)
        {
            readinessNowMs = _transport.nowMs(_transport.user);
            if (deadlineExpired(operation))
            {
                return timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
        }
        if (!out && timeReached(readinessNowMs, _nextReadyMs))
        {
            // A protected-input voltage fault can halt ADC updates beyond the
            // documented conversion maximum, leaving wired DRDY high forever.
            // Once that maximum has elapsed, inspect the latched always-active
            // detector so stop/recover can distinguish a real halt from an
            // ordinary not-ready level and remain recoverable.
            uint8_t rawFault = 0U;
            status = readRegistersInternal(
                max31865_cmd::REG_FAULT_STATUS,
                &rawFault,
                1U,
                operation);
            if (!status.ok())
            {
                return status;
            }
            const MAX31865FaultStatus observed =
                decodeFaultStatus(rawFault);
            _hasLastFaultStatus = true;
            _lastFaultStatus = observed.raw;
            if ((rawFault &
                 max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0U)
            {
                recordFaultObservation(observed);
                _timedReadinessHalted = true;
                return MAX31865Status::Error(
                    MAX31865Error::DeviceFault,
                    "voltage fault halted DRDY conversion readiness",
                    rawFault);
            }
        }
        return MAX31865Status::Ok();
    }

    const bool wasTimedHalted = _timedReadinessHalted;
    const uint32_t readinessNowMs = _transport.nowMs(_transport.user);
    updateTimedReadiness(readinessNowMs);
    if (deadlineExpired(operation))
    {
        return timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }
    if (!_readyLatched)
    {
        out = false;
        return MAX31865Status::Ok();
    }

    // Elapsed maximum timing is authoritative only while the ADC has not been
    // halted by the always-active protected-input voltage detector. D2 is
    // latched, so it also catches a transient halt which ended before polling.
    uint8_t rawFault = 0U;
    MAX31865Status status = readRegistersInternal(
        max31865_cmd::REG_FAULT_STATUS, &rawFault, 1U, operation);
    if (!status.ok())
    {
        return status;
    }
    const MAX31865FaultStatus observed = decodeFaultStatus(rawFault);
    _hasLastFaultStatus = true;
    _lastFaultStatus = observed.raw;
    if ((rawFault & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0U)
    {
        recordFaultObservation(observed);
        _timedReadinessHalted = true;
        out = false;
        return MAX31865Status::Error(
            MAX31865Error::DeviceFault,
            "voltage fault halted elapsed-time conversion readiness",
            rawFault);
    }
    if (wasTimedHalted)
    {
        // The ADC may only just have resumed after a D2 condition/clear. The
        // old nominal latch is stale; require a fresh guarded full horizon.
        _timedReadinessHalted = false;
        _readyLatched = false;
        _conversionStartMs = _transport.nowMs(_transport.user);
        _nextReadyMs = _conversionStartMs + singleConversionTimeMs() +
            kDeadlineClockQuantizationGuardMs;
        out = false;
        return deadlineExpired(operation)
            ? timeoutStatus(MAX31865Error::OperationTimeout, operation)
            : MAX31865Status::Ok();
    }
    out = true;
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::waitForReadyInternal(
    MAX31865OperationContext &operation)
{
    for (;;)
    {
        bool ready = false;
        MAX31865Status status = dataReadyInternal(ready, operation);
        if (!status.ok())
        {
            return status;
        }
        if (ready)
        {
            return MAX31865Status::Ok();
        }
        if (operation.timeoutMs == 0U)
        {
            saturatingIncrement(_noDataCount);
            return MAX31865Status::Error(
                MAX31865Error::NoData,
                "fresh conversion is not ready");
        }
        status = sleepWithinDeadline(
            1000U,
            operation,
            _transport.capabilities.hasDrdy
                ? MAX31865Error::DrdyTimeout
                : MAX31865Error::OperationTimeout);
        if (!status.ok())
        {
            return status;
        }
    }
}

MAX31865Status MAX31865::startContinuousInternal(
    MAX31865OperationContext &operation)
{
    MAX31865Status status = clearFaultsInternal(operation);
    bool runtimeMutationAttempted = false;
    if (status.ok())
    {
        status = writeRuntimeConfig(true, false, 0U, operation);
        runtimeMutationAttempted = status.ok() ||
            operation.lastTransferAttempted;
    }
    if (status.ok())
    {
        status = sleepWithinDeadline(
            biasSettleTimeUs(),
            operation,
            MAX31865Error::OperationTimeout);
    }
    if (status.ok())
    {
        // Reading RTD data is the documented DRDY-high acknowledgement. This
        // prevents a stale-low level from satisfying the new START barrier.
        status = flushRtdReadyState(operation);
    }
    if (status.ok())
    {
        status = writeRuntimeConfig(true, true, 0U, operation);
    }
    if (!status.ok())
    {
        return runtimeMutationAttempted
            ? restoreIdleAfterPrimaryFailure(status, operation)
            : status;
    }

    _continuous = true;
    _oneShotArmed = false;
    _oneShotCommitPending = false;
    _oneShotCommitUncertain = false;
    _freshResultArmed = true;
    _readyLatched = false;
    _timedReadinessHalted = false;
    _conversionStartMs = _transport.nowMs(_transport.user);
    _nextReadyMs = _conversionStartMs + singleConversionTimeMs() +
        kDeadlineClockQuantizationGuardMs;
    _state = MAX31865State::Converting;
    if (deadlineExpired(operation))
    {
        const MAX31865Status timeout = timeoutStatus(
            MAX31865Error::OperationTimeout, operation);
        return restoreIdleAfterPrimaryFailure(timeout, operation);
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::triggerSingleInternal(
    MAX31865OperationContext &operation)
{
    MAX31865Status status = clearFaultsInternal(operation);
    bool runtimeMutationAttempted = false;
    if (status.ok())
    {
        status = writeRuntimeConfig(true, false, 0U, operation);
        runtimeMutationAttempted = status.ok() ||
            operation.lastTransferAttempted;
    }
    if (status.ok())
    {
        status = sleepWithinDeadline(
            biasSettleTimeUs(),
            operation,
            MAX31865Error::OperationTimeout);
    }
    if (status.ok())
    {
        status = flushRtdReadyState(operation);
    }
    if (status.ok())
    {
        status = writeRuntimeConfig(
            true,
            false,
            max31865_cmd::CONFIG_ONE_SHOT,
            operation);
        if (!status.ok() && operation.lastTransferAttempted)
        {
            // The transfer may have committed D5 on CS rising even though its
            // callback failed. Quarantine new starts until the documented
            // maximum has elapsed and stop() can discard that possible result.
            _continuous = false;
            _oneShotArmed = true;
            _oneShotCommitPending = operation.lastDeassertUnknown;
            _oneShotCommitUncertain = !operation.lastTransferApplied;
            _freshResultArmed = true;
            _readyLatched = false;
            _timedReadinessHalted = false;
            _conversionStartMs = _transport.nowMs(_transport.user);
            _nextReadyMs = _conversionStartMs + singleConversionTimeMs() +
                kDeadlineClockQuantizationGuardMs;
            _state = MAX31865State::Converting;
        }
    }
    if (!status.ok())
    {
        return runtimeMutationAttempted
            ? restoreIdleAfterPrimaryFailure(status, operation)
            : status;
    }

    _continuous = false;
    _oneShotArmed = true;
    _oneShotCommitPending = false;
    _oneShotCommitUncertain = false;
    _freshResultArmed = true;
    _readyLatched = false;
    _timedReadinessHalted = false;
    _conversionStartMs = _transport.nowMs(_transport.user);
    _nextReadyMs = _conversionStartMs + singleConversionTimeMs() +
        kDeadlineClockQuantizationGuardMs;
    _state = MAX31865State::Converting;
    if (deadlineExpired(operation))
    {
        const MAX31865Status timeout = timeoutStatus(
            MAX31865Error::OperationTimeout, operation);
        return restoreIdleAfterPrimaryFailure(timeout, operation);
    }
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::stopInternal(
    MAX31865OperationContext &operation)
{
    if (_oneShotArmed &&
        (_oneShotCommitPending || _chipSelectUncertain))
    {
        const MAX31865Status synchronization =
            synchronizeChipSelect(operation);
        if (!synchronization.ok())
        {
            return synchronization;
        }
    }
    uint8_t noWaitPassesRemaining = kNoWaitStopPassLimit;
    while (_oneShotArmed)
    {
        if (operation.timeoutMs == 0U)
        {
            if (noWaitPassesRemaining == 0U)
            {
                return timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
            --noWaitPassesRemaining;
        }
        if (_oneShotCommitUncertain)
        {
            const uint32_t now = _transport.nowMs(_transport.user);
            if (deadlineExpired(operation))
            {
                return timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
            if (!timeReached(now, _nextReadyMs))
            {
                const MAX31865Status wait = sleepWithinDeadline(
                    1000U,
                    operation,
                    MAX31865Error::OperationTimeout);
                if (!wait.ok())
                {
                    return wait;
                }
                continue;
            }

            bool committedResultReady = false;
            if (_transport.capabilities.hasDrdy)
            {
                bool level = true;
                MAX31865Status gpio = deadlineExpired(operation)
                    ? timeoutStatus(
                          MAX31865Error::OperationTimeout, operation)
                    : normalizeGpioStatus(
                          _transport.readPin(
                              _transport.user,
                              MAX31865Pin::DRDY,
                              &level));
                if (!gpio.ok())
                {
                    if (gpio.code == MAX31865Error::GpioFailed)
                    {
                        saturatingIncrement(_gpioFailureCount);
                    }
                }
                if (deadlineExpired(operation))
                {
                    return timeoutStatus(
                        MAX31865Error::OperationTimeout, operation);
                }
                if (!gpio.ok())
                {
                    return gpio;
                }
                committedResultReady = !level;
            }

            uint8_t rawFault = 0U;
            const bool wasTimedHalted = _timedReadinessHalted;
            MAX31865Status status = readRegistersInternal(
                max31865_cmd::REG_FAULT_STATUS,
                &rawFault,
                1U,
                operation);
            if (!status.ok())
            {
                return status;
            }
            const MAX31865FaultStatus observed =
                decodeFaultStatus(rawFault);
            _hasLastFaultStatus = true;
            _lastFaultStatus = observed.raw;
            if ((rawFault & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0U)
            {
                _timedReadinessHalted = true;
                recordFaultObservation(observed);
                status = writeRuntimeConfig(
                    true,
                    false,
                    max31865_cmd::CONFIG_FAULT_CLEAR,
                    operation);
                if (status.ok())
                {
                    status = readRegistersInternal(
                        max31865_cmd::REG_FAULT_STATUS,
                        &rawFault,
                        1U,
                        operation);
                }
                if (!status.ok())
                {
                    return status;
                }
                _hasLastFaultStatus = true;
                _lastFaultStatus = rawFault;
                if ((rawFault &
                     max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0U)
                {
                    return MAX31865Status::Error(
                        MAX31865Error::DeviceFault,
                        "voltage fault still halts the uncertain one-shot",
                        rawFault);
                }
                _conversionStartMs = _transport.nowMs(_transport.user);
                _nextReadyMs =
                    _conversionStartMs + singleConversionTimeMs() +
                    kDeadlineClockQuantizationGuardMs;
                _readyLatched = false;
                _timedReadinessHalted = false;
                if (deadlineExpired(operation))
                {
                    return timeoutStatus(
                        MAX31865Error::OperationTimeout, operation);
                }
                continue;
            }

            if (wasTimedHalted)
            {
                _conversionStartMs = _transport.nowMs(_transport.user);
                _nextReadyMs =
                    _conversionStartMs + singleConversionTimeMs() +
                    kDeadlineClockQuantizationGuardMs;
                _readyLatched = false;
                _timedReadinessHalted = false;
                if (deadlineExpired(operation))
                {
                    return timeoutStatus(
                        MAX31865Error::OperationTimeout, operation);
                }
                continue;
            }

            if (_transport.capabilities.hasDrdy && committedResultReady)
            {
                _oneShotCommitUncertain = false;
                continue;
            }

            // With no D2 halt, the documented maximum has elapsed. A wired
            // high DRDY proves the uncertain command did not produce a result;
            // without DRDY either no command or a completed result is safe to
            // discard. Never publish data from this uncertain path.
            _oneShotArmed = false;
            _oneShotCommitPending = false;
            _oneShotCommitUncertain = false;
            _freshResultArmed = false;
            _readyLatched = false;
            _timedReadinessHalted = false;
            break;
        }

        bool ready = false;
        MAX31865Status status = dataReadyInternal(ready, operation);
        if (!status.ok() &&
            status.code == MAX31865Error::DeviceFault &&
            (static_cast<uint8_t>(status.detail) &
             max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0U)
        {
            // D2 means the nominal 55/66 ms maximum did not apply. If the
            // physical condition has since cleared, clear the latch and start
            // a fresh full conversion horizon before discarding the result.
            status = writeRuntimeConfig(
                true,
                false,
                max31865_cmd::CONFIG_FAULT_CLEAR,
                operation);
            uint8_t rawFault = 0U;
            if (status.ok())
            {
                status = readRegistersInternal(
                    max31865_cmd::REG_FAULT_STATUS,
                    &rawFault,
                    1U,
                    operation);
            }
            if (!status.ok())
            {
                return status;
            }
            const MAX31865FaultStatus fault = decodeFaultStatus(rawFault);
            _hasLastFaultStatus = true;
            _lastFaultStatus = fault.raw;
            if (fault.overUnderVoltage)
            {
                return MAX31865Status::Error(
                    MAX31865Error::DeviceFault,
                    "voltage fault still halts the armed one-shot",
                    rawFault);
            }
            _conversionStartMs = _transport.nowMs(_transport.user);
            _nextReadyMs = _conversionStartMs + singleConversionTimeMs() +
                kDeadlineClockQuantizationGuardMs;
            _readyLatched = false;
            _timedReadinessHalted = false;
            if (deadlineExpired(operation))
            {
                return timeoutStatus(
                    MAX31865Error::OperationTimeout, operation);
            }
            continue;
        }
        if (!status.ok())
        {
            return status;
        }
        if (ready)
        {
            uint8_t discarded[2]{};
            status = readRegistersInternal(
                max31865_cmd::REG_RTD_MSB,
                discarded,
                sizeof(discarded),
                operation);
            if (operation.lastTransferAttempted)
            {
                // RTD access may already have acknowledged DRDY/consumed the
                // only one-shot result. Recovery must never roll local
                // freshness back to its pre-stop snapshot after this point.
                operation.unsafeStateObserved = true;
            }
            _oneShotArmed = false;
            _oneShotCommitPending = false;
            _oneShotCommitUncertain = false;
            _freshResultArmed = false;
            _readyLatched = false;
            _timedReadinessHalted = false;
            if (!status.ok())
            {
                _state = MAX31865State::Fault;
                invalidateObservedState();
                return status;
            }
            break;
        }
        const MAX31865Status wait = sleepWithinDeadline(
            1000U,
            operation,
            MAX31865Error::OperationTimeout);
        if (!wait.ok())
        {
            return wait;
        }
    }

    MAX31865Status status = writeRuntimeConfig(
        _desiredConfig.biasEnabled, false, 0U, operation);
    if (!status.ok())
    {
        return status;
    }
    _continuous = false;
    _oneShotArmed = false;
    _oneShotCommitPending = false;
    _oneShotCommitUncertain = false;
    _freshResultArmed = false;
    _readyLatched = false;
    _timedReadinessHalted = false;
    _state = _configurationKnown
        ? MAX31865State::Ready
        : MAX31865State::Fault;
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::restoreIdleAfterPrimaryFailure(
    const MAX31865Status &primary,
    MAX31865OperationContext &operation)
{
    // Normal cleanup remains inside the caller's one whole-operation budget.
    // Once that budget has expired, perform only one finite no-wait safety
    // attempt: fixed frames are still allowed, but no callback-owned wait or
    // retry receives a second deadline.
    const bool cleanupAfterDeadline = deadlineExpired(operation);
    if (cleanupAfterDeadline)
    {
        (void)timeoutStatus(MAX31865Error::OperationTimeout, operation);
    }
    MAX31865OperationContext cleanupOperation = operation;
    if (cleanupAfterDeadline)
    {
        cleanupOperation.timeoutMs = 0U;
    }
    cleanupOperation.lastTransferAttempted = false;
    cleanupOperation.lastTransferApplied = false;
    cleanupOperation.lastFrameCommitted = false;
    cleanupOperation.lastDeassertUnknown = false;
    const MAX31865Status cleanup = stopInternal(cleanupOperation);
    mergeTimeoutObservations(operation, cleanupOperation);
    if (!cleanup.ok())
    {
        _state = MAX31865State::Fault;
        invalidateObservedState();
        return MAX31865Status::Error(
            MAX31865Error::RestoreFailed,
            "conversion setup and idle-state restore both failed",
            combinedFailureDetail(primary.code, cleanup.code));
    }
    return primary;
}

MAX31865Status MAX31865::restoreIdleAfterConsumedOneShot(
    const MAX31865Status &primary,
    MAX31865OperationContext &operation)
{
    const bool cleanupAfterDeadline = deadlineExpired(operation);
    MAX31865Status result = primary;
    if (cleanupAfterDeadline)
    {
        if (result.ok())
        {
            result = timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        else
        {
            (void)timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
    }
    MAX31865OperationContext cleanupOperation = operation;
    if (cleanupAfterDeadline)
    {
        cleanupOperation.timeoutMs = 0U;
    }
    cleanupOperation.lastTransferAttempted = false;
    cleanupOperation.lastTransferApplied = false;
    cleanupOperation.lastFrameCommitted = false;
    cleanupOperation.lastDeassertUnknown = false;
    const MAX31865Status cleanup = stopInternal(cleanupOperation);
    mergeTimeoutObservations(operation, cleanupOperation);
    if (cleanup.ok())
    {
        if (primary.ok() && !result.ok())
        {
            // The frame was valid and consumed, but the complete public
            // acquisition missed its deadline and cannot publish it.
            saturatingIncrement(_droppedSampleCount);
        }
        return result;
    }

    if (primary.ok())
    {
        // The RTD frame itself was valid and remains counted as such, but the
        // composite acquisition cannot publish it without a proven idle image.
        // The next delivered sample counter therefore exposes the same gap.
        saturatingIncrement(_droppedSampleCount);
    }

    // The conversion frame has already been consumed, but the actual idle
    // VBIAS state no longer matches the desired image until recover() proves it.
    _state = MAX31865State::Fault;
    invalidateObservedState();
    return result.ok()
        ? cleanup
        : MAX31865Status::Error(
              MAX31865Error::RestoreFailed,
              "one-shot result and idle-state restore both failed",
              combinedFailureDetail(result.code, cleanup.code));
}

MAX31865Status MAX31865::startContinuous(uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = startContinuousInternal(operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::triggerSingleConversion(uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = triggerSingleInternal(operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::stop(uint32_t timeoutMs)
{
    MAX31865Status status = requireOperational();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = stopInternal(operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::dataReady(bool &out)
{
    return dataReady(out, _defaultOperationTimeoutMs);
}

MAX31865Status MAX31865::dataReady(bool &out, uint32_t timeoutMs)
{
    MAX31865Status status = requireOperational();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok() && !_freshResultArmed)
    {
        // Match the managed ADS-style availability contract: a cached
        // no-conversion result is zero-I/O and does not create a protocol
        // operation merely to sample the clock.
        out = false;
        return finishOperation(
            MAX31865Status::Ok(),
            MAX31865TrackingOutcome::Untracked,
            operation);
    }
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    bool candidate = false;
    if (status.ok())
    {
        status = dataReadyInternal(candidate, operation);
    }
    if (status.ok())
    {
        out = candidate;
    }
    return finishOperation(
        status,
        status.ok()
            ? (operation.lastFrameCommitted
                   ? MAX31865TrackingOutcome::Success
                   : MAX31865TrackingOutcome::Untracked)
            : trackingFor(status, false),
        operation);
}

void MAX31865::commitSample(
    MAX31865Sample &out,
    MAX31865Sample &candidate)
{
    out = candidate;
    if ((candidate.flags & MAX31865_SAMPLE_FLAG_DATA_VALID) != 0U &&
        (candidate.flags & MAX31865_SAMPLE_FLAG_READ_TIMESTAMP) != 0U)
    {
        _hasLastSampleTimestamp = true;
        _lastSampleTimestampUs = candidate.readTimestampUs;
    }
}

MAX31865Status MAX31865::readSampleInternal(
    MAX31865Sample &out,
    const MAX31865ReadOptions *options,
    MAX31865OperationContext &operation,
    bool requireFresh)
{
    if (requireFresh && !_freshResultArmed)
    {
        return MAX31865Status::Error(
            MAX31865Error::NoData,
            "no fresh conversion is armed");
    }
    if (_state == MAX31865State::Converting &&
        _oneShotArmed && !requireFresh)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "immediate read is unsafe during an armed one-shot");
    }

    const auto consumeConversion = [this, &operation]() -> MAX31865Status
    {
        if (_continuous)
        {
            const uint32_t now = _transport.nowMs(_transport.user);
            if (!_transport.capabilities.hasDrdy)
            {
                // Advance cadence/overrun bookkeeping before clearing the
                // latch represented by the buffered result just consumed.
                updateTimedReadiness(now);
            }
            _readyLatched = false;
            _oneShotArmed = false;
            _oneShotCommitPending = false;
            _oneShotCommitUncertain = false;
            _freshResultArmed = true;
            _timedReadinessHalted = false;
            if (_transport.capabilities.hasDrdy)
            {
                _conversionStartMs = now;
                _nextReadyMs = now + continuousConversionTimeMs();
            }
            if (_configurationKnown && _state != MAX31865State::Fault)
            {
                _state = MAX31865State::Converting;
            }
        }
        else
        {
            _readyLatched = false;
            _oneShotArmed = false;
            _oneShotCommitPending = false;
            _oneShotCommitUncertain = false;
            _freshResultArmed = false;
            _timedReadinessHalted = false;
            if (_configurationKnown && _state != MAX31865State::Fault)
            {
                _state = MAX31865State::Ready;
            }
        }
        return deadlineExpired(operation)
            ? timeoutStatus(MAX31865Error::OperationTimeout, operation)
            : MAX31865Status::Ok();
    };

    saturatingIncrement(_sampleFrameAttemptCount);
    uint8_t bytes[2]{};
    MAX31865Status status = readRegistersInternal(
        max31865_cmd::REG_RTD_MSB, bytes, sizeof(bytes), operation);
    if (!status.ok())
    {
        saturatingIncrement(_sampleFrameFailureCount);
        saturatingIncrement(_droppedSampleCount);
        if (operation.lastFrameCommitted)
        {
            // An RTD read that crossed the deadline still acknowledged DRDY.
            // Consume that result even though its bytes cannot be committed.
            (void)consumeConversion();
        }
        else if (operation.lastTransferAttempted)
        {
            // A failed transfer may have reached the device. Freshness is now
            // ambiguous, so require explicit recovery instead of risking a
            // stale one-shot result.
            _continuous = false;
            _oneShotArmed = false;
            _oneShotCommitPending = false;
            _oneShotCommitUncertain = false;
            _freshResultArmed = false;
            _readyLatched = false;
            _timedReadinessHalted = false;
            _state = MAX31865State::Fault;
            invalidateObservedState();
        }
        return status;
    }

    MAX31865Sample candidate{};
    bool faultFlag = false;
    status = max31865DecodeRtd(
        bytes,
        sizeof(bytes),
        candidate.rawRegister,
        candidate.rawCode,
        faultFlag);
    if (!status.ok())
    {
        saturatingIncrement(_sampleFrameFailureCount);
        saturatingIncrement(_droppedSampleCount);
        return status;
    }
    candidate.flags = MAX31865_SAMPLE_FLAG_FRAME_VALID;
    if (_transport.nowUs != nullptr)
    {
        candidate.readTimestampUs = _transport.nowUs(_transport.user);
        if (deadlineExpired(operation))
        {
            saturatingIncrement(_sampleFrameFailureCount);
            saturatingIncrement(_droppedSampleCount);
            if (faultFlag)
            {
                // RTD D0 is already committed fault evidence even though the
                // deadline prevents a category read and sample publication.
                saturatingIncrement(_faultObservationCount);
            }
            (void)consumeConversion();
            return timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        candidate.flags = static_cast<uint8_t>(
            candidate.flags | MAX31865_SAMPLE_FLAG_READ_TIMESTAMP);
    }
    if (options != nullptr)
    {
        candidate.channelId = options->channelId;
        if (options->hasReadyTimestamp)
        {
            candidate.readyTimestampUs = options->readyTimestampUs;
            candidate.flags = static_cast<uint8_t>(
                candidate.flags | MAX31865_SAMPLE_FLAG_READY_TIMESTAMP);
        }
    }

    if (faultFlag)
    {
        MAX31865FaultStatus fault{};
        status = readFaultStatusInternal(fault, operation, true);
        if (!status.ok() && status.code != MAX31865Error::DeviceFault)
        {
            saturatingIncrement(_sampleFrameFailureCount);
            saturatingIncrement(_droppedSampleCount);
            // RTD D0 already proves one fault observation even if the
            // category-register transaction fails.
            saturatingIncrement(_faultObservationCount);
            const MAX31865Status consumption = consumeConversion();
            return consumption.ok() ? status : consumption;
        }
        if (!fault.any())
        {
            saturatingIncrement(_faultObservationCount);
        }
        candidate.faultStatus = fault;
        candidate.flags = static_cast<uint8_t>(
            candidate.flags | MAX31865_SAMPLE_FLAG_FAULT_STATUS);
        candidate.sampleCounter = _sampleCounter;
        saturatingIncrement(_sampleFrameFailureCount);
        saturatingIncrement(_droppedSampleCount);
        const MAX31865Status consumption = consumeConversion();
        if (_continuous && fault.overUnderVoltage)
        {
            _timedReadinessHalted = true;
        }
        if (!consumption.ok())
        {
            return consumption;
        }
        // The owning public operation may still require one-shot idle
        // restoration. Stage the frame locally; only that outer commit may
        // update caller output and the last committed sample timestamp.
        out = candidate;
        return MAX31865Status::Error(
            MAX31865Error::DeviceFault,
            fault.any()
                ? "RTD frame reports a decoded MAX31865 fault"
                : "RTD frame fault flag has no documented status bit",
            fault.raw);
    }

    status = codeToResistance(candidate.rawCode, candidate.resistanceOhms);
    if (status.ok())
    {
        status = resistanceToTemperature(
            candidate.resistanceOhms, candidate.temperatureC);
    }
    if (!status.ok())
    {
        saturatingIncrement(_sampleFrameFailureCount);
        saturatingIncrement(_droppedSampleCount);
        const MAX31865Status consumption = consumeConversion();
        return consumption.ok() ? status : consumption;
    }

    const MAX31865Status consumption = consumeConversion();
    if (!consumption.ok())
    {
        saturatingIncrement(_sampleFrameFailureCount);
        saturatingIncrement(_droppedSampleCount);
        return consumption;
    }
    if (_sampleCounter < UINT32_MAX)
    {
        ++_sampleCounter;
    }
    candidate.sampleCounter = _sampleCounter;
    candidate.flags = static_cast<uint8_t>(
        candidate.flags | MAX31865_SAMPLE_FLAG_DATA_VALID);
    saturatingIncrement(_sampleFrameSuccessCount);
    out = candidate;
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::poll(
    MAX31865Sample &out,
    const MAX31865ReadOptions *options)
{
    return poll(out, _defaultOperationTimeoutMs, options);
}

MAX31865Status MAX31865::poll(
    MAX31865Sample &out,
    uint32_t timeoutMs,
    const MAX31865ReadOptions *options)
{
    MAX31865Status status = requireOperational();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok() && !_freshResultArmed)
    {
        // This is a local availability observation, not a timed protocol
        // operation. Preserve the MAX31865 NoData/counter contract without a
        // gratuitous nowMs callback.
        saturatingIncrement(_noDataCount);
        return finishOperation(
            MAX31865Status::Error(
                MAX31865Error::NoData,
                "fresh conversion is not ready"),
            MAX31865TrackingOutcome::Untracked,
            operation);
    }
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    MAX31865Sample candidate{};
    const bool wasOneShot = _oneShotArmed && !_continuous;
    bool ready = false;
    if (status.ok())
    {
        status = dataReadyInternal(ready, operation);
    }
    if (status.ok() && !ready)
    {
        saturatingIncrement(_noDataCount);
        status = MAX31865Status::Error(
            MAX31865Error::NoData,
            "fresh conversion is not ready");
    }
    if (status.ok())
    {
        status = readSampleInternal(candidate, options, operation, true);
    }
    if (wasOneShot && !_oneShotArmed)
    {
        status = restoreIdleAfterConsumedOneShot(status, operation);
    }
    if ((status.ok() || status.code == MAX31865Error::DeviceFault) &&
        (candidate.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U)
    {
        commitSample(out, candidate);
    }
    const MAX31865TrackingOutcome tracking =
        status.code == MAX31865Error::NoData &&
            operation.lastFrameCommitted
        ? MAX31865TrackingOutcome::Success
        : trackingFor(status, true);
    return finishOperation(status, tracking, operation);
}

MAX31865Status MAX31865::readSample(
    MAX31865Sample &out,
    const MAX31865ReadOptions *options)
{
    return readSample(out, _defaultOperationTimeoutMs, options);
}

MAX31865Status MAX31865::readSample(
    MAX31865Sample &out,
    uint32_t timeoutMs,
    const MAX31865ReadOptions *options)
{
    MAX31865Status status = requireOperational();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    if (status.ok() && _state == MAX31865State::Converting &&
        _oneShotArmed)
    {
        status = MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "immediate read is unsafe during an armed one-shot");
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    MAX31865Sample candidate{};
    if (status.ok())
    {
        status = readSampleInternal(candidate, options, operation, false);
    }
    if ((status.ok() || status.code == MAX31865Error::DeviceFault) &&
        (candidate.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U)
    {
        commitSample(out, candidate);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::readSingle(
    MAX31865Sample &out,
    uint32_t timeoutMs,
    const MAX31865ReadOptions *options)
{
    MAX31865Status status = requireOperational();
    if (status.ok() &&
        (_state != MAX31865State::Converting || !_freshResultArmed))
    {
        status = MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "readSingle requires an armed conversion");
    }
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, true);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    MAX31865Sample candidate{};
    const bool wasOneShot = _oneShotArmed && !_continuous;
    if (status.ok())
    {
        status = waitForReadyInternal(operation);
    }
    if (status.ok())
    {
        status = readSampleInternal(candidate, options, operation, true);
    }
    if (wasOneShot && !_oneShotArmed)
    {
        status = restoreIdleAfterConsumedOneShot(status, operation);
    }
    if ((status.ok() || status.code == MAX31865Error::DeviceFault) &&
        (candidate.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U)
    {
        commitSample(out, candidate);
    }
    const MAX31865TrackingOutcome tracking =
        status.code == MAX31865Error::NoData &&
            operation.lastFrameCommitted
        ? MAX31865TrackingOutcome::Success
        : trackingFor(status, true);
    return finishOperation(status, tracking, operation);
}

MAX31865Status MAX31865::readOneShot(
    MAX31865Sample &out,
    uint32_t timeoutMs,
    const MAX31865ReadOptions *options)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    bool started = false;
    MAX31865Sample candidate{};
    if (status.ok())
    {
        status = triggerSingleInternal(operation);
        started = status.ok();
    }
    if (status.ok())
    {
        status = waitForReadyInternal(operation);
    }
    if (status.ok())
    {
        status = readSampleInternal(candidate, options, operation, true);
    }

    // A plain readiness timeout deliberately leaves the one-shot armed so the
    // caller can finish it with readSingle() or discard it with stop(). Once an
    // RTD read consumed/ambiguously consumed the frame, restore idle VBIAS.
    if (started && !_oneShotArmed)
    {
        status = restoreIdleAfterConsumedOneShot(status, operation);
    }
    if ((status.ok() || status.code == MAX31865Error::DeviceFault) &&
        (candidate.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U)
    {
        commitSample(out, candidate);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::readFaultStatus(
    MAX31865FaultStatus &out,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireOperational();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, true);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    MAX31865FaultStatus candidate{};
    if (status.ok())
    {
        status = readFaultStatusInternal(candidate, operation, true);
    }
    if (status.ok() || status.code == MAX31865Error::DeviceFault)
    {
        out = candidate;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::clearFaults(uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = clearFaultsInternal(operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::runAutomaticFaultDetection(
    MAX31865FaultStatus &out,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    if (status.ok() &&
        _rtdConfig.inputFilterTimeConstantUs >
            max31865_cmd::AUTO_FAULT_MAX_RC_US)
    {
        status = MAX31865Status::Error(
            MAX31865Error::UnsupportedCommand,
            "automatic fault detection requires RC <= 100 us",
            static_cast<int32_t>(_rtdConfig.inputFilterTimeConstantUs));
    }

    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    MAX31865FaultStatus candidate{};
    bool sequenceEntered = false;
    if (status.ok())
    {
        status = clearFaultsInternal(operation);
    }
    if (status.ok())
    {
        status = writeRuntimeConfig(true, false, 0U, operation);
        sequenceEntered = status.ok() || operation.lastTransferAttempted;
    }
    if (status.ok())
    {
        status = sleepWithinDeadline(
            biasSettleTimeUs(),
            operation,
            MAX31865Error::OperationTimeout);
    }
    if (status.ok())
    {
        status = writeRuntimeConfig(
            true,
            false,
            max31865_cmd::CONFIG_FAULT_CYCLE_AUTO,
            operation);
    }
    if (status.ok())
    {
        status = sleepWithinDeadline(
            max31865_cmd::AUTO_FAULT_DETECTION_MAX_US,
            operation,
            MAX31865Error::OperationTimeout);
    }
    if (status.ok())
    {
        status = waitForFaultCycle(operation);
    }
    if (status.ok())
    {
        status = readFaultStatusInternal(candidate, operation, true);
    }

    if (sequenceEntered)
    {
        // Once a transient CONFIG command may have committed, make one
        // immediate safety-restoration attempt even when the caller's primary
        // budget has just expired. Timeout zero is the transport's no-wait
        // sentinel: fixed frames remain possible, but fault-cycle sleeps and
        // callback-owned waiting do not receive a second deadline.
        const bool cleanupAfterDeadline = deadlineExpired(operation);
        if (cleanupAfterDeadline && status.ok())
        {
            status = timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        else if (cleanupAfterDeadline)
        {
            // Preserve an already-meaningful primary result while retaining
            // the fact that the composite operation crossed its deadline.
            (void)timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        MAX31865OperationContext cleanupOperation = operation;
        if (cleanupAfterDeadline)
        {
            cleanupOperation.timeoutMs = 0U;
        }
        cleanupOperation.lastTransferAttempted = false;
        cleanupOperation.lastTransferApplied = false;
        cleanupOperation.lastFrameCommitted = false;
        cleanupOperation.lastDeassertUnknown = false;

        MAX31865Status cleanup =
            resolveFaultCycleInternal(cleanupOperation);
        if (cleanup.ok())
        {
            cleanup = writeRuntimeConfig(
                _desiredConfig.biasEnabled, false, 0U, cleanupOperation);
        }
        if (cleanup.ok() && _desiredConfig.biasEnabled)
        {
            cleanup = sleepWithinDeadline(
                postFaultSettleUsFor(_rtdConfig),
                cleanupOperation,
                MAX31865Error::OperationTimeout);
        }
        if (cleanup.ok())
        {
            cleanup = verifyDesiredImage(cleanupOperation);
        }
        mergeTimeoutObservations(operation, cleanupOperation);
        if (cleanup.ok())
        {
            _state = MAX31865State::Ready;
            _continuous = false;
            _oneShotArmed = false;
            _oneShotCommitPending = false;
            _oneShotCommitUncertain = false;
            _freshResultArmed = false;
            _readyLatched = false;
            _timedReadinessHalted = false;
        }
        else
        {
            _state = MAX31865State::Fault;
            invalidateObservedState();
            const MAX31865Error primaryCode = status.code;
            status = status.ok()
                ? cleanup
                : MAX31865Status::Error(
                      MAX31865Error::RestoreFailed,
                      "fault cycle and configuration restore both failed",
                      combinedFailureDetail(primaryCode, cleanup.code));
        }
    }
    if (status.ok() || status.code == MAX31865Error::DeviceFault)
    {
        out = candidate;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::runManualFaultDetection(
    MAX31865FaultStatus &out,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    MAX31865FaultStatus candidate{};
    bool sequenceEntered = false;
    if (status.ok())
    {
        status = clearFaultsInternal(operation);
    }
    if (status.ok())
    {
        status = writeRuntimeConfig(true, false, 0U, operation);
        sequenceEntered = status.ok() || operation.lastTransferAttempted;
    }
    if (status.ok())
    {
        status = sleepWithinDeadline(
            biasSettleTimeUs(),
            operation,
            MAX31865Error::OperationTimeout);
    }
    if (status.ok())
    {
        status = writeRuntimeConfig(
            true,
            false,
            max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1,
            operation);
    }
    if (status.ok())
    {
        // Figure 4 performs two internal 100 us phases before FORCE- opens.
        status = sleepWithinDeadline(
            max31865_cmd::MANUAL_FAULT_STEP1_TO_OPEN_US,
            operation,
            MAX31865Error::OperationTimeout);
    }
    if (status.ok())
    {
        const uint64_t settle =
            static_cast<uint64_t>(_rtdConfig.inputFilterTimeConstantUs) *
            max31865_cmd::MANUAL_FAULT_SETTLE_MULTIPLIER;
        status = sleepWithinDeadline(
            static_cast<uint32_t>(settle),
            operation,
            MAX31865Error::OperationTimeout);
    }
    if (status.ok())
    {
        status = writeRuntimeConfig(
            true,
            false,
            max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2,
            operation);
    }
    if (status.ok())
    {
        status = sleepWithinDeadline(
            max31865_cmd::MANUAL_FAULT_STEP2_MAX_US,
            operation,
            MAX31865Error::OperationTimeout);
    }
    if (status.ok())
    {
        status = waitForFaultCycle(operation);
    }
    if (status.ok())
    {
        status = readFaultStatusInternal(candidate, operation, true);
    }

    if (sequenceEntered)
    {
        // See the automatic path above. In particular, an observed manual
        // step-1 state is not advanced under this no-wait cleanup context:
        // resolveFaultCycleInternal() reports the missing mandatory settling
        // budget and recover(timeout) later completes FORCE- safely.
        const bool cleanupAfterDeadline = deadlineExpired(operation);
        if (cleanupAfterDeadline && status.ok())
        {
            status = timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        else if (cleanupAfterDeadline)
        {
            (void)timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        MAX31865OperationContext cleanupOperation = operation;
        if (cleanupAfterDeadline)
        {
            cleanupOperation.timeoutMs = 0U;
        }
        cleanupOperation.lastTransferAttempted = false;
        cleanupOperation.lastTransferApplied = false;
        cleanupOperation.lastFrameCommitted = false;
        cleanupOperation.lastDeassertUnknown = false;

        MAX31865Status cleanup =
            resolveFaultCycleInternal(cleanupOperation);
        if (cleanup.ok())
        {
            cleanup = writeRuntimeConfig(
                _desiredConfig.biasEnabled, false, 0U, cleanupOperation);
        }
        if (cleanup.ok() && _desiredConfig.biasEnabled)
        {
            cleanup = sleepWithinDeadline(
                postFaultSettleUsFor(_rtdConfig),
                cleanupOperation,
                MAX31865Error::OperationTimeout);
        }
        if (cleanup.ok())
        {
            cleanup = verifyDesiredImage(cleanupOperation);
        }
        mergeTimeoutObservations(operation, cleanupOperation);
        if (cleanup.ok())
        {
            _state = MAX31865State::Ready;
            _continuous = false;
            _oneShotArmed = false;
            _oneShotCommitPending = false;
            _oneShotCommitUncertain = false;
            _freshResultArmed = false;
            _readyLatched = false;
            _timedReadinessHalted = false;
        }
        else
        {
            _state = MAX31865State::Fault;
            invalidateObservedState();
            const MAX31865Error primaryCode = status.code;
            status = status.ok()
                ? cleanup
                : MAX31865Status::Error(
                      MAX31865Error::RestoreFailed,
                      "manual fault cycle and restore both failed",
                      combinedFailureDetail(primaryCode, cleanup.code));
        }
    }
    if (status.ok() || status.code == MAX31865Error::DeviceFault)
    {
        out = candidate;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::setFaultThresholdsRaw(
    const MAX31865FaultThresholds &thresholds,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865DeviceConfig candidate = _desiredConfig;
    candidate.thresholds = thresholds;
    if (status.ok())
    {
        status = validateDeviceConfig(candidate);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(candidate, true, operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::readFaultThresholdsRaw(
    MAX31865FaultThresholds &out,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    uint8_t bytes[4]{};
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_HIGH_FAULT_MSB,
            bytes,
            sizeof(bytes),
            operation);
    }
    MAX31865FaultThresholds candidate{};
    if (status.ok())
    {
        status = max31865DecodeThreshold(&bytes[0], candidate.highCode);
    }
    if (status.ok())
    {
        status = max31865DecodeThreshold(&bytes[2], candidate.lowCode);
    }
    if (status.ok())
    {
        out = candidate;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::setFaultThresholdsResistance(
    float lowOhms,
    float highOhms,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    if (status.ok() &&
        (!finiteFloat(lowOhms) || !finiteFloat(highOhms) ||
         lowOhms < 0.0F || lowOhms > highOhms))
    {
        status = MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid or reversed resistance thresholds");
    }
    MAX31865FaultThresholds thresholds{};
    if (status.ok())
    {
        status = resistanceToCode(lowOhms, thresholds.lowCode);
    }
    if (status.ok())
    {
        status = resistanceToCode(highOhms, thresholds.highCode);
    }
    if (status.ok() && thresholds.lowCode > thresholds.highCode)
    {
        status = MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "low resistance threshold exceeds high threshold");
    }
    MAX31865DeviceConfig candidate = _desiredConfig;
    candidate.thresholds = thresholds;
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(candidate, true, operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::readFaultThresholdsResistance(
    float &lowOhms,
    float &highOhms,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    uint8_t bytes[4]{};
    uint16_t highCode = 0U;
    uint16_t lowCode = 0U;
    float candidateLow = 0.0F;
    float candidateHigh = 0.0F;
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_HIGH_FAULT_MSB,
            bytes,
            sizeof(bytes),
            operation);
    }
    if (status.ok())
    {
        status = max31865DecodeThreshold(&bytes[0], highCode);
    }
    if (status.ok())
    {
        status = max31865DecodeThreshold(&bytes[2], lowCode);
    }
    if (status.ok())
    {
        status = codeToResistance(lowCode, candidateLow);
    }
    if (status.ok())
    {
        status = codeToResistance(highCode, candidateHigh);
    }
    if (status.ok())
    {
        lowOhms = candidateLow;
        highOhms = candidateHigh;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::setFaultThresholdsTemperature(
    float lowC,
    float highC,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    if (status.ok() && (!finiteFloat(lowC) || !finiteFloat(highC) ||
                        lowC > highC))
    {
        status = MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "invalid temperature-threshold ordering");
    }
    float lowResistance = 0.0F;
    float highResistance = 0.0F;
    MAX31865FaultThresholds thresholds{};
    if (status.ok())
    {
        status = temperatureToResistance(lowC, lowResistance);
    }
    if (status.ok())
    {
        status = temperatureToResistance(highC, highResistance);
    }
    if (status.ok())
    {
        status = resistanceToCode(lowResistance, thresholds.lowCode);
    }
    if (status.ok())
    {
        status = resistanceToCode(highResistance, thresholds.highCode);
    }
    MAX31865DeviceConfig candidate = _desiredConfig;
    candidate.thresholds = thresholds;
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(candidate, true, operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::readFaultThresholdsTemperature(
    float &lowC,
    float &highC,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    uint8_t bytes[4]{};
    uint16_t highCode = 0U;
    uint16_t lowCode = 0U;
    float candidateLow = 0.0F;
    float candidateHigh = 0.0F;
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_HIGH_FAULT_MSB,
            bytes,
            sizeof(bytes),
            operation);
    }
    if (status.ok())
    {
        status = max31865DecodeThreshold(&bytes[0], highCode);
    }
    if (status.ok())
    {
        status = max31865DecodeThreshold(&bytes[2], lowCode);
    }
    if (status.ok())
    {
        status = faultThresholdCodeToTemperature(lowCode, candidateLow);
    }
    if (status.ok())
    {
        status = faultThresholdCodeToTemperature(highCode, candidateHigh);
    }
    if (status.ok())
    {
        lowC = candidateLow;
        highC = candidateHigh;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::prepareRegisterWriteCandidate(
    uint8_t address,
    uint8_t value,
    MAX31865DeviceConfig &out) const
{
    if (!max31865RegisterAddressValid(address))
    {
        return MAX31865Status::Error(
            MAX31865Error::RegisterAddressInvalid,
            "register address is outside 0..7",
            address);
    }
    if (max31865RegisterAccess(address) !=
        MAX31865RegisterAccess::ReadWrite)
    {
        return MAX31865Status::Error(
            MAX31865Error::UnsupportedCommand,
            "register is read-only",
            address);
    }

    MAX31865DeviceConfig candidate = _desiredConfig;
    if (address == max31865_cmd::REG_CONFIG)
    {
        if ((value & (max31865_cmd::CONFIG_AUTO |
                      max31865_cmd::CONFIG_COMMAND_MASK)) != 0U)
        {
            return MAX31865Status::Error(
                MAX31865Error::UnsupportedCommand,
                "raw CONFIG conversion/fault commands are not allowed",
                value);
        }
        candidate.wireMode =
            (value & max31865_cmd::CONFIG_3WIRE) != 0U
                ? MAX31865WireMode::ThreeWire
                : (_desiredConfig.wireMode == MAX31865WireMode::TwoWire
                       ? MAX31865WireMode::TwoWire
                       : MAX31865WireMode::FourWire);
        candidate.filter =
            (value & max31865_cmd::CONFIG_FILTER_50HZ) != 0U
                ? MAX31865Filter::Hz50
                : MAX31865Filter::Hz60;
        candidate.biasEnabled =
            (value & max31865_cmd::CONFIG_BIAS) != 0U;
        out = candidate;
        return MAX31865Status::Ok();
    }

    uint8_t thresholds[4]{};
    MAX31865Status status = max31865EncodeThreshold(
        _desiredConfig.thresholds.highCode, &thresholds[0]);
    if (status.ok())
    {
        status = max31865EncodeThreshold(
            _desiredConfig.thresholds.lowCode, &thresholds[2]);
    }
    if (!status.ok())
    {
        return status;
    }
    const size_t index = static_cast<size_t>(
        address - max31865_cmd::REG_HIGH_FAULT_MSB);
    thresholds[index] =
        (address == max31865_cmd::REG_HIGH_FAULT_LSB ||
         address == max31865_cmd::REG_LOW_FAULT_LSB)
            ? static_cast<uint8_t>(
                  value & max31865_cmd::THRESHOLD_LSB_DEFINED_MASK)
            : value;
    status = max31865DecodeThreshold(
        &thresholds[0], candidate.thresholds.highCode);
    if (status.ok())
    {
        status = max31865DecodeThreshold(
            &thresholds[2], candidate.thresholds.lowCode);
    }
    if (status.ok())
    {
        status = validateDeviceConfig(candidate);
    }
    if (status.ok())
    {
        out = candidate;
    }
    return status;
}

MAX31865Status MAX31865::readRegister(
    uint8_t address,
    uint8_t &out,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, true);
    }
    if (status.ok() && !max31865RegisterAddressValid(address))
    {
        status = MAX31865Status::Error(
            MAX31865Error::RegisterAddressInvalid,
            "register address is outside 0..7",
            address);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    uint8_t candidate = 0U;
    if (status.ok())
    {
        status = readRegistersInternal(
            address, &candidate, 1U, operation);
    }
    if (status.ok())
    {
        out = candidate;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::readRegisters(
    uint8_t startAddress,
    uint8_t *out,
    size_t length,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, length == 1U);
    }
    if (status.ok() &&
        (out == nullptr || length == 0U ||
         !max31865RegisterAddressValid(startAddress) ||
         length > max31865_cmd::NUM_REGISTERS - startAddress))
    {
        status = MAX31865Status::Error(
            out == nullptr || length == 0U
                ? MAX31865Error::InvalidArgument
                : MAX31865Error::RegisterAddressInvalid,
            "invalid register read span",
            startAddress);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    uint8_t candidate[max31865_cmd::NUM_REGISTERS]{};
    if (status.ok())
    {
        status = readRegistersInternal(
            startAddress, candidate, length, operation);
    }
    if (status.ok())
    {
        memcpy(out, candidate, length);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::writeRegister(
    uint8_t address,
    uint8_t value,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865DeviceConfig candidate{};
    if (status.ok())
    {
        status = prepareRegisterWriteCandidate(address, value, candidate);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(candidate, true, operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::writeRegisterVerified(
    uint8_t address,
    uint8_t value,
    uint8_t &readBack,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865DeviceConfig candidate{};
    if (status.ok())
    {
        status = prepareRegisterWriteCandidate(address, value, candidate);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(candidate, true, operation);
    }
    uint8_t observed = 0U;
    if (status.ok())
    {
        status = readRegistersInternal(address, &observed, 1U, operation);
    }
    if (status.ok() &&
        !persistentByteMatches(address, observed, value))
    {
        status = MAX31865Status::Error(
            MAX31865Error::RegisterVerifyFailed,
            "target diagnostic readback mismatch",
            static_cast<int32_t>(
                (static_cast<uint32_t>(value) << 8U) | observed));
        _state = MAX31865State::Fault;
        invalidateObservedState();
    }
    if (status.ok())
    {
        readBack = observed;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::dumpRegisters(
    MAX31865RegisterDump *out,
    size_t capacity,
    size_t &count,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    if (status.ok() &&
        (out == nullptr || capacity < max31865_cmd::NUM_REGISTERS))
    {
        status = MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "register dump output capacity is smaller than eight");
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    uint8_t registers[max31865_cmd::NUM_REGISTERS]{};
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_CONFIG,
            registers,
            sizeof(registers),
            operation);
    }
    if (status.ok())
    {
        MAX31865RegisterDump candidate[max31865_cmd::NUM_REGISTERS]{};
        for (size_t index = 0U;
             index < max31865_cmd::NUM_REGISTERS;
             ++index)
        {
            candidate[index].address = static_cast<uint8_t>(index);
            candidate[index].name = max31865RegisterName(
                static_cast<uint8_t>(index));
            candidate[index].value = registers[index];
        }
        memcpy(out, candidate, sizeof(candidate));
        count = max31865_cmd::NUM_REGISTERS;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::restoreWritableDefaults(uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    if (status.ok())
    {
        status = applyConfigurationInternal(
            max31865DefaultDeviceConfig(), true, operation);
    }
    if (status.ok())
    {
        status = clearFaultsInternal(operation);
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::registerReadbackTest(
    uint8_t &readBack,
    uint32_t timeoutMs)
{
    MAX31865Status status = requireReady();
    if (status.ok())
    {
        status = validateTimeout(timeoutMs, false);
    }
    MAX31865OperationContext operation{};
    if (status.ok())
    {
        operation = makeOperation(timeoutMs);
    }
    uint8_t saved = 0U;
    bool savedValid = false;
    bool patternMayHaveApplied = false;
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_LOW_FAULT_LSB, &saved, 1U, operation);
        savedValid = status.ok();
    }
    const uint8_t pattern = static_cast<uint8_t>(
        (saved ^ 0xA8U) & max31865_cmd::THRESHOLD_LSB_DEFINED_MASK);
    if (status.ok())
    {
        status = writeRegistersInternal(
            max31865_cmd::REG_LOW_FAULT_LSB,
            &pattern,
            1U,
            operation);
        patternMayHaveApplied = status.ok() ||
            operation.lastTransferAttempted;
    }
    uint8_t observed = 0U;
    if (status.ok())
    {
        status = readRegistersInternal(
            max31865_cmd::REG_LOW_FAULT_LSB,
            &observed,
            1U,
            operation);
    }
    if (status.ok() &&
        !persistentByteMatches(
            max31865_cmd::REG_LOW_FAULT_LSB, observed, pattern))
    {
        status = MAX31865Status::Error(
            MAX31865Error::RegisterVerifyFailed,
            "threshold communication-test readback mismatch",
            observed);
    }

    MAX31865Status cleanup = MAX31865Status::Ok();
    bool restoreVerified = false;
    if (savedValid && patternMayHaveApplied)
    {
        // The saved threshold byte is application state. Restore it with one
        // immediate/no-wait cleanup attempt after deadline expiry rather than
        // silently skipping every cleanup frame on the expired primary context.
        const bool cleanupAfterDeadline = deadlineExpired(operation);
        if (cleanupAfterDeadline && status.ok())
        {
            status = timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        else if (cleanupAfterDeadline)
        {
            (void)timeoutStatus(
                MAX31865Error::OperationTimeout, operation);
        }
        MAX31865OperationContext cleanupOperation = operation;
        if (cleanupAfterDeadline)
        {
            cleanupOperation.timeoutMs = 0U;
        }
        cleanupOperation.lastTransferAttempted = false;
        cleanupOperation.lastTransferApplied = false;
        cleanupOperation.lastFrameCommitted = false;
        cleanupOperation.lastDeassertUnknown = false;
        cleanup = writeRegistersInternal(
            max31865_cmd::REG_LOW_FAULT_LSB,
            &saved,
            1U,
            cleanupOperation);
        if (cleanup.ok())
        {
            cleanup = verifyDesiredImage(cleanupOperation);
            restoreVerified = cleanup.ok();
        }
        mergeTimeoutObservations(operation, cleanupOperation);
    }
    if (restoreVerified)
    {
        // A prior CS/framing failure may have put the lifecycle in Fault, but
        // the outer cleanup has now proven CS inactive, restored the saved
        // byte, and verified the complete desired persistent image.
        _state = MAX31865State::Ready;
    }
    if (!cleanup.ok())
    {
        _state = MAX31865State::Fault;
        invalidateObservedState();
        const MAX31865Error primaryCode = status.code;
        status = status.ok()
            ? cleanup
            : MAX31865Status::Error(
                  MAX31865Error::RestoreFailed,
                  "communication test and threshold restore both failed",
                  combinedFailureDetail(primaryCode, cleanup.code));
    }
    if (status.ok())
    {
        readBack = observed;
    }
    return finishOperation(status, trackingFor(status, true), operation);
}

MAX31865Status MAX31865::codeToRatio(uint16_t code, float &out)
{
    if (code > max31865_cmd::ADC_CODE_MAX)
    {
        return MAX31865Status::Error(
            MAX31865Error::ConversionOutOfRange,
            "RTD code exceeds the 15-bit conversion range",
            static_cast<int32_t>(code));
    }

    const float candidate = static_cast<float>(code) /
                            static_cast<float>(max31865_cmd::ADC_FULL_SCALE);
    out = candidate;
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::codeToResistance(uint16_t code, float &out) const
{
    float ratio = 0.0F;
    MAX31865Status status = codeToRatio(code, ratio);
    if (!status.ok())
    {
        return status;
    }
    if (!finiteFloat(_rtdConfig.referenceResistorOhms) ||
        _rtdConfig.referenceResistorOhms <= 0.0F)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "RTD reference-resistor configuration is invalid");
    }

    const float candidate = ratio * _rtdConfig.referenceResistorOhms;
    if (!finiteFloat(candidate))
    {
        return MAX31865Status::Error(
            MAX31865Error::ConversionOutOfRange,
            "RTD resistance calculation overflowed");
    }
    out = candidate;
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::resistanceToCode(
    float resistanceOhms,
    uint16_t &out) const
{
    if (!finiteFloat(resistanceOhms) || resistanceOhms < 0.0F)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "resistance must be finite and nonnegative");
    }
    if (!finiteFloat(_rtdConfig.referenceResistorOhms) ||
        _rtdConfig.referenceResistorOhms <= 0.0F)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "RTD reference-resistor configuration is invalid");
    }

    const double scaled =
        (static_cast<double>(resistanceOhms) /
         static_cast<double>(_rtdConfig.referenceResistorOhms)) *
        static_cast<double>(max31865_cmd::ADC_FULL_SCALE);
    const float maximumInput =
        (static_cast<float>(max31865_cmd::ADC_CODE_MAX) /
         static_cast<float>(max31865_cmd::ADC_FULL_SCALE)) *
        _rtdConfig.referenceResistorOhms;
    if (!isfinite(scaled) || scaled < 0.0 ||
        resistanceOhms > maximumInput)
    {
        return MAX31865Status::Error(
            MAX31865Error::ConversionOutOfRange,
            "resistance is outside the representable ADC-code range");
    }

    const uint32_t rounded = static_cast<uint32_t>(floor(scaled + 0.5));
    out = rounded > static_cast<uint32_t>(max31865_cmd::ADC_CODE_MAX)
        ? max31865_cmd::ADC_CODE_MAX
        : static_cast<uint16_t>(rounded);
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::resistanceToTemperature(
    float resistanceOhms,
    float &out) const
{
    if (!finiteFloat(resistanceOhms) || resistanceOhms < 0.0F)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "resistance must be finite and nonnegative");
    }

    const double minimumResistance = resistanceAtTemperature(
        _rtdConfig, static_cast<double>(_rtdConfig.minimumTemperatureC));
    const double maximumResistance = resistanceAtTemperature(
        _rtdConfig, static_cast<double>(_rtdConfig.maximumTemperatureC));
    double resistance = static_cast<double>(resistanceOhms);
    if (!isfinite(minimumResistance) || !isfinite(maximumResistance) ||
        minimumResistance >= maximumResistance)
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "configured RTD curve is not invertible");
    }
    // The forward API returns float, so compare against float-rounded endpoint
    // resistances before clamping back to the exact double-domain endpoints.
    // This guarantees deterministic endpoint round trips without extending the
    // caller-visible configured temperature domain by an arbitrary epsilon.
    const float minimumInput = static_cast<float>(minimumResistance);
    const float maximumInput = static_cast<float>(maximumResistance);
    if (resistanceOhms < minimumInput || resistanceOhms > maximumInput)
    {
        return MAX31865Status::Error(
            MAX31865Error::ConversionOutOfRange,
            "resistance is outside the configured temperature domain");
    }
    if (resistance < minimumResistance)
    {
        resistance = minimumResistance;
    }
    else if (resistance > maximumResistance)
    {
        resistance = maximumResistance;
    }

    double low = static_cast<double>(_rtdConfig.minimumTemperatureC);
    double high = static_cast<double>(_rtdConfig.maximumTemperatureC);
    for (uint8_t iteration = 0U; iteration < 48U; ++iteration)
    {
        const double midpoint = low + ((high - low) * 0.5);
        if (resistanceAtTemperature(_rtdConfig, midpoint) < resistance)
        {
            low = midpoint;
        }
        else
        {
            high = midpoint;
        }
    }

    const double candidate = low + ((high - low) * 0.5);
    if (!isfinite(candidate))
    {
        return MAX31865Status::Error(
            MAX31865Error::ConversionOutOfRange,
            "temperature inversion failed");
    }
    out = static_cast<float>(candidate);
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::faultThresholdCodeToTemperature(
    uint16_t code,
    float &out) const
{
    float resistanceOhms = 0.0F;
    MAX31865Status status = codeToResistance(code, resistanceOhms);
    if (!status.ok())
    {
        return status;
    }

    float candidate = 0.0F;
    status = resistanceToTemperature(resistanceOhms, candidate);
    if (status.ok())
    {
        out = candidate;
        return status;
    }
    if (status.code != MAX31865Error::ConversionOutOfRange)
    {
        return status;
    }

    // A typed endpoint is rounded to the nearest 15-bit hardware code. That
    // code can decode by less than half an LSB beyond the strict CVD domain.
    // Recognize exactly the code produced from the float-rounded endpoint,
    // while leaving the general resistance conversion and all other raw codes
    // strictly out of range.
    const float minimumInput = static_cast<float>(resistanceAtTemperature(
        _rtdConfig,
        static_cast<double>(_rtdConfig.minimumTemperatureC)));
    const float maximumInput = static_cast<float>(resistanceAtTemperature(
        _rtdConfig,
        static_cast<double>(_rtdConfig.maximumTemperatureC)));
    uint16_t endpointCode = 0U;
    if (resistanceOhms < minimumInput)
    {
        const MAX31865Status endpointStatus = resistanceToCode(
            minimumInput, endpointCode);
        if (endpointStatus.ok() && endpointCode == code)
        {
            out = _rtdConfig.minimumTemperatureC;
            return MAX31865Status::Ok();
        }
    }
    else if (resistanceOhms > maximumInput)
    {
        const MAX31865Status endpointStatus = resistanceToCode(
            maximumInput, endpointCode);
        if (endpointStatus.ok() && endpointCode == code)
        {
            out = _rtdConfig.maximumTemperatureC;
            return MAX31865Status::Ok();
        }
    }
    return status;
}

MAX31865Status MAX31865::temperatureToResistance(
    float temperatureC,
    float &out) const
{
    if (!finiteFloat(temperatureC))
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidArgument,
            "temperature must be finite");
    }
    if (temperatureC < _rtdConfig.minimumTemperatureC ||
        temperatureC > _rtdConfig.maximumTemperatureC)
    {
        return MAX31865Status::Error(
            MAX31865Error::ConversionOutOfRange,
            "temperature is outside the configured RTD domain");
    }

    const double resistance = resistanceAtTemperature(
        _rtdConfig, static_cast<double>(temperatureC));
    const double maximumRepresentable =
        static_cast<double>(_rtdConfig.referenceResistorOhms) *
        static_cast<double>(max31865_cmd::ADC_CODE_MAX) /
        static_cast<double>(max31865_cmd::ADC_FULL_SCALE);
    if (!isfinite(resistance) || resistance < 0.0 ||
        resistance > maximumRepresentable)
    {
        return MAX31865Status::Error(
            MAX31865Error::ConversionOutOfRange,
            "temperature maps outside the representable ADC range");
    }

    out = static_cast<float>(resistance);
    return MAX31865Status::Ok();
}

MAX31865Status MAX31865::temperatureToCode(
    float temperatureC,
    uint16_t &out) const
{
    float resistanceOhms = 0.0F;
    MAX31865Status status = temperatureToResistance(
        temperatureC, resistanceOhms);
    if (status.ok())
    {
        status = resistanceToCode(resistanceOhms, out);
    }
    return status;
}

uint32_t MAX31865::singleConversionTimeMs() const
{
    return _desiredConfig.filter == MAX31865Filter::Hz50
        ? max31865_cmd::SINGLE_CONVERSION_50HZ_MS
        : max31865_cmd::SINGLE_CONVERSION_60HZ_MS;
}

uint32_t MAX31865::continuousConversionTimeMs() const
{
    return _desiredConfig.filter == MAX31865Filter::Hz50
        ? max31865_cmd::CONTINUOUS_CONVERSION_50HZ_MS
        : max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS;
}

uint32_t MAX31865::biasSettleTimeUs() const
{
    return biasSettleUsFor(_rtdConfig);
}

MAX31865FaultStatus MAX31865::decodeFaultStatus(uint8_t raw)
{
    const uint8_t documented = static_cast<uint8_t>(
        raw & max31865_cmd::FAULT_DEFINED_MASK);
    return MAX31865FaultStatus{
        documented,
        (documented & max31865_cmd::FAULT_HIGH_THRESHOLD) != 0U,
        (documented & max31865_cmd::FAULT_LOW_THRESHOLD) != 0U,
        (documented & max31865_cmd::FAULT_REFIN_HIGH) != 0U,
        (documented & max31865_cmd::FAULT_REFIN_LOW) != 0U,
        (documented & max31865_cmd::FAULT_RTDIN_LOW) != 0U,
        (documented & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0U};
}
