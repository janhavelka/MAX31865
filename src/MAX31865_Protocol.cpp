/**
 * @file MAX31865_Protocol.cpp
 * @brief Canonical framework-neutral MAX31865 register-frame codec.
 */

#include "MAX31865/Protocol.h"

namespace
{
MAX31865Status invalidArgument(const char *message, int32_t detail = 0)
{
    return MAX31865Status::Error(
        MAX31865Error::InvalidArgument,
        message,
        detail);
}

MAX31865Status invalidAddress(const char *message, uint8_t address)
{
    return MAX31865Status::Error(
        MAX31865Error::RegisterAddressInvalid,
        message,
        address);
}

MAX31865Status validateSpan(uint8_t startAddress, size_t length)
{
    if (!max31865RegisterAddressValid(startAddress))
    {
        return invalidAddress(
            "invalid MAX31865 register start address",
            startAddress);
    }
    if (length == 0U)
    {
        return invalidArgument("zero-length MAX31865 register span");
    }
    if (length > (max31865_cmd::NUM_REGISTERS - startAddress))
    {
        return invalidAddress(
            "MAX31865 register span exceeds the register map",
            startAddress);
    }
    return MAX31865Status::Ok();
}
} // namespace

bool max31865RegisterAddressValid(uint8_t address)
{
    return address <= max31865_cmd::REG_LAST;
}

MAX31865RegisterAccess max31865RegisterAccess(uint8_t address)
{
    switch (address)
    {
        case max31865_cmd::REG_CONFIG:
        case max31865_cmd::REG_HIGH_FAULT_MSB:
        case max31865_cmd::REG_HIGH_FAULT_LSB:
        case max31865_cmd::REG_LOW_FAULT_MSB:
        case max31865_cmd::REG_LOW_FAULT_LSB:
            return MAX31865RegisterAccess::ReadWrite;
        default:
            // Invalid addresses are classified conservatively as read-only.
            return MAX31865RegisterAccess::ReadOnly;
    }
}

const char *max31865RegisterName(uint8_t address)
{
    switch (address)
    {
        case max31865_cmd::REG_CONFIG: return "CONFIG";
        case max31865_cmd::REG_RTD_MSB: return "RTD_MSB";
        case max31865_cmd::REG_RTD_LSB: return "RTD_LSB";
        case max31865_cmd::REG_HIGH_FAULT_MSB: return "HIGH_FAULT_MSB";
        case max31865_cmd::REG_HIGH_FAULT_LSB: return "HIGH_FAULT_LSB";
        case max31865_cmd::REG_LOW_FAULT_MSB: return "LOW_FAULT_MSB";
        case max31865_cmd::REG_LOW_FAULT_LSB: return "LOW_FAULT_LSB";
        case max31865_cmd::REG_FAULT_STATUS: return "FAULT_STATUS";
        default: return "INVALID";
    }
}

uint8_t max31865RegisterVerifyMask(uint8_t address)
{
    switch (address)
    {
        case max31865_cmd::REG_CONFIG:
            return max31865_cmd::CONFIG_PERSISTENT_MASK;
        case max31865_cmd::REG_RTD_MSB:
        case max31865_cmd::REG_RTD_LSB:
        case max31865_cmd::REG_HIGH_FAULT_MSB:
        case max31865_cmd::REG_LOW_FAULT_MSB:
            return 0xFFU;
        case max31865_cmd::REG_HIGH_FAULT_LSB:
        case max31865_cmd::REG_LOW_FAULT_LSB:
            return max31865_cmd::THRESHOLD_LSB_DEFINED_MASK;
        case max31865_cmd::REG_FAULT_STATUS:
            return max31865_cmd::FAULT_DEFINED_MASK;
        default:
            return 0U;
    }
}

bool max31865RegisterReadAcknowledgesDrdy(uint8_t address)
{
    return (address == max31865_cmd::REG_RTD_MSB) ||
           (address == max31865_cmd::REG_RTD_LSB);
}

MAX31865Status max31865EncodeReadFrame(
    uint8_t startAddress,
    size_t length,
    uint8_t *tx,
    size_t capacity,
    size_t &frameLength)
{
    const MAX31865Status spanStatus = validateSpan(startAddress, length);
    if (!spanStatus.ok())
    {
        return spanStatus;
    }
    if (tx == nullptr)
    {
        return invalidArgument("null MAX31865 read-frame destination");
    }
    const size_t required = length + 1U;
    if (capacity < required)
    {
        return invalidArgument(
            "MAX31865 read-frame destination is too small",
            static_cast<int32_t>(capacity));
    }

    uint8_t frame[max31865_cmd::MAX_FRAME_BYTES] = {};
    frame[0] = static_cast<uint8_t>(startAddress & max31865_cmd::READ_MASK);
    for (size_t index = 1U; index < required; ++index)
    {
        frame[index] = 0U;
    }
    for (size_t index = 0U; index < required; ++index)
    {
        tx[index] = frame[index];
    }
    frameLength = required;
    return MAX31865Status::Ok();
}

MAX31865Status max31865EncodeWriteFrame(
    uint8_t startAddress,
    const uint8_t *values,
    size_t length,
    uint8_t *tx,
    size_t capacity,
    size_t &frameLength)
{
    const MAX31865Status spanStatus = validateSpan(startAddress, length);
    if (!spanStatus.ok())
    {
        return spanStatus;
    }
    if ((values == nullptr) || (tx == nullptr))
    {
        return invalidArgument("null MAX31865 write-frame buffer");
    }
    const size_t required = length + 1U;
    if (capacity < required)
    {
        return invalidArgument(
            "MAX31865 write-frame destination is too small",
            static_cast<int32_t>(capacity));
    }

    for (size_t index = 0U; index < length; ++index)
    {
        const uint8_t address = static_cast<uint8_t>(startAddress + index);
        if (max31865RegisterAccess(address) != MAX31865RegisterAccess::ReadWrite)
        {
            return MAX31865Status::Error(
                MAX31865Error::UnsupportedCommand,
                "MAX31865 register span includes a read-only register",
                address);
        }
    }

    // Stage before committing so overlapping values/tx ranges are safe.
    uint8_t frame[max31865_cmd::MAX_FRAME_BYTES] = {};
    frame[0] = static_cast<uint8_t>(startAddress | max31865_cmd::WRITE_BIT);
    for (size_t index = 0U; index < length; ++index)
    {
        frame[index + 1U] = values[index];
    }
    for (size_t index = 0U; index < required; ++index)
    {
        tx[index] = frame[index];
    }
    frameLength = required;
    return MAX31865Status::Ok();
}

MAX31865Status max31865DecodeRtd(
    const uint8_t *bytes,
    size_t length,
    uint16_t &rawRegister,
    uint16_t &rawCode,
    bool &fault)
{
    if ((bytes == nullptr) || (length != 2U))
    {
        return invalidArgument(
            "MAX31865 RTD response must contain exactly two bytes",
            static_cast<int32_t>(length));
    }

    const uint16_t packed = static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[0]) << 8U) |
        static_cast<uint16_t>(bytes[1]));
    const uint16_t code = static_cast<uint16_t>(packed >> 1U);
    const bool hasFault = (packed & max31865_cmd::RTD_FAULT_BIT) != 0U;
    rawRegister = packed;
    rawCode = code;
    fault = hasFault;
    return MAX31865Status::Ok();
}

MAX31865Status max31865EncodeThreshold(uint16_t code, uint8_t out[2])
{
    if (out == nullptr)
    {
        return invalidArgument("null MAX31865 threshold destination");
    }
    if (code > max31865_cmd::ADC_CODE_MAX)
    {
        return invalidArgument(
            "MAX31865 threshold code exceeds 15 bits",
            code);
    }

    const uint16_t packed = static_cast<uint16_t>(code << 1U);
    const uint8_t msb = static_cast<uint8_t>(packed >> 8U);
    const uint8_t lsb = static_cast<uint8_t>(packed & 0xFFU);
    out[0] = msb;
    out[1] = lsb;
    return MAX31865Status::Ok();
}

MAX31865Status max31865DecodeThreshold(
    const uint8_t bytes[2],
    uint16_t &code)
{
    if (bytes == nullptr)
    {
        return invalidArgument("null MAX31865 threshold source");
    }

    const uint16_t packed = static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[0]) << 8U) |
        static_cast<uint16_t>(bytes[1]));
    const uint16_t decoded = static_cast<uint16_t>(packed >> 1U);
    code = decoded;
    return MAX31865Status::Ok();
}
