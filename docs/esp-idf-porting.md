# MAX31865 ESP-IDF Port Notes

This document defines the intended ESP-IDF migration path. It is not the
current runtime target; Arduino/PlatformIO remains the supported runtime.

## Current Status

- Current package version: `0.1.2`
- Target ESP-IDF major version: `2.0.0`
- Current public include: `#include "MAX31865.h"`
- Current namespace: global scope
- Suggested v2 namespace: `MAX31865Driver`
- Difficulty: low to medium

The public API should stay recognizable in the port:

- `MAX31865BeginConfig`
- `MAX31865State`
- `MAX31865Error`
- `MAX31865Health`
- `MAX31865Settings`
- register helpers
- fault helpers
- conversion helpers

The implementation work is mostly in SPI/GPIO/timing glue. The MAX31865
register behavior should not be redesigned during the port.

## Arduino Dependencies

The current library code under `include/` and `src/` intentionally keeps
Arduino-specific calls concentrated.

| Dependency | Current use |
| --- | --- |
| `#include <Arduino.h>` | Public Arduino header |
| `SPIClass`, `SPISettings` | Public begin config, positional begin overload, and SPI glue |
| FreeRTOS semaphore | Internal SPI transaction serialization |
| `delay()` | 1 call |
| `delayMicroseconds()` | 1 call |
| `millis()` | 1 call |
| `yield()` | 2 calls |
| `pinMode()` | GPIO setup |
| `digitalWrite()` | CS framing |
| `digitalRead()` | optional DRDY polling |

`tools/check_core_timing_guard.py` enforces the expected timing-call locations.
Update that guard when porting, not before.

## Migration Steps

1. Freeze the last Arduino-only API tag.
2. Add ESP-IDF component metadata (`CMakeLists.txt`, `idf_component.yml`).
3. Replace `SPIClass` usage with `spi_master`.
4. Replace GPIO calls with `driver/gpio.h`.
5. Replace Arduino timing calls with `esp_timer`, `esp_rom_delay_us`, and
   FreeRTOS delays.
6. Decide whether v2 keeps global names or introduces a namespace.
7. Preserve typed config, state/error/health, manual reads, continuous
   conversion, register helpers, fault helpers, and conversion helpers.
8. Add an ESP-IDF example that builds for ESP32-S2 and ESP32-S3.
9. Add CI jobs for the ESP-IDF example before removing Arduino-only guards.
10. Bump version to `2.0.0` only after the port builds and examples pass.

## Replacement Map

| Arduino | ESP-IDF |
| --- | --- |
| `SPIClass`, `SPISettings`, `transfer()` | `spi_bus_initialize`, `spi_bus_add_device`, `spi_device_polling_transmit` |
| `pinMode()` | `gpio_set_direction()` |
| `digitalWrite()` | `gpio_set_level()` |
| `digitalRead()` | `gpio_get_level()` |
| `delay(ms)` | `vTaskDelay(pdMS_TO_TICKS(ms))` where scheduling is acceptable |
| `delayMicroseconds(us)` | `esp_rom_delay_us(us)` |
| `millis()` | `esp_timer_get_time() / 1000` |
| `yield()` | `vTaskDelay(1)` or event-driven wait |

## Verification Checklist

- [ ] ESP-IDF example builds for ESP32-S2.
- [ ] ESP-IDF example builds for ESP32-S3.
- [ ] No Arduino include remains in the ESP-IDF component build.
- [ ] No `SPIClass` remains in the ESP-IDF code path.
- [ ] GPIO and timing calls are behind the new ESP-IDF glue boundary.
- [ ] Public API smoke coverage exists for ESP-IDF.
- [ ] Version is bumped to `2.0.0`.
- [ ] Migration is documented in the changelog.
