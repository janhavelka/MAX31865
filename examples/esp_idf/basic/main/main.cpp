#include "MAX31865/MAX31865.h"
#include "Max31865IdfSpi.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"

namespace {

constexpr char TAG[] = "max31865_basic";
constexpr spi_host_device_t SPI_HOST = SPI2_HOST;
constexpr gpio_num_t PIN_MOSI = GPIO_NUM_11;
constexpr gpio_num_t PIN_MISO = GPIO_NUM_13;
constexpr gpio_num_t PIN_SCLK = GPIO_NUM_12;
constexpr gpio_num_t PIN_CS = GPIO_NUM_10;
constexpr gpio_num_t PIN_DRDY = GPIO_NUM_14;

void logStatus(const char* label, const MAX31865Status& status) {
    ESP_LOGI(TAG, "%s: code=%u detail=%ld msg=%s",
             label, static_cast<unsigned>(status.code), static_cast<long>(status.detail),
             status.msg);
}

}  // namespace

extern "C" void app_main() {
    spi_bus_config_t bus = {};
    bus.mosi_io_num = PIN_MOSI;
    bus.miso_io_num = PIN_MISO;
    bus.sclk_io_num = PIN_SCLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;

    esp_err_t err = spi_bus_initialize(SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.mode = 1;
    devcfg.clock_speed_hz = 1000000;
    devcfg.spics_io_num = PIN_CS;
    devcfg.queue_size = 1;

    Max31865IdfSpi transport;
    err = spi_bus_add_device(SPI_HOST, &devcfg, &transport.dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
        (void)spi_bus_free(SPI_HOST);
        return;
    }

    gpio_config_t drdy = {};
    drdy.pin_bit_mask = 1ULL << PIN_DRDY;
    drdy.mode = GPIO_MODE_INPUT;
    drdy.pull_up_en = GPIO_PULLUP_ENABLE;
    drdy.pull_down_en = GPIO_PULLDOWN_DISABLE;
    drdy.intr_type = GPIO_INTR_DISABLE;
    (void)gpio_config(&drdy);
    transport.drdy = PIN_DRDY;

    MAX31865 rtd;
    MAX31865BeginConfig cfg{};
    cfg.referenceResistorOhms = 400.0f;
    cfg.rtdNominalOhms = 100.0f;
    cfg.wireMode = MAX31865WireMode::FourWire;
    cfg.filter = MAX31865Filter::Hz60;
    cfg.verifyProbe = true;
    cfg.transport.transfer = max31865IdfTransfer;
    cfg.transport.readDrdy = max31865IdfReadDrdy;
    cfg.transport.nowMs = max31865IdfNowMs;
    cfg.transport.delayMs = max31865IdfDelayMs;
    cfg.transport.delayUs = max31865IdfDelayUs;
    cfg.transport.cooperativeYield = max31865IdfYield;
    cfg.transport.user = &transport;
    cfg.transport.timeoutMs = 50;

    if (!rtd.begin(cfg)) {
        logStatus("begin failed", rtd.lastOperationStatus());
    } else {
        MAX31865Sample sample{};
        if (rtd.readSingle(sample, 250)) {
            ESP_LOGI(TAG, "resistance=%.3f ohm temperature=%.3f C",
                     static_cast<double>(sample.resistance_ohms),
                     static_cast<double>(sample.temperature_c));
        } else {
            logStatus("readSingle failed", rtd.lastOperationStatus());
        }
        rtd.end();
    }

    (void)spi_bus_remove_device(transport.dev);
    (void)spi_bus_free(SPI_HOST);
}
