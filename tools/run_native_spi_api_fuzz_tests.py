#!/usr/bin/env python3
"""Build and run adversarial public managed-SPI deadline tests."""

from native_compile import DRIVER_MODEL_SOURCES, build_and_run


def main() -> int:
    build_and_run(
        "native_spi_api_fuzz_tests",
        (*DRIVER_MODEL_SOURCES, "test/spi_api_fuzz_tests.cpp"),
        include_dirs=("include", "src", "test"),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
