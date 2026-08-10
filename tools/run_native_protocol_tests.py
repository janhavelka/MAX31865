#!/usr/bin/env python3
"""Build and run framework-neutral MAX31865 protocol codec tests."""

from native_compile import build_and_run


def main() -> int:
    build_and_run(
        "native_protocol_tests",
        (
            "src/MAX31865_Protocol.cpp",
            "test/support/Max31865DeviceModel.cpp",
            "test/native_protocol_tests.cpp",
        ),
        include_dirs=("include", "test"),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
