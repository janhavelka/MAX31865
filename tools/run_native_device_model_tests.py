#!/usr/bin/env python3
"""Build and self-test the deterministic MAX31865 device/transport model."""

from __future__ import annotations

import subprocess

from native_compile import DEVICE_MODEL_SOURCES, ROOT, build_and_run


def main() -> int:
    executable = build_and_run(
        "native_device_model_tests",
        (*DEVICE_MODEL_SOURCES, "test/device_model_tests.cpp"),
        include_dirs=("include", "test"),
    )
    overflow = subprocess.run(
        [str(executable), "--trigger-event-overflow"],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if overflow.returncode == 0:
        print("Event transcript overflow did not fail immediately.")
        return 1
    print("PASS: event transcript overflow fails immediately")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
