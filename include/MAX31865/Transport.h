/**
 * @file Transport.h
 * @brief Framework-neutral SPI, GPIO, timing, and lock callbacks.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MAX31865/Status.h"

/**
 * @brief Full MAX31865 register-frame transfer callback.
 *
 * The driver passes the complete SPI frame, including the register command
 * byte. The callback owns chip-select framing, transfer timeout enforcement,
 * and framework error mapping.
 */
typedef MAX31865Status (*MAX31865TransferFn)(const uint8_t* tx,
                                             uint8_t* rx,
                                             size_t len,
                                             uint32_t timeoutMs,
                                             void* user);

/// Optional DRDY reader. Return true when the MAX31865 /DRDY signal is ready.
typedef bool (*MAX31865ReadDrdyFn)(void* user);

/// Monotonic millisecond source used for deadlines and sample timestamps.
typedef uint32_t (*MAX31865NowMsFn)(void* user);

/// Bounded millisecond delay hook.
typedef void (*MAX31865DelayMsFn)(uint32_t ms, void* user);

/// Bounded microsecond delay hook.
typedef void (*MAX31865DelayUsFn)(uint32_t us, void* user);

/// Cooperative scheduler yield hook.
typedef void (*MAX31865YieldFn)(void* user);

/// Optional bounded backend lock hook.
typedef MAX31865Status (*MAX31865LockFn)(uint32_t timeoutMs, void* user);

/// Optional backend unlock hook paired with MAX31865LockFn.
typedef void (*MAX31865UnlockFn)(void* user);

/**
 * @brief Application-owned backend hooks for framework-neutral operation.
 *
 * transfer, nowMs, delayMs, and delayUs are required whenever this transport is
 * used with MAX31865BeginConfig. readDrdy, cooperativeYield, lock, and unlock
 * are optional. lock and unlock must be supplied as a pair.
 */
typedef struct MAX31865TransportConfig {
    MAX31865TransferFn transfer; ///< Full register-frame transfer callback.
    MAX31865ReadDrdyFn readDrdy; ///< Optional DRDY reader; true when data is ready.
    MAX31865NowMsFn nowMs;       ///< Required monotonic millisecond source.
    MAX31865DelayMsFn delayMs;   ///< Required scheduler-friendly millisecond delay.
    MAX31865DelayUsFn delayUs;   ///< Required protocol microsecond delay.
    MAX31865YieldFn cooperativeYield; ///< Optional cooperative yield hook.
    MAX31865LockFn lock;         ///< Optional bounded backend lock callback.
    MAX31865UnlockFn unlock;     ///< Optional backend unlock callback.
    void* user;                  ///< User context passed to callbacks.
    uint32_t timeoutMs;          ///< Transfer timeout in milliseconds; zero selects default.
    uint32_t lockTimeoutMs;      ///< Lock timeout in milliseconds; zero selects default.
} MAX31865TransportConfig;
