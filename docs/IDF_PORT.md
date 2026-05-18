# MAX31865 ESP-IDF v6.0.1 Port Audit

Scope: future ESP-IDF support for the current MAX31865 RTD driver while keeping
the Arduino SPI API and examples compatible. This file supersedes the older
`docs/esp-idf-porting.md` for new implementation work.

## Current State

- The library is Arduino/PlatformIO oriented. `include/MAX31865/Config.h` exposes
  `SPIClass*`, and `include/MAX31865/MAX31865.h` exposes `SPIClass`,
  `SPISettings`, and FreeRTOS semaphore types.
- `src/MAX31865.cpp` owns SPI bus initialization through `_spi->begin(...)`,
  controls `/CS` through Arduino GPIO calls, reads DRDY through `digitalRead`,
  and uses Arduino timing/yield calls.
- The driver already has production behavior worth preserving: explicit
  `begin/tick/stop/end`, bounded polling, health tracking, manual `recover`,
  register helpers, fault handling, one-shot and continuous conversion support.
- Examples are Arduino-only and configure board pins in example code.
- `library.json` and `platformio.ini` are Arduino metadata only.

## Blockers

1. Pure ESP-IDF cannot compile the public headers because Arduino SPI types are
   part of the public class/config layout.
2. The implementation owns bus setup and chip-select GPIO. For IDF, bus/device
   setup must live in the application or adapter using `spi_master`; the core
   should only request register transactions.
3. Timing and delays are Arduino-specific. IDF needs `esp_timer_get_time`,
   `esp_rom_delay_us`, and FreeRTOS task delay/yield wrappers.
4. SPI health tracking currently assumes `bool transfer(...)`; IDF must preserve
   `MAX31865Status` detail by mapping `esp_err_t`.
5. `/DRDY` pin support is tied to Arduino GPIO and must become an optional
   callback/adapter field.

## Exact Files/APIs To Change

- `include/MAX31865/Config.h`
  - Guard `#include <SPI.h>` and `SPIClass*` fields with `ARDUINO`.
  - Add an additive transport configuration for non-Arduino builds. Do not
    remove `MAX31865BeginConfig::spi` in an existing minor/patch release.
  - Add callback types for SPI transaction, optional DRDY read, time, delay, and
    optional lock/unlock if the adapter owns shared SPI arbitration.
- `include/MAX31865/MAX31865.h`
  - Remove unguarded Arduino and SPI includes from the common public path.
  - Replace private `SPIClass*`, `SPISettings`, and direct mutex details with a
    transport member or put Arduino-only members behind `ARDUINO`.
  - Keep existing `begin(SPIClass&, ...)` overloads for Arduino.
- `src/MAX31865.cpp`
  - Route all register reads/writes through one transport function.
  - Keep the existing health transitions, probe/recover rules, and fault logic.
  - Replace `pinMode`, `digitalWrite`, `digitalRead`, `delay`, `delayMicroseconds`,
    `millis`, and `yield` with port wrappers.
- `examples/common/TransportAdapter.h`
  - Keep Arduino helper code there; add a separate IDF adapter in an IDF example
    rather than mixing framework code in the library.
- Add component metadata during implementation: root `CMakeLists.txt` and
  `idf_component.yml`.

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

Add a non-Arduino config shape equivalent to:

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

struct MAX31865TransportConfig {
  MAX31865TransferFn transfer;
  MAX31865ReadDrdyFn readDrdy;   // optional
  MAX31865NowMsFn nowMs;         // optional, esp_timer_get_time()/1000
  MAX31865DelayUsFn delayUs;     // optional, esp_rom_delay_us
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

Core component after the port:

```cmake
idf_component_register(
  SRCS "src/MAX31865.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_timer freertos
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

1. Add the transport config types in `Config.h` without removing Arduino fields.
2. Hide Arduino-only includes and private members behind `ARDUINO`.
3. Implement Arduino transport adapter to preserve current behavior.
4. Refactor register helpers to use the transport transaction callback.
5. Add IDF time/delay wrappers and `esp_err_t` to `MAX31865Status` mapping.
6. Add root `CMakeLists.txt` and `idf_component.yml`.
7. Add an IDF SPI example using `driver/spi_master.h`.
8. Add fake-transport native tests for register and fault behavior.
9. Add ESP-IDF v6.0.1 build tests for ESP32-S2 and ESP32-S3.
10. Re-run Arduino PlatformIO examples/tests and verify no public API regression.
