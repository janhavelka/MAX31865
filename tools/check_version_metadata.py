#!/usr/bin/env python3
"""Validate stable version metadata and the exact embedded build matrix."""

from __future__ import annotations

import configparser
import json
import pathlib
import re
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
CI_DOXYGEN_PACKAGE = "doxygen=1.9.8+ds-2build5"
CI_PLATFORMIO_PACKAGE = "platformio==6.1.19"
CI_PACKAGE_CACHE_INPUTS = (
    "hashFiles('platformio.ini', 'library.json', 'tools/package_manifest.txt')"
)
CI_ACTIONS = {
    "actions/cache@v4",
    "actions/checkout@v4",
    "actions/setup-python@v5",
}
PLATFORM_PIN = (
    "https://github.com/pioarduino/platform-espressif32/releases/download/"
    "55.03.311/platform-espressif32.zip"
)
EXPECTED_EXAMPLE_ENVS = {
    "ex_bringup_s2": "examples/01_basic_bringup/**",
    "ex_bringup_s3": "examples/01_basic_bringup/**",
    "ex_continuous_s2": "examples/02_continuous_sampling/**",
    "ex_continuous_s3": "examples/02_continuous_sampling/**",
    "ex_one_shot_s2": "examples/03_one_shot/**",
    "ex_one_shot_s3": "examples/03_one_shot/**",
    "ex_fault_diagnostics_s2": "examples/04_fault_diagnostics/**",
    "ex_fault_diagnostics_s3": "examples/04_fault_diagnostics/**",
    "ex_rtd_configuration_s2": "examples/05_rtd_configuration/**",
    "ex_rtd_configuration_s3": "examples/05_rtd_configuration/**",
    "ex_diagnostic_s2": "examples/06_diagnostic_cli/**",
    "ex_diagnostic_s3": "examples/06_diagnostic_cli/**",
}


def fail(message: str) -> None:
    print(f"Version metadata check FAILED: {message}")
    raise SystemExit(1)


def read_library_version() -> str:
    data = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
    version = data.get("version")
    if not isinstance(version, str):
        fail("library.json version is missing or invalid")
    if re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version) is None:
        fail("library.json version is not a three-part SemVer")
    return version


def read_idf_version() -> str:
    text = (ROOT / "idf_component.yml").read_text(encoding="utf-8")
    match = re.search(
        r'^version:\s*["\']?([^"\'\s]+)["\']?\s*$',
        text,
        flags=re.MULTILINE,
    )
    if match is None:
        fail("idf_component.yml version is missing")
    return match.group(1)


def read_header_version() -> str:
    text = (ROOT / "include" / "MAX31865" / "Version.h").read_text(
        encoding="utf-8"
    )
    match = re.search(
        r'static constexpr const char\*\s+VERSION\s*=\s*"([^"]+)"',
        text,
    )
    if match is None:
        fail("Version.h VERSION constant is missing")
    for pattern in ("Generated:", "Git commit:", "dirty", "clean"):
        if pattern in text:
            fail(f"Version.h contains volatile build metadata: {pattern}")
    version = match.group(1)
    semver = re.fullmatch(
        r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)",
        version,
    )
    if semver is None:
        fail("Version.h VERSION is not a three-part SemVer")
    major, minor, patch = (int(value) for value in semver.groups())
    expected_constants = {
        "VERSION_MAJOR": str(major),
        "VERSION_MINOR": str(minor),
        "VERSION_PATCH": str(patch),
        "VERSION_CODE": str(major * 10000 + minor * 100 + patch),
    }
    for name, expected in expected_constants.items():
        constant = re.search(
            rf"static constexpr uint(?:16|32)_t\s+{name}\s*=\s*([0-9]+)",
            text,
        )
        if constant is None or constant.group(1) != expected:
            fail(f"Version.h {name} is not synchronized")
    for name in (
        "VERSION_FULL",
        "BUILD_DATE",
        "BUILD_TIME",
        "BUILD_TIMESTAMP",
        "GIT_COMMIT",
        "GIT_STATUS",
    ):
        if re.search(rf"\b{name}\b", text):
            fail(f"Version.h retains obsolete volatile constant {name}")
    return version


def read_doxygen_version() -> str:
    text = (ROOT / "Doxyfile").read_text(encoding="utf-8")
    match = re.search(
        r'^PROJECT_NUMBER\s*=\s*"?([^"\s]+)"?\s*$',
        text,
        flags=re.MULTILINE,
    )
    if match is None:
        fail("Doxyfile PROJECT_NUMBER is missing")
    return match.group(1)


def check_version_documents(version: str) -> None:
    changelog = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
    if changelog.count("## [Unreleased]") != 1:
        fail("CHANGELOG.md must contain exactly one Unreleased section")
    released_heading = re.search(
        r"^## \[(?!Unreleased\])[^]]+\](?:\s+-\s+\d{4}-\d{2}-\d{2})?\s*$",
        changelog,
        flags=re.MULTILINE,
    )
    if released_heading is not None:
        fail(f"CHANGELOG.md contains release heading {released_heading.group(0)!r}")
    release_link = re.search(
        r"^\[(?:Unreleased|[0-9]+\.[0-9]+\.[0-9]+)\]:\s+\S+$",
        changelog,
        flags=re.MULTILINE,
    )
    if release_link is not None:
        fail(f"CHANGELOG.md contains unpublished release link {release_link.group(0)!r}")
    planned_release = (
        f"`{version}` is the planned first release. No project version has been "
        "published or\nvalidated on real MAX31865 hardware."
    )
    if planned_release not in changelog:
        fail(f"CHANGELOG.md does not identify {version} as planned and unpublished")
    for required in (
        "required HIL matrix",
        "publication is explicitly authorized",
    ):
        if required not in changelog:
            fail(f"CHANGELOG.md lacks pre-release gate wording: {required!r}")


def check_platformio() -> None:
    parser = configparser.ConfigParser(interpolation=None)
    parser.optionxform = str
    parser.read(ROOT / "platformio.ini", encoding="utf-8")
    if parser.get("env", "platform", fallback="") != PLATFORM_PIN:
        fail("platformio.ini does not use the exact MAX31865 platform pin")
    if parser.get("env", "framework", fallback="") != "arduino":
        fail("platformio.ini shared framework must be Arduino")
    if parser.get("env", "extra_scripts", fallback="") != "pre:scripts/generate_version.py":
        fail("version verification must be one shared pre-build script")
    if parser.get("platformio", "default_envs", fallback="") != "ex_diagnostic_s3":
        fail("default environment must be ex_diagnostic_s3")

    actual_envs = {
        section.removeprefix("env:")
        for section in parser.sections()
        if section.startswith("env:")
    }
    if actual_envs != set(EXPECTED_EXAMPLE_ENVS):
        missing = sorted(set(EXPECTED_EXAMPLE_ENVS) - actual_envs)
        extra = sorted(actual_envs - set(EXPECTED_EXAMPLE_ENVS))
        fail(f"example environment matrix differs; missing={missing}, extra={extra}")
    for environment, source_path in EXPECTED_EXAMPLE_ENVS.items():
        source_directory = ROOT / source_path.removesuffix("/**")
        if not source_directory.is_dir():
            fail(
                f"{environment} source directory is missing: "
                f"{source_directory.relative_to(ROOT).as_posix()}"
            )
        if not any(source_directory.rglob("*.cpp")):
            fail(
                f"{environment} source directory has no C++ translation unit: "
                f"{source_directory.relative_to(ROOT).as_posix()}"
            )
        source_filter = parser.get(f"env:{environment}", "build_src_filter", fallback="")
        if f"+<{source_path}>" not in source_filter:
            fail(f"{environment} does not select {source_path}")
        target = environment.rsplit("_", 1)[1]
        expected_base = "example_s2" if target == "s2" else "example_s3"
        if parser.get(f"env:{environment}", "extends", fallback="") != expected_base:
            fail(f"{environment} does not extend {expected_base}")


def check_ci_versions() -> None:
    workflow_path = ROOT / ".github" / "workflows" / "ci.yml"
    if not workflow_path.is_file():
        fail(".github/workflows/ci.yml is missing")
    workflow = workflow_path.read_text(encoding="utf-8")
    runners = re.findall(r"^\s*runs-on:\s*(\S+)\s*$", workflow, flags=re.MULTILINE)
    if not runners or set(runners) != {"ubuntu-24.04"}:
        fail("CI jobs must use the deliberate ubuntu-24.04 runner pin")
    actions = set(re.findall(r"^\s*uses:\s*(\S+)\s*$", workflow, flags=re.MULTILINE))
    if actions != CI_ACTIONS:
        fail(f"CI action pins differ from the release contract: {sorted(actions)}")
    if workflow.count(CI_PLATFORMIO_PACKAGE) != 2:
        fail("CI must install exact PlatformIO Core 6.1.19 in both embedded jobs")
    if workflow.count(CI_DOXYGEN_PACKAGE) != 1:
        fail("CI must install the exact Ubuntu 24.04 Doxygen 1.9.8 package")
    if workflow.count('test "$(doxygen --version)" = "1.9.8"') != 1:
        fail("CI must verify the installed Doxygen executable version")
    if workflow.count(CI_PACKAGE_CACHE_INPUTS) != 1:
        fail("packaged-consumer CI cache must include the immutable manifest")


def main() -> int:
    library_version = read_library_version()
    versions = {
        "idf_component.yml": read_idf_version(),
        "Version.h": read_header_version(),
        "Doxyfile": read_doxygen_version(),
    }
    for source, version in versions.items():
        if version != library_version:
            fail(f"{source} version {version} != library.json {library_version}")
    check_version_documents(library_version)
    result = subprocess.run(
        [sys.executable, "scripts/generate_version.py", "--check"],
        cwd=ROOT,
        check=False,
    )
    if result.returncode != 0:
        fail("the deterministic Version.h generator reports stale output")
    check_platformio()
    check_ci_versions()
    print("Version metadata check PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
