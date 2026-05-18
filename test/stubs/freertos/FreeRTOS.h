#pragma once

#include <stdint.h>

using TickType_t = uint32_t;

static constexpr int pdTRUE = 1;

inline TickType_t pdMS_TO_TICKS(uint32_t ms) {
    return ms;
}
