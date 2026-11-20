from __future__ import annotations

import csv
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_encounter_recovery_chaos as MODULE


class EncounterRecoveryChaosValidatorTests(unittest.TestCase):
    def _fixture(self, temp_dir: str) -> Path:
        source_root = Path(__file__).resolve().parents[2]
        target_root = Path(temp_dir)
        paths = set(MODULE.SOURCE_TOKENS) | {str(MODULE.MATRIX_PATH)}
        for relative in paths:
            source = source_root / relative
            destination = target_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        return target_root

    def test_repository_chaos_contract_passes(self) -> None:
        root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(root), [])

    def test_duplicate_matrix_case_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path = root / MODULE.MATRIX_PATH
            with path.open("r", encoding="utf-8", newline="") as stream:
                rows = list(csv.reader(stream))
            rows.append(rows[1])
            with path.open("w", encoding="utf-8", newline="") as stream:
                csv.writer(stream).writerows(rows)
            errors = MODULE.validate_matrix(path)
            self.assertTrue(any("duplicate CaseId" in error for error in errors), "\n".join(errors))

    def test_required_failure_case_cannot_be_removed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path = root / MODULE.MATRIX_PATH
            with path.open("r", encoding="utf-8", newline="") as stream:
                rows = list(csv.DictReader(stream))
            rows = [row for row in rows if row["CaseId"] != "MISSING_PARTICIPANT"]
            with path.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=MODULE.MATRIX_COLUMNS)
                writer.writeheader()
                writer.writerows(rows)
            errors = MODULE.validate_matrix(path)
            self.assertTrue(any("MISSING_PARTICIPANT" in error for error in errors), "\n".join(errors))

    def test_lease_loss_must_still_fence_checkpoint_writes(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace("bCheckpointWriteBlockedUntilResync = true;", "bCheckpointWriteBlockedUntilResync = false;")
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("bCheckpointWriteBlockedUntilResync = true;" in error for error in errors))

    def test_chaos_workflow_must_keep_prior_stage_regressions(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path = root / MODULE.WORKFLOW_PATH
            source = path.read_text(encoding="utf-8")
            source = source.replace("python Scripts/validate_encounter_startup_coordinator.py", "python Scripts/skip_startup_validator.py")
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("validate_encounter_startup_coordinator.py" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
