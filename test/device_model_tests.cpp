#include "support/Max31865DeviceModel.h"
#include "support/ScriptedMax31865Transport.h"
#include "support/TestHarness.h"

#include <stdlib.h>
#include <string.h>

using namespace max31865_test;

namespace {

bool exchange(
    Max31865DeviceModel &device,
    const uint8_t *tx,
    uint8_t *rx,
    size_t length)
{
    if (!device.setChipSelect(true).ok()) {
        return false;
    }
    const MAX31865Status transferStatus = device.transfer(tx, rx, length);
    const MAX31865Status deassertStatus = device.setChipSelect(false);
    return transferStatus.ok() && deassertStatus.ok();
}

bool writeBytes(
    Max31865DeviceModel &device,
    uint8_t start,
    const uint8_t *values,
    size_t length)
{
    if (length == 0U || length >= max31865_cmd::MAX_FRAME_BYTES) {
        return false;
    }
    uint8_t tx[max31865_cmd::MAX_FRAME_BYTES] = {};
    uint8_t rx[max31865_cmd::MAX_FRAME_BYTES] = {};
    tx[0] = static_cast<uint8_t>(start | max31865_cmd::WRITE_BIT);
    memcpy(&tx[1], values, length);
    return exchange(device, tx, rx, length + 1U);
}

bool writeByte(Max31865DeviceModel &device, uint8_t address, uint8_t value)
{
    return writeBytes(device, address, &value, 1U);
}

bool readBytes(
    Max31865DeviceModel &device,
    uint8_t start,
    uint8_t *values,
    size_t length)
{
    if (values == nullptr || length == 0U ||
        length >= max31865_cmd::MAX_FRAME_BYTES) {
        return false;
    }
    uint8_t tx[max31865_cmd::MAX_FRAME_BYTES] = {};
    uint8_t rx[max31865_cmd::MAX_FRAME_BYTES] = {};
    tx[0] = static_cast<uint8_t>(start & max31865_cmd::READ_MASK);
    if (!exchange(device, tx, rx, length + 1U)) {
        return false;
    }
    memcpy(values, &rx[1], length);
    return true;
}

bool resetImageAndReadOnlyWrites()
{
    Max31865DeviceModel device;
    const uint8_t expected[max31865_cmd::NUM_REGISTERS] = {
        max31865_cmd::CONFIG_RESET,
        max31865_cmd::RTD_MSB_RESET,
        max31865_cmd::RTD_LSB_RESET,
        max31865_cmd::HIGH_FAULT_MSB_RESET,
        max31865_cmd::HIGH_FAULT_LSB_RESET,
        max31865_cmd::LOW_FAULT_MSB_RESET,
        max31865_cmd::LOW_FAULT_LSB_RESET,
        max31865_cmd::FAULT_STATUS_RESET};
    for (uint8_t address = 0U; address <= max31865_cmd::REG_LAST; ++address) {
        CHECK_EQ(expected[address], device.registerValue(address));
    }
    CHECK(!device.chipSelected());
    CHECK(!device.dataReady());
    CHECK(!device.conversionRunning());

    CHECK(writeByte(device, max31865_cmd::REG_RTD_MSB, 0xA5U));
    CHECK(writeByte(device, max31865_cmd::REG_RTD_LSB, 0x5AU));
    CHECK(writeByte(device, max31865_cmd::REG_FAULT_STATUS, 0xFCU));
    CHECK_EQ(max31865_cmd::RTD_MSB_RESET,
             device.registerValue(max31865_cmd::REG_RTD_MSB));
    CHECK_EQ(max31865_cmd::RTD_LSB_RESET,
             device.registerValue(max31865_cmd::REG_RTD_LSB));
    CHECK_EQ(max31865_cmd::FAULT_STATUS_RESET,
             device.registerValue(max31865_cmd::REG_FAULT_STATUS));
    return true;
}

bool frameBoundsAndAutoIncrement()
{
    Max31865DeviceModel device;
    uint8_t rx[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    const uint8_t shortTx[1] = {0U};
    CHECK_EQ(MAX31865Error::InvalidState,
             device.transfer(shortTx, rx, sizeof(shortTx)).code);
    CHECK_EQ(1U, device.invalidFrameCount());

    CHECK(device.setChipSelect(true).ok());
    CHECK_EQ(MAX31865Error::InvalidArgument,
             device.transfer(shortTx, rx, sizeof(shortTx)).code);
    const uint8_t invalidSpan[3] = {max31865_cmd::REG_FAULT_STATUS, 0U, 0U};
    CHECK_EQ(MAX31865Error::RegisterAddressInvalid,
             device.transfer(invalidSpan, rx, sizeof(invalidSpan)).code);
    CHECK(device.setChipSelect(false).ok());
    CHECK_EQ(3U, device.invalidFrameCount());

    const uint8_t thresholds[4] = {0x12U, 0x35U, 0x56U, 0x79U};
    CHECK(writeBytes(
        device,
        max31865_cmd::REG_HIGH_FAULT_MSB,
        thresholds,
        sizeof(thresholds)));
    CHECK_EQ(0x12U, device.registerValue(max31865_cmd::REG_HIGH_FAULT_MSB));
    CHECK_EQ(0x34U, device.registerValue(max31865_cmd::REG_HIGH_FAULT_LSB));
    CHECK_EQ(0x56U, device.registerValue(max31865_cmd::REG_LOW_FAULT_MSB));
    CHECK_EQ(0x78U, device.registerValue(max31865_cmd::REG_LOW_FAULT_LSB));

    uint8_t observed[4] = {};
    CHECK(readBytes(
        device,
        max31865_cmd::REG_HIGH_FAULT_MSB,
        observed,
        sizeof(observed)));
    CHECK(memcmp(observed, "\x12\x34\x56\x78", sizeof(observed)) == 0);
    return true;
}

bool oneTransferCallbackRequiresAChipSelectBoundary()
{
    Max31865DeviceModel device;
    const uint8_t configTx[2] = {max31865_cmd::REG_CONFIG, 0U};
    const uint8_t faultTx[2] = {max31865_cmd::REG_FAULT_STATUS, 0U};
    uint8_t configRx[2] = {0xA5U, 0x5AU};
    uint8_t faultRx[2] = {0x3CU, 0xC3U};

    CHECK(device.setChipSelect(true).ok());
    CHECK(device.transfer(configTx, configRx, sizeof(configTx)).ok());
    CHECK_EQ(max31865_cmd::CONFIG_RESET, configRx[1]);
    CHECK_EQ(
        MAX31865Error::InvalidState,
        device.transfer(faultTx, faultRx, sizeof(faultTx)).code);
    CHECK_EQ(0x3CU, faultRx[0]);
    CHECK_EQ(0xC3U, faultRx[1]);
    CHECK_EQ(1U, device.invalidFrameCount());

    // Reasserting an already-low CS is not a frame boundary. Only an actual
    // rising edge closes the previous transaction and permits another frame.
    CHECK(device.setChipSelect(true).ok());
    CHECK_EQ(
        MAX31865Error::InvalidState,
        device.transfer(faultTx, faultRx, sizeof(faultTx)).code);
    CHECK_EQ(2U, device.invalidFrameCount());
    CHECK(device.setChipSelect(false).ok());
    CHECK(device.setChipSelect(true).ok());
    CHECK(device.transfer(faultTx, faultRx, sizeof(faultTx)).ok());
    CHECK_EQ(max31865_cmd::FAULT_STATUS_RESET, faultRx[1]);
    CHECK(device.setChipSelect(false).ok());
    return true;
}

bool drdyAcknowledgementIsLimitedToRtdReads()
{
    Max31865DeviceModel device;
    device.forceReadySample(0x1234U, max31865_cmd::FAULT_REFIN_LOW);
    CHECK(device.dataReady());
    bool level = true;
    CHECK(device.readDrdy(level).ok());
    CHECK(!level);

    uint8_t value = 0U;
    CHECK(readBytes(device, max31865_cmd::REG_FAULT_STATUS, &value, 1U));
    CHECK(device.dataReady());
    CHECK_EQ(0U, device.rtdReadAcknowledgeCount());
    CHECK_EQ(max31865_cmd::FAULT_REFIN_LOW, value);

    const uint8_t rtdTx[2] = {max31865_cmd::REG_RTD_LSB, 0U};
    uint8_t rtdRx[2]{};
    CHECK(device.setChipSelect(true).ok());
    CHECK(device.transfer(rtdTx, rtdRx, sizeof(rtdTx)).ok());
    value = rtdRx[1];
    CHECK(device.dataReady());
    CHECK_EQ(0U, device.rtdReadAcknowledgeCount());
    CHECK(device.setChipSelect(false).ok());
    CHECK(!device.dataReady());
    CHECK_EQ(1U, device.rtdReadAcknowledgeCount());
    CHECK(device.readDrdy(level).ok());
    CHECK(level);

    device.forceReadySample(0x4321U, 0U);
    uint8_t frame[2] = {};
    CHECK(readBytes(device, max31865_cmd::REG_RTD_MSB, frame, 2U));
    CHECK_EQ(0x86U, frame[0]);
    CHECK_EQ(0x43U, frame[1]);
    CHECK_EQ(2U, device.rtdReadAcknowledgeCount());
    return true;
}

bool oneShotTimingAndSelfClearingCommands()
{
    Max31865DeviceModel device;
    CHECK(device.enqueueSample(8192U));
    const uint8_t command60 = static_cast<uint8_t>(
        max31865_cmd::CONFIG_BIAS | max31865_cmd::CONFIG_ONE_SHOT);
    CHECK(writeByte(device, max31865_cmd::REG_CONFIG, command60));
    CHECK(device.oneShotActive());
    CHECK((device.registerValue(max31865_cmd::REG_CONFIG) &
           max31865_cmd::CONFIG_ONE_SHOT) != 0U);
    device.advanceTimeUs(
        static_cast<uint64_t>(max31865_cmd::SINGLE_CONVERSION_60HZ_MS) *
            1000ULL -
        1ULL);
    CHECK(!device.dataReady());
    CHECK(device.oneShotActive());
    device.advanceTimeUs(1U);
    CHECK(device.dataReady());
    CHECK(!device.oneShotActive());
    CHECK_EQ(1U, device.conversionCount());
    CHECK_EQ(max31865_cmd::CONFIG_BIAS,
             device.registerValue(max31865_cmd::REG_CONFIG));

    device.powerOnReset();
    CHECK(device.enqueueSample(9000U));
    const uint8_t command50 = static_cast<uint8_t>(
        max31865_cmd::CONFIG_BIAS |
        max31865_cmd::CONFIG_FILTER_50HZ |
        max31865_cmd::CONFIG_ONE_SHOT);
    CHECK(writeByte(device, max31865_cmd::REG_CONFIG, command50));
    device.advanceTimeUs(
        static_cast<uint64_t>(max31865_cmd::SINGLE_CONVERSION_50HZ_MS) *
            1000ULL -
        1ULL);
    CHECK(!device.dataReady());
    device.advanceTimeUs(1U);
    CHECK(device.dataReady());
    CHECK_EQ(static_cast<uint8_t>(
                 max31865_cmd::CONFIG_BIAS |
                 max31865_cmd::CONFIG_FILTER_50HZ),
             device.registerValue(max31865_cmd::REG_CONFIG));
    return true;
}

bool continuousTimingAndOverwriteAccounting()
{
    Max31865DeviceModel device;
    CHECK(device.enqueueSample(1000U));
    CHECK(device.enqueueSample(2000U));
    CHECK(device.enqueueSample(3000U));
    const uint8_t command = static_cast<uint8_t>(
        max31865_cmd::CONFIG_BIAS | max31865_cmd::CONFIG_AUTO);
    CHECK(writeByte(device, max31865_cmd::REG_CONFIG, command));
    CHECK(device.continuousConversion());
    device.advanceTimeMs(max31865_cmd::SINGLE_CONVERSION_60HZ_MS - 1U);
    CHECK(!device.dataReady());
    device.advanceTimeMs(1U);
    CHECK(device.dataReady());
    CHECK_EQ(1U, device.conversionCount());
    device.advanceTimeMs(max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS * 2U);
    CHECK_EQ(3U, device.conversionCount());
    CHECK_EQ(2U, device.overwrittenConversionCount());

    uint8_t rtd[2] = {};
    CHECK(readBytes(device, max31865_cmd::REG_RTD_MSB, rtd, sizeof(rtd)));
    CHECK_EQ(3000U, static_cast<uint16_t>(
                        (static_cast<uint16_t>(rtd[0]) << 7U) |
                        (static_cast<uint16_t>(rtd[1]) >> 1U)));
    CHECK(!device.dataReady());

    CHECK(writeByte(device, max31865_cmd::REG_CONFIG, 0U));
    CHECK(!device.conversionRunning());
    device.advanceTimeMs(max31865_cmd::CONTINUOUS_CONVERSION_60HZ_MS * 4U);
    CHECK_EQ(3U, device.conversionCount());
    return true;
}

bool thresholdsFaultLatchAndClearCommand()
{
    Max31865DeviceModel device;
    const uint16_t highCode = 200U;
    const uint16_t lowCode = 100U;
    const uint8_t thresholds[4] = {
        static_cast<uint8_t>((highCode << 1U) >> 8U),
        static_cast<uint8_t>((highCode << 1U) & 0xFFU),
        static_cast<uint8_t>((lowCode << 1U) >> 8U),
        static_cast<uint8_t>((lowCode << 1U) & 0xFFU)};
    CHECK(writeBytes(
        device,
        max31865_cmd::REG_HIGH_FAULT_MSB,
        thresholds,
        sizeof(thresholds)));
    CHECK(device.enqueueSample(lowCode));
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_ONE_SHOT)));
    device.advanceTimeMs(max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    CHECK_EQ(max31865_cmd::FAULT_LOW_THRESHOLD, device.faultLatch());
    CHECK((device.registerValue(max31865_cmd::REG_RTD_LSB) & 0x01U) != 0U);

    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        max31865_cmd::CONFIG_FAULT_CLEAR));
    CHECK_EQ(0U, device.faultLatch());
    CHECK((device.registerValue(max31865_cmd::REG_RTD_LSB) & 0x01U) == 0U);

    CHECK(device.enqueueSample(highCode));
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_ONE_SHOT)));
    device.advanceTimeMs(max31865_cmd::SINGLE_CONVERSION_60HZ_MS);
    CHECK_EQ(max31865_cmd::FAULT_HIGH_THRESHOLD, device.faultLatch());
    return true;
}

bool automaticAndManualFaultCycles()
{
    Max31865DeviceModel device;
    const uint8_t source = static_cast<uint8_t>(
        max31865_cmd::FAULT_REFIN_HIGH |
        max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    device.setFaultInputs(source);
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_FAULT_CYCLE_AUTO)));
    CHECK_EQ(max31865_cmd::CONFIG_FAULT_CYCLE_AUTO,
             static_cast<uint8_t>(
                 device.registerValue(max31865_cmd::REG_CONFIG) &
                 max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    device.advanceTimeUs(max31865_cmd::AUTO_FAULT_DETECTION_MAX_US - 1U);
    CHECK_EQ(source, device.faultLatch());
    CHECK_EQ(max31865_cmd::CONFIG_FAULT_CYCLE_AUTO,
             static_cast<uint8_t>(
                 device.registerValue(max31865_cmd::REG_CONFIG) &
                 max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    device.advanceTimeUs(1U);
    CHECK_EQ(source, device.faultLatch());
    CHECK_EQ(0U, static_cast<uint8_t>(
                     device.registerValue(max31865_cmd::REG_CONFIG) &
                     max31865_cmd::CONFIG_FAULT_CYCLE_MASK));

    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        max31865_cmd::CONFIG_FAULT_CLEAR));
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1)));
    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_STEP1_TO_OPEN_US);
    CHECK_EQ(source, device.faultLatch());
    CHECK_EQ(max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1,
             static_cast<uint8_t>(
                 device.registerValue(max31865_cmd::REG_CONFIG) &
                 max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    CHECK(writeByte(device, max31865_cmd::REG_CONFIG, 0U));
    CHECK_EQ(max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1,
             static_cast<uint8_t>(
                 device.registerValue(max31865_cmd::REG_CONFIG) &
                 max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2)));
    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_STEP2_MAX_US);
    CHECK_EQ(source, device.faultLatch());
    return true;
}

bool automaticFaultStagesLatchAtDocumentedBoundaries()
{
    Max31865DeviceModel device;
    const uint8_t stagedInputs = static_cast<uint8_t>(
        max31865_cmd::FAULT_REFIN_HIGH |
        max31865_cmd::FAULT_REFIN_LOW |
        max31865_cmd::FAULT_RTDIN_LOW);
    device.setFaultInputs(stagedInputs);

    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_FAULT_CYCLE_AUTO)));

    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_PHASE_US - 1U);
    CHECK_EQ(0U, device.faultLatch());
    device.advanceTimeUs(1U);
    CHECK_EQ(max31865_cmd::FAULT_REFIN_HIGH, device.faultLatch());
    CHECK((device.registerValue(max31865_cmd::REG_RTD_LSB) & 0x01U) != 0U);

    // The automatic flow opens FORCE- at +200 us, then samples REFIN- after
    // the documented 210 us settling interval.
    device.advanceTimeUs(309U);
    CHECK_EQ(max31865_cmd::FAULT_REFIN_HIGH, device.faultLatch());
    device.advanceTimeUs(1U);
    CHECK_EQ(
        static_cast<uint8_t>(
            max31865_cmd::FAULT_REFIN_HIGH |
            max31865_cmd::FAULT_REFIN_LOW),
        device.faultLatch());

    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_PHASE_US - 1U);
    CHECK_EQ(
        static_cast<uint8_t>(
            max31865_cmd::FAULT_REFIN_HIGH |
            max31865_cmd::FAULT_REFIN_LOW),
        device.faultLatch());
    device.advanceTimeUs(1U);
    CHECK_EQ(stagedInputs, device.faultLatch());
    CHECK_EQ(max31865_cmd::CONFIG_FAULT_CYCLE_AUTO,
             static_cast<uint8_t>(
                 device.registerValue(max31865_cmd::REG_CONFIG) &
                 max31865_cmd::CONFIG_FAULT_CYCLE_MASK));

    // Comparator phases finish by +510 us; retain the active cycle until the
    // conservative data-sheet maximum and then self-clear D3:D2 at +600 us.
    device.advanceTimeUs(89U);
    CHECK_EQ(max31865_cmd::CONFIG_FAULT_CYCLE_AUTO,
             static_cast<uint8_t>(
                 device.registerValue(max31865_cmd::REG_CONFIG) &
                 max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    device.advanceTimeUs(1U);
    CHECK_EQ(0U, static_cast<uint8_t>(
                     device.registerValue(max31865_cmd::REG_CONFIG) &
                     max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    return true;
}

bool manualFaultStagesLatchOnlyTheirDocumentedInputs()
{
    Max31865DeviceModel device;
    const uint8_t stagedInputs = static_cast<uint8_t>(
        max31865_cmd::FAULT_REFIN_HIGH |
        max31865_cmd::FAULT_REFIN_LOW |
        max31865_cmd::FAULT_RTDIN_LOW);
    device.setFaultInputs(stagedInputs);

    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1)));
    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_PHASE_US - 1U);
    CHECK_EQ(0U, device.faultLatch());
    device.advanceTimeUs(1U);
    CHECK_EQ(max31865_cmd::FAULT_REFIN_HIGH, device.faultLatch());
    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_PHASE_US - 1U);
    CHECK_EQ(max31865_cmd::FAULT_REFIN_HIGH, device.faultLatch());
    device.advanceTimeUs(1U);
    CHECK_EQ(
        max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1,
        static_cast<uint8_t>(
            device.registerValue(max31865_cmd::REG_CONFIG) &
            max31865_cmd::CONFIG_FAULT_CYCLE_MASK));

    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS |
            max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2)));
    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_PHASE_US - 1U);
    CHECK_EQ(max31865_cmd::FAULT_REFIN_HIGH, device.faultLatch());
    device.advanceTimeUs(1U);
    CHECK_EQ(
        static_cast<uint8_t>(
            max31865_cmd::FAULT_REFIN_HIGH |
            max31865_cmd::FAULT_REFIN_LOW),
        device.faultLatch());
    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_PHASE_US - 1U);
    CHECK_EQ(
        static_cast<uint8_t>(
            max31865_cmd::FAULT_REFIN_HIGH |
            max31865_cmd::FAULT_REFIN_LOW),
        device.faultLatch());
    device.advanceTimeUs(1U);
    CHECK_EQ(stagedInputs, device.faultLatch());
    CHECK_EQ(0U, static_cast<uint8_t>(
                     device.registerValue(max31865_cmd::REG_CONFIG) &
                     max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    return true;
}

bool commandInteractionsAndPersistentVoltageFaults()
{
    Max31865DeviceModel device;
    device.setFaultInputs(max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    CHECK_EQ(max31865_cmd::FAULT_OVER_UNDER_VOLTAGE, device.faultLatch());
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        max31865_cmd::CONFIG_FAULT_CLEAR));
    CHECK_EQ(max31865_cmd::FAULT_OVER_UNDER_VOLTAGE, device.faultLatch());

    device.setFaultInputs(0U);
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        max31865_cmd::CONFIG_FAULT_CLEAR));
    CHECK_EQ(0U, device.faultLatch());

    device.forceReadySample(100U, max31865_cmd::FAULT_RTDIN_LOW);
    const uint8_t conflicting = static_cast<uint8_t>(
        max31865_cmd::CONFIG_ONE_SHOT |
        max31865_cmd::CONFIG_FAULT_CYCLE_AUTO |
        max31865_cmd::CONFIG_FAULT_CLEAR);
    CHECK(writeByte(device, max31865_cmd::REG_CONFIG, conflicting));
    CHECK(!device.oneShotActive());
    CHECK_EQ(0U, static_cast<uint8_t>(
                     device.registerValue(max31865_cmd::REG_CONFIG) &
                     (max31865_cmd::CONFIG_ONE_SHOT |
                      max31865_cmd::CONFIG_FAULT_CYCLE_MASK)));
    CHECK_EQ(max31865_cmd::FAULT_RTDIN_LOW, device.faultLatch());

    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        max31865_cmd::CONFIG_FAULT_CLEAR));
    CHECK_EQ(0U, device.faultLatch());
    device.setFaultInputs(max31865_cmd::FAULT_REFIN_HIGH);
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2));
    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_PHASE_US - 1U);
    CHECK_EQ(0U, device.faultLatch());
    CHECK_EQ(max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2,
             static_cast<uint8_t>(
                 device.registerValue(max31865_cmd::REG_CONFIG) &
                 max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    device.advanceTimeUs(1U);
    CHECK_EQ(max31865_cmd::FAULT_REFIN_HIGH, device.faultLatch());
    device.advanceTimeUs(max31865_cmd::MANUAL_FAULT_PHASE_US);
    CHECK_EQ(max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2,
             static_cast<uint8_t>(
                 device.registerValue(max31865_cmd::REG_CONFIG) &
                 max31865_cmd::CONFIG_FAULT_CYCLE_MASK));
    device.advanceTimeUs(
        max31865_cmd::AUTO_FAULT_DETECTION_MAX_US -
        max31865_cmd::MANUAL_FAULT_STEP2_MAX_US);
    CHECK_EQ(max31865_cmd::FAULT_REFIN_HIGH, device.faultLatch());
    CHECK_EQ(0U, static_cast<uint8_t>(
                     device.registerValue(max31865_cmd::REG_CONFIG) &
                     max31865_cmd::CONFIG_FAULT_CYCLE_MASK));

    device.powerOnReset();
    CHECK(device.enqueueSample(1234U));
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        static_cast<uint8_t>(
            max31865_cmd::CONFIG_BIAS | max31865_cmd::CONFIG_AUTO)));
    device.setFaultInputs(max31865_cmd::FAULT_OVER_UNDER_VOLTAGE);
    device.advanceTimeMs(200U);
    CHECK_EQ(0U, device.conversionCount());
    CHECK(!device.dataReady());
    device.setFaultInputs(0U);
    device.advanceTimeUs(
        static_cast<uint64_t>(max31865_cmd::SINGLE_CONVERSION_60HZ_MS) *
            1000ULL -
        1ULL);
    CHECK_EQ(0U, device.conversionCount());
    device.advanceTimeUs(1U);
    CHECK_EQ(1U, device.conversionCount());
    CHECK(device.dataReady());
    CHECK((device.registerValue(max31865_cmd::REG_RTD_LSB) & 0x01U) != 0U);
    CHECK_EQ(max31865_cmd::FAULT_OVER_UNDER_VOLTAGE, device.faultLatch());
    CHECK(writeByte(
        device,
        max31865_cmd::REG_CONFIG,
        max31865_cmd::CONFIG_FAULT_CLEAR));
    CHECK_EQ(0U, device.faultLatch());
    CHECK((device.registerValue(max31865_cmd::REG_RTD_LSB) & 0x01U) == 0U);
    return true;
}

bool deterministicReadOverride()
{
    Max31865DeviceModel device;
    device.setRegisterReadOverride(
        max31865_cmd::REG_CONFIG,
        0xA5U,
        2U);
    uint8_t value = 0xFFU;
    CHECK(readBytes(device, max31865_cmd::REG_CONFIG, &value, 1U));
    CHECK_EQ(max31865_cmd::CONFIG_RESET, value);
    CHECK(readBytes(device, max31865_cmd::REG_CONFIG, &value, 1U));
    CHECK_EQ(0xA5U, value);
    CHECK(readBytes(device, max31865_cmd::REG_CONFIG, &value, 1U));
    CHECK_EQ(max31865_cmd::CONFIG_RESET, value);
    return true;
}

bool transportTranscriptAndTimeCallbacks()
{
    ScriptedMax31865Transport scripted;
    MAX31865Transport transport = scripted.makeTransport();
    CHECK(transport.capabilities.hasDrdy);
    CHECK(transport.lockBus != nullptr);
    CHECK(transport.unlockBus != nullptr);
    CHECK(transport.nowUs != nullptr);
    CHECK(transport.lockBus(transport.user, 9U).ok());
    CHECK(transport.setChipSelect(transport.user, true).ok());
    const uint8_t tx[2] = {
        static_cast<uint8_t>(
            max31865_cmd::WRITE_BIT | max31865_cmd::REG_CONFIG),
        max31865_cmd::CONFIG_BIAS};
    uint8_t rx[2] = {};
    CHECK(transport.transfer(
              transport.user,
              tx,
              rx,
              sizeof(tx),
              8U)
              .ok());
    transport.delayUs(transport.user, 7U);
    CHECK(transport.setChipSelect(transport.user, false).ok());
    transport.sleepMs(transport.user, 3U);
    transport.unlockBus(transport.user);
    CHECK_EQ(3007U, transport.nowUs(transport.user));
    CHECK_EQ(3U, transport.nowMs(transport.user));
    CHECK(!scripted.busLocked());
    CHECK_EQ(7U, scripted.eventCount());
    CHECK_EQ(EventKind::LockBus, scripted.event(0U).kind);
    CHECK_EQ(9U, scripted.event(0U).argument);
    CHECK_EQ(EventKind::ChipSelectAssert, scripted.event(1U).kind);
    CHECK_EQ(EventKind::Transfer, scripted.event(2U).kind);
    CHECK_EQ(2U, scripted.event(2U).length);
    CHECK(memcmp(scripted.event(2U).tx, tx, sizeof(tx)) == 0);
    CHECK_EQ(EventKind::DelayUs, scripted.event(3U).kind);
    CHECK_EQ(7U, scripted.event(3U).argument);
    CHECK_EQ(EventKind::ChipSelectDeassert, scripted.event(4U).kind);
    CHECK_EQ(7U, scripted.event(4U).timeUs);
    CHECK_EQ(EventKind::SleepMs, scripted.event(5U).kind);
    CHECK_EQ(EventKind::UnlockBus, scripted.event(6U).kind);
    CHECK_EQ(max31865_cmd::CONFIG_BIAS,
             scripted.device().registerValue(max31865_cmd::REG_CONFIG));

    const MAX31865Transport noOptional = scripted.makeTransport(
        false,
        false,
        false);
    CHECK(!noOptional.capabilities.hasDrdy);
    CHECK(noOptional.lockBus == nullptr);
    CHECK(noOptional.unlockBus == nullptr);
    CHECK(noOptional.readPin == nullptr);
    CHECK(noOptional.nowUs == nullptr);
    return true;
}

bool transportFailureInjectionEffectsAndOutputPreservation()
{
    ScriptedMax31865Transport scripted;
    CHECK(scripted.setChipSelect(true).ok());
    scripted.resetLog();
    CHECK(scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed,
        FailureEffect::NotApplied,
        2500U,
        -77)));
    const uint8_t tx[2] = {
        static_cast<uint8_t>(
            max31865_cmd::WRITE_BIT |
            max31865_cmd::REG_HIGH_FAULT_MSB),
        0x12U};
    uint8_t rx[2] = {0xA5U, 0x5AU};
    const MAX31865Status failed = scripted.transfer(
        tx,
        rx,
        sizeof(tx),
        4U);
    CHECK_EQ(MAX31865Error::SpiTransferFailed, failed.code);
    CHECK_EQ(-77, failed.detail);
    CHECK_EQ(0xA5U, rx[0]);
    CHECK_EQ(0x5AU, rx[1]);
    CHECK_EQ(max31865_cmd::HIGH_FAULT_MSB_RESET,
             scripted.device().registerValue(
                 max31865_cmd::REG_HIGH_FAULT_MSB));
    CHECK_EQ(2500U, scripted.timeUs());
    CHECK_EQ(MAX31865Error::SpiTransferFailed, scripted.event(0U).result);

    scripted.clearFailures();
    scripted.resetLog();
    CHECK(scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed,
        FailureEffect::Applied)));
    CHECK_EQ(MAX31865Error::SpiTransferFailed,
             scripted.transfer(tx, rx, sizeof(tx), 1U).code);
    CHECK(scripted.setChipSelect(false).ok());
    CHECK_EQ(0x12U,
             scripted.device().registerValue(
                 max31865_cmd::REG_HIGH_FAULT_MSB));

    scripted.device().forceReadySample(123U);
    scripted.clearFailures();
    scripted.resetLog();
    CHECK(scripted.addFailure(makeFailure(
        EventKind::ReadDrdy,
        1U,
        MAX31865Error::GpioFailed)));
    bool level = true;
    CHECK_EQ(MAX31865Error::GpioFailed, scripted.readDrdy(level).code);
    CHECK(level);
    CHECK(scripted.device().dataReady());
    return true;
}

bool lockFailuresAndModuloClocks()
{
    ScriptedMax31865Transport scripted;
    scripted.setTimeUs(
        (static_cast<uint64_t>(UINT32_MAX) - 1ULL) * 1000ULL);
    CHECK_EQ(UINT32_MAX - 1U, scripted.nowMs());
    scripted.advanceTimeMs(3U);
    CHECK_EQ(1U, scripted.nowMs());

    scripted.resetLog();
    CHECK(scripted.addFailure(makeFailure(
        EventKind::LockBus,
        1U,
        MAX31865Error::BusLockTimeout,
        FailureEffect::NotApplied,
        1000U)));
    CHECK_EQ(MAX31865Error::BusLockTimeout, scripted.lockBus(1U).code);
    CHECK(!scripted.busLocked());
    CHECK_EQ(1U, scripted.matchingEventCount(EventKind::LockBus));
    CHECK_EQ(1U, scripted.event(0U).argument);
    return true;
}

void triggerEventOverflow()
{
    ScriptedMax31865Transport scripted;
    for (size_t index = 0U; index <= TRANSPORT_EVENT_CAPACITY; ++index) {
        scripted.delayUs(0U);
    }
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--trigger-event-overflow") == 0) {
        triggerEventOverflow();
        return 0;
    }

    const TestCase tests[] = {
        {"reset image and read-only writes", resetImageAndReadOnlyWrites},
        {"frame bounds and auto increment", frameBoundsAndAutoIncrement},
        {"single transfer per CS frame",
         oneTransferCallbackRequiresAChipSelectBoundary},
        {"DRDY acknowledgement", drdyAcknowledgementIsLimitedToRtdReads},
        {"one-shot timing and self-clearing", oneShotTimingAndSelfClearingCommands},
        {"continuous timing and overwrites", continuousTimingAndOverwriteAccounting},
        {"threshold latch and fault clear", thresholdsFaultLatchAndClearCommand},
         {"automatic and manual fault cycles", automaticAndManualFaultCycles},
        {"automatic fault phase timing",
         automaticFaultStagesLatchAtDocumentedBoundaries},
        {"manual fault stage masks",
         manualFaultStagesLatchOnlyTheirDocumentedInputs},
        {"command interactions and persistent voltage faults",
         commandInteractionsAndPersistentVoltageFaults},
        {"deterministic register override", deterministicReadOverride},
        {"transport transcript and time", transportTranscriptAndTimeCallbacks},
        {"transport injected failure effects",
         transportFailureInjectionEffectsAndOutputPreservation},
        {"lock failures and modulo clocks", lockFailuresAndModuloClocks}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
