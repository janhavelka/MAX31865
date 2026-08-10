#!/usr/bin/env python3
"""Validate the exact release archive and build clean installed consumers."""

from __future__ import annotations

import io
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

from component_contract import CORE_COMPONENT_CMAKE, CORE_COMPONENT_DESCRIPTION
from native_compile import STRICT_CXX_FLAGS, find_compiler


ROOT = pathlib.Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tools" / "package_manifest.txt"
PLATFORMIO_VERSION = "6.1.19"
PLATFORMIO_PLATFORM = (
    "https://github.com/pioarduino/platform-espressif32/releases/download/"
    "55.03.311/platform-espressif32.zip"
)
PLATFORMIO_ARCHIVE_PREFIX = ""
INTENDED_ROOT_FILES = {
    "CHANGELOG.md",
    "CMakeLists.txt",
    "LICENSE",
    "README.md",
    "idf_component.yml",
    "library.json",
}
INTENDED_DIRECTORIES = (
    "include/MAX31865",
    "src",
    "examples/01_basic_bringup",
    "examples/02_continuous_sampling",
    "examples/03_one_shot",
    "examples/04_fault_diagnostics",
    "examples/05_rtd_configuration",
    "examples/06_diagnostic_cli",
    "examples/common",
)
REQUIRED_COMPONENT_EXCLUSIONS = {
    ".git*",
    ".github/**/*",
    ".pio/**/*",
    ".vscode/**/*",
    "*.o",
    "*.obj",
    "*.pdf",
    "*.tar.gz",
    "AGENTS.md",
    "CONTRIBUTING.md",
    "Doxyfile",
    "SECURITY.md",
    "build/**/*",
    "cmake-build-*/*",
    "compile_commands.json",
    "docs/**/*",
    "hil_logs/**/*",
    "platformio.ini",
    "scripts/**/*",
    "test/**/*",
    "tools/**/*",
}


NATIVE_CONSUMER = r'''#include "MAX31865/MAX31865.h"
#include "MAX31865/CommandTable.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace
{
struct DeviceStub
{
    uint8_t registers[max31865_cmd::NUM_REGISTERS]{};
    uint64_t microseconds = 0U;
    size_t transferCount = 0U;
    bool chipSelected = false;
    bool pendingConfigValid = false;
    uint8_t pendingConfig = 0U;
    bool busLocked = false;

    void reset()
    {
        const uint8_t defaults[max31865_cmd::NUM_REGISTERS] = {
            max31865_cmd::CONFIG_RESET,
            max31865_cmd::RTD_MSB_RESET,
            max31865_cmd::RTD_LSB_RESET,
            max31865_cmd::HIGH_FAULT_MSB_RESET,
            max31865_cmd::HIGH_FAULT_LSB_RESET,
            max31865_cmd::LOW_FAULT_MSB_RESET,
            max31865_cmd::LOW_FAULT_LSB_RESET,
            max31865_cmd::FAULT_STATUS_RESET,
        };
        for (size_t index = 0U; index < max31865_cmd::NUM_REGISTERS; ++index)
        {
            registers[index] = defaults[index];
        }
        microseconds = 0U;
        transferCount = 0U;
        chipSelected = false;
        pendingConfigValid = false;
        pendingConfig = 0U;
        busLocked = false;
    }

    void commitConfig()
    {
        if (!pendingConfigValid)
        {
            return;
        }
        pendingConfigValid = false;
        if ((pendingConfig & max31865_cmd::CONFIG_FAULT_CLEAR) != 0U)
        {
            registers[max31865_cmd::REG_FAULT_STATUS] = 0U;
            registers[max31865_cmd::REG_RTD_LSB] = static_cast<uint8_t>(
                registers[max31865_cmd::REG_RTD_LSB] & 0xFEU);
        }
        registers[max31865_cmd::REG_CONFIG] = static_cast<uint8_t>(
            pendingConfig & max31865_cmd::CONFIG_PERSISTENT_MASK);
    }
};

MAX31865Status lockBus(void *user, uint32_t)
{
    DeviceStub &device = *static_cast<DeviceStub *>(user);
    if (device.busLocked)
    {
        return MAX31865Status::Error(
            MAX31865Error::BusLockFailed,
            "bus already locked");
    }
    device.busLocked = true;
    return MAX31865Status::Ok();
}

void unlockBus(void *user)
{
    static_cast<DeviceStub *>(user)->busLocked = false;
}

MAX31865Status setChipSelect(void *user, bool asserted)
{
    DeviceStub &device = *static_cast<DeviceStub *>(user);
    if (!asserted && device.chipSelected)
    {
        device.commitConfig();
    }
    device.chipSelected = asserted;
    return MAX31865Status::Ok();
}

MAX31865Status transfer(
    void *user,
    const uint8_t *tx,
    uint8_t *rx,
    size_t length,
    uint32_t)
{
    DeviceStub &device = *static_cast<DeviceStub *>(user);
    if (!device.chipSelected || tx == nullptr || rx == nullptr ||
        length < 2U || length > max31865_cmd::MAX_FRAME_BYTES)
    {
        return MAX31865Status::Error(
            MAX31865Error::SpiTransferFailed,
            "invalid transfer",
            static_cast<int32_t>(length));
    }

    const bool write = (tx[0] & max31865_cmd::WRITE_BIT) != 0U;
    const uint8_t start = static_cast<uint8_t>(
        tx[0] & max31865_cmd::READ_MASK);
    const size_t count = length - 1U;
    if (start > max31865_cmd::REG_LAST ||
        count > max31865_cmd::NUM_REGISTERS - static_cast<size_t>(start))
    {
        return MAX31865Status::Error(
            MAX31865Error::SpiTransferFailed,
            "invalid register span",
            static_cast<int32_t>(start));
    }

    for (size_t index = 0U; index < length; ++index)
    {
        rx[index] = 0U;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        const uint8_t address = static_cast<uint8_t>(
            static_cast<size_t>(start) + index);
        if (!write)
        {
            rx[index + 1U] = device.registers[address];
            continue;
        }
        const uint8_t value = tx[index + 1U];
        if (address == max31865_cmd::REG_CONFIG)
        {
            device.pendingConfig = value;
            device.pendingConfigValid = true;
        }
        else if (address == max31865_cmd::REG_HIGH_FAULT_LSB ||
                 address == max31865_cmd::REG_LOW_FAULT_LSB)
        {
            device.registers[address] = static_cast<uint8_t>(
                value & max31865_cmd::THRESHOLD_LSB_DEFINED_MASK);
        }
        else if (address == max31865_cmd::REG_HIGH_FAULT_MSB ||
                 address == max31865_cmd::REG_LOW_FAULT_MSB)
        {
            device.registers[address] = value;
        }
    }
    ++device.transferCount;
    return MAX31865Status::Ok();
}

uint32_t nowMs(void *user)
{
    const DeviceStub &device = *static_cast<DeviceStub *>(user);
    return static_cast<uint32_t>(device.microseconds / 1000ULL);
}

uint32_t nowUs(void *user)
{
    return static_cast<uint32_t>(
        static_cast<DeviceStub *>(user)->microseconds);
}

void sleepMs(void *user, uint32_t milliseconds)
{
    DeviceStub &device = *static_cast<DeviceStub *>(user);
    device.microseconds += static_cast<uint64_t>(milliseconds) * 1000ULL;
}

void delayUs(void *user, uint32_t microseconds)
{
    static_cast<DeviceStub *>(user)->microseconds += microseconds;
}

bool sameConfiguration(
    const MAX31865DeviceConfig &left,
    const MAX31865DeviceConfig &right)
{
    return left.wireMode == right.wireMode &&
           left.filter == right.filter &&
           left.biasEnabled == right.biasEnabled &&
           left.thresholds.lowCode == right.thresholds.lowCode &&
           left.thresholds.highCode == right.thresholds.highCode;
}

int failStatus(const char *operation, MAX31865Status status)
{
    std::printf(
        "FAIL: %s: code=%u detail=%ld msg=%s\n",
        operation,
        static_cast<unsigned>(status.code),
        static_cast<long>(status.detail),
        status.msg == nullptr ? "" : status.msg);
    return 1;
}
} // namespace

int main()
{
    DeviceStub device;
    device.reset();

    MAX31865Transport transport{};
    transport.user = &device;
    transport.capabilities = {false};
    transport.lockBus = lockBus;
    transport.unlockBus = unlockBus;
    transport.setChipSelect = setChipSelect;
    transport.transfer = transfer;
    transport.nowMs = nowMs;
    transport.nowUs = nowUs;
    transport.sleepMs = sleepMs;
    transport.delayUs = delayUs;

    MAX31865BeginConfig begin = max31865DefaultBeginConfig();
    begin.transport = transport;
    begin.powerReadyDelayMs = 0U;
    begin.defaultOperationTimeoutMs = 1000U;
    begin.initialDeviceConfig.wireMode = MAX31865WireMode::ThreeWire;
    begin.initialDeviceConfig.filter = MAX31865Filter::Hz50;
    begin.initialDeviceConfig.biasEnabled = false;
    begin.initialDeviceConfig.thresholds = {8000U, 12000U};

    MAX31865 driver;
    MAX31865Status status = driver.begin(begin);
    if (!status.ok())
    {
        return failStatus("begin", status);
    }
    if (driver.state() != MAX31865State::Ready ||
        !driver.health().configurationKnown)
    {
        std::printf("FAIL: begin did not establish verified Ready state\n");
        return 1;
    }

    MAX31865Settings settings{};
    status = driver.readConfiguration(settings, 1000U);
    if (!status.ok())
    {
        return failStatus("readConfiguration(initial)", status);
    }
    if (!sameConfiguration(
            settings.deviceConfig,
            begin.initialDeviceConfig))
    {
        std::printf("FAIL: initial verified configuration mismatch\n");
        return 1;
    }

    MAX31865DeviceConfig desired = settings.deviceConfig;
    desired.wireMode = MAX31865WireMode::FourWire;
    desired.filter = MAX31865Filter::Hz60;
    desired.biasEnabled = true;
    desired.thresholds = {9000U, 14000U};
    status = driver.applyConfiguration(desired, 1000U);
    if (!status.ok())
    {
        return failStatus("applyConfiguration", status);
    }
    status = driver.readConfiguration(settings, 1000U);
    if (!status.ok())
    {
        return failStatus("readConfiguration(updated)", status);
    }
    if (!sameConfiguration(settings.deviceConfig, desired))
    {
        std::printf("FAIL: updated verified configuration mismatch\n");
        return 1;
    }

    MAX31865DeviceInfo info{};
    status = driver.probe(info);
    if (!status.ok())
    {
        return failStatus("probe", status);
    }
    if (!info.configurationMatches || device.busLocked)
    {
        std::printf("FAIL: probe or bus ownership invariant failed\n");
        return 1;
    }

    const size_t transfersBeforeEnd = device.transferCount;
    driver.end();
    if (driver.state() != MAX31865State::Uninitialized ||
        device.transferCount != transfersBeforeEnd || device.busLocked)
    {
        std::printf("FAIL: end was not a zero-I/O unbind\n");
        return 1;
    }

    std::printf("Clean packaged native lifecycle consumer passed\n");
    return 0;
}
'''


ARDUINO_CONSUMER = r'''#include <Arduino.h>
#include <SPI.h>

#include "MAX31865/ArduinoBackend.h"
#include "MAX31865/MAX31865.h"

MAX31865ArduinoBackend backend;
MAX31865 rtd;

void setup()
{
    MAX31865Pins pins{};
    pins.sck = 12;
    pins.miso = 13;
    pins.mosi = 11;
    pins.chipSelect = 10;
    pins.dataReady = 9;
    SPI.begin(
        pins.sck,
        pins.miso,
        pins.mosi,
        MAX31865_PIN_UNUSED);
    const SPISettings settings(1000000U, MSBFIRST, SPI_MODE1);
    MAX31865Status status = max31865ArduinoBackendBind(
        backend, SPI, settings, 1000000U, pins);
    if (status.ok())
    {
        status = max31865ArduinoConfigureControlPins(backend);
    }
    if (status.ok())
    {
        MAX31865BeginConfig config = max31865DefaultBeginConfig();
        config.transport = max31865ArduinoTransport(backend);
        status = rtd.begin(config);
    }
    (void)status;
}

void loop() {}
'''


ARDUINO_PLATFORMIO_INI = f"""[env]
platform = {PLATFORMIO_PLATFORM}
framework = arduino
lib_ldf_mode = deep+
build_flags = -std=c++17

[env:package_s2]
board = esp32-s2-saola-1

[env:package_s3]
board = esp32-s3-devkitc-1
"""


def fail(message: str) -> None:
    raise SystemExit(f"Packaged consumer check FAILED: {message}")


def safe_relative_path(raw: str) -> pathlib.PurePosixPath:
    if not raw or "\0" in raw or "\\" in raw:
        fail(f"unsafe empty, NUL, or backslash archive path: {raw!r}")
    if raw.startswith("/") or re.match(r"^[A-Za-z]:", raw):
        fail(f"absolute or drive-qualified archive path: {raw!r}")
    path = pathlib.PurePosixPath(raw)
    if path.is_absolute() or any(part in ("", ".", "..") for part in path.parts):
        fail(f"unsafe archive path: {raw!r}")
    return path


def read_manifest() -> tuple[str, ...]:
    try:
        text = MANIFEST.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"could not read package manifest: {exc}")
    entries = text.splitlines()
    if not entries or any(not entry for entry in entries):
        fail("manifest must not be empty or contain blank lines")
    if entries != sorted(entries):
        fail("manifest entries must be sorted")
    if len(entries) != len(set(entries)):
        fail("manifest entries must be unique")
    for entry in entries:
        if entry.lstrip().startswith("#"):
            fail(f"manifest entry must not be a comment: {entry!r}")
        if any(character in entry for character in "*?["):
            fail(f"manifest entry contains a glob: {entry}")
        if safe_relative_path(entry).as_posix() != entry:
            fail(f"manifest entry is not a normalized POSIX path: {entry}")
        candidate = ROOT / pathlib.Path(*pathlib.PurePosixPath(entry).parts)
        if candidate.is_symlink() or not candidate.is_file():
            fail(f"manifest entry is not a regular non-symlink file: {entry}")
    return tuple(entries)


def intended_source_files() -> set[str]:
    actual = set(INTENDED_ROOT_FILES)
    for relative in INTENDED_DIRECTORIES:
        directory = ROOT / relative
        if not directory.is_dir() or directory.is_symlink():
            fail(f"intended source directory is missing or a symlink: {relative}")
        for candidate in directory.rglob("*"):
            if candidate.is_symlink() or candidate.is_file():
                actual.add(candidate.relative_to(ROOT).as_posix())
    return actual


def platformio_command(*arguments: str) -> list[str]:
    if os.name == "nt":
        wrapper = ROOT / "scripts" / "pio.cmd"
        if not wrapper.is_file():
            fail(f"PlatformIO wrapper is missing: {wrapper}")
        return ["cmd.exe", "/d", "/c", str(wrapper), *arguments]
    return [sys.executable, "-m", "platformio", *arguments]


def require_platformio() -> None:
    try:
        result = subprocess.run(
            platformio_command("--version"),
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError as exc:
        fail(f"PlatformIO Core {PLATFORMIO_VERSION} is unavailable: {exc}")
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        fail(
            f"PlatformIO Core {PLATFORMIO_VERSION} is unavailable through the "
            f"required launcher: {detail}"
        )
    match = re.search(
        r"\bPlatformIO Core,\s*version\s+([^\s]+)",
        result.stdout,
    )
    if match is None:
        fail(f"could not parse PlatformIO version output: {result.stdout.strip()}")
    installed = match.group(1)
    if installed != PLATFORMIO_VERSION:
        fail(f"PlatformIO Core {installed} is installed; require {PLATFORMIO_VERSION}")

    platformio_ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    platform_assignments = re.findall(
        r"^[ \t]*platform\s*=\s*(\S+)\s*$",
        platformio_ini,
        flags=re.MULTILINE,
    )
    framework_assignments = re.findall(
        r"^[ \t]*framework\s*=\s*(\S+)\s*$",
        platformio_ini,
        flags=re.MULTILINE,
    )
    if platform_assignments != [PLATFORMIO_PLATFORM]:
        fail("platformio.ini must contain one shared exact pioarduino pin")
    if framework_assignments != ["arduino"]:
        fail("platformio.ini must contain one shared Arduino framework selection")


def inspect_and_extract(
    archive_path: pathlib.Path,
    extraction_root: pathlib.Path,
    payload: tuple[str, ...],
) -> None:
    regular_names: list[str] = []
    allowed_directories = {
        parent.as_posix()
        for entry in payload
        for parent in pathlib.PurePosixPath(entry).parents
        if parent.as_posix() != "."
    }
    member_names: set[str] = set()
    with tarfile.open(archive_path, mode="r:*") as archive:
        members = archive.getmembers()
        for member in members:
            raw = member.name.rstrip("/") if member.isdir() else member.name
            path = safe_relative_path(raw)
            normalized = path.as_posix()
            canonical_name = (
                f"{normalized}/"
                if member.isdir() and member.name.endswith("/")
                else normalized
            )
            if member.name != canonical_name:
                fail(
                    "archive member path is not canonical POSIX form: "
                    f"{member.name!r}"
                )
            if normalized in member_names:
                fail(f"archive contains a duplicate member path: {normalized}")
            member_names.add(normalized)
            if member.isfile():
                regular_names.append(normalized)
            elif member.isdir() and normalized not in allowed_directories:
                fail(f"archive contains an unexpected directory: {member.name}")
            elif not member.isdir():
                fail(f"archive contains a nonregular member: {member.name}")

            destination = (extraction_root / pathlib.Path(*path.parts)).resolve()
            try:
                destination.relative_to(extraction_root.resolve())
            except ValueError:
                fail(f"archive member escapes extraction root: {member.name}")

        if sorted(regular_names) != list(payload):
            missing = sorted(set(payload) - set(regular_names))
            extra = sorted(set(regular_names) - set(payload))
            fail(
                "archive payload differs from manifest; "
                f"missing={missing}, extra={extra}"
            )

        for member in members:
            raw = member.name.rstrip("/") if member.isdir() else member.name
            path = safe_relative_path(raw)
            destination = extraction_root / pathlib.Path(*path.parts)
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            source = archive.extractfile(member)
            if source is None:
                fail(f"could not read archive member: {member.name}")
            with source, destination.open("xb") as output:
                shutil.copyfileobj(source, output)


def verify_malicious_archives_are_rejected(temporary: pathlib.Path) -> None:
    """Prove unsafe members are rejected before any extraction occurs."""
    cases = (
        ("empty", "", tarfile.REGTYPE, "unsafe empty"),
        ("absolute", "/absolute", tarfile.REGTYPE, "absolute or drive-qualified"),
        (
            "drive-qualified",
            "C:/escape",
            tarfile.REGTYPE,
            "absolute or drive-qualified",
        ),
        (
            "backslash",
            "directory\\file",
            tarfile.REGTYPE,
            "unsafe empty, NUL, or backslash",
        ),
        ("dot-segment", "./file", tarfile.REGTYPE, "not canonical POSIX form"),
        (
            "double-separator",
            "directory//file",
            tarfile.REGTYPE,
            "not canonical POSIX form",
        ),
        ("traversal", "../escape", tarfile.REGTYPE, "unsafe archive path"),
        (
            "unexpected-directory",
            "unexpected/",
            tarfile.DIRTYPE,
            "unexpected directory",
        ),
        ("symlink", "link", tarfile.SYMTYPE, "nonregular member"),
        ("hardlink", "hardlink", tarfile.LNKTYPE, "nonregular member"),
        (
            "character-device",
            "character-device",
            tarfile.CHRTYPE,
            "nonregular member",
        ),
        ("block-device", "block-device", tarfile.BLKTYPE, "nonregular member"),
        ("fifo", "fifo", tarfile.FIFOTYPE, "nonregular member"),
    )
    payload = ("README.md",)
    try:
        safe_relative_path("nul\0member")
    except SystemExit as exc:
        if "unsafe empty, NUL, or backslash" not in str(exc):
            fail(f"NUL-member validation failed for the wrong reason: {exc}")
    else:
        fail("NUL-member validation was accepted")

    for case_name, member_name, member_type, expected_failure in cases:
        archive_path = temporary / f"malicious-{case_name}.tar"
        with tarfile.open(archive_path, mode="w") as archive:
            valid = tarfile.TarInfo(payload[0])
            valid_data = b"validated only; must never be extracted"
            valid.size = len(valid_data)
            archive.addfile(valid, io.BytesIO(valid_data))

            malicious = tarfile.TarInfo(member_name)
            malicious.type = member_type
            malicious.linkname = (
                "README.md"
                if member_type in (tarfile.SYMTYPE, tarfile.LNKTYPE)
                else ""
            )
            malicious_data = b"escape" if member_type == tarfile.REGTYPE else b""
            malicious.size = len(malicious_data)
            archive.addfile(
                malicious,
                io.BytesIO(malicious_data) if malicious_data else None,
            )

        extraction_root = temporary / f"malicious-{case_name}-extracted"
        extraction_root.mkdir()
        try:
            inspect_and_extract(archive_path, extraction_root, payload)
        except SystemExit as exc:
            if expected_failure not in str(exc):
                fail(
                    f"malicious {case_name} archive failed for the wrong reason: "
                    f"{exc}"
                )
        else:
            fail(f"malicious {case_name} archive was accepted")
        if any(extraction_root.iterdir()):
            fail(f"malicious {case_name} archive wrote data before rejection")
        if (extraction_root.parent / "escape").exists():
            fail("traversal archive wrote outside the extraction root")

    duplicate_archive = temporary / "malicious-duplicate.tar"
    with tarfile.open(duplicate_archive, mode="w") as archive:
        for contents in (b"first", b"second"):
            member = tarfile.TarInfo(payload[0])
            member.size = len(contents)
            archive.addfile(member, io.BytesIO(contents))
    duplicate_root = temporary / "malicious-duplicate-extracted"
    duplicate_root.mkdir()
    try:
        inspect_and_extract(duplicate_archive, duplicate_root, payload)
    except SystemExit as exc:
        if "duplicate member path" not in str(exc):
            fail(f"duplicate archive failed for the wrong reason: {exc}")
    else:
        fail("duplicate archive member was accepted")
    if any(duplicate_root.iterdir()):
        fail("duplicate archive wrote data before rejection")


def validate_component_contract(package: pathlib.Path) -> None:
    library = json.loads((package / "library.json").read_text(encoding="utf-8"))
    version = library.get("version")
    component = (package / "idf_component.yml").read_text(encoding="utf-8")
    required_description = f"description: {CORE_COMPONENT_DESCRIPTION}"
    if required_description not in component:
        fail("packaged component does not describe callback-supplied IDF readiness")
    component_version = re.search(
        r'^version:\s*["\']?([^"\'\s]+)',
        component,
        flags=re.MULTILINE,
    )
    if component_version is None or component_version.group(1) != version:
        fail("packaged component version is not synchronized")
    cmake = (package / "CMakeLists.txt").read_text(encoding="utf-8")
    if cmake != CORE_COMPONENT_CMAKE:
        fail("packaged CMakeLists.txt is not the exact core-only contract")
    for token in (
        "REQUIRES",
        "PRIV_REQUIRES",
        "src/platform",
        "ArduinoBackend",
        "IdfBackend",
        "examples/",
        "driver/gpio",
        "driver/spi",
        "freertos",
    ):
        if token in cmake or token in component:
            fail(f"packaged component metadata contains forbidden token {token}")
    exclusions = set(
        re.findall(
            r'^\s{4}-\s+"([^"]+)"\s*$',
            component,
            flags=re.MULTILINE,
        )
    )
    if exclusions != REQUIRED_COMPONENT_EXCLUSIONS:
        missing = sorted(REQUIRED_COMPONENT_EXCLUSIONS - exclusions)
        extra = sorted(exclusions - REQUIRED_COMPONENT_EXCLUSIONS)
        fail(f"component exclusions differ; missing={missing}, extra={extra}")


def validate_packaged_readme_links(package: pathlib.Path) -> None:
    readme = (package / "README.md").read_text(encoding="utf-8")
    for token in ('href="docs/', "](docs/"):
        if token in readme:
            fail(
                "packaged README contains a relative docs link although docs "
                f"are excluded: {token}"
            )
    required_prefix = (
        "https://github.com/janhavelka/MAX31865/blob/main/docs/"
    )
    if readme.count(required_prefix) < 9:
        fail("packaged README does not retain absolute repository guide links")


def compile_native_consumer(package: pathlib.Path, temporary: pathlib.Path) -> None:
    compiler = find_compiler()
    source = temporary / "native_consumer.cpp"
    executable = temporary / (
        "native_consumer.exe" if os.name == "nt" else "native_consumer"
    )
    source.write_text(NATIVE_CONSUMER, encoding="utf-8", newline="\n")
    command = [
        compiler,
        *STRICT_CXX_FLAGS,
        f"-I{package / 'include'}",
        f"-I{package / 'src'}",
        str(package / "src" / "MAX31865_Protocol.cpp"),
        str(package / "src" / "MAX31865.cpp"),
        str(source),
        "-o",
        str(executable),
    ]
    subprocess.run(command, cwd=temporary, check=True)
    subprocess.run([str(executable)], cwd=temporary, check=True)


def compile_arduino_project(
    project: pathlib.Path,
    temporary: pathlib.Path,
    failure_message: str,
) -> None:
    try:
        subprocess.run(
            platformio_command(
                "run",
                "-d",
                str(project),
                "-e",
                "package_s2",
                "-e",
                "package_s3",
            ),
            cwd=temporary,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        fail(f"{failure_message}: {exc}")


def compile_arduino_consumers(
    package: pathlib.Path,
    temporary: pathlib.Path,
) -> None:
    project = temporary / "arduino_consumer"
    (project / "src").mkdir(parents=True)
    (project / "lib").mkdir()
    shutil.copytree(package, project / "lib" / "MAX31865")
    (project / "src" / "main.cpp").write_text(
        ARDUINO_CONSUMER,
        encoding="utf-8",
        newline="\n",
    )
    (project / "platformio.ini").write_text(
        ARDUINO_PLATFORMIO_INI,
        encoding="utf-8",
        newline="\n",
    )
    compile_arduino_project(
        project,
        temporary,
        "clean Arduino consumers could not use the exact-pinned platform",
    )


def compile_installed_diagnostic_example(
    package: pathlib.Path,
    temporary: pathlib.Path,
) -> None:
    project = temporary / "diagnostic_example"
    shutil.copytree(package / "examples" / "06_diagnostic_cli", project / "src")
    (project / "lib").mkdir()
    shutil.copytree(package, project / "lib" / "MAX31865")
    (project / "platformio.ini").write_text(
        ARDUINO_PLATFORMIO_INI,
        encoding="utf-8",
        newline="\n",
    )
    compile_arduino_project(
        project,
        temporary,
        "the copied installed diagnostic example did not compile for S2/S3",
    )


def main() -> int:
    payload = PLATFORMIO_PAYLOAD
    if PLATFORMIO_ARCHIVE_PREFIX != "":
        fail("PlatformIO archive prefix must remain empty and root-relative")
    actual = intended_source_files()
    if actual != set(payload):
        missing = sorted(set(payload) - actual)
        extra = sorted(actual - set(payload))
        fail(
            "manifest does not exactly cover intended source groups; "
            f"missing={missing}, extra={extra}"
        )
    require_platformio()

    with tempfile.TemporaryDirectory(prefix="max31865-package-check-") as name:
        temporary = pathlib.Path(name)
        verify_malicious_archives_are_rejected(temporary)
        archive_path = temporary / "MAX31865.tar.gz"
        try:
            subprocess.run(
                platformio_command(
                    "package",
                    "pack",
                    str(ROOT),
                    "--output",
                    str(archive_path),
                ),
                cwd=temporary,
                check=True,
            )
        except (OSError, subprocess.CalledProcessError) as exc:
            fail(f"PlatformIO package creation failed: {exc}")
        extracted = temporary / "extracted"
        extracted.mkdir()
        inspect_and_extract(archive_path, extracted, payload)
        validate_component_contract(extracted)
        validate_packaged_readme_links(extracted)
        compile_native_consumer(extracted, temporary)
        compile_arduino_consumers(extracted, temporary)
        compile_installed_diagnostic_example(extracted, temporary)

    print("Packaged consumer check PASSED")
    return 0


PLATFORMIO_PAYLOAD = read_manifest()


if __name__ == "__main__":
    raise SystemExit(main())
