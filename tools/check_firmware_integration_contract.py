#!/usr/bin/env python3
"""Enforce the consumer-neutral firmware-owner integration boundary."""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "test" / "firmware_owner_fixture.cpp"
CPP_COMMENT_OR_LITERAL = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
    re.DOTALL,
)


def fail(message: str) -> None:
    raise RuntimeError(message)


def executable_cpp(text: str) -> str:
    return CPP_COMMENT_OR_LITERAL.sub(" ", text)


def core_files() -> list[pathlib.Path]:
    suffixes = {".c", ".cc", ".cpp", ".h", ".hpp"}
    files = [
        path
        for root in (ROOT / "include" / "MAX31865", ROOT / "src")
        for path in root.rglob("*")
        if path.is_file() and path.suffix in suffixes
    ]
    return sorted(files)


def check_core_is_consumer_neutral() -> None:
    forbidden = (
        "FirmwareRtdSample",
        "FirmwareRtdOwner",
        "FirmwareRtdInterface",
        "FirmwareRtdEvent",
        "FirmwareRequestDeadline",
        "FirmwareSamplePublisher",
        "FirmwareInstanceGuard",
        "FirmwareTransportHarness",
        "FirmwareLockState",
    )
    for path in core_files():
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(ROOT)
        for token in forbidden:
            if token in text:
                fail(f"{relative} leaks consumer symbol {token!r}")
        implemented = executable_cpp(text)
        for token in (
            "std::mutex",
            "std::timed_mutex",
            "std::thread",
            "xTaskCreate",
            "QueueHandle_t",
        ):
            if token in implemented:
                fail(f"{relative} owns forbidden scheduler/lock {token!r}")


def check_fixture_contract() -> None:
    if not FIXTURE.is_file():
        fail("test/firmware_owner_fixture.cpp is missing")
    text = FIXTURE.read_text(encoding="utf-8")
    implemented = executable_cpp(text)
    required = (
        "struct FirmwareRtdSample",
        "uint16_t rawCode;",
        "uint32_t sequence;",
        "uint32_t timestampUs;",
        "float resistanceOhms;",
        "float temperatureC;",
        "uint8_t channelId;",
        "uint8_t qualityFlags;",
        "std::is_standard_layout<FirmwareRtdSample>",
        "std::is_trivially_copyable<FirmwareRtdSample>",
        "sizeof(FirmwareRtdSample) <= 32U",
        "FIRMWARE_OWNER_CALL_BUDGET_MS = 20U",
        "FIRMWARE_OWNER_NATIVE_TOLERANCE_MS = 50U",
        "std::timed_mutex instanceMutex",
        "std::timed_mutex busMutex",
        "try_lock_for",
        "class FirmwareRtdInterface final",
        "owner.applicationInterface()",
        "DedicatedHost",
        "SharedHostApplicationArbiter",
        "sharedHostMultipleContextsAreAdmissible(false)",
        "InstanceLock",
        "BusLock",
        "exactDirectOwnerCallOrder",
        "exactSharedArbiterCallOrder",
        "remainingMs",
        "clampTimeout",
        "RequestExpired",
        "publisher.publish",
        "RecoveryNotAuthorized",
        "recoverIfAuthorized",
        "_device.recover(timeoutMs)",
        "NoData",
        "DegradedHealth",
        "OfflineHealth",
        "FaultState",
    )
    for token in required:
        if token not in implemented:
            fail(f"firmware owner fixture is missing {token!r}")

    dto = re.search(
        r"struct\s+FirmwareRtdSample\s*\{(?P<body>.*?)\};",
        implemented,
        re.DOTALL,
    )
    if dto is None:
        fail("FirmwareRtdSample could not be parsed")
    fields = [
        line.strip()
        for line in dto.group("body").splitlines()
        if line.strip()
    ]
    expected = [
        "uint16_t rawCode;",
        "uint32_t sequence;",
        "uint32_t timestampUs;",
        "float resistanceOhms;",
        "float temperatureC;",
        "uint8_t channelId;",
        "uint8_t qualityFlags;",
    ]
    if fields != expected:
        fail(f"FirmwareRtdSample fields changed: {fields!r}")

    interface = re.search(
        r"class\s+FirmwareRtdInterface\s+final\s*\{(?P<body>.*?)\n\};",
        implemented,
        re.DOTALL,
    )
    if interface is None:
        fail("FirmwareRtdInterface could not be parsed")
    if "MAX31865" in interface.group("body"):
        fail("application-facing firmware interface leaks a driver type")

    for token in (
        "xTaskCreate",
        "QueueHandle_t",
        "attachInterrupt",
        "IRAM_ATTR",
    ):
        if token in implemented:
            fail(f"fixture contains forbidden hidden async mechanism {token!r}")

    main = re.search(
        r"int\s+main\s*\(\s*\)\s*\{(?P<body>.*?)\n\}",
        implemented,
        re.DOTALL,
    )
    if main is None:
        fail("firmware owner fixture main could not be parsed")
    for test_name in (
        "dtoDedicatedAndPublicationBoundary",
        "sharedArbiterDeadlineAndHealthBoundary",
        "outerDeadlineAndPublicationSuppressionBoundary",
        "instanceFaultAndExplicitRecoveryBoundary",
    ):
        if test_name not in main.group("body"):
            fail(f"firmware owner fixture does not execute {test_name}")


def check_documented_boundaries() -> None:
    required = {
        "docs/ARCHITECTURE.md": (
            "compiled firmware-owner fixture",
            "outer request deadline",
            "publication suppression",
            "explicit recovery authorization",
        ),
        "docs/TESTING.md": (
            "run_firmware_owner_fixture.py",
            "check_firmware_integration_contract.py",
            "FIRMWARE_OWNER_CALL_BUDGET_MS",
        ),
        "docs/repository-contract.md": (
            "consumer-neutral firmware-owner fixture",
            "FirmwareRtdSample",
            "application-owned",
        ),
    }
    for relative, tokens in required.items():
        text = (ROOT / relative).read_text(encoding="utf-8")
        normalized = " ".join(text.split())
        for token in tokens:
            if token not in normalized:
                fail(f"{relative} lacks contract phrase {token!r}")


def main() -> int:
    try:
        check_core_is_consumer_neutral()
        check_fixture_contract()
        check_documented_boundaries()
    except RuntimeError as error:
        print(
            f"firmware integration contract check failed: {error}",
            file=sys.stderr,
        )
        return 1
    print("Firmware integration contract check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
