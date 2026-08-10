/**
 * @file Transport.h
 * @brief Framework-neutral borrowed MAX31865 transport contract.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MAX31865/Status.h"

/** @brief Logical optional input role supported by a transport. */
enum class MAX31865Pin : uint8_t
{
    DRDY = 0 ///< Active-low conversion-ready input.
};

/** @brief Optional-pin capabilities of one callback table. */
struct MAX31865TransportCapabilities
{
    bool hasDrdy; ///< readPin supports MAX31865Pin::DRDY.
};

/**
 * @brief Application-owned synchronous SPI/GPIO/timing callbacks.
 *
 * The driver copies this table but borrows `user` and every resource reachable
 * through it until end() or destruction. Destruction and end() invoke no
 * callback. The binding identity and capability claims remain stable for that
 * lifetime: applications must not rebind or reconfigure callback-owned buses,
 * pins, or contexts behind a live driver. Ordinary runtime state changes made
 * by the callbacks themselves are expected. Callbacks make exactly one
 * attempt, do not retry or recover, do not re-enter the same driver, and remain
 * externally serialized per instance.
 *
 * A successful transfer performs exactly `length` contiguous full-duplex bytes
 * and initializes the complete RX buffer. It neither controls CS nor locks the
 * bus. The application configures that transfer MSB-first, with CPHA=1 (SPI
 * mode 1 or mode 3), at no more than `max31865_cmd::SPI_MAX_HZ`; the neutral
 * core cannot inspect those platform settings. `setChipSelect` is required
 * because MAX31865 commands and one-shot/fault cycles are committed by the CS
 * rising edge. `lockBus`/`unlockBus` are either both null for a dedicated or
 * single-owner bus, or both non-null and shared by every client of that SPI
 * host.
 *
 * `nowMs`, `sleepMs`, and `delayUs` are required. `nowUs` is optional and only
 * controls sample timestamp validity. Millisecond deadlines use modulo-2^32
 * subtraction. Arduino/ESP-IDF/native errors are mapped into MAX31865Status by
 * the callbacks; no framework status escapes through the core API. The driver
 * preserves each role's documented status codes and action-free
 * `InvalidState`. Any other callback error is normalized to the role-specific
 * bus-lock, chip-select, transfer, or GPIO failure; its original
 * MAX31865Error value is retained in `detail`. Every non-null callback status
 * message follows MAX31865Status's static-lifetime contract; a null message
 * receives a role-specific static fallback before it reaches the public
 * status API. One framing-safety exception
 * applies: after CS may already be asserted, an `InvalidState` rejection of
 * the required deassertion remains action-free but is reported and counted as
 * `ChipSelectFailed`, because the driver still cannot prove CS returned high.
 */
typedef struct MAX31865Transport
{
    void *user; ///< Borrowed callback context; may be null if callbacks allow it.
    MAX31865TransportCapabilities capabilities; ///< Explicit optional support.

    /**
     * @brief Make one finite shared-bus acquisition attempt.
     * @param user Borrowed callback context.
     * @param timeoutMs Remaining whole-operation budget; zero means no waiting.
     * @return Ok, BusLockTimeout, BusLockFailed, or InvalidState. InvalidState
     * is a backend precondition rejection and performs no bus/device action.
     */
    MAX31865Status (*lockBus)(void *user, uint32_t timeoutMs);
    /** @brief Release exactly one successfully acquired bus lock. */
    void (*unlockBus)(void *user);

    /**
     * @brief Apply active-low chip select.
     * @return Ok, ChipSelectFailed, or InvalidState. InvalidState is a backend
     * precondition rejection and performs no GPIO/device action. If CS may
     * already be asserted, the driver contextually promotes a rejected
     * required deassertion to ChipSelectFailed without implying that this
     * callback invocation changed the pin.
     */
    MAX31865Status (*setChipSelect)(void *user, bool asserted);
    /**
     * @brief Perform one exact fixed-length full-duplex SPI transfer.
     * @param timeoutMs Remaining whole-operation budget; zero means one
     * immediate attempt with no callback-owned waiting.
     * @return Ok, SpiTransferFailed, or InvalidState. InvalidState is a backend
     * precondition rejection and transfers no byte.
     */
    MAX31865Status (*transfer)(
        void *user,
        const uint8_t *tx,
        uint8_t *rx,
        size_t length,
        uint32_t timeoutMs);

    /**
     * @brief Read the optional DRDY electrical level once without waiting.
     * @param[out] level Committed by the callback only on Ok; false means ready.
     * @return Ok, GpioFailed, or InvalidState. InvalidState is a backend
     * precondition rejection and preserves `level`.
     */
    MAX31865Status (*readPin)(
        void *user,
        MAX31865Pin pin,
        bool *level);

    /** @return Required modulo-2^32 deadline clock in milliseconds. */
    uint32_t (*nowMs)(void *user);
    /** @return Optional modulo-2^32 sample clock in microseconds. */
    uint32_t (*nowUs)(void *user);
    /** @brief Perform a required yielding sleep. */
    void (*sleepMs)(void *user, uint32_t ms);
    /** @brief Perform a required short protocol delay. */
    void (*delayUs)(void *user, uint32_t us);
} MAX31865Transport;
