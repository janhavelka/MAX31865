#pragma once

#include "freertos/FreeRTOS.h"

using SemaphoreHandle_t = void*;

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    return reinterpret_cast<SemaphoreHandle_t>(1);
}

inline int xSemaphoreTake(SemaphoreHandle_t, TickType_t) {
    return pdTRUE;
}

inline void xSemaphoreGive(SemaphoreHandle_t) {}
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
