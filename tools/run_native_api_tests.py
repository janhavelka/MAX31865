#!/usr/bin/env python3
"""Build and run compile-time/public-default API contract tests."""

from native_compile import CORE_DRIVER_SOURCES, build_and_run


def main() -> int:
    build_and_run(
        "native_api_tests",
        (*CORE_DRIVER_SOURCES, "test/public_api_contract_tests.cpp"),
        include_dirs=("include", "test"),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
