#include "MAX31865/Protocol.h"
#include "support/Max31865DeviceModel.h"
#include "support/TestHarness.h"

#include <string.h>

using namespace max31865_test;

namespace {

bool registerConstantsMatchTheEightByteMap()
{
    static_assert(max31865_cmd::REG_CONFIG == 0x00U, "CONFIG address");
    static_assert(max31865_cmd::REG_RTD_MSB == 0x01U, "RTD MSB address");
    static_assert(max31865_cmd::REG_RTD_LSB == 0x02U, "RTD LSB address");
    static_assert(max31865_cmd::REG_HIGH_FAULT_MSB == 0x03U,
                  "high threshold MSB");
    static_assert(max31865_cmd::REG_HIGH_FAULT_LSB == 0x04U,
                  "high threshold LSB");
    static_assert(max31865_cmd::REG_LOW_FAULT_MSB == 0x05U,
                  "low threshold MSB");
    static_assert(max31865_cmd::REG_LOW_FAULT_LSB == 0x06U,
                  "low threshold LSB");
    static_assert(max31865_cmd::REG_FAULT_STATUS == 0x07U,
                  "fault status address");
    static_assert(max31865_cmd::NUM_REGISTERS == 8U, "register count");
    static_assert(max31865_cmd::MAX_FRAME_BYTES == 9U, "frame size");
    static_assert(max31865_cmd::READ_MASK == 0x7FU, "read mask");
    static_assert(max31865_cmd::WRITE_BIT == 0x80U, "write bit");
    static_assert(max31865_cmd::SPI_MAX_HZ == 5000000UL, "SPI maximum");

    CHECK_EQ(0x80U, max31865_cmd::CONFIG_BIAS);
    CHECK_EQ(0x40U, max31865_cmd::CONFIG_AUTO);
    CHECK_EQ(0x20U, max31865_cmd::CONFIG_ONE_SHOT);
    CHECK_EQ(0x10U, max31865_cmd::CONFIG_3WIRE);
    CHECK_EQ(0x0CU, max31865_cmd::CONFIG_FAULT_CYCLE_MASK);
    CHECK_EQ(0x02U, max31865_cmd::CONFIG_FAULT_CLEAR);
    CHECK_EQ(0x01U, max31865_cmd::CONFIG_FILTER_50HZ);
    CHECK_EQ(0xD1U, max31865_cmd::CONFIG_PERSISTENT_MASK);
    CHECK_EQ(0x2EU, max31865_cmd::CONFIG_COMMAND_MASK);
    CHECK_EQ(0xFCU, max31865_cmd::FAULT_DEFINED_MASK);
    CHECK_EQ(0x7FFFU, max31865_cmd::ADC_CODE_MAX);
    CHECK_EQ(32768UL, max31865_cmd::ADC_FULL_SCALE);
    return true;
}

bool registerMetadataIsComplete()
{
    const char *const names[max31865_cmd::NUM_REGISTERS] = {
        "CONFIG",
        "RTD_MSB",
        "RTD_LSB",
        "HIGH_FAULT_MSB",
        "HIGH_FAULT_LSB",
        "LOW_FAULT_MSB",
        "LOW_FAULT_LSB",
        "FAULT_STATUS"};
    const MAX31865RegisterAccess access[max31865_cmd::NUM_REGISTERS] = {
        MAX31865RegisterAccess::ReadWrite,
        MAX31865RegisterAccess::ReadOnly,
        MAX31865RegisterAccess::ReadOnly,
        MAX31865RegisterAccess::ReadWrite,
        MAX31865RegisterAccess::ReadWrite,
        MAX31865RegisterAccess::ReadWrite,
        MAX31865RegisterAccess::ReadWrite,
        MAX31865RegisterAccess::ReadOnly};
    const uint8_t masks[max31865_cmd::NUM_REGISTERS] = {
        max31865_cmd::CONFIG_PERSISTENT_MASK,
        0xFFU,
        0xFFU,
        0xFFU,
        max31865_cmd::THRESHOLD_LSB_DEFINED_MASK,
        0xFFU,
        max31865_cmd::THRESHOLD_LSB_DEFINED_MASK,
        max31865_cmd::FAULT_DEFINED_MASK};

    for (uint8_t address = 0U; address <= max31865_cmd::REG_LAST; ++address) {
        CHECK(max31865RegisterAddressValid(address));
        CHECK_CSTR_EQ(names[address], max31865RegisterName(address));
        CHECK_EQ(access[address], max31865RegisterAccess(address));
        CHECK_EQ(masks[address], max31865RegisterVerifyMask(address));
        CHECK_EQ(address == max31865_cmd::REG_RTD_MSB ||
                     address == max31865_cmd::REG_RTD_LSB,
                 max31865RegisterReadAcknowledgesDrdy(address));
    }
    CHECK(!max31865RegisterAddressValid(8U));
    CHECK(!max31865RegisterAddressValid(0x7FU));
    CHECK(!max31865RegisterAddressValid(0xFFU));
    CHECK_CSTR_EQ("INVALID", max31865RegisterName(8U));
    CHECK_EQ(0U, max31865RegisterVerifyMask(8U));
    CHECK(!max31865RegisterReadAcknowledgesDrdy(8U));
    return true;
}

bool readFramesCoverAllValidSpans()
{
    for (uint8_t start = 0U; start <= max31865_cmd::REG_LAST; ++start) {
        const size_t maximum =
            max31865_cmd::NUM_REGISTERS - static_cast<size_t>(start);
        for (size_t length = 1U; length <= maximum; ++length) {
            uint8_t frame[max31865_cmd::MAX_FRAME_BYTES];
            memset(frame, 0xA5, sizeof(frame));
            size_t frameLength = 99U;
            const MAX31865Status status = max31865EncodeReadFrame(
                start,
                length,
                frame,
                sizeof(frame),
                frameLength);
            CHECK(status.ok());
            CHECK_EQ(length + 1U, frameLength);
            CHECK_EQ(static_cast<uint8_t>(start & max31865_cmd::READ_MASK),
                     frame[0]);
            for (size_t index = 1U; index < frameLength; ++index) {
                CHECK_EQ(0U, frame[index]);
            }
            for (size_t index = frameLength; index < sizeof(frame); ++index) {
                CHECK_EQ(0xA5U, frame[index]);
            }
        }
    }
    return true;
}

bool readFrameFailuresAreTransactional()
{
    uint8_t frame[max31865_cmd::MAX_FRAME_BYTES];
    memset(frame, 0x5A, sizeof(frame));
    size_t frameLength = 77U;
    CHECK_EQ(MAX31865Error::RegisterAddressInvalid,
             max31865EncodeReadFrame(
                 8U,
                 1U,
                 frame,
                 sizeof(frame),
                 frameLength)
                 .code);
    CHECK_EQ(77U, frameLength);
    for (uint8_t value : frame) {
        CHECK_EQ(0x5AU, value);
    }
    CHECK_EQ(MAX31865Error::InvalidArgument,
             max31865EncodeReadFrame(
                 0U,
                 0U,
                 frame,
                 sizeof(frame),
                 frameLength)
                 .code);
    CHECK_EQ(MAX31865Error::RegisterAddressInvalid,
             max31865EncodeReadFrame(
                 7U,
                 2U,
                 frame,
                 sizeof(frame),
                 frameLength)
                 .code);
    CHECK_EQ(MAX31865Error::InvalidArgument,
             max31865EncodeReadFrame(
                 0U,
                 8U,
                 frame,
                 8U,
                 frameLength)
                 .code);
    CHECK_EQ(MAX31865Error::InvalidArgument,
             max31865EncodeReadFrame(
                 0U,
                 1U,
                 nullptr,
                 sizeof(frame),
                 frameLength)
                 .code);
    CHECK_EQ(77U, frameLength);
    for (uint8_t value : frame) {
        CHECK_EQ(0x5AU, value);
    }
    return true;
}

bool writeFramesRejectReadOnlySpansAndSupportOverlap()
{
    const uint8_t config = static_cast<uint8_t>(
        max31865_cmd::CONFIG_BIAS |
        max31865_cmd::CONFIG_3WIRE |
        max31865_cmd::CONFIG_FILTER_50HZ);
    uint8_t frame[max31865_cmd::MAX_FRAME_BYTES] = {};
    size_t frameLength = 0U;
    CHECK(max31865EncodeWriteFrame(
              max31865_cmd::REG_CONFIG,
              &config,
              1U,
              frame,
              sizeof(frame),
              frameLength)
              .ok());
    CHECK_EQ(2U, frameLength);
    CHECK_EQ(static_cast<uint8_t>(
                 max31865_cmd::WRITE_BIT | max31865_cmd::REG_CONFIG),
             frame[0]);
    CHECK_EQ(config, frame[1]);

    const uint8_t thresholds[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    CHECK(max31865EncodeWriteFrame(
              max31865_cmd::REG_HIGH_FAULT_MSB,
              thresholds,
              sizeof(thresholds),
              frame,
              sizeof(frame),
              frameLength)
              .ok());
    CHECK_EQ(5U, frameLength);
    CHECK_EQ(0x83U, frame[0]);
    CHECK(memcmp(&frame[1], thresholds, sizeof(thresholds)) == 0);

    uint8_t overlap[6] = {0x12U, 0x34U, 0x56U, 0x78U, 0xEEU, 0xEEU};
    CHECK(max31865EncodeWriteFrame(
              max31865_cmd::REG_HIGH_FAULT_MSB,
              overlap,
              4U,
              overlap,
              sizeof(overlap),
              frameLength)
              .ok());
    CHECK_EQ(5U, frameLength);
    CHECK_EQ(0x83U, overlap[0]);
    CHECK(memcmp(&overlap[1], thresholds, sizeof(thresholds)) == 0);

    memset(frame, 0xA5, sizeof(frame));
    frameLength = 42U;
    CHECK_EQ(MAX31865Error::UnsupportedCommand,
             max31865EncodeWriteFrame(
                 max31865_cmd::REG_RTD_MSB,
                 thresholds,
                 1U,
                 frame,
                 sizeof(frame),
                 frameLength)
                 .code);
    CHECK_EQ(MAX31865Error::UnsupportedCommand,
             max31865EncodeWriteFrame(
                 max31865_cmd::REG_CONFIG,
                 thresholds,
                 4U,
                 frame,
                 sizeof(frame),
                 frameLength)
                 .code);
    CHECK_EQ(42U, frameLength);
    for (uint8_t value : frame) {
        CHECK_EQ(0xA5U, value);
    }
    CHECK_EQ(MAX31865Error::InvalidArgument,
             max31865EncodeWriteFrame(
                 max31865_cmd::REG_CONFIG,
                 nullptr,
                 1U,
                 frame,
                 sizeof(frame),
                 frameLength)
                 .code);
    return true;
}

bool rtdDecodeCoversFaultBitAndPreservesOutputsOnError()
{
    struct Vector {
        uint8_t msb;
        uint8_t lsb;
        uint16_t raw;
        uint16_t code;
        bool fault;
    };
    const Vector vectors[] = {
        {0x00U, 0x00U, 0x0000U, 0U, false},
        {0x00U, 0x01U, 0x0001U, 0U, true},
        {0x40U, 0x00U, 0x4000U, 8192U, false},
        {0xFFU, 0xFEU, 0xFFFEU, 32767U, false},
        {0xFFU, 0xFFU, 0xFFFFU, 32767U, true}};
    for (const Vector &vector : vectors) {
        const uint8_t bytes[2] = {vector.msb, vector.lsb};
        uint16_t raw = 0x1234U;
        uint16_t code = 0x5678U;
        bool fault = !vector.fault;
        CHECK(max31865DecodeRtd(bytes, sizeof(bytes), raw, code, fault).ok());
        CHECK_EQ(vector.raw, raw);
        CHECK_EQ(vector.code, code);
        CHECK_EQ(vector.fault, fault);
    }

    const uint8_t bytes[2] = {0x12U, 0x34U};
    uint16_t raw = 0xAAAAU;
    uint16_t code = 0xBBBBU;
    bool fault = true;
    CHECK_EQ(MAX31865Error::InvalidArgument,
             max31865DecodeRtd(bytes, 1U, raw, code, fault).code);
    CHECK_EQ(0xAAAAU, raw);
    CHECK_EQ(0xBBBBU, code);
    CHECK(fault);
    CHECK_EQ(MAX31865Error::InvalidArgument,
             max31865DecodeRtd(nullptr, 2U, raw, code, fault).code);
    CHECK_EQ(0xAAAAU, raw);
    CHECK_EQ(0xBBBBU, code);
    CHECK(fault);
    return true;
}

bool thresholdCodecRoundTripsTheEntireDomainSampled()
{
    for (uint32_t candidate = 0U;
         candidate <= max31865_cmd::ADC_CODE_MAX;
         candidate += 257U) {
        const uint16_t code = static_cast<uint16_t>(candidate);
        uint8_t bytes[2] = {0xA5U, 0xA5U};
        CHECK(max31865EncodeThreshold(code, bytes).ok());
        CHECK_EQ(0U, static_cast<uint8_t>(bytes[1] & 0x01U));
        uint16_t decoded = 0xFFFFU;
        CHECK(max31865DecodeThreshold(bytes, decoded).ok());
        CHECK_EQ(code, decoded);
    }
    uint8_t bytes[2] = {};
    CHECK(max31865EncodeThreshold(max31865_cmd::ADC_CODE_MAX, bytes).ok());
    CHECK_EQ(0xFFU, bytes[0]);
    CHECK_EQ(0xFEU, bytes[1]);

    bytes[0] = 0x12U;
    bytes[1] = 0x35U;
    uint16_t decoded = 0U;
    CHECK(max31865DecodeThreshold(bytes, decoded).ok());
    CHECK_EQ(0x091AU, decoded);

    bytes[0] = 0xA5U;
    bytes[1] = 0x5AU;
    CHECK_EQ(MAX31865Error::InvalidArgument,
             max31865EncodeThreshold(0x8000U, bytes).code);
    CHECK_EQ(0xA5U, bytes[0]);
    CHECK_EQ(0x5AU, bytes[1]);
    decoded = 0xBEEFU;
    CHECK_EQ(MAX31865Error::InvalidArgument,
             max31865DecodeThreshold(nullptr, decoded).code);
    CHECK_EQ(0xBEEFU, decoded);
    return true;
}

bool codecFramesInteroperateWithIndependentModel()
{
    Max31865DeviceModel device;
    const uint16_t high = 25000U;
    const uint16_t low = 1000U;
    uint8_t thresholds[4] = {};
    CHECK(max31865EncodeThreshold(high, &thresholds[0]).ok());
    CHECK(max31865EncodeThreshold(low, &thresholds[2]).ok());

    uint8_t tx[max31865_cmd::MAX_FRAME_BYTES] = {};
    uint8_t rx[max31865_cmd::MAX_FRAME_BYTES] = {};
    size_t frameLength = 0U;
    CHECK(max31865EncodeWriteFrame(
              max31865_cmd::REG_HIGH_FAULT_MSB,
              thresholds,
              sizeof(thresholds),
              tx,
              sizeof(tx),
              frameLength)
              .ok());
    CHECK(device.setChipSelect(true).ok());
    CHECK(device.transfer(tx, rx, frameLength).ok());
    CHECK(device.setChipSelect(false).ok());

    memset(tx, 0xA5, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    CHECK(max31865EncodeReadFrame(
              max31865_cmd::REG_HIGH_FAULT_MSB,
              4U,
              tx,
              sizeof(tx),
              frameLength)
              .ok());
    CHECK(device.setChipSelect(true).ok());
    CHECK(device.transfer(tx, rx, frameLength).ok());
    CHECK(device.setChipSelect(false).ok());
    CHECK(memcmp(&rx[1], thresholds, sizeof(thresholds)) == 0);
    uint16_t decodedHigh = 0U;
    uint16_t decodedLow = 0U;
    CHECK(max31865DecodeThreshold(&rx[1], decodedHigh).ok());
    CHECK(max31865DecodeThreshold(&rx[3], decodedLow).ok());
    CHECK_EQ(high, decodedHigh);
    CHECK_EQ(low, decodedLow);
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"register constants", registerConstantsMatchTheEightByteMap},
        {"register metadata", registerMetadataIsComplete},
        {"all valid read frames", readFramesCoverAllValidSpans},
        {"transactional read errors", readFrameFailuresAreTransactional},
        {"write access and overlap", writeFramesRejectReadOnlySpansAndSupportOverlap},
        {"RTD decode", rtdDecodeCoversFaultBitAndPreservesOutputsOnError},
        {"threshold codec", thresholdCodecRoundTripsTheEntireDomainSampled},
        {"codec/model interoperability", codecFramesInteroperateWithIndependentModel}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
