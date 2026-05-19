#pragma once

#include <cstdint>

#include "MAX31865/Config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

struct Max31865IdfSpi {
    spi_device_handle_t dev = nullptr;
    gpio_num_t drdy = GPIO_NUM_NC;
};

MAX31865Status max31865IdfTransfer(const uint8_t* tx,
                                   uint8_t* rx,
                                   size_t len,
                                   uint32_t timeoutMs,
                                   void* user);
bool max31865IdfReadDrdy(void* user);
uint32_t max31865IdfNowMs(void* user);
void max31865IdfDelayMs(uint32_t ms, void* user);
void max31865IdfDelayUs(uint32_t us, void* user);
void max31865IdfYield(void* user);
