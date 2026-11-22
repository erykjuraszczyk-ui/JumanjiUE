from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from pathlib import Path

from Scripts.validate_encounter_checkpoint_stage42 import REQUIRED_FILES, validate_project

ROOT = Path(__file__).resolve().parents[2]


class Stage42GovernanceTests(unittest.TestCase):
    def _copy_fixture(self, target: Path) -> None:
        for relative in REQUIRED_FILES:
            source = ROOT / relative
            destination = target / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)

    def test_repository_stage42_contract_passes(self) -> None:
        self.assertEqual(validate_project(ROOT), [])

    def test_production_ready_cannot_be_enabled_without_staging_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            target = Path(folder)
            self._copy_fixture(target)
            manifest_path = target / "Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["productionReady"] = True
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            errors = validate_project(target)
            self.assertTrue(any("productionReady=false" in error for error in errors))

    def test_stage41_production_boundary_remains_unchanged(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            target = Path(folder)
            self._copy_fixture(target)
            manifest_path = target / "Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["highAvailabilityApproved"] = True
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            errors = validate_project(target)
            self.assertTrue(any("Stage 41 HA" in error for error in errors))

    def test_slo_must_remain_unbaselined(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            target = Path(folder)
            self._copy_fixture(target)
            manifest_path = target / "Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["sloStatus"] = "APPROVED"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            errors = validate_project(target)
            self.assertTrue(any("UNBASELINED" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
