#!/usr/bin/env python3
"""Offline tests for the MAX31865 serial HIL parser and safety plan."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import run_max31865_hil as hil  # noqa: E402


def end(command_id: int, *, ok: int = 1, code: int = 0) -> str:
    name = "Ok" if code == 0 else "InvalidState"
    return (
        f"COMMAND_END id={command_id} ok={ok} code={code} "
        f"name={name} detail=0 elapsed_ms=1"
    )


def result(command_id: int, operation: str = "test") -> str:
    return (
        f"REC result id={command_id} op={operation} ok=1 "
        "code=0 name=Ok detail=0"
    )


class ParserTest(unittest.TestCase):
    def test_ansi_and_quoted_fields_are_parsed(self) -> None:
        record = hil.parse_record(
            '\x1b[32mCOMMAND_BEGIN\x1b[0m id=7 command="probe"\r'
        )
        self.assertIsNotNone(record)
        assert record is not None
        self.assertEqual("COMMAND_BEGIN", record.kind)
        self.assertEqual("7", record.fields["id"])
        self.assertEqual("probe", record.fields["command"])

    def test_version_requires_complete_machine_evidence(self) -> None:
        spec = hil.CommandSpec("version", "firmware provenance")
        capture = hil.capture_from_lines(
            spec,
            (
                "startup chatter",
                "COMMAND_BEGIN id=3 command=version",
                "REC version library=1.0.0 code=10000 "
                "git_commit=abc123 git_status=clean target=esp32s3 "
                "framework=arduino build=diagnostic "
                "evidence=software_only_unvalidated_hardware",
                result(3, "version"),
                end(3),
            ),
            last_command_id=2,
        )
        self.assertEqual("PASS", capture.result)
        self.assertEqual(3, capture.command_id)
        self.assertEqual(
            "1.0.0",
            hil.captured_firmware_provenance(capture)["library"],
        )

        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=4 command=version",
                    "REC version library=1.0.0 code=10000",
                    result(4, "version"),
                    end(4),
                ),
                last_command_id=3,
            )

    def test_wiring_requires_selected_physical_configuration(self) -> None:
        spec = hil.CommandSpec("wiring", "wiring evidence")
        lines = (
            "COMMAND_BEGIN id=5 command=wiring",
            "REC wiring sck=12 miso=13 mosi=11 cs=10 drdy=-1 "
            "drdy_enabled=0 spi_clock_hz=1000000 spi_max_hz=5000000 "
            "wire=4 filter_hz=60 ownership=application",
            result(5, "wiring"),
            end(5),
        )
        self.assertEqual("PASS", hil.capture_from_lines(spec, lines).result)

        invalid = list(lines)
        invalid[1] = invalid[1].replace(
            "spi_clock_hz=1000000", "spi_clock_hz=6000000"
        )
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(spec, invalid)

        missing_mode = list(lines)
        missing_mode[1] = missing_mode[1].replace(" wire=4", "")
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(spec, missing_mode)

    def test_command_ids_and_boundaries_are_strict(self) -> None:
        spec = hil.CommandSpec("format machine", "format")
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=2 command=format",
                    end(3),
                ),
            )
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=2 command=format",
                    "COMMAND_BEGIN id=3 command=format",
                ),
            )
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=2 command=format",
                    end(2),
                ),
                last_command_id=2,
            )

    def test_outcome_consistency_and_result_failure_are_enforced(self) -> None:
        spec = hil.CommandSpec("format machine", "format")
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=1 command=format",
                    "COMMAND_END id=1 ok=1 code=3 name=InvalidState "
                    "detail=0 elapsed_ms=1",
                ),
            )
        capture = hil.capture_from_lines(
            spec,
            (
                "COMMAND_BEGIN id=1 command=format",
                "REC result id=1 op=format ok=0 code=3 "
                "name=InvalidState detail=0",
                end(1, ok=0, code=3),
            ),
        )
        self.assertEqual("FAIL", capture.result)

        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=2 command=format",
                    end(2),
                ),
            )
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=2 command=format",
                    result(2, "format"),
                    result(2, "format"),
                    end(2),
                ),
            )

    def test_duplicate_machine_fields_are_rejected(self) -> None:
        with self.assertRaises(hil.ProtocolError):
            hil.parse_record(
                "COMMAND_END id=1 id=2 ok=1 code=0 name=Ok "
                "detail=0 elapsed_ms=1"
            )

    def test_result_id_must_match_owning_command(self) -> None:
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                hil.CommandSpec("format machine", "format"),
                (
                    "COMMAND_BEGIN id=8 command=format",
                    result(7, "format"),
                    end(8),
                ),
            )

    def test_probe_requires_matching_configuration(self) -> None:
        spec = hil.CommandSpec("probe", "consistency")
        capture = hil.capture_from_lines(
            spec,
            (
                "COMMAND_BEGIN id=4 command=probe",
                "REC probe config=0x00 high_register=0xFFFE "
                "low_register=0x0000 fault=0x00 matches=1",
                result(4, "probe"),
                end(4),
            ),
        )
        self.assertEqual("PASS", capture.result)
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=4 command=probe",
                    "REC probe config=0x00 high_register=0xFFFE "
                    "low_register=0x0000 fault=0x00 matches=0",
                    end(4),
                ),
            )

    def test_health_requires_all_three_typed_records(self) -> None:
        spec = hil.CommandSpec("health", "health")
        complete = (
            "COMMAND_BEGIN id=5 command=health",
            "REC health state=Ready driver=READY online=1 config_known=1 "
            "consecutive=0 offline_threshold=3 last_code=0 last_name=Ok "
            "last_detail=0",
            "REC health_counts tracked_ok=1 tracked_fail=0 frame_attempt=0 "
            "frame_ok=0 frame_fail=0 no_data=0 dropped=0 overruns=0 "
            "faults=0 threshold_faults=0 reference_faults=0 voltage_faults=0",
            "REC health_io lock_timeout=0 lock_fail=0 transfer_fail=0 "
            "cs_fail=0 gpio_fail=0 drdy_timeout=0 op_timeout=0 "
            "last_fault_valid=0 last_fault=0x00 last_ok_valid=1 "
            "last_ok_ms=0 last_error_valid=0 last_error_ms=0",
            result(5, "health"),
            end(5),
        )
        self.assertEqual(
            "PASS", hil.capture_from_lines(spec, complete).result
        )
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(spec, complete[:2] + complete[-2:])

    def test_async_job_requires_correlated_begin_and_end(self) -> None:
        spec = hil.CommandSpec(
            "stress 2 confirm", "stress", await_job=True
        )
        incomplete = hil.capture_from_lines(
            spec,
            (
                "COMMAND_BEGIN id=9 command=stress",
                "JOB_BEGIN command_id=9 kind=stress target=2 "
                "attempt_limit=2 interval_ms=1 operation_timeout_ms=250",
                result(9, "stress_start"),
                end(9),
            ),
        )
        self.assertEqual("TIMEOUT", incomplete.result)
        complete = hil.capture_from_lines(
            spec,
            (
                "COMMAND_BEGIN id=9 command=stress",
                "JOB_BEGIN command_id=9 kind=stress target=2 "
                "attempt_limit=2 interval_ms=1 operation_timeout_ms=250",
                result(9, "stress_start"),
                end(9),
                "REC job_end kind=stress reason=complete target=2 "
                "attempt_limit=2 attempts=2 successes=2 failures=0 "
                "no_data=0 elapsed_ms=150 cleanup_code=0 cleanup_name=Ok",
                "JOB_END command_id=9 kind=stress ok=1 failures=0 "
                "reason=complete",
            ),
        )
        self.assertEqual("PASS", complete.result)
        self.assertTrue(complete.job_started)
        self.assertTrue(complete.job_ended)

    def test_job_end_without_begin_or_wrong_id_is_rejected(self) -> None:
        spec = hil.CommandSpec(
            "stress 1 confirm", "stress", await_job=True
        )
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=9 command=stress",
                    end(9),
                    "JOB_END command_id=9 kind=stress ok=1 failures=0 "
                    "reason=complete",
                ),
            )
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(
                spec,
                (
                    "COMMAND_BEGIN id=9 command=stress",
                    "JOB_BEGIN command_id=8 kind=stress target=1 "
                    "attempt_limit=1 interval_ms=1 operation_timeout_ms=250",
                ),
            )

    def test_register_dump_requires_exact_span_and_warning(self) -> None:
        rows = tuple(
            f"REC register address=0x{address:02X} name=R{address} "
            "value=0x00"
            for address in range(8)
        )
        lines = (
            "COMMAND_BEGIN id=10 command=reg",
            *rows,
            "REC warning kind=register_dump_acknowledges_drdy",
            result(10, "reg_dump"),
            end(10),
        )
        spec = hil.CommandSpec("reg dump", "dump")
        self.assertEqual(
            "PASS", hil.capture_from_lines(spec, lines).result
        )
        with self.assertRaises(hil.ProtocolError):
            hil.capture_from_lines(spec, lines[:-3] + lines[-2:])

    def test_one_shot_requires_typed_sample(self) -> None:
        spec = hil.CommandSpec("one 1000 confirm", "sample")
        lines = (
            "COMMAND_BEGIN id=11 command=one",
            "REC sample source=one count=1 raw_reg=0x4000 code=8192 "
            "resistance_ohms=100.0 temperature_c=0.0 flags=0x0F "
            "channel=0 read_us=10 ready_us=0",
            result(11, "one"),
            end(11),
        )
        self.assertEqual(
            "PASS", hil.capture_from_lines(spec, lines).result
        )


class SafetyPlanTest(unittest.TestCase):
    @staticmethod
    def namespace(**changes: object) -> argparse.Namespace:
        values: dict[str, object] = {
            "include_register_dump": False,
            "include_sampling": False,
            "sample_count": 3,
            "one_timeout_ms": 1000,
            "include_fault_auto": False,
            "include_fault_manual": False,
            "include_stress": False,
            "stress_count": 100,
            "include_stream": False,
            "stream_count": 100,
            "stream_interval_ms": 100,
        }
        values.update(changes)
        return argparse.Namespace(**values)

    def test_default_plan_is_read_only_and_has_final_observations(self) -> None:
        plan = hil.build_plan(self.namespace())
        commands = [spec.command for spec in plan]
        self.assertFalse(any("confirm" in command for command in commands))
        self.assertNotIn("reg dump", commands)
        self.assertEqual(["state", "health"], commands[-2:])
        self.assertFalse(any(spec.await_job for spec in plan))

    def test_every_side_effect_family_requires_its_opt_in(self) -> None:
        plan = hil.build_plan(
            self.namespace(
                include_register_dump=True,
                include_sampling=True,
                sample_count=2,
                include_fault_auto=True,
                include_fault_manual=True,
                include_stress=True,
                stress_count=7,
                include_stream=True,
                stream_count=8,
                stream_interval_ms=60,
            )
        )
        commands = [spec.command for spec in plan]
        self.assertIn("reg dump", commands)
        self.assertEqual(2, commands.count("one 1000 confirm"))
        self.assertIn("fault-auto confirm", commands)
        self.assertIn("fault-manual confirm", commands)
        self.assertIn("stress 7 confirm", commands)
        self.assertIn("stream 8 60 confirm", commands)
        self.assertEqual(1, commands.count("stop confirm"))
        jobs = [spec.command for spec in plan if spec.await_job]
        self.assertEqual(
            ["stress 7 confirm", "stream 8 60 confirm"], jobs
        )

    def test_final_verdict_never_calls_dry_run_hil(self) -> None:
        self.assertEqual(
            "DRY_RUN_ONLY", hil.final_verdict([], dry_run=True)
        )

    def test_command_validation_is_bounded_printable_ascii(self) -> None:
        hil._validate_command("one 1000 confirm")
        for command in (
            "",
            " probe",
            "probe\nstate",
            "x" * (hil.MAX_COMMAND_BYTES + 1),
            "probe \N{SNOWMAN}",
        ):
            with self.subTest(command=command):
                with self.assertRaises((ValueError, UnicodeEncodeError)):
                    hil._validate_command(command)

    def test_dry_run_writes_nonaccepted_artifact_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(
                0,
                hil.main(
                    ["--dry-run", "--out-dir", directory]
                ),
            )
            output = pathlib.Path(directory)
            summaries = list(output.glob("*.json"))
            self.assertEqual(1, len(summaries))
            summary = json.loads(
                summaries[0].read_text(encoding="utf-8")
            )
            self.assertEqual(1, summary["schema_version"])
            self.assertEqual("DRY_RUN_ONLY", summary["verdict"])
            self.assertFalse(summary["hil_accepted"])
            self.assertTrue(summary["dry_run"])
            self.assertTrue(summary["results"])
            self.assertTrue(
                all(
                    row["result"] == "NOT_RUN_DRY_PLAN"
                    for row in summary["results"]
                )
            )
            self.assertEqual(1, len(list(output.glob("*.log"))))
            self.assertEqual(1, len(list(output.glob("*.md"))))

    def test_unavailable_git_is_not_mislabeled_dirty(self) -> None:
        with mock.patch.object(hil.subprocess, "run", side_effect=OSError):
            provenance = hil.repository_provenance()
        self.assertEqual("unavailable", provenance["commit"])
        self.assertEqual("unavailable", provenance["worktree"])

    def test_cleanup_and_final_observation_survive_failure(self) -> None:
        optional = hil.CommandSpec(
            "stress 2 confirm", "stress", group="stress", await_job=True
        )
        skipped = hil.skipped_after_failure(optional)
        self.assertEqual("SKIPPED_AFTER_FAILURE", skipped.result)
        plan = hil.build_plan(self.namespace(include_stress=True))
        self.assertEqual(
            ["stop confirm"],
            [spec.command for spec in plan if spec.group == "final-cleanup"],
        )
        self.assertEqual(
            ["state", "health"],
            [
                spec.command
                for spec in plan
                if spec.group == "final-observation"
            ],
        )


if __name__ == "__main__":
    unittest.main()
