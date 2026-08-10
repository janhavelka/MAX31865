"""Shared strict host-compiler contract for framework-neutral checks."""

from __future__ import annotations

import os
import pathlib
import shlex
import shutil
import subprocess
from collections.abc import Sequence


ROOT = pathlib.Path(__file__).resolve().parents[1]

STRICT_CXX_FLAGS = (
    "-std=c++17",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wshadow",
    "-Wundef",
    "-Werror",
    *tuple(shlex.split(os.environ.get("MAX31865_EXTRA_CXX_FLAGS", ""))),
)

CORE_DRIVER_SOURCES = (
    "src/MAX31865_Protocol.cpp",
    "src/MAX31865.cpp",
)
DEVICE_MODEL_SOURCES = (
    "test/support/Max31865DeviceModel.cpp",
    "test/support/ScriptedMax31865Transport.cpp",
)
DRIVER_MODEL_SOURCES = CORE_DRIVER_SOURCES + DEVICE_MODEL_SOURCES


def find_compiler() -> str:
    """Return the supported host C++ compiler or stop with one clear error."""
    requested = os.environ.get("MAX31865_CXX") or os.environ.get("CXX")
    if requested:
        compiler = shutil.which(requested)
        if compiler is None:
            raise SystemExit(
                f"Requested C++ compiler was not found: {requested}"
            )
        return compiler
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise SystemExit("No host C++ compiler found (g++ or clang++).")
    return compiler


def build_and_run(
    name: str,
    sources: Sequence[str],
    *,
    include_dirs: Sequence[str] = ("include", "src"),
    definitions: Sequence[str] = (),
    extra_flags: Sequence[str] = (),
) -> pathlib.Path:
    """Compile and run one warning-clean native executable."""
    compiler = find_compiler()
    build_dir = ROOT / ".pio" / name
    executable = build_dir / (f"{name}.exe" if os.name == "nt" else name)
    build_dir.mkdir(parents=True, exist_ok=True)
    command = [
        compiler,
        *STRICT_CXX_FLAGS,
        *extra_flags,
        *(f"-D{definition}" for definition in definitions),
        *(f"-I{include_dir}" for include_dir in include_dirs),
        *sources,
        "-o",
        str(executable),
    ]
    print(subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(executable)], cwd=ROOT, check=True)
    return executable
