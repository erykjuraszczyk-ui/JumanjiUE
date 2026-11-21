from __future__ import annotations

import csv
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from Scripts.validate_encounter_recovery_capacity_slo import COLUMNS, validate_profiles


class EncounterRecoverySLOProfileTests(unittest.TestCase):
    def test_repository_profiles_are_valid_and_unbaselined(self) -> None:
        path = ROOT / "Data/Encounter/EncounterRecoverySLOProfiles.csv"
        self.assertEqual(validate_profiles(path), [])

        with path.open("r", encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertTrue(rows)
        self.assertTrue(all(row["Status"] == "UNBASELINED" for row in rows))
        self.assertTrue(all(not row["TargetWriteP95Ms"] for row in rows))

    def test_approved_profile_requires_targets_and_review_evidence(self) -> None:
        path = ROOT / "Data/Encounter/EncounterRecoverySLOProfiles.csv"
        with path.open("r", encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        rows[1]["Status"] = "APPROVED"
        with tempfile.TemporaryDirectory() as temp:
            candidate = Path(temp) / "profiles.csv"
            with candidate.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=COLUMNS)
                writer.writeheader()
                writer.writerows(rows)
            errors = validate_profiles(candidate)
        self.assertTrue(any("requires all positive SLO targets" in item for item in errors))
        self.assertTrue(any("requires EvidenceId" in item for item in errors))

    def test_p95_budget_cannot_exceed_p99_budget(self) -> None:
        path = ROOT / "Data/Encounter/EncounterRecoverySLOProfiles.csv"
        with path.open("r", encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        row = rows[1]
        row.update({
            "Status": "PROPOSED",
            "TargetWriteP95Ms": "12",
            "TargetWriteP99Ms": "10",
            "TargetReadP95Ms": "3",
            "TargetReadP99Ms": "5",
            "TargetPairsPerSecond": "1",
            "EvidenceId": "EVIDENCE-1",
            "Approver": "reviewer",
            "ReviewedAtUtc": "2026-11-21T12:00:00Z",
        })
        with tempfile.TemporaryDirectory() as temp:
            candidate = Path(temp) / "profiles.csv"
            with candidate.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=COLUMNS)
                writer.writeheader()
                writer.writerows(rows)
            errors = validate_profiles(candidate)
        self.assertTrue(any("write p95 budget must not exceed write p99 budget" in item for item in errors))


if __name__ == "__main__":
    unittest.main()
