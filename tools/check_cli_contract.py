#!/usr/bin/env python3
"""Check the five focused examples and fixed-storage diagnostic CLI."""

from __future__ import annotations

import pathlib
import re


ROOT = pathlib.Path(__file__).resolve().parents[1]
COMMON = ROOT / "examples" / "common"
CLI = ROOT / "examples" / "06_diagnostic_cli"
OLD_PATHS = (
    ROOT / "examples" / "01_basic_bringup_cli",
    ROOT / "examples" / "02_api_smoke",
)

EXAMPLE_REQUIREMENTS = {
    "01_basic_bringup": (
        "rtd.begin(config)",
        "rtd.probe(info)",
        "rtd.readConfiguration(",
    ),
    "02_continuous_sampling": (
        "rtd.startContinuous(",
        "rtd.poll(sample)",
        "MAX31865Error::NoData",
    ),
    "03_one_shot": ("rtd.readOneShot(",),
    "04_fault_diagnostics": (
        "rtd.setFaultThresholdsTemperature(",
        "rtd.runManualFaultDetection(",
        "rtd.runAutomaticFaultDetection(",
    ),
    "05_rtd_configuration": (
        "max31865Iec60751Coefficients()",
        "rtd.temperatureToResistance(",
        "rtd.temperatureToCode(",
        "rtd.readOneShot(",
    ),
}

CLI_FILES = (
    "DiagnosticConfig.h",
    "Max31865Cli.cpp",
    "Max31865Cli.h",
    "Max31865CliShell.h",
    "main.cpp",
    "README.md",
)

EXPECTED_COMMANDS = {
    "help",
    "format",
    "version",
    "wiring",
    "timeout",
    "telemetry",
    "state",
    "health",
    "counters",
    "begin",
    "end",
    "recover",
    "probe",
    "settings",
    "offline",
    "bias",
    "wire",
    "filter",
    "start",
    "trigger",
    "stop",
    "ready",
    "poll",
    "read",
    "wait",
    "one",
    "stream",
    "stress",
    "job",
    "cancel",
    "faults",
    "fault-clear",
    "fault-auto",
    "fault-manual",
    "thresholds",
    "rtd",
    "cvd",
    "reg",
    "convert",
}

CLI_REQUIRED_CALLS = (
    "device->begin(",
    "device->end(",
    "device->recover(",
    "device->probe(",
    "device->state(",
    "device->health(",
    "device->clearLifetimeCounters(",
    "device->setOfflineThreshold(",
    "device->readConfiguration(",
    "device->setBias(",
    "device->setWireMode(",
    "device->setFilter(",
    "device->startContinuous(",
    "device->triggerSingleConversion(",
    "device->stop(",
    "device->dataReady(",
    "device->poll(",
    "device->readSample(",
    "device->readSingle(",
    "device->readOneShot(",
    "device->readFaultStatus(",
    "device->clearFaults(",
    "device->runAutomaticFaultDetection(",
    "device->runManualFaultDetection(",
    "device->setFaultThresholdsRaw(",
    "device->readFaultThresholdsRaw(",
    "device->setFaultThresholdsResistance(",
    "device->readFaultThresholdsResistance(",
    "device->setFaultThresholdsTemperature(",
    "device->readFaultThresholdsTemperature(",
    "device->setRtdConfig(",
    "device->rtdConfig(",
    "device->readRegister(",
    "device->readRegisters(",
    "device->writeRegister(",
    "device->writeRegisterVerified(",
    "device->dumpRegisters(",
    "device->restoreWritableDefaults(",
    "device->registerReadbackTest(",
    "device->codeToResistance(",
    "device->resistanceToCode(",
    "device->resistanceToTemperature(",
    "device->temperatureToResistance(",
    "device->temperatureToCode(",
    "device->biasSettleTimeUs(",
)

FORBIDDEN = (
    r"\bString\b",
    r"\bstd::(?:string|vector|deque|list|map|unordered_map|function)\b",
    r"\bnew\s+[A-Za-z_:]",
    r"\bdelete\b",
    r"\b(?:malloc|calloc|realloc|free)\s*\(",
    r"\b(?:xTaskCreate|vTask|QueueHandle_t|SemaphoreHandle_t|portMUX_TYPE)\b",
    r"#\s*include\s*[<\"](?:freertos|src)/",
    r"\battachInterrupt\s*\(",
)
CPP_COMMENT_OR_LITERAL = re.compile(
    r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
    re.DOTALL,
)


def fail(message: str) -> None:
    raise SystemExit(f"CLI contract FAILED: {message}")


def require_file(path: pathlib.Path) -> None:
    if not path.is_file():
        fail(f"required file is missing: {path.relative_to(ROOT).as_posix()}")


def one_cpp(directory: pathlib.Path) -> pathlib.Path:
    candidates = sorted(directory.rglob("*.cpp"))
    if len(candidates) != 1:
        fail(
            f"expected one translation unit in {directory.relative_to(ROOT).as_posix()}, "
            f"found {len(candidates)}"
        )
    return candidates[0]


def check_forbidden(path: pathlib.Path, text: str) -> None:
    for pattern in FORBIDDEN:
        if re.search(pattern, text):
            fail(f"forbidden pattern {pattern!r} in {path.relative_to(ROOT).as_posix()}")


def main() -> int:
    require_file(COMMON / "BoardConfig.h")
    require_file(COMMON / "ExampleStatus.h")
    board_config = (COMMON / "BoardConfig.h").read_text(
        encoding="utf-8", errors="strict"
    )
    for token in ("MAX31865WireMode WIRE_MODE", "MAX31865Filter FILTER"):
        if token not in board_config:
            fail(f"BoardConfig.h is missing explicit {token}")
    for old_path in OLD_PATHS:
        if old_path.is_file() or (
            old_path.is_dir() and any(old_path.rglob("*"))
        ):
            fail(f"legacy example remains: {old_path.relative_to(ROOT).as_posix()}")

    for name, requirements in EXAMPLE_REQUIREMENTS.items():
        path = one_cpp(ROOT / "examples" / name)
        text = path.read_text(encoding="utf-8", errors="strict")
        check_forbidden(path, text)
        for token in requirements:
            if token not in text:
                fail(f"{path.relative_to(ROOT).as_posix()} is missing {token}")
        for token in (
            "config.initialDeviceConfig.wireMode = board::WIRE_MODE",
            "config.initialDeviceConfig.filter = board::FILTER",
        ):
            if token not in text:
                fail(f"{path.relative_to(ROOT).as_posix()} is missing {token}")

    for filename in CLI_FILES:
        require_file(CLI / filename)
    cli_cpp = (CLI / "Max31865Cli.cpp").read_text(encoding="utf-8", errors="strict")
    cli_header = (CLI / "Max31865Cli.h").read_text(encoding="utf-8", errors="strict")
    shell = (CLI / "Max31865CliShell.h").read_text(encoding="utf-8", errors="strict")
    main_cpp = (CLI / "main.cpp").read_text(encoding="utf-8", errors="strict")
    diagnostic_config = (CLI / "DiagnosticConfig.h").read_text(
        encoding="utf-8", errors="strict"
    )
    combined = "\n".join(
        (cli_cpp, cli_header, shell, main_cpp, diagnostic_config)
    )
    for path, text in (
        (CLI / "Max31865Cli.cpp", cli_cpp),
        (CLI / "Max31865Cli.h", cli_header),
        (CLI / "Max31865CliShell.h", shell),
        (CLI / "main.cpp", main_cpp),
        (CLI / "DiagnosticConfig.h", diagnostic_config),
    ):
        check_forbidden(path, text)

    for token in (
        "MAX31865WireMode WIRE_MODE",
        "MAX31865Filter FILTER",
        "diagnostic_config::WIRE_MODE",
        "diagnostic_config::FILTER",
        "spi_clock_hz=%lu",
        "wire=%u",
        "filter_hz=%u",
    ):
        if token not in combined:
            fail(f"diagnostic configuration/evidence is missing {token}")

    command_block = re.search(
        r"COMMANDS\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\};",
        cli_cpp,
        flags=re.DOTALL,
    )
    if command_block is None:
        fail("Max31865Cli.cpp has no fixed COMMANDS catalog")
    commands = set(
        re.findall(r'^\s*\{\s*"([^"]+)"\s*,', command_block.group("body"), re.MULTILINE)
    )
    if commands != EXPECTED_COMMANDS:
        missing = sorted(EXPECTED_COMMANDS - commands)
        extra = sorted(commands - EXPECTED_COMMANDS)
        fail(f"CLI command catalog differs; missing={missing}, extra={extra}")

    for call in CLI_REQUIRED_CALLS:
        if call not in cli_cpp:
            fail(f"diagnostic CLI does not expose public operation {call}")
    for token in (
        "CommandSafety::ConfirmMutation",
        '"confirm"',
        "ExecutionKind::CliJob",
        "REC ",
        "REC result",
        "COMMAND_BEGIN",
        "COMMAND_END",
        "JOB_BEGIN",
        "JOB_END",
        "TransportObserver",
        "downstream",
    ):
        if token not in combined:
            fail(f"diagnostic CLI is missing contract token {token}")
    if "void service()" not in cli_cpp:
        fail("diagnostic CLI has no cooperative service() implementation")
    if "MAX_BYTES_PER_POLL = 32U" not in shell:
        fail("bounded serial shell is not limited to 32 bytes per poll")
    if not re.search(r"char\s+_input\s*\[\s*\d+\s*\]", shell):
        fail("bounded serial shell has no fixed input buffer")
    cli_code = CPP_COMMENT_OR_LITERAL.sub("", cli_cpp + cli_header)
    if re.search(r"\bMAX31865\s+[A-Za-z_]", cli_code):
        fail("CLI implementation owns a driver instead of borrowing Context::device")
    if "MAX31865 *device;" not in cli_header:
        fail("CLI Context does not expose the borrowed driver pointer")

    for token in (
        "MAX31865ArduinoBackend",
        "MAX31865 device",
        "SPI.begin(",
        "max31865ArduinoBackendBind(",
        "max31865ArduinoConfigureControlPins(",
        "observeTransport(",
        "max31865_cli::service()",
    ):
        if token not in main_cpp:
            fail(f"diagnostic main does not own/wire required integration token {token}")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
