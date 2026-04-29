#!/usr/bin/env python3
"""Check that MAX31865 timing constants remain conservative."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
COMMAND_TABLE = ROOT / "include" / "MAX31865" / "CommandTable.h"


def value(name: str, text: str) -> int:
    match = re.search(rf"{name}\s*=\s*(\d+)", text)
    if not match:
        raise ValueError(f"Missing {name}")
    return int(match.group(1))


def main() -> int:
    text = COMMAND_TABLE.read_text(encoding="utf-8")
    checks = {
        "SINGLE_CONVERSION_60HZ_MS": 55,
        "SINGLE_CONVERSION_50HZ_MS": 66,
        "CONTINUOUS_CONVERSION_60HZ_MS": 18,
        "CONTINUOUS_CONVERSION_50HZ_MS": 21,
        "AUTO_FAULT_DETECTION_MAX_US": 600,
    }
    failures = [name for name, minimum in checks.items() if value(name, text) < minimum]
    if failures:
      print("Timing constants are below datasheet max: " + ", ".join(failures), file=sys.stderr)
      return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
