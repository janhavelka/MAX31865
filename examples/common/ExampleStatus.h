#pragma once

#include <Arduino.h>

#include <cstdio>

#include "MAX31865/MAX31865.h"

namespace example_status
{
inline bool report(const char *operation, const MAX31865Status &status)
{
    char line[192];
    std::snprintf(
        line,
        sizeof(line),
        "%s: %s detail=%ld msg=%s\n",
        operation,
        max31865ErrorName(status.code),
        static_cast<long>(status.detail),
        status.msg == nullptr ? "" : status.msg);
    Serial.print(line);
    return status.ok();
}

inline void printSample(const MAX31865Sample &sample)
{
    char line[192];
    std::snprintf(
        line,
        sizeof(line),
        "sample=%lu raw=0x%04X code=%u resistance=%.5f ohm temperature=%.4f C flags=0x%02X\n",
        static_cast<unsigned long>(sample.sampleCounter),
        static_cast<unsigned>(sample.rawRegister),
        static_cast<unsigned>(sample.rawCode),
        static_cast<double>(sample.resistanceOhms),
        static_cast<double>(sample.temperatureC),
        static_cast<unsigned>(sample.flags));
    Serial.print(line);
}

inline void printFault(const MAX31865FaultStatus &fault)
{
    char line[224];
    std::snprintf(
        line,
        sizeof(line),
        "fault=0x%02X high=%u low=%u refinHigh=%u refinLow=%u rtdinLow=%u voltage=%u\n",
        static_cast<unsigned>(fault.raw),
        fault.highThreshold ? 1U : 0U,
        fault.lowThreshold ? 1U : 0U,
        fault.refinHigh ? 1U : 0U,
        fault.refinLow ? 1U : 0U,
        fault.rtdinLow ? 1U : 0U,
        fault.overUnderVoltage ? 1U : 0U);
    Serial.print(line);
}

inline void printHealth(const MAX31865Health &health)
{
    char line[256];
    std::snprintf(
        line,
        sizeof(line),
        "health lifecycle=%s driver=%s online=%u known=%u consecutive=%u ok=%lu fail=%lu samples=%lu/%lu dropped=%lu overruns=%lu faults=%lu\n",
        max31865StateName(health.state),
        max31865DriverStateName(health.driverState),
        health.online ? 1U : 0U,
        health.configurationKnown ? 1U : 0U,
        static_cast<unsigned>(health.consecutiveFailures),
        static_cast<unsigned long>(health.trackedSuccessCount),
        static_cast<unsigned long>(health.trackedFailureCount),
        static_cast<unsigned long>(health.sampleFrameSuccessCount),
        static_cast<unsigned long>(health.sampleFrameAttemptCount),
        static_cast<unsigned long>(health.droppedSampleCount),
        static_cast<unsigned long>(health.overrunCount),
        static_cast<unsigned long>(health.faultObservationCount));
    Serial.print(line);
}
} // namespace example_status
