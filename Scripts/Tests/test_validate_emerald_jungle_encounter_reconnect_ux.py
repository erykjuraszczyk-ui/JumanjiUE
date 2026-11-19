from __future__ import annotations

import csv
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_emerald_jungle_encounter_reconnect_ux as MODULE


class EncounterReconnectUXValidationTests(unittest.TestCase):
    def test_repository_sources_and_matrix_pass(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_sources(project_root), [])
        self.assertEqual(
            MODULE.validate_matrix(project_root / "Data/Encounter/EncounterReconnectSessionQAMatrix.csv"),
            [],
        )

    def test_duplicate_scenario_ids_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "matrix.csv"
            rows = [
                list(MODULE.MATRIX_COLUMNS),
                ["REJOIN_TRIALS_001", "Returning", "Trials", "5", "Reconnected", "true", "None", "PIE", "first"],
                ["REJOIN_TRIALS_001", "Returning", "Trials", "6", "Reconnected", "true", "None", "PIE", "duplicate"],
            ]
            with path.open("w", encoding="utf-8", newline="") as stream:
                csv.writer(stream).writerows(rows)
            errors = MODULE.validate_matrix(path)
            self.assertTrue(any("duplicate ScenarioId" in error for error in errors))

    def test_none_outcome_must_not_show_banner(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "matrix.csv"
            row = {
                column: "" for column in MODULE.MATRIX_COLUMNS
            }
            row.update({
                "ScenarioId": "REJOIN_TRIALS_001",
                "ClientState": "New",
                "DisconnectPhase": "Trials",
                "ElapsedSeconds": "0",
                "ExpectedOutcome": "None",
                "ExpectedFeedbackVisible": "true",
                "ExpectedEncounterMutation": "None",
                "ValidationLayer": "PIE",
                "Notes": "incorrect banner state",
            })
            with path.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=MODULE.MATRIX_COLUMNS)
                writer.writeheader()
                writer.writerow(row)
            errors = MODULE.validate_matrix(path)
            self.assertTrue(any("banner visibility does not match" in error for error in errors))

    def test_expired_window_has_visible_warning(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "matrix.csv"
            row = {
                column: "" for column in MODULE.MATRIX_COLUMNS
            }
            row.update({
                "ScenarioId": "GRACE_EXPIRED_001",
                "ClientState": "Returning",
                "DisconnectPhase": "Combat",
                "ElapsedSeconds": "301",
                "ExpectedOutcome": "GraceWindowExpired",
                "ExpectedFeedbackVisible": "true",
                "ExpectedEncounterMutation": "None",
                "ValidationLayer": "Session test",
                "Notes": "default 300 second grace",
            })
            with path.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=MODULE.MATRIX_COLUMNS)
                writer.writeheader()
                writer.writerow(row)
            errors = MODULE.validate_matrix(path)
            self.assertTrue(any("missing required scenario" in error for error in errors))
            self.assertFalse(any("banner visibility does not match" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
