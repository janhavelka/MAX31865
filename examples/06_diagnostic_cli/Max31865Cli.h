#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MAX31865/MAX31865.h"

namespace max31865_cli
{
/** Example-owned output/time hooks; not part of the library API. */
struct Platform
{
    void *user;
    void (*write)(void *user, const char *data, size_t length);
    uint32_t (*nowMs)(void *user);
    uint32_t (*freeHeapBytes)(void *user); ///< Optional diagnostic telemetry.
};

/** Fixed-size counters around the real production transport. */
struct TransportCounters
{
    uint32_t lockCalls;
    uint32_t unlockCalls;
    uint32_t chipSelectAssertCalls;
    uint32_t chipSelectDeassertCalls;
    uint32_t transferCalls;
    uint32_t transferBytes;
    uint32_t transferFailures;
    uint32_t drdyReadCalls;
    uint32_t callbackFailures;
};

/** Borrowed example-only transport observer. */
struct TransportObserver
{
    MAX31865Transport downstream;
    TransportCounters counters;
};

MAX31865Transport observeTransport(
    TransportObserver &observer,
    const MAX31865Transport &downstream);
void clearTransportCounters(TransportObserver &observer);

struct Context
{
    MAX31865 *device;
    MAX31865BeginConfig *beginConfig;
    const MAX31865Pins *pins;
    TransportObserver *observer;
    uint32_t spiClockHz; ///< Selected application SPI clock metadata.
    Platform platform;
    bool transportReady;
};

/** Bind one fixed CLI context; performs no device I/O. */
void bind(const Context &context);
/** Attempt initialization when transport is ready, then print help. */
void start();
/** Strictly parse and execute exactly one bounded command line. */
void processLine(const char *line);
/** Emit one correlated parser error after the serial shell discards a line. */
void processInputOverflow();
/** Advance at most one cooperative sample/stress job step. */
void service();
/** Print the prompt unless machine-record output is selected. */
void printPrompt();
} // namespace max31865_cli
