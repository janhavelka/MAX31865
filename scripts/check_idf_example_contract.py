#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_FILES = [
    "CMakeLists.txt",
    "idf_component.yml",
    "examples/esp_idf/basic/CMakeLists.txt",
    "examples/esp_idf/basic/main/CMakeLists.txt",
    "examples/esp_idf/basic/main/main.cpp",
    "examples/esp_idf/basic/main/Max31865IdfSpi.h",
    "examples/esp_idf/basic/main/Max31865IdfSpi.cpp",
]

REQUIRED_MAIN_TOKENS = [
    'extern "C" void app_main()',
    "spi_bus_initialize",
    "spi_bus_add_device",
    "spi_bus_remove_device",
    "spi_bus_free",
    "gpio_config",
    "MAX31865BeginConfig",
    "cfg.transport.transfer",
    "cfg.transport.readDrdy",
    "cfg.transport.nowMs",
    "cfg.transport.delayMs",
    "cfg.transport.delayUs",
]

REQUIRED_ADAPTER_TOKENS = [
    "spi_device_polling_transmit",
    "gpio_get_level",
    "esp_timer_get_time",
    "vTaskDelay",
    "esp_rom_delay_us",
    "ESP_ERR_TIMEOUT",
    "MAX31865Error::SpiTransferFailed",
]

FORBIDDEN_IDF_TOKENS = [
    "Arduino.h",
    "SPI.h",
    "Serial",
    "SPIClass",
    "IdfArduinoCompat",
]

REQUIRED_COMPONENTS = [
    "MAX31865",
    "esp_driver_spi",
    "esp_driver_gpio",
    "esp_timer",
    "freertos",
]


def fail(msg: str) -> None:
    print(f"IDF example contract FAILED: {msg}")
    raise SystemExit(1)


def require_token(text: str, token: str, label: str) -> None:
    if token not in text:
        fail(f"{label} missing token '{token}'")


def main() -> int:
    for rel in REQUIRED_FILES:
        if not (ROOT / rel).exists():
            fail(f"missing {rel}")

    main_cpp = (ROOT / "examples/esp_idf/basic/main/main.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    adapter_cpp = (ROOT / "examples/esp_idf/basic/main/Max31865IdfSpi.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    idf_text = main_cpp + "\n" + adapter_cpp

    for token in REQUIRED_MAIN_TOKENS:
        require_token(main_cpp, token, "IDF main.cpp")
    for token in REQUIRED_ADAPTER_TOKENS:
        require_token(adapter_cpp, token, "IDF adapter")
    for token in FORBIDDEN_IDF_TOKENS:
        if token in idf_text:
            fail(f"IDF example must not use Arduino compatibility token '{token}'")

    cmake = (ROOT / "examples/esp_idf/basic/main/CMakeLists.txt").read_text(
        encoding="utf-8", errors="replace"
    )
    for component in REQUIRED_COMPONENTS:
        if re.search(rf"\b{re.escape(component)}\b", cmake) is None:
            fail(f"IDF example CMake missing required component '{component}'")

    manifest = (ROOT / "idf_component.yml").read_text(encoding="utf-8", errors="replace")
    for token in ("esp32s2", "esp32s3", "idf:"):
        require_token(manifest, token, "idf_component.yml")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
