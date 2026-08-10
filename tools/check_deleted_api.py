#!/usr/bin/env python3
"""Reject removed bool/error-side-channel APIs and legacy integration glue."""

from __future__ import annotations

import pathlib
import re


ROOT = pathlib.Path(__file__).resolve().parents[1]
CODE_SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".ino", ".py"}
DOC_SUFFIXES = {".md", ".rst", ".txt"}
ALLOW_PREFIXES = (
    "docs/vendor-reference-code/",
    "docs/pdf-extracted-md/",
    "docs/extracted-md/",
)
ALLOW_FILES = {
    "CHANGELOG.md",
    "test/public_api_contract_tests.cpp",
    "tools/check_deleted_api.py",
}
REMOVED_PATHS = (
    "examples/common/BuildConfig.h",
    "examples/common/BusDiag.h",
    "examples/common/CliShell.h",
    "examples/common/CliStyle.h",
    "examples/common/HealthView.h",
    "examples/common/Log.h",
    "examples/common/TransportAdapter.h",
)

TEXT_PATTERNS = {
    "legacy raw RTD type": r"\bMAX31865RawRtd\b",
    "legacy default SPI macro": r"\bMAX31865_DEFAULT_SPI_HZ\b",
    "legacy offline macro": r"\bMAX31865_DEFAULT_OFFLINE_THRESHOLD\b",
    "legacy lock-timeout macro": r"\bMAX31865_SPI_LOCK_TIMEOUT_MS\b",
    "legacy settings status API": r"\bgetSettingsStatus\s*\(",
    "legacy local RTD mutator": r"\bsetRtdParameters\s*\(",
    "legacy auto-convert API": r"\bsetAutoConvert\s*\(",
    "legacy readiness aliases": r"\b(?:isDataReady|readIfReady)\s*\(",
    "legacy automatic fault-cycle name": r"\brunAutoFaultDetection\s*\(",
    "legacy register names":
        r"\b(?:readReg|readRegs|writeReg|writeRegVerify)\s*\(",
    "legacy default restore name": r"\bresetRegisters\s*\(",
    "legacy conversion timing getters":
        r"\b(?:getSingleConversionTimeMs|getContinuousConversionTimeMs|getBiasSettleTimeUs)\s*\(",
    "legacy scalar health/error getter":
        r"\b(?:driverState|healthState|isInitialized|isOnline|lastError|lastErrorName|lastOkMs|lastErrorMs|consecutiveFailures|totalFailures|totalSuccess|spiLockTimeoutMs|offlineThreshold)\s*\(",
    "legacy health mutation":
        r"\b(?:setSpiLockTimeoutMs|clearHealthCounters)\s*\(",
    "legacy sample/counter getter":
        r"\b(?:droppedCount|overrunCount|totalReadCount|keptSampleCount)\s*\(",
    "legacy direct measurement helper":
        r"\b(?:readRawRtd|readResistance|readTemperature)\s*\(",
    "legacy cached configuration getter":
        r"\b(?:spiHz|wireMode|filter|biasEnabled|autoConvertEnabled|referenceResistorOhms|rtdNominalOhms|inputFilterTimeConstantUs|rtdCoefficients)\s*\(",
    "legacy status helper": r"\b(?:isOk|inProgress)\s*\(",
    "legacy example common header":
        r"\b(?:BuildConfig|BusDiag|CliShell|CliStyle|HealthView|TransportAdapter)\.h\b",
}

CODE_PATTERNS = {
    "legacy lifecycle state":
        r"MAX31865State::(?:Configuring|Recovering)",
    "legacy error code":
        r"MAX31865Error::(?:InvalidConfig|ResourceAllocationFailed|SpiLockTimeout|DeviceNotFound|ConversionNotReady|Timeout|FaultPresent)",
    "legacy begin member": r"\.(?:spiHz|verifyProbe)\b",
    "legacy Arduino pin member": r"\.(?:cs|drdy)\b",
    "legacy core SPI storage": r"\b_(?:spiHz|spiMutex|spiLockTimeoutMs)\b",
}

CPP_COMMENT_OR_LITERAL = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
    re.DOTALL,
)


def allowed(relative: str) -> bool:
    return relative in ALLOW_FILES or relative.startswith(ALLOW_PREFIXES)


def candidates() -> list[pathlib.Path]:
    result: list[pathlib.Path] = []
    for root_name in ("include", "src", "examples", "test", "tools", "docs"):
        root = ROOT / root_name
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in (
                CODE_SUFFIXES | DOC_SUFFIXES
            ):
                result.append(path)
    for name in ("README.md", "CONTRIBUTING.md", "SECURITY.md"):
        path = ROOT / name
        if path.is_file():
            result.append(path)
    return sorted(result)


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def check_public_class_has_no_bool_fallible_api(
    failures: list[str],
) -> None:
    header = ROOT / "include" / "MAX31865" / "MAX31865.h"
    text = CPP_COMMENT_OR_LITERAL.sub(
        " ", header.read_text(encoding="utf-8")
    )
    match = re.search(
        r"class\s+MAX31865\s+final\s*\{(?P<body>.*?)\bprivate\s*:",
        text,
        re.DOTALL,
    )
    if match is None:
        failures.append("include/MAX31865/MAX31865.h: public class not parsed")
        return
    bool_api = re.search(r"\bbool\s+[A-Za-z_]\w*\s*\(", match.group("body"))
    if bool_api is not None:
        absolute = match.start("body") + bool_api.start()
        failures.append(
            "include/MAX31865/MAX31865.h:"
            f"{line_number(text, absolute)}: fallible bool API: "
            f"{bool_api.group(0)}"
        )
    for signature in (
        r"\bbegin\s*\(\s*SPIClass",
        r"\bMAX31865Error\s+lastError\s*\(",
        r"\bconst\s+char\s*\*\s*lastErrorName\s*\(",
    ):
        legacy = re.search(signature, match.group("body"))
        if legacy is not None:
            absolute = match.start("body") + legacy.start()
            failures.append(
                "include/MAX31865/MAX31865.h:"
                f"{line_number(text, absolute)}: deleted public signature: "
                f"{legacy.group(0)}"
            )


def main() -> int:
    failures: list[str] = []
    for relative in REMOVED_PATHS:
        if (ROOT / relative).exists():
            failures.append(f"{relative}: removed compatibility file reappeared")

    text_patterns = {
        label: re.compile(pattern)
        for label, pattern in TEXT_PATTERNS.items()
    }
    code_patterns = {
        label: re.compile(pattern)
        for label, pattern in CODE_PATTERNS.items()
    }
    for path in candidates():
        relative = path.relative_to(ROOT).as_posix()
        if allowed(relative):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for label, pattern in text_patterns.items():
            match = pattern.search(text)
            if match is not None:
                failures.append(
                    f"{relative}:{line_number(text, match.start())}: "
                    f"{label}: {match.group(0)}"
                )
        if path.suffix.lower() in CODE_SUFFIXES:
            executable = CPP_COMMENT_OR_LITERAL.sub(" ", text)
            for label, pattern in code_patterns.items():
                match = pattern.search(executable)
                if match is not None:
                    failures.append(
                        f"{relative}:{line_number(executable, match.start())}: "
                        f"{label}: {match.group(0)}"
                    )

    check_public_class_has_no_bool_fallible_api(failures)
    if failures:
        print("Deleted API check FAILED")
        for failure in failures:
            print(f" - {failure}")
        return 1
    print("Deleted API check PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
