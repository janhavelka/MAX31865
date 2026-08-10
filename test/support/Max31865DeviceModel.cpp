#include "support/Max31865DeviceModel.h"

#include <limits.h>
#include <string.h>

namespace max31865_test {
namespace {

static constexpr uint32_t kAutomaticForceOpenSettleUs = 210U;
static constexpr uint32_t kAutomaticTerminalSlackUs =
    max31865_cmd::AUTO_FAULT_DETECTION_MAX_US -
    (3U * max31865_cmd::MANUAL_FAULT_PHASE_US) -
    kAutomaticForceOpenSettleUs;
static constexpr uint8_t kMaximumFaultPhaseTransitions = 5U;

MAX31865Status modelError(
    MAX31865Error code,
    const char *message,
    int32_t detail = 0)
{
    return MAX31865Status::Error(code, message, detail);
}

bool spanContains(uint8_t start, size_t length, uint8_t address)
{
    const size_t first = static_cast<size_t>(start);
    const size_t target = static_cast<size_t>(address);
    return target >= first && target < (first + length);
}

} // namespace

Max31865DeviceModel::Max31865DeviceModel()
{
    powerOnReset();
}

void Max31865DeviceModel::powerOnReset()
{
    memset(_registers, 0, sizeof(_registers));
    _registers[max31865_cmd::REG_CONFIG] = max31865_cmd::CONFIG_RESET;
    _registers[max31865_cmd::REG_RTD_MSB] = max31865_cmd::RTD_MSB_RESET;
    _registers[max31865_cmd::REG_RTD_LSB] = max31865_cmd::RTD_LSB_RESET;
    _registers[max31865_cmd::REG_HIGH_FAULT_MSB] =
        max31865_cmd::HIGH_FAULT_MSB_RESET;
    _registers[max31865_cmd::REG_HIGH_FAULT_LSB] =
        max31865_cmd::HIGH_FAULT_LSB_RESET;
    _registers[max31865_cmd::REG_LOW_FAULT_MSB] =
        max31865_cmd::LOW_FAULT_MSB_RESET;
    _registers[max31865_cmd::REG_LOW_FAULT_LSB] =
        max31865_cmd::LOW_FAULT_LSB_RESET;
    _registers[max31865_cmd::REG_FAULT_STATUS] =
        max31865_cmd::FAULT_STATUS_RESET;

    _chipSelected = false;
    _transferSeenWhileSelected = false;
    _pendingConfigValid = false;
    _pendingConfig = 0U;
    _pendingRtdAcknowledge = false;
    _timeUs = 0U;
    _continuous = false;
    _oneShot = false;
    _conversionDueUs = 0U;
    _faultCyclePending = false;
    _faultCycle = MAX31865FaultCycle::None;
    _faultCyclePhase = FaultCyclePhase::None;
    _faultCycleDueUs = 0U;
    _faultInputs = 0U;
    memset(_samples, 0, sizeof(_samples));
    _sampleHead = 0U;
    _sampleCount = 0U;
    _lastSample = {0U, 0U};
    _drdyLow = false;
    _conversionCount = 0U;
    _overwrittenConversionCount = 0U;
    _rtdReadAcknowledgeCount = 0U;
    _invalidFrameCount = 0U;
    _readOverrideEnabled = false;
    _readOverrideAddress = 0U;
    _readOverrideValue = 0U;
    _readOverrideOccurrence = 0U;
    _readOverrideSeen = 0U;
    _alternatingReadOverrideEnabled = false;
    _alternatingReadOverrideAddress = 0U;
    _alternatingReadOverrideFirstValue = 0U;
    _alternatingReadOverrideSecondValue = 0U;
    _alternatingReadOverrideUseFirst = true;
}

void Max31865DeviceModel::advanceTimeUs(uint64_t elapsedUs)
{
    if (UINT64_MAX - _timeUs < elapsedUs) {
        _timeUs = UINT64_MAX;
    } else {
        _timeUs += elapsedUs;
    }
    updateTimeDependentState();
}

void Max31865DeviceModel::advanceTimeMs(uint32_t elapsedMs)
{
    advanceTimeUs(static_cast<uint64_t>(elapsedMs) * 1000ULL);
}

MAX31865Status Max31865DeviceModel::setChipSelect(bool asserted)
{
    updateTimeDependentState();
    if (asserted && !_chipSelected) {
        _transferSeenWhileSelected = false;
    }
    if (!asserted && _chipSelected) {
        applyPendingConfig();
        if (_pendingRtdAcknowledge) {
            acknowledgeRtdRead();
            _pendingRtdAcknowledge = false;
        }
        _transferSeenWhileSelected = false;
    }
    _chipSelected = asserted;
    return MAX31865Status::Ok();
}

MAX31865Status Max31865DeviceModel::transfer(
    const uint8_t *tx,
    uint8_t *rx,
    size_t length)
{
    updateTimeDependentState();
    if (!_chipSelected) {
        _invalidFrameCount = saturatingAdd(_invalidFrameCount, 1U);
        return modelError(
            MAX31865Error::InvalidState,
            "Transfer while chip select is inactive");
    }
    if (_transferSeenWhileSelected) {
        _invalidFrameCount = saturatingAdd(_invalidFrameCount, 1U);
        return modelError(
            MAX31865Error::InvalidState,
            "Second transfer before chip select deassertion");
    }
    if (tx == nullptr || rx == nullptr || length < 2U ||
        length > max31865_cmd::MAX_FRAME_BYTES) {
        _invalidFrameCount = saturatingAdd(_invalidFrameCount, 1U);
        return modelError(
            MAX31865Error::InvalidArgument,
            "Invalid model SPI frame",
            static_cast<int32_t>(length));
    }
    _transferSeenWhileSelected = true;

    const bool write = (tx[0] & max31865_cmd::WRITE_BIT) != 0U;
    const uint8_t start = static_cast<uint8_t>(
        tx[0] & max31865_cmd::READ_MASK);
    const size_t registerCount = length - 1U;
    if (start > max31865_cmd::REG_LAST ||
        registerCount >
            (max31865_cmd::NUM_REGISTERS - static_cast<size_t>(start))) {
        _invalidFrameCount = saturatingAdd(_invalidFrameCount, 1U);
        return modelError(
            MAX31865Error::RegisterAddressInvalid,
            "Model register span is invalid",
            static_cast<int32_t>(start));
    }

    memset(rx, 0, length);
    for (size_t index = 0U; index < registerCount; ++index) {
        const uint8_t address = static_cast<uint8_t>(
            static_cast<size_t>(start) + index);
        if (write) {
            writeRegister(address, tx[index + 1U]);
        } else {
            rx[index + 1U] = readRegister(address);
        }
    }

    if (!write &&
        (spanContains(start, registerCount, max31865_cmd::REG_RTD_MSB) ||
         spanContains(start, registerCount, max31865_cmd::REG_RTD_LSB))) {
        _pendingRtdAcknowledge = true;
    }
    return MAX31865Status::Ok();
}

MAX31865Status Max31865DeviceModel::readDrdy(bool &level) const
{
    level = !_drdyLow;
    return MAX31865Status::Ok();
}

bool Max31865DeviceModel::chipSelected() const
{
    return _chipSelected;
}

bool Max31865DeviceModel::dataReady() const
{
    return _drdyLow;
}

bool Max31865DeviceModel::conversionRunning() const
{
    return _continuous || _oneShot;
}

bool Max31865DeviceModel::continuousConversion() const
{
    return _continuous;
}

bool Max31865DeviceModel::oneShotActive() const
{
    return _oneShot;
}

uint64_t Max31865DeviceModel::timeUs() const
{
    return _timeUs;
}

uint8_t Max31865DeviceModel::registerValue(uint8_t address) const
{
    return address <= max31865_cmd::REG_LAST ? _registers[address] : 0U;
}

uint8_t Max31865DeviceModel::faultLatch() const
{
    return static_cast<uint8_t>(
        _registers[max31865_cmd::REG_FAULT_STATUS] &
        max31865_cmd::FAULT_DEFINED_MASK);
}

uint32_t Max31865DeviceModel::conversionCount() const
{
    return _conversionCount;
}

uint32_t Max31865DeviceModel::overwrittenConversionCount() const
{
    return _overwrittenConversionCount;
}

uint32_t Max31865DeviceModel::rtdReadAcknowledgeCount() const
{
    return _rtdReadAcknowledgeCount;
}

uint32_t Max31865DeviceModel::invalidFrameCount() const
{
    return _invalidFrameCount;
}

bool Max31865DeviceModel::enqueueSample(uint16_t code, uint8_t faultStatus)
{
    if (code > max31865_cmd::ADC_CODE_MAX ||
        _sampleCount >= MODEL_SAMPLE_CAPACITY) {
        return false;
    }
    const size_t tail = (_sampleHead + _sampleCount) % MODEL_SAMPLE_CAPACITY;
    _samples[tail] = {
        code,
        static_cast<uint8_t>(
            faultStatus & max31865_cmd::FAULT_DEFINED_MASK)};
    ++_sampleCount;
    return true;
}

void Max31865DeviceModel::forceReadySample(
    uint16_t code,
    uint8_t faultStatus)
{
    const uint16_t bounded = code <= max31865_cmd::ADC_CODE_MAX
        ? code
        : max31865_cmd::ADC_CODE_MAX;
    setReadySample(
        bounded,
        static_cast<uint8_t>(
            faultStatus & max31865_cmd::FAULT_DEFINED_MASK));
}

void Max31865DeviceModel::setFaultInputs(uint8_t documentedFaultBits)
{
    updateTimeDependentState();
    const bool voltageWasActive =
        (_faultInputs & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0U;
    _faultInputs = static_cast<uint8_t>(
        documentedFaultBits & max31865_cmd::FAULT_DEFINED_MASK);
    const uint8_t voltage = static_cast<uint8_t>(
        _faultInputs & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    if (voltage != 0U) {
        _registers[max31865_cmd::REG_FAULT_STATUS] = static_cast<uint8_t>(
            _registers[max31865_cmd::REG_FAULT_STATUS] | voltage);
        _registers[max31865_cmd::REG_RTD_LSB] = static_cast<uint8_t>(
            _registers[max31865_cmd::REG_RTD_LSB] | 0x01U);
    }
    else if (voltageWasActive && (_oneShot || _continuous)) {
        // The ADC halts while D2 is physically active. Resume from a fresh,
        // conservative full-conversion horizon instead of accumulating every
        // nominal period that elapsed during the halt.
        _conversionDueUs = _timeUs +
            static_cast<uint64_t>(oneShotDurationUs());
    }
}

void Max31865DeviceModel::setRegisterReadOverride(
    uint8_t address,
    uint8_t value,
    uint32_t matchingOccurrence)
{
    _readOverrideEnabled = address <= max31865_cmd::REG_LAST &&
        matchingOccurrence != 0U;
    _readOverrideAddress = address;
    _readOverrideValue = value;
    _readOverrideOccurrence = matchingOccurrence;
    _readOverrideSeen = 0U;
}

void Max31865DeviceModel::setAlternatingRegisterReadOverride(
    uint8_t address,
    uint8_t firstValue,
    uint8_t secondValue)
{
    _alternatingReadOverrideEnabled =
        address <= max31865_cmd::REG_LAST;
    _alternatingReadOverrideAddress = address;
    _alternatingReadOverrideFirstValue = firstValue;
    _alternatingReadOverrideSecondValue = secondValue;
    _alternatingReadOverrideUseFirst = true;
}

void Max31865DeviceModel::updateTimeDependentState()
{
    for (uint8_t transition = 0U;
         transition < kMaximumFaultPhaseTransitions &&
             _faultCyclePending && _timeUs >= _faultCycleDueUs;
         ++transition) {
        advanceFaultCyclePhase();
    }

    if ((_faultInputs & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE) != 0U) {
        return;
    }

    if (_oneShot && _timeUs >= _conversionDueUs) {
        completeOneConversion();
        _oneShot = false;
        _registers[max31865_cmd::REG_CONFIG] = static_cast<uint8_t>(
            _registers[max31865_cmd::REG_CONFIG] &
            static_cast<uint8_t>(~max31865_cmd::CONFIG_ONE_SHOT));
    }

    if (!_continuous || _timeUs < _conversionDueUs) {
        return;
    }

    const uint64_t period = static_cast<uint64_t>(continuousPeriodUs());
    const uint64_t dueCount = ((_timeUs - _conversionDueUs) / period) + 1U;
    const uint64_t explicitCount = dueCount < MODEL_SAMPLE_CAPACITY
        ? dueCount
        : MODEL_SAMPLE_CAPACITY;
    for (uint64_t index = 0U; index < explicitCount; ++index) {
        completeOneConversion();
    }
    if (dueCount > explicitCount) {
        const uint64_t omitted = dueCount - explicitCount;
        if (_drdyLow) {
            _overwrittenConversionCount = saturatingAdd(
                _overwrittenConversionCount,
                omitted);
        }
        _conversionCount = saturatingAdd(_conversionCount, omitted);
    }

    if (dueCount > ((UINT64_MAX - _conversionDueUs) / period)) {
        _conversionDueUs = UINT64_MAX;
    } else {
        _conversionDueUs += dueCount * period;
    }
}

void Max31865DeviceModel::applyPendingConfig()
{
    if (!_pendingConfigValid) {
        return;
    }
    _pendingConfigValid = false;
    const uint8_t requested = _pendingConfig;
    const uint8_t faultCommand = static_cast<uint8_t>(
        requested & max31865_cmd::CONFIG_FAULT_CYCLE_MASK);
    const bool oneShotCommand =
        (requested & max31865_cmd::CONFIG_ONE_SHOT) != 0U;
    const bool faultCommandPresent =
        faultCommand != max31865_cmd::CONFIG_FAULT_CYCLE_NONE;
    const bool commandConflict = oneShotCommand && faultCommandPresent;
    const uint8_t activeFaultCommand = static_cast<uint8_t>(
        _registers[max31865_cmd::REG_CONFIG] &
        max31865_cmd::CONFIG_FAULT_CYCLE_MASK);

    // D1 clears latched faults only in an isolated command with D5/D3:D2=0.
    if ((requested & max31865_cmd::CONFIG_FAULT_CLEAR) != 0U &&
        !oneShotCommand && !faultCommandPresent) {
        _registers[max31865_cmd::REG_FAULT_STATUS] = 0U;
        _registers[max31865_cmd::REG_RTD_LSB] = static_cast<uint8_t>(
            _registers[max31865_cmd::REG_RTD_LSB] & 0xFEU);
        const uint8_t persistentVoltage = static_cast<uint8_t>(
            _faultInputs & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
        if (persistentVoltage != 0U) {
            _registers[max31865_cmd::REG_FAULT_STATUS] = persistentVoltage;
            _registers[max31865_cmd::REG_RTD_LSB] = static_cast<uint8_t>(
                _registers[max31865_cmd::REG_RTD_LSB] | 0x01U);
        }
    }

    uint8_t committed = static_cast<uint8_t>(
        requested & max31865_cmd::CONFIG_PERSISTENT_MASK);

    // A conversion command and its persistent filter selection are committed
    // together on CS rising, so timing must observe this just-written image.
    _registers[max31865_cmd::REG_CONFIG] = committed;

    if ((requested & max31865_cmd::CONFIG_AUTO) != 0U) {
        startContinuous();
    } else {
        _continuous = false;
        if (oneShotCommand && !commandConflict) {
            committed = static_cast<uint8_t>(
                committed | max31865_cmd::CONFIG_ONE_SHOT);
            startOneShot();
        }
    }

    if (faultCommandPresent && !commandConflict) {
        const bool continuingManual =
            _faultCycle == MAX31865FaultCycle::ManualStep1;
        committed = static_cast<uint8_t>(committed | faultCommand);
        // A standalone 11 command runs the complete automatic sequence; it is
        // manual step 2 only when the persistent step-1 state was observed.
        _faultCycle =
            faultCommand == max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2 &&
                !continuingManual
            ? MAX31865FaultCycle::Automatic
            : static_cast<MAX31865FaultCycle>(faultCommand);
        _faultCyclePending = true;
        if (_faultCycle == MAX31865FaultCycle::Automatic) {
            _faultCyclePhase = FaultCyclePhase::AutomaticRefinHigh;
        } else if (_faultCycle == MAX31865FaultCycle::ManualStep1) {
            _faultCyclePhase = FaultCyclePhase::ManualRefinHigh;
        } else {
            _faultCyclePhase = FaultCyclePhase::ManualRefinLow;
        }
        _faultCycleDueUs = _timeUs + static_cast<uint64_t>(
            max31865_cmd::MANUAL_FAULT_PHASE_US);
    } else if (_faultCyclePending ||
               _faultCycle == MAX31865FaultCycle::ManualStep1) {
        // Writing 00 is "no action". In particular, manual step 1 leaves
        // FORCE- open and D3:D2=10 until the host explicitly writes step 2.
        committed = static_cast<uint8_t>(
            committed | activeFaultCommand);
    }
    _registers[max31865_cmd::REG_CONFIG] = committed;
}

void Max31865DeviceModel::startOneShot()
{
    _continuous = false;
    _oneShot = true;
    _conversionDueUs = _timeUs +
        static_cast<uint64_t>(oneShotDurationUs());
}

void Max31865DeviceModel::startContinuous()
{
    const bool wasContinuous = _continuous;
    _continuous = true;
    _oneShot = false;
    if (!wasContinuous) {
        _conversionDueUs = _timeUs +
            static_cast<uint64_t>(oneShotDurationUs());
    }
}

void Max31865DeviceModel::stopConversions()
{
    _continuous = false;
    _oneShot = false;
    _conversionDueUs = 0U;
}

void Max31865DeviceModel::completeOneConversion()
{
    ModelSample sample = _lastSample;
    static_cast<void>(popSample(sample));

    uint8_t faults = static_cast<uint8_t>(
        sample.faultStatus |
        (_faultInputs & max31865_cmd::FAULT_OVER_UNDER_VOLTAGE));
    const uint16_t low = thresholdCode(max31865_cmd::REG_LOW_FAULT_MSB);
    const uint16_t high = thresholdCode(max31865_cmd::REG_HIGH_FAULT_MSB);
    if (sample.code <= low) {
        faults = static_cast<uint8_t>(
            faults | max31865_cmd::FAULT_LOW_THRESHOLD);
    }
    if (sample.code >= high) {
        faults = static_cast<uint8_t>(
            faults | max31865_cmd::FAULT_HIGH_THRESHOLD);
    }
    setReadySample(sample.code, faults);
    _lastSample = sample;
    _conversionCount = saturatingAdd(_conversionCount, 1U);
}

void Max31865DeviceModel::latchFaultInputs(uint8_t mask)
{
    const uint8_t observed = static_cast<uint8_t>(_faultInputs & mask);
    _registers[max31865_cmd::REG_FAULT_STATUS] = static_cast<uint8_t>(
        _registers[max31865_cmd::REG_FAULT_STATUS] | observed);
    if (observed != 0U) {
        _registers[max31865_cmd::REG_RTD_LSB] = static_cast<uint8_t>(
            _registers[max31865_cmd::REG_RTD_LSB] | 0x01U);
    }
}

void Max31865DeviceModel::finishFaultCycle()
{
    _faultCyclePending = false;
    _faultCycle = MAX31865FaultCycle::None;
    _faultCyclePhase = FaultCyclePhase::None;
    _registers[max31865_cmd::REG_CONFIG] = static_cast<uint8_t>(
        _registers[max31865_cmd::REG_CONFIG] &
        static_cast<uint8_t>(~max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
}

void Max31865DeviceModel::advanceFaultCyclePhase()
{
    switch (_faultCyclePhase) {
        case FaultCyclePhase::AutomaticRefinHigh:
            latchFaultInputs(max31865_cmd::FAULT_REFIN_HIGH);
            _faultCyclePhase = FaultCyclePhase::AutomaticForceOpen;
            _faultCycleDueUs += max31865_cmd::MANUAL_FAULT_PHASE_US;
            break;
        case FaultCyclePhase::AutomaticForceOpen:
            _faultCyclePhase = FaultCyclePhase::AutomaticRefinLow;
            _faultCycleDueUs += kAutomaticForceOpenSettleUs;
            break;
        case FaultCyclePhase::AutomaticRefinLow:
            latchFaultInputs(max31865_cmd::FAULT_REFIN_LOW);
            _faultCyclePhase = FaultCyclePhase::AutomaticRtdinLow;
            _faultCycleDueUs += max31865_cmd::MANUAL_FAULT_PHASE_US;
            break;
        case FaultCyclePhase::AutomaticRtdinLow:
            latchFaultInputs(max31865_cmd::FAULT_RTDIN_LOW);
            _faultCyclePhase = FaultCyclePhase::AutomaticComplete;
            _faultCycleDueUs += kAutomaticTerminalSlackUs;
            break;
        case FaultCyclePhase::AutomaticComplete:
            finishFaultCycle();
            break;
        case FaultCyclePhase::ManualRefinHigh:
            latchFaultInputs(max31865_cmd::FAULT_REFIN_HIGH);
            _faultCyclePhase = FaultCyclePhase::ManualForceOpen;
            _faultCycleDueUs += max31865_cmd::MANUAL_FAULT_PHASE_US;
            break;
        case FaultCyclePhase::ManualForceOpen:
            _faultCyclePending = false;
            _faultCyclePhase = FaultCyclePhase::None;
            break;
        case FaultCyclePhase::ManualRefinLow:
            latchFaultInputs(max31865_cmd::FAULT_REFIN_LOW);
            _faultCyclePhase = FaultCyclePhase::ManualRtdinLow;
            _faultCycleDueUs += max31865_cmd::MANUAL_FAULT_PHASE_US;
            break;
        case FaultCyclePhase::ManualRtdinLow:
            latchFaultInputs(max31865_cmd::FAULT_RTDIN_LOW);
            finishFaultCycle();
            break;
        case FaultCyclePhase::None:
        default:
            _faultCyclePending = false;
            break;
    }
}

void Max31865DeviceModel::setReadySample(
    uint16_t code,
    uint8_t faultStatus)
{
    if (_drdyLow) {
        _overwrittenConversionCount = saturatingAdd(
            _overwrittenConversionCount,
            1U);
    }
    const uint8_t documented = static_cast<uint8_t>(
        faultStatus & max31865_cmd::FAULT_DEFINED_MASK);
    _registers[max31865_cmd::REG_FAULT_STATUS] = static_cast<uint8_t>(
        _registers[max31865_cmd::REG_FAULT_STATUS] | documented);
    const bool anyLatchedFault =
        (_registers[max31865_cmd::REG_FAULT_STATUS] &
         max31865_cmd::FAULT_DEFINED_MASK) != 0U;
    const uint16_t raw = static_cast<uint16_t>(
        static_cast<uint16_t>(code << 1U) |
        static_cast<uint16_t>(anyLatchedFault ? 1U : 0U));
    _registers[max31865_cmd::REG_RTD_MSB] = static_cast<uint8_t>(raw >> 8U);
    _registers[max31865_cmd::REG_RTD_LSB] = static_cast<uint8_t>(raw & 0xFFU);
    _drdyLow = true;
}

void Max31865DeviceModel::acknowledgeRtdRead()
{
    _drdyLow = false;
    _rtdReadAcknowledgeCount = saturatingAdd(
        _rtdReadAcknowledgeCount,
        1U);
}

bool Max31865DeviceModel::popSample(ModelSample &sample)
{
    if (_sampleCount == 0U) {
        return false;
    }
    sample = _samples[_sampleHead];
    _sampleHead = (_sampleHead + 1U) % MODEL_SAMPLE_CAPACITY;
    --_sampleCount;
    return true;
}

uint16_t Max31865DeviceModel::thresholdCode(uint8_t msbAddress) const
{
    const uint16_t raw = static_cast<uint16_t>(
        (static_cast<uint16_t>(_registers[msbAddress]) << 8U) |
        _registers[static_cast<size_t>(msbAddress) + 1U]);
    return static_cast<uint16_t>(raw >> 1U);
}

uint32_t Max31865DeviceModel::oneShotDurationUs() const
{
    const bool hz50 =
        (_registers[max31865_cmd::REG_CONFIG] &
         max31865_cmd::CONFIG_FILTER_50HZ) != 0U ||
        (_pendingConfigValid &&
         (_pendingConfig & max31865_cmd::CONFIG_FILTER_50HZ) != 0U);
    const uint32_t durationMs = hz50
        ? max31865_cmd::SINGLE_CONVERSION_50HZ_MS
        : max31865_cmd::SINGLE_CONVERSION_60HZ_MS;
    return durationMs * 1000U;
}

uint32_t Max31865DeviceModel::continuousPeriodUs() const
{
    const bool hz50 =
        (_registers[max31865_cmd::REG_CONFIG] &
         max31865_cmd::CONFIG_FILTER_50HZ) != 0U ||
        (_pendingConfigValid &&
         (_pendingConfig & max31865_cmd::CONFIG_FILTER_50HZ) != 0U);
    const uint32_t durationMs = hz50
        ? max31865_cmd::CONTINUOUS_CONVERSION_50HZ_MS
        : max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS;
    return durationMs * 1000U;
}

uint8_t Max31865DeviceModel::readRegister(uint8_t address)
{
    if (_readOverrideEnabled && address == _readOverrideAddress) {
        ++_readOverrideSeen;
        if (_readOverrideSeen == _readOverrideOccurrence) {
            _readOverrideEnabled = false;
            return _readOverrideValue;
        }
    }
    if (_alternatingReadOverrideEnabled &&
        address == _alternatingReadOverrideAddress) {
        const uint8_t value = _alternatingReadOverrideUseFirst
            ? _alternatingReadOverrideFirstValue
            : _alternatingReadOverrideSecondValue;
        _alternatingReadOverrideUseFirst =
            !_alternatingReadOverrideUseFirst;
        return value;
    }
    return _registers[address];
}

void Max31865DeviceModel::writeRegister(uint8_t address, uint8_t value)
{
    switch (address) {
        case max31865_cmd::REG_CONFIG:
            _pendingConfig = value;
            _pendingConfigValid = true;
            break;
        case max31865_cmd::REG_HIGH_FAULT_MSB:
        case max31865_cmd::REG_LOW_FAULT_MSB:
            _registers[address] = value;
            break;
        case max31865_cmd::REG_HIGH_FAULT_LSB:
        case max31865_cmd::REG_LOW_FAULT_LSB:
            _registers[address] = static_cast<uint8_t>(
                value & max31865_cmd::THRESHOLD_LSB_DEFINED_MASK);
            break;
        default:
            break;
    }
}

uint32_t Max31865DeviceModel::saturatingAdd(
    uint32_t value,
    uint64_t increment)
{
    const uint64_t sum = static_cast<uint64_t>(value) + increment;
    return sum > static_cast<uint64_t>(UINT32_MAX)
        ? UINT32_MAX
        : static_cast<uint32_t>(sum);
}

} // namespace max31865_test
