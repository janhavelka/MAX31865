#!/usr/bin/env python3
"""Run the complete strict native suite, optionally under ASan or UBSan."""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
RUNNERS = (
    "run_native_core_compile.py",
    "run_native_api_tests.py",
    "run_native_device_model_tests.py",
    "run_native_protocol_tests.py",
    "run_native_transport_tests.py",
    "run_native_spi_api_fuzz_tests.py",
    "run_native_conversion_tests.py",
    "run_native_fault_tests.py",
    "run_native_surface_tests.py",
    "run_native_cli_tests.py",
    "run_arduino_backend_tests.py",
    "run_firmware_owner_fixture.py",
)
SANITIZER_FLAGS = {
    "address": "-fsanitize=address -fno-omit-frame-pointer",
    "undefined": "-fsanitize=undefined -fno-omit-frame-pointer",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sanitizer", choices=tuple(SANITIZER_FLAGS))
    args = parser.parse_args()

    environment = os.environ.copy()
    if args.sanitizer is not None:
        if environment.get("MAX31865_EXTRA_CXX_FLAGS"):
            raise SystemExit(
                "MAX31865_EXTRA_CXX_FLAGS must be unset with --sanitizer"
            )
        environment["MAX31865_EXTRA_CXX_FLAGS"] = SANITIZER_FLAGS[
            args.sanitizer
        ]
        if args.sanitizer == "address":
            environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
        else:
            environment["UBSAN_OPTIONS"] = (
                "halt_on_error=1:print_stacktrace=1"
            )

    for runner in RUNNERS:
        print(f"Running {runner}", flush=True)
        subprocess.run(
            [sys.executable, str(ROOT / "tools" / runner)],
            cwd=ROOT,
            env=environment,
            check=True,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
