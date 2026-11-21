from __future__ import annotations

import csv
import tempfile
import unittest
from pathlib import Path

from Scripts.validate_encounter_recovery_benchmark import (
    COLUMNS,
    validate_matrix,
    validate_project,
)


ROOT = Path(__file__).resolve().parents[2]


class EncounterRecoveryBenchmarkValidatorTests(unittest.TestCase):
    def test_repository_contracts_and_profiles_pass(self) -> None:
        self.assertEqual(validate_project(ROOT), [])

    def test_duplicate_profile_names_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "profiles.csv"
            rows = [
                list(COLUMNS),
                ["Smoke", "3", "2", "120", "1", "smoke", "pass"],
                ["Smoke", "3", "2", "120", "1", "duplicate", "pass"],
            ]
            with path.open("w", encoding="utf-8", newline="") as stream:
                csv.writer(stream).writerows(rows)
            errors = validate_matrix(path)
            self.assertTrue(any("duplicate Profile Smoke" in error for error in errors))

    def test_out_of_range_workers_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "profiles.csv"
            rows = [list(COLUMNS)]
            for name, duration, workers, ttl, warmup in (
                ("Smoke", 3, 2, 120, 1),
                ("Baseline", 60, 4, 120, 3),
                ("Load", 180, 99, 120, 3),
                ("Soak30", 1800, 8, 120, 3),
                ("Soak60", 3600, 8, 120, 3),
            ):
                rows.append([name, duration, workers, ttl, warmup, "test", "pass"])
            with path.open("w", encoding="utf-8", newline="") as stream:
                csv.writer(stream).writerows(rows)
            errors = validate_matrix(path)
            self.assertTrue(any("Workers must be 1..64" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
