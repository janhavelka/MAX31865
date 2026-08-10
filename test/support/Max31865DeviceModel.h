#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MAX31865/CommandTable.h"
#include "MAX31865/Config.h"
#include "MAX31865/Status.h"

namespace max31865_test {

static constexpr size_t MODEL_SAMPLE_CAPACITY = 32U;

/** One deterministic conversion result queued for the next completed cycle. */
struct ModelSample {
    uint16_t code;
    uint8_t faultStatus;
};

/**
 * Framework-neutral behavioral model of the MAX31865 register interface.
 *
 * The model deliberately does not call production protocol helpers. This keeps
 * it useful as an independent oracle for frame encoding, register side effects,
 * DRDY acknowledgement, fault latching, and conversion timing tests.
 */
class Max31865DeviceModel final {
public:
    Max31865DeviceModel();

    void powerOnReset();
    void advanceTimeUs(uint64_t elapsedUs);
    void advanceTimeMs(uint32_t elapsedMs);

    MAX31865Status setChipSelect(bool asserted);
    MAX31865Status transfer(
        const uint8_t *tx,
        uint8_t *rx,
        size_t length);
    MAX31865Status readDrdy(bool &level) const;

    bool chipSelected() const;
    bool dataReady() const;
    bool conversionRunning() const;
    bool continuousConversion() const;
    bool oneShotActive() const;
    uint64_t timeUs() const;
    uint8_t registerValue(uint8_t address) const;
    uint8_t faultLatch() const;
    uint32_t conversionCount() const;
    uint32_t overwrittenConversionCount() const;
    uint32_t rtdReadAcknowledgeCount() const;
    uint32_t invalidFrameCount() const;

    bool enqueueSample(uint16_t code, uint8_t faultStatus = 0U);
    void forceReadySample(uint16_t code, uint8_t faultStatus = 0U);
    void setFaultInputs(uint8_t documentedFaultBits);
    void setRegisterReadOverride(
        uint8_t address,
        uint8_t value,
        uint32_t matchingOccurrence = 1U);
    void setAlternatingRegisterReadOverride(
        uint8_t address,
        uint8_t firstValue,
        uint8_t secondValue);

private:
    enum class FaultCyclePhase : uint8_t {
        None = 0,
        AutomaticRefinHigh,
        AutomaticForceOpen,
        AutomaticRefinLow,
        AutomaticRtdinLow,
        AutomaticComplete,
        ManualRefinHigh,
        ManualForceOpen,
        ManualRefinLow,
        ManualRtdinLow
    };

    void updateTimeDependentState();
    void applyPendingConfig();
    void startOneShot();
    void startContinuous();
    void stopConversions();
    void completeOneConversion();
    void advanceFaultCyclePhase();
    void latchFaultInputs(uint8_t mask);
    void finishFaultCycle();
    void setReadySample(uint16_t code, uint8_t faultStatus);
    void acknowledgeRtdRead();
    bool popSample(ModelSample &sample);
    uint16_t thresholdCode(uint8_t msbAddress) const;
    uint32_t oneShotDurationUs() const;
    uint32_t continuousPeriodUs() const;
    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);
    static uint32_t saturatingAdd(uint32_t value, uint64_t increment);

    uint8_t _registers[max31865_cmd::NUM_REGISTERS];
    bool _chipSelected;
    bool _transferSeenWhileSelected;
    bool _pendingConfigValid;
    uint8_t _pendingConfig;
    bool _pendingRtdAcknowledge;
    uint64_t _timeUs;

    bool _continuous;
    bool _oneShot;
    uint64_t _conversionDueUs;
    bool _faultCyclePending;
    MAX31865FaultCycle _faultCycle;
    FaultCyclePhase _faultCyclePhase;
    uint64_t _faultCycleDueUs;
    uint8_t _faultInputs;

    ModelSample _samples[MODEL_SAMPLE_CAPACITY];
    size_t _sampleHead;
    size_t _sampleCount;
    ModelSample _lastSample;

    bool _drdyLow;
    uint32_t _conversionCount;
    uint32_t _overwrittenConversionCount;
    uint32_t _rtdReadAcknowledgeCount;
    uint32_t _invalidFrameCount;

    bool _readOverrideEnabled;
    uint8_t _readOverrideAddress;
    uint8_t _readOverrideValue;
    uint32_t _readOverrideOccurrence;
    uint32_t _readOverrideSeen;
    bool _alternatingReadOverrideEnabled;
    uint8_t _alternatingReadOverrideAddress;
    uint8_t _alternatingReadOverrideFirstValue;
    uint8_t _alternatingReadOverrideSecondValue;
    bool _alternatingReadOverrideUseFirst;
};

} // namespace max31865_test
