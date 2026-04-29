#!/usr/bin/env python3
"""Lightweight CLI contract check for the bringup example."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp"
REQUIRED = ("help", "probe", "recover", "drv", "cfg", "read", "fault", "threshold", "stress")


def main() -> int:
    text = MAIN.read_text(encoding="utf-8") if MAIN.exists() else ""
    missing = [cmd for cmd in REQUIRED if f'"{cmd}"' not in text]
    if missing:
        print("Missing CLI commands: " + ", ".join(missing), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
