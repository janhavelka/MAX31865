#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys
from typing import Dict

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_DIRS = ("src", "include")
VALID_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}

FORBIDDEN_CALLS = {
    "millis": re.compile(r"\bmillis\s*\("),
    "micros": re.compile(r"\bmicros\s*\("),
    "delay": re.compile(r"\bdelay\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield": re.compile(r"\byield\s*\("),
}

INCLUDE_ARDUINO_RE = re.compile(r'^\s*#\s*include\s*[<"]Arduino\.h[>"]', re.MULTILINE)
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

ALLOWED_CALL_COUNTS: Dict[str, Dict[str, int]] = {
    "src/MAX31865Arduino.cpp": {
        "millis": 1,
        "delay": 1,
        "delayMicroseconds": 1,
        "yield": 1,
    },
}
ALLOWED_INCLUDE_COUNTS: Dict[str, int] = {
    "src/MAX31865Arduino.cpp": 1,
}

TIMING_MINIMUMS = {
    "SINGLE_CONVERSION_60HZ_MS": 55,
    "SINGLE_CONVERSION_50HZ_MS": 66,
    "CONTINUOUS_CONVERSION_60HZ_MS": 18,
    "CONTINUOUS_CONVERSION_50HZ_MS": 21,
    "AUTO_FAULT_DETECTION_MAX_US": 600,
}

CLEAN_CORE_HEADERS = (
    "include/MAX31865/Core.h",
    "include/MAX31865/Transport.h",
    "include/MAX31865/Config.h",
    "include/MAX31865/Status.h",
    "include/MAX31865/CommandTable.h",
)

CLEAN_HEADER_FORBIDDEN = {
    "SPIClass": re.compile(r"\bSPIClass\b"),
    "SPI.h": re.compile(r"#\s*include\s*[<\"]SPI\.h[>\"]"),
    "Arduino.h": re.compile(r"#\s*include\s*[<\"]Arduino\.h[>\"]"),
    "FreeRTOS": re.compile(r"\bFreeRTOS\b|freertos/"),
    "SemaphoreHandle_t": re.compile(r"\bSemaphoreHandle_t\b"),
    "ESP-IDF": re.compile(r"\bESP_PLATFORM\b|\besp_[a-zA-Z0-9_]+\b|driver/"),
}

CORE_SOURCE_FORBIDDEN = {
    "FreeRTOS": re.compile(r"\bFreeRTOS\b|freertos/"),
    "SemaphoreHandle_t": re.compile(r"\bSemaphoreHandle_t\b"),
    "xSemaphore": re.compile(r"\bxSemaphore[A-Za-z_]*\b"),
    "vSemaphoreDelete": re.compile(r"\bvSemaphoreDelete\b"),
    "SPIClass": re.compile(r"\bSPIClass\b"),
    "SPISettings": re.compile(r"\bSPISettings\b"),
    "pinMode": re.compile(r"\bpinMode\s*\("),
    "digitalWrite": re.compile(r"\bdigitalWrite\s*\("),
    "digitalRead": re.compile(r"\bdigitalRead\s*\("),
    "Arduino.h": re.compile(r"#\s*include\s*[<\"]Arduino\.h[>\"]"),
    "SPI.h": re.compile(r"#\s*include\s*[<\"]SPI\.h[>\"]"),
}

CORE_SOURCE_BOUNDARY_FILES = (
    "src/MAX31865.cpp",
    "CMakeLists.txt",
)


def strip_non_code(text: str) -> str:
    text = BLOCK_COMMENT_RE.sub("", text)
    text = LINE_COMMENT_RE.sub("", text)
    return STRING_RE.sub('""', text)


def collect_sources() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for dirname in SCAN_DIRS:
        root = ROOT / dirname
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in VALID_SUFFIXES:
                files.append(path)
    return files


def constant_value(name: str, text: str) -> int:
    match = re.search(rf"{name}\s*=\s*(\d+)", text)
    if not match:
        raise ValueError(f"Missing {name}")
    return int(match.group(1))


def main() -> int:
    observed_calls: Dict[str, Dict[str, int]] = {}
    observed_includes: Dict[str, int] = {}

    for path in collect_sources():
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_text(encoding="utf-8", errors="replace")
        code = strip_non_code(raw)

        call_counts: Dict[str, int] = {}
        for call_name, pattern in FORBIDDEN_CALLS.items():
            count = len(pattern.findall(code))
            if count > 0:
                call_counts[call_name] = count
        if call_counts:
            observed_calls[rel] = call_counts

        include_count = len(INCLUDE_ARDUINO_RE.findall(raw))
        if include_count > 0:
            observed_includes[rel] = include_count

    errors: list[str] = []

    for rel, counts in observed_calls.items():
        if rel not in ALLOWED_CALL_COUNTS:
            errors.append(f"forbidden timing calls in unexpected file: {rel} -> {counts}")
            continue
        expected = ALLOWED_CALL_COUNTS[rel]
        for call_name, count in counts.items():
            exp = expected.get(call_name, 0)
            if count != exp:
                errors.append(
                    f"timing call count mismatch in {rel}: {call_name} observed={count}, expected={exp}"
                )

    for rel, expected in ALLOWED_CALL_COUNTS.items():
        observed = observed_calls.get(rel, {})
        if rel not in observed_calls:
            for call_name, exp in expected.items():
                if exp != 0:
                    errors.append(
                        f"timing call count mismatch in {rel}: {call_name} observed=0, expected={exp}"
                    )
        unexpected_calls = set(observed.keys()) - set(expected.keys())
        if unexpected_calls:
            errors.append(f"unexpected timing call types in {rel}: {sorted(unexpected_calls)}")

    for rel, count in observed_includes.items():
        exp = ALLOWED_INCLUDE_COUNTS.get(rel, 0)
        if count != exp:
            errors.append(
                f"Arduino include count mismatch in {rel}: observed={count}, expected={exp}"
            )

    for rel, exp in ALLOWED_INCLUDE_COUNTS.items():
        obs = observed_includes.get(rel, 0)
        if obs != exp:
            errors.append(
                f"Arduino include count mismatch in {rel}: observed={obs}, expected={exp}"
            )

    timing_text = (ROOT / "include" / "MAX31865" / "CommandTable.h").read_text(
        encoding="utf-8",
        errors="replace",
    )
    for name, minimum in TIMING_MINIMUMS.items():
        if constant_value(name, timing_text) < minimum:
            errors.append(f"timing constant below datasheet max: {name}")

    for rel in CLEAN_CORE_HEADERS:
        path = ROOT / rel
        if not path.exists():
            errors.append(f"missing clean core header: {rel}")
            continue
        code = strip_non_code(path.read_text(encoding="utf-8", errors="replace"))
        for token, pattern in CLEAN_HEADER_FORBIDDEN.items():
            if pattern.search(code):
                errors.append(f"framework token '{token}' leaked into clean core header {rel}")

    core_header = ROOT / "include" / "MAX31865" / "Core.h"
    if core_header.exists():
        core_text = core_header.read_text(encoding="utf-8", errors="replace")
        if '#include "MAX31865/MAX31865.h"' not in core_text:
            errors.append("Core.h no longer exposes the transport-backed driver include")
        if "using MAX31865Core = MAX31865;" not in core_text:
            errors.append("Core.h no longer exposes the MAX31865Core pure-core alias")

    for rel in CORE_SOURCE_BOUNDARY_FILES:
        path = ROOT / rel
        if not path.exists():
            errors.append(f"missing core source boundary file: {rel}")
            continue
        code = strip_non_code(path.read_text(encoding="utf-8", errors="replace"))
        for token, pattern in CORE_SOURCE_FORBIDDEN.items():
            if pattern.search(code):
                errors.append(f"framework token '{token}' leaked into core source path {rel}")

    if errors:
        print("Core timing guard FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Core timing guard PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
