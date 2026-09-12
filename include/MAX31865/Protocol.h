/**
 * @file Protocol.h
 * @brief Framework-neutral MAX31865 register-frame codec helpers.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MAX31865/CommandTable.h"
#include "MAX31865/Status.h"

/** @brief Static register access classification. */
enum class MAX31865RegisterAccess : uint8_t
{
    ReadOnly = 0,
    ReadWrite
};

/** @return True exactly for a documented address in 0..7. */
bool max31865RegisterAddressValid(uint8_t address);
/** @return Read-only/read-write classification for a valid address. */
MAX31865RegisterAccess max31865RegisterAccess(uint8_t address);
/** @return Static register name, or "INVALID" outside 0..7. */
const char *max31865RegisterName(uint8_t address);
/** @return Defined-bit mask used for register verification. */
uint8_t max31865RegisterVerifyMask(uint8_t address);
/** @return True when reading this address acknowledges active-low DRDY. */
bool max31865RegisterReadAcknowledgesDrdy(uint8_t address);

/**
 * @brief Encode one address-plus-dummy-byte contiguous register read.
 * @param startAddress First address in 0..7.
 * @param length Register count in 1..8-startAddress.
 * @param[out] tx Destination with capacity of at least length+1.
 * @param capacity Destination capacity.
 * @param[out] frameLength Committed only on success.
 */
MAX31865Status max31865EncodeReadFrame(
    uint8_t startAddress,
    size_t length,
    uint8_t *tx,
    size_t capacity,
    size_t &frameLength);

/**
 * @brief Encode one address-plus-payload contiguous register write.
 * @note Every address in the span must be writable.
 */
MAX31865Status max31865EncodeWriteFrame(
    uint8_t startAddress,
    const uint8_t *values,
    size_t length,
    uint8_t *tx,
    size_t capacity,
    size_t &frameLength);

/** @brief Decode the two RTD data bytes transactionally. */
MAX31865Status max31865DecodeRtd(
    const uint8_t *bytes,
    size_t length,
    uint16_t &rawRegister,
    uint16_t &rawCode,
    bool &fault);

/** @brief Encode one 15-bit threshold into its two register bytes. */
MAX31865Status max31865EncodeThreshold(uint16_t code, uint8_t out[2]);
/** @brief Decode one threshold register pair into a 15-bit code. */
MAX31865Status max31865DecodeThreshold(
    const uint8_t bytes[2],
    uint16_t &code);
