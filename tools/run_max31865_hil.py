#!/usr/bin/env python3
"""Capture bounded serial evidence from the MAX31865 diagnostic CLI.

The runner does not flash firmware, choose wiring, or apply a physical
stimulus. Its default plan is read-only. Commands that start conversions,
acknowledge DRDY through a full dump, or run fault cycles require explicit
opt-in flags. A dry run is only a plan and can never become HIL evidence.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import importlib.metadata
import json
import pathlib
import platform
import re
import shlex
import subprocess
import time
from collections.abc import Iterable
from typing import TextIO


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / ".pio" / "hil_logs"
INSTALL_HINT = "python -m pip install pyserial"
MAX_COMMAND_BYTES = 191
MAX_SERIAL_LINE_BYTES = 4096
CLAIM_BOUNDARY = (
    "A passing serial capture is an evidence input, not proof of RTD "
    "accuracy, wiring correctness, fault coverage, timing compliance, "
    "field readiness, or accepted HIL. Review the fixture, stimulus, logic-"
    "analyzer traces, references, and raw transcript independently."
)
ANSI_RE = re.compile(
    r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))"
)


@dataclasses.dataclass(frozen=True)
class CommandSpec:
    command: str
    purpose: str
    group: str = "safe-default"
    timeout_s: float = 8.0
    await_job: bool = False


@dataclasses.dataclass(frozen=True)
class Record:
    kind: str
    fields: dict[str, str]


@dataclasses.dataclass
class CommandCapture:
    command: str
    purpose: str
    group: str
    command_id: int | None
    started: bool
    ended: bool
    job_started: bool
    job_ended: bool
    lines: list[str]
    result: str
    reason: str
    elapsed_s: float

    def as_dict(self) -> dict[str, object]:
        return dataclasses.asdict(self)


SAFE_DEFAULT_PLAN = (
    CommandSpec("format machine", "Select the stable machine record format."),
    CommandSpec("version", "Capture library and firmware provenance."),
    CommandSpec("wiring", "Capture application-owned pins and SPI limit."),
    CommandSpec("state", "Capture initial lifecycle and passive health."),
    CommandSpec("health", "Capture initial counters and validity."),
    CommandSpec(
        "probe",
        "Run the read-only, health-neutral desired-image consistency probe.",
    ),
    CommandSpec(
        "settings",
        "Read typed configuration without acknowledging the RTD registers.",
    ),
    CommandSpec("faults", "Read and decode the latched fault register."),
)


class ProtocolError(RuntimeError):
    """The CLI violated its documented machine-record contract."""


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text).replace("\r", "")


def parse_record(line: str) -> Record | None:
    """Parse one boundary or REC record, including quoted field values."""
    clean = strip_ansi(line).strip()
    if not clean:
        return None
    try:
        tokens = shlex.split(clean)
    except ValueError:
        return None
    if not tokens:
        return None
    if tokens[0] == "REC":
        if len(tokens) < 2:
            return None
        kind = f"REC {tokens[1]}"
        field_tokens = tokens[2:]
    elif tokens[0] in {
        "COMMAND_BEGIN",
        "COMMAND_END",
        "JOB_BEGIN",
        "JOB_END",
    }:
        kind = tokens[0]
        field_tokens = tokens[1:]
    else:
        return None
    fields: dict[str, str] = {}
    for token in field_tokens:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        normalized_key = key.lower()
        if normalized_key in fields:
            raise ProtocolError(
                f"duplicate field {normalized_key} in {kind}"
            )
        fields[normalized_key] = value
    return Record(kind, fields)


def _required_integer(record: Record, field: str) -> int:
    value = record.fields.get(field)
    if value is None:
        raise ProtocolError(f"{record.kind} lacks required {field}")
    try:
        return int(value, 10)
    except ValueError as exc:
        raise ProtocolError(
            f"non-numeric {field} in {record.kind}"
        ) from exc


def _validate_outcome(record: Record) -> None:
    if record.kind in {"COMMAND_END", "REC result"}:
        ok = _required_integer(record, "ok")
        code = _required_integer(record, "code")
        if ok not in {0, 1} or (ok == 1) != (code == 0):
            raise ProtocolError(
                f"inconsistent ok/code in {record.kind}"
            )
        required = ("name", "detail")
        if record.kind == "COMMAND_END":
            required = (*required, "elapsed_ms")
        if record.kind == "REC result":
            required = (*required, "id", "op")
        _require_fields(record, required)
    elif record.kind == "JOB_END":
        ok = _required_integer(record, "ok")
        failures = _required_integer(record, "failures")
        if ok not in {0, 1} or failures < 0 or (
            ok == 1 and failures != 0
        ):
            raise ProtocolError("inconsistent ok/failures in JOB_END")
        if "reason" not in record.fields or "kind" not in record.fields:
            raise ProtocolError("JOB_END lacks kind/reason")


def _records(lines: Iterable[str]) -> list[Record]:
    return [
        record
        for line in lines
        if (record := parse_record(line)) is not None
    ]


def _records_of_kind(lines: Iterable[str], kind: str) -> list[Record]:
    return [record for record in _records(lines) if record.kind == kind]


def _require_fields(record: Record, fields: Iterable[str]) -> None:
    for field in fields:
        if field not in record.fields:
            raise ProtocolError(f"{record.kind} lacks required {field}")


def _single_record(lines: Iterable[str], kind: str) -> Record:
    matches = _records_of_kind(lines, kind)
    if len(matches) != 1:
        raise ProtocolError(
            f"expected exactly one {kind} record, observed {len(matches)}"
        )
    return matches[0]


def _validate_required_evidence(
    spec: CommandSpec, lines: Iterable[str]
) -> None:
    tokens = spec.command.split()
    if not tokens:
        raise ProtocolError("empty command has no evidence contract")
    command = tokens[0]
    materialized = list(lines)
    results = _records_of_kind(materialized, "REC result")
    if len(results) != 1:
        raise ProtocolError(
            "a successful command must report exactly one REC result"
        )
    if command == "version":
        record = _single_record(materialized, "REC version")
        _require_fields(
            record,
            (
                "library",
                "code",
                "git_commit",
                "git_status",
                "target",
                "framework",
                "build",
                "evidence",
            ),
        )
        if record.fields["evidence"] != "software_only_unvalidated_hardware":
            raise ProtocolError("version did not preserve the evidence boundary")
    elif command == "wiring":
        record = _single_record(materialized, "REC wiring")
        _require_fields(
            record,
            (
                "sck",
                "miso",
                "mosi",
                "cs",
                "drdy",
                "drdy_enabled",
                "spi_clock_hz",
                "spi_max_hz",
                "wire",
                "filter_hz",
                "ownership",
            ),
        )
        selected_clock = _required_integer(record, "spi_clock_hz")
        maximum_clock = _required_integer(record, "spi_max_hz")
        if maximum_clock != 5_000_000:
            raise ProtocolError("wiring did not report the 5 MHz device limit")
        if selected_clock <= 0 or selected_clock > maximum_clock:
            raise ProtocolError("wiring reported an invalid selected SPI clock")
        if _required_integer(record, "wire") not in {2, 3, 4}:
            raise ProtocolError("wiring reported an invalid RTD wire mode")
        if _required_integer(record, "filter_hz") not in {50, 60}:
            raise ProtocolError("wiring reported an invalid notch filter")
        if record.fields["ownership"] != "application":
            raise ProtocolError("wiring did not report application ownership")
    elif command == "state":
        record = _single_record(materialized, "REC state")
        _require_fields(
            record,
            (
                "lifecycle",
                "driver",
                "config_known",
                "job",
                "job_active",
                "last_code",
                "last_name",
            ),
        )
    elif command == "health":
        health = _single_record(materialized, "REC health")
        counts = _single_record(materialized, "REC health_counts")
        io = _single_record(materialized, "REC health_io")
        _require_fields(
            health,
            (
                "state",
                "driver",
                "online",
                "config_known",
                "consecutive",
                "offline_threshold",
                "last_code",
                "last_name",
                "last_detail",
            ),
        )
        _require_fields(
            counts,
            (
                "tracked_ok",
                "tracked_fail",
                "frame_attempt",
                "frame_ok",
                "frame_fail",
                "no_data",
                "dropped",
                "overruns",
                "faults",
            ),
        )
        _require_fields(
            io,
            (
                "lock_timeout",
                "lock_fail",
                "transfer_fail",
                "cs_fail",
                "gpio_fail",
                "drdy_timeout",
                "op_timeout",
            ),
        )
    elif command == "probe":
        record = _single_record(materialized, "REC probe")
        _require_fields(
            record,
            (
                "config",
                "high_register",
                "low_register",
                "fault",
                "matches",
            ),
        )
        if record.fields["matches"] != "1":
            raise ProtocolError("probe did not report matches=1")
    elif command == "settings":
        settings = _single_record(materialized, "REC settings")
        thresholds = _single_record(
            materialized, "REC settings_thresholds"
        )
        _require_fields(
            settings,
            (
                "raw_config",
                "wire",
                "filter_hz",
                "bias",
                "conversion",
                "one_shot",
                "fault_cycle",
                "low_code",
                "high_code",
            ),
        )
        _require_fields(
            thresholds,
            (
                "low_ohms",
                "high_ohms",
                "low_c_valid",
                "high_c_valid",
            ),
        )
    elif command in {"faults", "fault-auto", "fault-manual"}:
        record = _single_record(materialized, "REC fault")
        _require_fields(
            record,
            (
                "raw",
                "high",
                "low",
                "refin_high",
                "refin_low",
                "rtdin_low",
                "over_under_voltage",
            ),
        )
    elif command == "one":
        record = _single_record(materialized, "REC sample")
        _require_fields(
            record,
            (
                "source",
                "count",
                "raw_reg",
                "code",
                "resistance_ohms",
                "temperature_c",
                "flags",
            ),
        )
        if record.fields["source"] != "one":
            raise ProtocolError("one-shot sample has the wrong source")
    elif command == "reg" and tokens[1:] == ["dump"]:
        registers = _records_of_kind(materialized, "REC register")
        if len(registers) != 8:
            raise ProtocolError(
                "register dump did not report exactly eight registers"
            )
        addresses = {record.fields.get("address") for record in registers}
        expected = {f"0x{address:02X}" for address in range(8)}
        if addresses != expected:
            raise ProtocolError("register dump addresses are incomplete")
        warning = _single_record(materialized, "REC warning")
        if warning.fields.get("kind") != "register_dump_acknowledges_drdy":
            raise ProtocolError("register dump lacks its DRDY side-effect record")
    elif command in {"stress", "stream"}:
        job_end = _single_record(materialized, "REC job_end")
        _require_fields(
            job_end,
            (
                "kind",
                "reason",
                "target",
                "attempt_limit",
                "attempts",
                "successes",
                "failures",
                "no_data",
                "cleanup_code",
                "cleanup_name",
            ),
        )
        if job_end.fields["kind"] != command:
            raise ProtocolError("job summary kind does not match command")


def classify_capture(
    *,
    ended: bool,
    job_ended: bool,
    await_job: bool,
    lines: Iterable[str],
) -> tuple[str, str]:
    if not ended:
        return "TIMEOUT", "No matching COMMAND_END arrived before deadline."
    if await_job and not job_ended:
        return "TIMEOUT", "No matching JOB_END arrived before deadline."
    for record in _records(lines):
        if record.kind not in {"COMMAND_END", "JOB_END", "REC result"}:
            continue
        _validate_outcome(record)
        if record.kind in {"COMMAND_END", "JOB_END"} and (
            record.fields["ok"] != "1"
        ):
            return "FAIL", f"{record.kind} reported ok=0."
        if record.kind == "REC result" and record.fields["ok"] != "1":
            return (
                "FAIL",
                f"REC result op={record.fields.get('op', 'unknown')} "
                "reported ok=0.",
            )
    return "PASS", "Exact completion boundary and evidence reported success."


def capture_from_lines(
    spec: CommandSpec,
    lines: Iterable[str],
    *,
    last_command_id: int = 0,
) -> CommandCapture:
    """Parse one command capture offline; shared by tests and live runs."""
    collected: list[str] = []
    active_id: int | None = None
    ended = False
    job_started = False
    job_ended = False
    expected_command = spec.command.split(maxsplit=1)[0]

    for raw_line in lines:
        line = strip_ansi(raw_line).rstrip("\n")
        record = parse_record(line)
        if active_id is None:
            if record is None or record.kind != "COMMAND_BEGIN":
                continue
            command_id = _required_integer(record, "id")
            if command_id <= last_command_id:
                raise ProtocolError("command IDs are not strictly monotonic")
            echoed = record.fields.get("command")
            if echoed is None:
                raise ProtocolError("COMMAND_BEGIN lacks command")
            if echoed != expected_command:
                raise ProtocolError(
                    f"COMMAND_BEGIN echoed {echoed!r}, expected "
                    f"{expected_command!r}"
                )
            active_id = command_id
            collected.append(line)
            continue

        collected.append(line)
        if record is None:
            continue
        if record.kind == "COMMAND_BEGIN":
            raise ProtocolError("nested COMMAND_BEGIN before completion")
        if record.kind == "REC result":
            _validate_outcome(record)
            result_id = _required_integer(record, "id")
            if result_id != active_id:
                raise ProtocolError("REC result command ID mismatch")
        elif record.kind == "COMMAND_END":
            _validate_outcome(record)
            if _required_integer(record, "id") != active_id:
                raise ProtocolError("COMMAND_END command ID mismatch")
            if ended:
                raise ProtocolError("duplicate COMMAND_END")
            ended = True
            if not spec.await_job or job_ended:
                break
        elif record.kind == "JOB_BEGIN":
            _require_fields(
                record,
                (
                    "kind",
                    "target",
                    "attempt_limit",
                    "interval_ms",
                    "operation_timeout_ms",
                ),
            )
            if _required_integer(record, "command_id") != active_id:
                raise ProtocolError("JOB_BEGIN command ID mismatch")
            if job_started:
                raise ProtocolError("duplicate JOB_BEGIN")
            job_started = True
        elif record.kind == "JOB_END":
            _validate_outcome(record)
            if _required_integer(record, "command_id") != active_id:
                raise ProtocolError("JOB_END command ID mismatch")
            if not job_started:
                raise ProtocolError("JOB_END arrived without JOB_BEGIN")
            if job_ended:
                raise ProtocolError("duplicate JOB_END")
            job_ended = True
            if ended:
                break

    if spec.await_job and ended and not job_started:
        raise ProtocolError("job command completed without JOB_BEGIN")
    result, reason = classify_capture(
        ended=ended,
        job_ended=job_ended,
        await_job=spec.await_job,
        lines=collected,
    )
    if result == "PASS":
        _validate_required_evidence(spec, collected)
    return CommandCapture(
        command=spec.command,
        purpose=spec.purpose,
        group=spec.group,
        command_id=active_id,
        started=active_id is not None,
        ended=ended,
        job_started=job_started,
        job_ended=job_ended,
        lines=collected,
        result=result,
        reason=reason,
        elapsed_s=0.0,
    )


def _validate_command(command: str) -> None:
    encoded = command.encode("ascii", errors="strict")
    if not command or command != command.strip():
        raise ValueError("command must have no surrounding whitespace")
    if len(encoded) > MAX_COMMAND_BYTES:
        raise ValueError(f"command exceeds {MAX_COMMAND_BYTES} bytes")
    if any(character < 0x20 or character > 0x7E for character in encoded):
        raise ValueError("command must contain printable ASCII only")


def _write_transcript_line(
    transcript: TextIO, direction: str, value: str
) -> None:
    timestamp = dt.datetime.now(dt.timezone.utc).isoformat(
        timespec="milliseconds"
    )
    transcript.write(f"{timestamp} {direction} {value.rstrip()}\n")
    transcript.flush()


def _read_bounded_line(serial_port: object) -> bytes:
    if hasattr(serial_port, "read_until"):
        raw = serial_port.read_until(b"\n", MAX_SERIAL_LINE_BYTES + 1)
    else:
        raw = serial_port.readline()
    if len(raw) > MAX_SERIAL_LINE_BYTES:
        raise ProtocolError(
            f"serial line exceeded {MAX_SERIAL_LINE_BYTES} bytes"
        )
    return raw


def run_serial_command(
    serial_port: object,
    spec: CommandSpec,
    transcript: TextIO,
    *,
    last_command_id: int,
) -> CommandCapture:
    _validate_command(spec.command)
    if hasattr(serial_port, "reset_input_buffer"):
        serial_port.reset_input_buffer()
    _write_transcript_line(transcript, "TX", spec.command)
    serial_port.write((spec.command + "\n").encode("ascii"))
    serial_port.flush()

    started = time.monotonic()
    deadline = started + spec.timeout_s
    received: list[str] = []
    while time.monotonic() < deadline:
        raw = _read_bounded_line(serial_port)
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        received.append(line)
        _write_transcript_line(transcript, "RX", line)
        capture = capture_from_lines(
            spec, received, last_command_id=last_command_id
        )
        if capture.ended and (
            not spec.await_job or capture.job_ended
        ):
            capture.elapsed_s = time.monotonic() - started
            return capture

    capture = capture_from_lines(
        spec, received, last_command_id=last_command_id
    )
    capture.elapsed_s = time.monotonic() - started
    return capture


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--port", help="Serial port for a live run, for example COM6."
    )
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--out-dir", type=pathlib.Path, default=DEFAULT_OUTPUT
    )
    parser.add_argument("--startup-wait-s", type=float, default=2.0)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Write only the command plan; never claim HIL.",
    )
    parser.add_argument("--include-register-dump", action="store_true")
    parser.add_argument("--include-sampling", action="store_true")
    parser.add_argument("--sample-count", type=int, default=3)
    parser.add_argument("--one-timeout-ms", type=int, default=1000)
    parser.add_argument("--include-fault-auto", action="store_true")
    parser.add_argument("--include-fault-manual", action="store_true")
    parser.add_argument("--include-stress", action="store_true")
    parser.add_argument("--stress-count", type=int, default=100)
    parser.add_argument("--include-stream", action="store_true")
    parser.add_argument("--stream-count", type=int, default=100)
    parser.add_argument("--stream-interval-ms", type=int, default=100)
    args = parser.parse_args(argv)
    if not args.dry_run and not args.port:
        parser.error(
            "a live run requires --port; use --dry-run to inspect the plan"
        )
    if args.baud <= 0 or args.startup_wait_s < 0.0:
        parser.error("baud must be positive and startup wait non-negative")
    if not 1 <= args.sample_count <= 1000:
        parser.error("--sample-count must be in 1..1000")
    if not 1 <= args.one_timeout_ms <= 2_147_483_647:
        parser.error("--one-timeout-ms must be in 1..2147483647")
    if not 1 <= args.stress_count <= 100000:
        parser.error("--stress-count must be in 1..100000")
    if not 1 <= args.stream_count <= 100000:
        parser.error("--stream-count must be in 1..100000")
    if not 1 <= args.stream_interval_ms <= 60000:
        parser.error("--stream-interval-ms must be in 1..60000")
    return args


def build_plan(args: argparse.Namespace) -> list[CommandSpec]:
    plan = list(SAFE_DEFAULT_PLAN)
    side_effect_selected = False
    if args.include_register_dump:
        side_effect_selected = True
        plan.append(
            CommandSpec(
                "reg dump",
                "Read all registers; this explicitly acknowledges DRDY.",
                group="register-dump",
            )
        )
    if args.include_sampling:
        side_effect_selected = True
        per_sample_timeout = args.one_timeout_ms / 1000.0 + 3.0
        for index in range(args.sample_count):
            plan.append(
                CommandSpec(
                    f"one {args.one_timeout_ms} confirm",
                    f"Acquire bounded one-shot sample {index + 1}/"
                    f"{args.sample_count}.",
                    group="sampling",
                    timeout_s=per_sample_timeout,
                )
            )
    if args.include_fault_auto:
        side_effect_selected = True
        plan.append(
            CommandSpec(
                "fault-auto confirm",
                "Run a fresh automatic fault cycle; fixture RC must be <=100 us.",
                group="fault-cycle",
            )
        )
    if args.include_fault_manual:
        side_effect_selected = True
        plan.append(
            CommandSpec(
                "fault-manual confirm",
                "Run the fully timed two-step manual fault cycle.",
                group="fault-cycle",
            )
        )
    if args.include_stress:
        side_effect_selected = True
        plan.append(
            CommandSpec(
                f"stress {args.stress_count} confirm",
                "Run the cooperative bounded one-shot stress job.",
                group="stress",
                timeout_s=max(20.0, args.stress_count * 1.0 + 10.0),
                await_job=True,
            )
        )
    if args.include_stream:
        side_effect_selected = True
        interval_s = args.stream_interval_ms / 1000.0
        plan.append(
            CommandSpec(
                f"stream {args.stream_count} "
                f"{args.stream_interval_ms} confirm",
                "Run the cooperative continuous-conversion stream job.",
                group="stream",
                timeout_s=max(
                    20.0,
                    args.stream_count * interval_s * 8.0 + 10.0,
                ),
                await_job=True,
            )
        )
    if side_effect_selected:
        plan.append(
            CommandSpec(
                "stop confirm",
                "Best-effort bounded final conversion cleanup.",
                group="final-cleanup",
            )
        )
    plan.extend(
        (
            CommandSpec(
                "state",
                "Capture final lifecycle state.",
                group="final-observation",
            ),
            CommandSpec(
                "health",
                "Capture final health and counters.",
                group="final-observation",
            ),
        )
    )
    for spec in plan:
        _validate_command(spec.command)
    return plan


def repository_provenance() -> dict[str, object]:
    def git(*arguments: str) -> str:
        try:
            result = subprocess.run(
                ("git", *arguments),
                cwd=ROOT,
                text=True,
                capture_output=True,
                check=False,
            )
        except OSError:
            return "unavailable"
        if result.returncode != 0:
            return "unavailable"
        return result.stdout.strip()

    metadata = json.loads(
        (ROOT / "library.json").read_text(encoding="utf-8")
    )
    status = git("status", "--porcelain")
    return {
        "library_version": metadata.get("version", "unknown"),
        "branch": git("branch", "--show-current"),
        "commit": git("rev-parse", "HEAD"),
        "worktree": (
            "unavailable"
            if status == "unavailable"
            else "clean"
            if status == ""
            else "dirty"
        ),
        "python": platform.python_version(),
        "platform": platform.platform(),
    }


def captured_firmware_provenance(
    capture: CommandCapture,
) -> dict[str, str]:
    if capture.command != "version" or capture.result != "PASS":
        return {}
    records = _records_of_kind(capture.lines, "REC version")
    return {} if len(records) != 1 else dict(records[0].fields)


def final_verdict(
    results: Iterable[CommandCapture], *, dry_run: bool
) -> str:
    if dry_run:
        return "DRY_RUN_ONLY"
    values = [capture.result for capture in results]
    if any(
        value in {"FAIL", "TIMEOUT", "PROTOCOL_ERROR"}
        for value in values
    ):
        return "FAIL"
    return "SERIAL_CHECKS_PASSED_REVIEW_REQUIRED"


def planned_capture(spec: CommandSpec) -> CommandCapture:
    return CommandCapture(
        command=spec.command,
        purpose=spec.purpose,
        group=spec.group,
        command_id=None,
        started=False,
        ended=False,
        job_started=False,
        job_ended=False,
        lines=[],
        result="NOT_RUN_DRY_PLAN",
        reason="Dry-run plan only; no serial port was opened.",
        elapsed_s=0.0,
    )


def skipped_after_failure(spec: CommandSpec) -> CommandCapture:
    return CommandCapture(
        command=spec.command,
        purpose=spec.purpose,
        group=spec.group,
        command_id=None,
        started=False,
        ended=False,
        job_started=False,
        job_ended=False,
        lines=[],
        result="SKIPPED_AFTER_FAILURE",
        reason=(
            "Skipped after a hard failure; final cleanup and observations "
            "remain eligible."
        ),
        elapsed_s=0.0,
    )


def write_markdown(path: pathlib.Path, summary: dict[str, object]) -> None:
    lines = [
        "# MAX31865 serial evidence summary",
        "",
        f"- Verdict: `{summary['verdict']}`",
        f"- HIL accepted: `{str(summary['hil_accepted']).lower()}`",
        f"- Dry run: `{str(summary['dry_run']).lower()}`",
        f"- Started UTC: `{summary['started_utc']}`",
        f"- Serial port: `{summary.get('serial_port') or 'none'}`",
        f"- Claim boundary: {CLAIM_BOUNDARY}",
        "",
        "## Repository provenance",
        "",
    ]
    provenance = summary["provenance"]
    assert isinstance(provenance, dict)
    for key, value in provenance.items():
        lines.append(f"- {key}: `{value}`")
    lines.extend(("", "## Firmware provenance", ""))
    firmware = summary["firmware_provenance"]
    assert isinstance(firmware, dict)
    if firmware:
        for key, value in firmware.items():
            lines.append(f"- {key}: `{value}`")
    else:
        lines.append("- Not captured (dry run or failed version command).")
    lines.extend(
        (
            "",
            "## Results",
            "",
            "| Group | Command | Result | Reason |",
            "|---|---|---|---|",
        )
    )
    results = summary["results"]
    assert isinstance(results, list)
    for result in results:
        assert isinstance(result, dict)
        command = str(result["command"]).replace("|", "\\|")
        reason = str(result["reason"]).replace("|", "\\|")
        lines.append(
            f"| {result['group']} | `{command}` | "
            f"{result['result']} | {reason} |"
        )
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    plan = build_plan(args)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    started = dt.datetime.now(dt.timezone.utc).isoformat()
    stamp = dt.datetime.now(dt.timezone.utc).strftime(
        "%Y%m%dT%H%M%S%fZ"
    )
    base = args.out_dir / f"max31865_hil_{stamp}"
    transcript_path = base.with_suffix(".log")
    json_path = base.with_suffix(".json")
    markdown_path = base.with_suffix(".md")
    captures: list[CommandCapture] = []
    firmware_provenance: dict[str, str] = {}

    with transcript_path.open("w", encoding="utf-8") as transcript:
        _write_transcript_line(transcript, "META", CLAIM_BOUNDARY)
        if args.dry_run:
            for spec in plan:
                _write_transcript_line(transcript, "PLAN", spec.command)
                captures.append(planned_capture(spec))
        else:
            try:
                import serial
            except ImportError as exc:
                raise SystemExit(
                    f"pyserial is required for a live run; {INSTALL_HINT}"
                ) from exc
            _write_transcript_line(
                transcript,
                "META",
                f"pyserial={importlib.metadata.version('pyserial')} "
                f"port={args.port} baud={args.baud}",
            )
            with serial.Serial(args.port, args.baud, timeout=0.1) as port:
                time.sleep(args.startup_wait_s)
                last_command_id = 0
                hard_failure = False
                for spec in plan:
                    always_run = spec.group in {
                        "final-cleanup",
                        "final-observation",
                    }
                    if hard_failure and not always_run:
                        captures.append(skipped_after_failure(spec))
                        continue
                    try:
                        capture = run_serial_command(
                            port,
                            spec,
                            transcript,
                            last_command_id=last_command_id,
                        )
                    except ProtocolError as exc:
                        capture = CommandCapture(
                            command=spec.command,
                            purpose=spec.purpose,
                            group=spec.group,
                            command_id=None,
                            started=False,
                            ended=False,
                            job_started=False,
                            job_ended=False,
                            lines=[],
                            result="PROTOCOL_ERROR",
                            reason=str(exc),
                            elapsed_s=0.0,
                        )
                    captures.append(capture)
                    if capture.command_id is not None:
                        last_command_id = capture.command_id
                    captured = captured_firmware_provenance(capture)
                    if captured:
                        firmware_provenance = captured
                    if capture.result in {
                        "FAIL",
                        "TIMEOUT",
                        "PROTOCOL_ERROR",
                    }:
                        hard_failure = True

    verdict = final_verdict(captures, dry_run=args.dry_run)
    summary: dict[str, object] = {
        "schema_version": 1,
        "verdict": verdict,
        "hil_accepted": False,
        "claim_boundary": CLAIM_BOUNDARY,
        "dry_run": args.dry_run,
        "started_utc": started,
        "completed_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "serial_port": args.port,
        "baud": args.baud,
        "opt_ins": {
            "register_dump": args.include_register_dump,
            "sampling": args.include_sampling,
            "fault_auto": args.include_fault_auto,
            "fault_manual": args.include_fault_manual,
            "stress": args.include_stress,
            "stream": args.include_stream,
        },
        "provenance": repository_provenance(),
        "firmware_provenance": firmware_provenance,
        "artifacts": {
            "raw_transcript": str(transcript_path),
            "json_summary": str(json_path),
            "markdown_summary": str(markdown_path),
        },
        "results": [capture.as_dict() for capture in captures],
    }
    json_path.write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8"
    )
    write_markdown(markdown_path, summary)
    print(f"{verdict}: {json_path}")
    return 2 if verdict == "FAIL" else 0


if __name__ == "__main__":
    raise SystemExit(main())
