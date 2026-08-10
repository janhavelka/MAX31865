#!/usr/bin/env python3
"""Enforce the framework-neutral core and guarded Arduino adapter boundary."""

from __future__ import annotations

import json
import pathlib
import re
import sys

from component_contract import CORE_COMPONENT_CMAKE, CORE_COMPONENT_DESCRIPTION


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_ROOTS = ("include", "src", "examples")
SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp"}
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

ARDUINO_PATTERNS = {
    "Arduino.h": re.compile(r"\bArduino\.h\b"),
    "SPI.h": re.compile(r"\bSPI\.h\b"),
    "SPIClass": re.compile(r"\bSPIClass\b"),
    "SPISettings": re.compile(r"\bSPISettings\b"),
    "pinMode": re.compile(r"\bpinMode\s*\("),
    "digitalWrite": re.compile(r"\bdigitalWrite\s*\("),
    "digitalRead": re.compile(r"\bdigitalRead\s*\("),
    "delay": re.compile(r"\bdelay\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "millis": re.compile(r"\bmillis\s*\("),
    "micros": re.compile(r"\bmicros\s*\("),
}

IDF_PATTERNS = {
    "driver/spi": re.compile(r"\bdriver/spi"),
    "driver/gpio": re.compile(r"\bdriver/gpio"),
    "esp_timer": re.compile(r"\besp_timer\b"),
    "esp_rom": re.compile(r"\besp_rom"),
    "ESP_PLATFORM": re.compile(r"\bESP_PLATFORM\b"),
    "HSPI": re.compile(r"\bHSPI\b"),
    "VSPI": re.compile(r"\bVSPI\b"),
}

FREERTOS_PATTERNS = {
    "freertos include": re.compile(r"\bfreertos/"),
    "TaskHandle_t": re.compile(r"\bTaskHandle_t\b"),
    "SemaphoreHandle_t": re.compile(r"\bSemaphoreHandle_t\b"),
    "portMUX_TYPE": re.compile(r"\bportMUX_TYPE\b"),
    "TickType_t": re.compile(r"\bTickType_t\b"),
}

ARDUINO_ALLOWED = {
    "include/MAX31865/ArduinoBackend.h",
    "src/platform/arduino/MAX31865ArduinoBackend.cpp",
}

CORE_PUBLIC_HEADERS = {
    "include/MAX31865/MAX31865.h",
    "include/MAX31865/CommandTable.h",
    "include/MAX31865/Config.h",
    "include/MAX31865/Protocol.h",
    "include/MAX31865/Status.h",
    "include/MAX31865/Transport.h",
    "include/MAX31865/Version.h",
}

REQUIRED_EXPORT_EXCLUSIONS = {
    ".git*",
    ".github",
    ".pio",
    ".vscode",
    "*.o",
    "*.obj",
    "*.pdf",
    "*.tar.gz",
    "AGENTS.md",
    "CONTRIBUTING.md",
    "Doxyfile",
    "SECURITY.md",
    "build",
    "cmake-build-*",
    "compile_commands.json",
    "docs",
    "hil_logs",
    "platformio.ini",
    "scripts",
    "test",
    "tools",
}


def iter_sources() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for root_name in SCAN_ROOTS:
        root = ROOT / root_name
        if not root.exists():
            continue
        files.extend(
            path
            for path in root.rglob("*")
            if path.is_file() and path.suffix.lower() in SUFFIXES
        )
    return sorted(files)


def strip_non_code(text: str) -> str:
    text = BLOCK_COMMENT_RE.sub("", text)
    text = LINE_COMMENT_RE.sub("", text)
    return STRING_RE.sub('""', text)


def main() -> int:
    errors: list[str] = []
    for path in iter_sources():
        rel = path.relative_to(ROOT).as_posix()
        code = strip_non_code(path.read_text(encoding="utf-8", errors="replace"))
        arduino_allowed = rel in ARDUINO_ALLOWED or rel.startswith("examples/")

        for label, pattern in ARDUINO_PATTERNS.items():
            if pattern.search(code) and not arduino_allowed:
                errors.append(f"Arduino token {label} found in disallowed file {rel}")

        for label, pattern in IDF_PATTERNS.items():
            if pattern.search(code):
                errors.append(f"ESP-IDF token {label} found in framework source {rel}")

        for label, pattern in FREERTOS_PATTERNS.items():
            if pattern.search(code):
                errors.append(f"FreeRTOS token {label} found in framework source {rel}")

        if rel in CORE_PUBLIC_HEADERS:
            for label, pattern in ARDUINO_PATTERNS.items():
                if pattern.search(code):
                    errors.append(f"core public header leaks Arduino token {label}: {rel}")

    required_paths = (
        *CORE_PUBLIC_HEADERS,
        *ARDUINO_ALLOWED,
        "src/MAX31865.cpp",
        "src/MAX31865_Protocol.cpp",
    )
    for relative in required_paths:
        if not (ROOT / relative).is_file():
            errors.append(f"required framework-boundary file is missing: {relative}")

    forbidden_paths = (
        "include/MAX31865/IdfBackend.h",
        "src/platform/esp_idf",
        "examples/esp_idf",
    )
    for relative in forbidden_paths:
        if (ROOT / relative).exists():
            errors.append(f"bundled native ESP-IDF surface is not core-only: {relative}")

    cmake_path = ROOT / "CMakeLists.txt"
    if not cmake_path.is_file():
        errors.append("core-only CMakeLists.txt is missing")
    elif cmake_path.read_text(encoding="utf-8") != CORE_COMPONENT_CMAKE:
        errors.append("CMakeLists.txt differs from the canonical two-source core contract")

    component_path = ROOT / "idf_component.yml"
    if not component_path.is_file():
        errors.append("idf_component.yml is missing")
    else:
        component = component_path.read_text(encoding="utf-8")
        if f"description: {CORE_COMPONENT_DESCRIPTION}" not in component:
            errors.append("idf_component.yml does not describe callback-supplied transport")
        for required in ("esp32s2", "esp32s3"):
            if required not in component:
                errors.append(f"idf_component.yml is missing target {required}")
        for forbidden in ("REQUIRES", "PRIV_REQUIRES", "src/platform", "ArduinoBackend"):
            if forbidden in component:
                errors.append(f"idf_component.yml contains backend/dependency token {forbidden}")

    metadata_path = ROOT / "library.json"
    if not metadata_path.is_file():
        errors.append("library.json is missing")
    else:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        frameworks = metadata.get("frameworks", [])
        if len(frameworks) != 2 or set(frameworks) != {"arduino", "espidf"}:
            errors.append("library.json must advertise Arduino and core-only ESP-IDF")
        if metadata.get("headers") != [
            "MAX31865/MAX31865.h",
            "MAX31865/Transport.h",
        ]:
            errors.append("library.json public entry headers differ from the core contract")
        exclusions = set(metadata.get("export", {}).get("exclude", []))
        if exclusions != REQUIRED_EXPORT_EXCLUSIONS:
            errors.append("library.json export exclusions differ from the release contract")

    if errors:
        print("Framework boundary check FAILED:")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Framework boundary check PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
