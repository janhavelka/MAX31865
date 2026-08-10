#!/usr/bin/env python3
"""Build and run the real diagnostic CLI against the native device model."""

from native_compile import DRIVER_MODEL_SOURCES, build_and_run


def main() -> int:
    build_and_run(
        "native_cli_tests",
        (
            *DRIVER_MODEL_SOURCES,
            "examples/06_diagnostic_cli/Max31865Cli.cpp",
            "test/diagnostic_cli_tests.cpp",
        ),
        include_dirs=(
            "include",
            "src",
            "test",
            "examples/06_diagnostic_cli",
        ),
        definitions=(
            'MAX31865_CLI_GIT_COMMIT="native-test"',
            'MAX31865_CLI_GIT_STATUS="clean"',
        ),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
