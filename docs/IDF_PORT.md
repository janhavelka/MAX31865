# MAX31865 ESP-IDF Port Status

Last updated: 2026-05-19

Scope: ESP-IDF support for the current MAX31865 RTD driver while keeping the
Arduino SPI API and examples compatible. This file supersedes the older
`docs/esp-idf-porting.md` for implementation work.

## Current State

- The library supports Arduino/PlatformIO and an application-owned transport
  callback backend. `include/MAX31865/Config.h` still exposes the Arduino
  `SPIClass*` compatibility field when Arduino headers are available.
- `src/MAX31865.cpp` routes register frames through `MAX31865TransportConfig`
  when supplied. The legacy Arduino fallback still owns `_spi->begin(...)`,
  `/CS`, DRDY GPIO, and Arduino timing when no transport is supplied.
- The driver already has production behavior worth preserving: explicit
  `begin/tick/stop/end`, bounded polling, health tracking, manual `recover`,
  register helpers, fault handling, one-shot and continuous conversion support.
- Examples are Arduino-only and configure board pins in example code.
- `library.json` declares Arduino and ESP-IDF. A root ESP-IDF `CMakeLists.txt`,
  `idf_component.yml`, and `examples/esp_idf/basic` are present.

## Blockers

1. The ESP-IDF example still needs to be built with a sourced ESP-IDF toolchain.
2. Hardware validation is still required for one-shot reads, continuous reads,
   DRDY readiness, and fault cycles.
3. Native fake-transport tests should be expanded for callback transfer errors,
   timeout mapping, and DRDY readiness.

## Exact Files/APIs To Change

- `include/MAX31865/Config.h`
  - Done: Arduino SPI include is guarded through `MAX31865_HAS_ARDUINO_BACKEND`.
  - Done: additive `MAX31865TransportConfig` supports non-Arduino transfer,
    DRDY, time, delay, and yield callbacks.
  - Preserve `MAX31865BeginConfig::spi` for Arduino compatibility.
- `include/MAX31865/MAX31865.h`
  - Done: unguarded Arduino and SPI includes were removed from this header.
  - The private Arduino `SPIClass*` compatibility pointer remains, but it is
    forward-declared when Arduino headers are unavailable.
  - Keep existing `begin(SPIClass&, ...)` overloads for Arduino.
- `src/MAX31865.cpp`
  - Done: register reads/writes route through one transport function, using the
    callback backend when supplied.
  - Keep the existing health transitions, probe/recover rules, and fault logic.
  - Done: `pinMode`, `digitalWrite`, `digitalRead`, `delay`,
    `delayMicroseconds`, `millis`, and `yield` are behind callback or guarded
    Arduino fallback paths.
- `examples/common/TransportAdapter.h`
  - Keep Arduino helper code there; add a separate IDF adapter in an IDF example
    rather than mixing framework code in the library.
- Done: root `CMakeLists.txt` and `idf_component.yml` were added.

## Architecture Preserving Arduino Compatibility

Use a transport-neutral core with two adapters:

- Arduino adapter: wraps the existing `SPIClass`, `SPISettings`, `/CS` GPIO,
  DRDY GPIO, and FreeRTOS mutex behavior. Existing examples keep compiling.
- ESP-IDF adapter: wraps a caller-created `spi_device_handle_t`, optional DRDY
  GPIO, timing functions, and an optional bus lock. The adapter uses
  `<driver/spi_master.h>` from `esp_driver_spi`.
- Core driver: MAX31865 register protocol, settings, fault handling, conversions,
  and health tracking. It must not call Arduino or ESP-IDF bus setup APIs.

Preferred transaction contract: one callback transfers one complete register
frame while the adapter controls chip select. This avoids mixing automatic IDF
CS handling with manual GPIO toggling.

The IDF adapter must own SPI transaction serialization as well as CS assertion.
If the application shares a SPI bus or if future continuous/background reads are
added, the adapter must hold a bus/device lock around the full register frame.
The core driver must never toggle CS around an IDF device configured with
`spics_io_num`, and it must never split one MAX31865 register frame across
multiple independently locked transactions.

## Adapter Contract

Implemented non-Arduino config shape:

```cpp
typedef MAX31865Status (*MAX31865TransferFn)(
    const uint8_t* tx,
    uint8_t* rx,
    size_t len,
    uint32_t timeoutMs,
    void* user);

typedef bool (*MAX31865ReadDrdyFn)(void* user);
typedef uint32_t (*MAX31865NowMsFn)(void* user);
typedef void (*MAX31865DelayUsFn)(uint32_t us, void* user);
typedef void (*MAX31865YieldFn)(void* user);

struct MAX31865TransportConfig {
  MAX31865TransferFn transfer;
  MAX31865ReadDrdyFn readDrdy;   // optional
  MAX31865NowMsFn nowMs;         // optional, esp_timer_get_time()/1000
  MAX31865DelayMsFn delayMs;     // optional, FreeRTOS delay/yield
  MAX31865DelayUsFn delayUs;     // optional, esp_rom_delay_us
  MAX31865YieldFn cooperativeYield;
  void* user;
  uint32_t timeoutMs;
};
```

IDF adapter responsibilities:

- Configure SPI device externally with mode 1, MSB first, <= 5 MHz, and one
  transaction queue entry is sufficient for polling transactions.
- Use `spi_device_polling_transmit` or `spi_device_transmit` with
  `spi_transaction_t`.
- Use automatic CS from `spi_device_interface_config_t::spics_io_num`, or handle
  CS in the adapter, but not both.
- Map `ESP_OK` to `MAX31865Status::ok()`, `ESP_ERR_TIMEOUT` to timeout, invalid
  arguments to invalid parameter/config, and all other `esp_err_t` values to an
  SPI error with `detail = err`.

## CMake/Component Plan

Core component:

```cmake
idf_component_register(
  SRCS "src/MAX31865.cpp"
  INCLUDE_DIRS "include"
  REQUIRES freertos
)
```

IDF adapter/example component:

```cmake
idf_component_register(
  SRCS "main.cpp" "max31865_idf_spi.cpp"
  INCLUDE_DIRS "."
  REQUIRES MAX31865 esp_driver_spi esp_driver_gpio esp_timer freertos
)
```

Use ESP-IDF v6 headers/components:

- SPI: `driver/spi_master.h` from `esp_driver_spi`
- GPIO/DRDY example: `driver/gpio.h` from `esp_driver_gpio`
- Time: `esp_timer.h` from `esp_timer`
- Task delay/yield/lock: FreeRTOS headers from `freertos`

## Examples

Arduino example remains:

```cpp
MAX31865BeginConfig cfg{};
cfg.spi = &SPI;
cfg.pins = {sck, miso, mosi, cs, drdy};
rtd.begin(cfg);
```

IDF example should create the bus/device before constructing the driver:

```cpp
spi_bus_config_t bus = {};
spi_device_interface_config_t dev = {};
dev.mode = 1;
dev.clock_speed_hz = 1000000;
dev.spics_io_num = cs_gpio;
dev.queue_size = 1;

ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &ctx.dev));

MAX31865BeginConfig cfg{};
cfg.transport.transfer = max31865IdfTransfer;
cfg.transport.readDrdy = max31865IdfReadDrdy;
cfg.transport.user = &ctx;
rtd.begin(cfg);
```

The final names may differ, but ownership must remain the same: the example owns
pins, host, and device handles; the library owns MAX31865 protocol state.

## Test And Validation Plan

- Native protocol tests with a fake transfer callback: config register writes,
  threshold writes, fault clear, one-shot sample, continuous sample, and readback
  failure mapping.
- Header compile tests for Arduino and ESP-IDF modes to catch accidental
  `SPIClass` leakage.
- IDF build tests for ESP32-S2 and ESP32-S3 with no Arduino framework.
- Hardware smoke test: probe, configure 2/3/4-wire mode, one-shot read,
  continuous read with `tick`, fault cycle, DRDY read if wired.
- Regression test that Arduino examples still compile with `SPIClass` overloads.
- Regenerate or redirect generated documentation after the port. The old
  generated HTML currently publishes stale ESP-IDF notes and should not remain
  the implementation handoff once `docs/IDF_PORT.md` is authoritative.

## IDF v6.0.1 Hazards

- Do not depend on the legacy `driver` component. Add `esp_driver_spi` and
  `esp_driver_gpio` where their headers are included.
- `spi_bus_initialize` must be called once per SPI host by the application, not
  by each device library.
- IDF SPI transactions own CS automatically if `spics_io_num` is set. Manual CS
  toggling around the same device will corrupt framing.
- Buffers used by queued SPI transactions must remain valid until completion.
  Polling transactions are simpler and sufficient for this driver.
- `esp_rom_delay_us` busy-waits; use it for MAX31865 microsecond timing, but
  prefer FreeRTOS delay for millisecond waits where protocol timing allows.
- SPI flash/PSRAM pins are not general-purpose SPI pins. Examples must not bake
  board-specific pin choices into the library.
- Do not call the driver from an ISR; SPI master and FreeRTOS locks are task
  context APIs.

## Ordered Checklist

1. Done: add the transport config types in `Config.h` without removing Arduino fields.
2. Done: hide Arduino-only includes/fallback code behind `MAX31865_HAS_ARDUINO_BACKEND`.
3. Done: preserve current Arduino behavior as the fallback backend.
4. Done: refactor register helpers to use the transport transaction callback.
5. Done: add IDF example time/delay wrappers and `esp_err_t` to
   `MAX31865Status` mapping.
6. Done: add root `CMakeLists.txt` and `idf_component.yml`.
7. Done: add an IDF SPI example using `driver/spi_master.h`.
8. Pending: add fake-transport native tests for register and fault behavior.
9. Pending local ESP-IDF toolchain: build tests for ESP32-S2 and ESP32-S3.
10. Done locally: re-run Arduino PlatformIO examples/tests.
