#!/usr/bin/env python3
"""Compile standalone public-header and framework-boundary probes."""

from __future__ import annotations

import pathlib
import subprocess
import tempfile

from native_compile import STRICT_CXX_FLAGS, find_compiler


ROOT = pathlib.Path(__file__).resolve().parents[1]
NEUTRAL_HEADERS = (
    "MAX31865/MAX31865.h",
    "MAX31865/CommandTable.h",
    "MAX31865/Config.h",
    "MAX31865/Protocol.h",
    "MAX31865/Status.h",
    "MAX31865/Transport.h",
    "MAX31865/Version.h",
)
CORE_SOURCES = (
    "src/MAX31865.cpp",
    "src/MAX31865_Protocol.cpp",
)
GUARDED_HEADERS = ("MAX31865/ArduinoBackend.h",)


def compile_probe(
    compiler: str,
    source: pathlib.Path,
    output: pathlib.Path,
    *extra: str,
) -> None:
    subprocess.run(
        [
            compiler,
            *STRICT_CXX_FLAGS,
            *extra,
            "-c",
            str(source),
            "-o",
            str(output),
        ],
        cwd=ROOT,
        check=True,
    )


def main() -> int:
    compiler = find_compiler()
    for relative in (
        *(f"include/{header}" for header in NEUTRAL_HEADERS),
        *CORE_SOURCES,
    ):
        source = ROOT / relative
        if not source.is_file():
            raise SystemExit(f"Required framework-neutral file is missing: {relative}")
        text = source.read_text(encoding="utf-8", errors="strict")
        for token in ("ARDUINO", "ESP_PLATFORM"):
            if token in text:
                raise SystemExit(
                    f"Framework-neutral API/source must not branch on {token}: "
                    f"{relative}"
                )

    for header in GUARDED_HEADERS:
        if not (ROOT / "include" / header).is_file():
            raise SystemExit(f"Required guarded header is missing: include/{header}")

    with tempfile.TemporaryDirectory(prefix="max31865-header-hygiene-") as name:
        temporary = pathlib.Path(name)
        for esp_platform in (False, True):
            label = "esp-platform" if esp_platform else "neutral"
            definitions = ("-DESP_PLATFORM=1",) if esp_platform else ()
            for index, header in enumerate(NEUTRAL_HEADERS):
                source = temporary / f"{label}-{index}.cpp"
                source.write_text(
                    f'#include "{header}"\nint main() {{ return 0; }}\n',
                    encoding="utf-8",
                    newline="\n",
                )
                compile_probe(
                    compiler,
                    source,
                    temporary / f"{label}-{index}.o",
                    "-Iinclude",
                    "-UARDUINO",
                    *definitions,
                )

            for index, header in enumerate(GUARDED_HEADERS):
                source = temporary / f"{label}-guarded-{index}.cpp"
                source.write_text(
                    f'#include "{header}"\nint main() {{ return 0; }}\n',
                    encoding="utf-8",
                    newline="\n",
                )
                compile_probe(
                    compiler,
                    source,
                    temporary / f"{label}-guarded-{index}.o",
                    "-Iinclude",
                    "-UARDUINO",
                    *definitions,
                )

        for index, source_name in enumerate(CORE_SOURCES):
            compile_probe(
                compiler,
                ROOT / source_name,
                temporary / f"core-esp-platform-{index}.o",
                "-Iinclude",
                "-Isrc",
                "-UARDUINO",
                "-DESP_PLATFORM=1",
            )

        arduino_source = temporary / "arduino-backend.cpp"
        arduino_source.write_text(
            '#include "MAX31865/ArduinoBackend.h"\n'
            "int main() { return 0; }\n",
            encoding="utf-8",
            newline="\n",
        )
        for target in ("CONFIG_IDF_TARGET_ESP32S2", "CONFIG_IDF_TARGET_ESP32S3"):
            compile_probe(
                compiler,
                arduino_source,
                temporary / f"arduino-{target}.o",
                "-Iinclude",
                "-Itest/stubs",
                "-DARDUINO=10800",
                f"-D{target}=1",
            )

    print("Header hygiene check PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
