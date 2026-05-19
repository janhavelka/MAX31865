#include "Max31865IdfSpi.h"

#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

MAX31865Status mapEspError(esp_err_t err) {
    if (err == ESP_OK) {
        return MAX31865Status::Ok();
    }
    if (err == ESP_ERR_TIMEOUT) {
        return MAX31865Status::Error(MAX31865Error::SpiLockTimeout,
                                     "ESP-IDF SPI timeout", err);
    }
    if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_STATE) {
        return MAX31865Status::Error(MAX31865Error::InvalidArgument,
                                     "ESP-IDF SPI invalid argument", err);
    }
    return MAX31865Status::Error(MAX31865Error::SpiTransferFailed,
                                 "ESP-IDF SPI transfer failed", err);
}

}  // namespace

MAX31865Status max31865IdfTransfer(const uint8_t* tx,
                                   uint8_t* rx,
                                   size_t len,
                                   uint32_t timeoutMs,
                                   void* user) {
    (void)timeoutMs;
    Max31865IdfSpi* ctx = static_cast<Max31865IdfSpi*>(user);
    if (ctx == nullptr || ctx->dev == nullptr || tx == nullptr || rx == nullptr || len == 0U) {
        return MAX31865Status::Error(MAX31865Error::InvalidArgument,
                                     "Invalid IDF SPI transfer");
    }

    spi_transaction_t transaction = {};
    transaction.length = static_cast<uint32_t>(len * 8U);
    transaction.tx_buffer = tx;
    transaction.rx_buffer = rx;
    return mapEspError(spi_device_polling_transmit(ctx->dev, &transaction));
}

bool max31865IdfReadDrdy(void* user) {
    Max31865IdfSpi* ctx = static_cast<Max31865IdfSpi*>(user);
    if (ctx == nullptr || ctx->drdy == GPIO_NUM_NC) {
        return false;
    }
    return gpio_get_level(ctx->drdy) == 0;
}

uint32_t max31865IdfNowMs(void*) {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void max31865IdfDelayMs(uint32_t ms, void*) {
    if (ms == 0U) {
        taskYIELD();
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void max31865IdfDelayUs(uint32_t us, void*) {
    esp_rom_delay_us(us);
}

void max31865IdfYield(void*) {
    taskYIELD();
}
