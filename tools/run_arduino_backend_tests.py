#!/usr/bin/env python3
"""Build and run the fake-Arduino backend matrix for ESP32-S2/S3."""

from __future__ import annotations

from native_compile import CORE_DRIVER_SOURCES, ROOT, build_and_run


def structural_checks() -> None:
    source = (
        ROOT / "src" / "platform" / "arduino" /
        "MAX31865ArduinoBackend.cpp"
    ).read_text(encoding="utf-8")
    forbidden = (
        "SPI.begin(",
        "xSemaphore",
        "FreeRTOS",
        "new ",
        "delete ",
        "transactionActive",
    )
    for token in forbidden:
        if token in source:
            raise SystemExit(
                f"Arduino backend structural check failed: {token}"
            )


def compile_and_run(target: str) -> None:
    build_and_run(
        f"arduino_backend_tests_{target}",
        (*CORE_DRIVER_SOURCES,
            "src/platform/arduino/MAX31865ArduinoBackend.cpp",
            "test/support/Max31865DeviceModel.cpp",
            "test/arduino_backend_tests.cpp",
        ),
        include_dirs=("test/fake_arduino_backend", "include", "test"),
        definitions=(
            "ARDUINO=10800",
            f"CONFIG_IDF_TARGET_ESP32{target.upper()}=1",
        ),
    )


def main() -> int:
    structural_checks()
    compile_and_run("s2")
    compile_and_run("s3")
    print("Arduino backend ESP32-S2/S3 host matrix passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
