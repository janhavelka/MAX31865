#!/usr/bin/env python3
"""Build and run the consumer-neutral synchronous firmware-owner fixture."""

from native_compile import DRIVER_MODEL_SOURCES, build_and_run


def main() -> int:
    build_and_run(
        "firmware_owner_fixture",
        (*DRIVER_MODEL_SOURCES, "test/firmware_owner_fixture.cpp"),
        include_dirs=("include", "src", "test"),
        extra_flags=("-pthread",),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
