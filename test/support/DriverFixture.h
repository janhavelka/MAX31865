#pragma once

#include "MAX31865/MAX31865.h"
#include "support/ScriptedMax31865Transport.h"

namespace max31865_test {

struct DriverFixture {
    ScriptedMax31865Transport scripted;
    MAX31865 driver;
    MAX31865BeginConfig config;

    explicit DriverFixture(
        bool hasDrdy = true,
        bool includeBusLock = true,
        bool includeNowUs = true)
        : scripted(), driver(), config(max31865DefaultBeginConfig())
    {
        config.transport = scripted.makeTransport(
            hasDrdy,
            includeBusLock,
            includeNowUs);
    }

    MAX31865Status begin()
    {
        return driver.begin(config);
    }
};

inline size_t firstEventOfKind(
    const ScriptedMax31865Transport &scripted,
    EventKind kind)
{
    for (size_t index = 0U; index < scripted.eventCount(); ++index) {
        if (scripted.event(index).kind == kind) {
            return index;
        }
    }
    return scripted.eventCount();
}

} // namespace max31865_test
