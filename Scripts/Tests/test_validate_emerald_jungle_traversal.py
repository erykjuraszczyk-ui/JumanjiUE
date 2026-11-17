from __future__ import annotations

import csv
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_emerald_jungle_traversal as MODULE


class TraversalWorldManifestTests(unittest.TestCase):
    def test_repository_traversal_data_passes(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(project_root), [])

    def _copy_fixture(self, temp_dir: str) -> Path:
        source_root = Path(__file__).resolve().parents[2]
        target_root = Path(temp_dir)
        for relative in (
            "Data/Traversal/EmeraldJungle_TraversalPlacement.csv",
            "Data/Traversal/EmeraldJungle_LiftPlatformManifest.csv",
            "Data/Traversal/EmeraldJungle_TraversalManifest.json",
            "Data/World/DT_Jumanji_WorldRegions.csv",
            "Data/World/EmeraldJungle_GreyboxPlacement.csv",
        ):
            destination = target_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_root / relative, destination)
        return target_root

    def test_duplicate_traversal_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Data/Traversal/EmeraldJungle_TraversalPlacement.csv"
            with path.open("r", encoding="utf-8", newline="") as handle:
                rows = list(csv.DictReader(handle))
                columns = list(rows[0].keys())
            rows[1]["TraversalId"] = rows[0]["TraversalId"]
            with path.open("w", encoding="utf-8", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=columns)
                writer.writeheader()
                writer.writerows(rows)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("Duplicate TraversalId" in error for error in errors))

    def test_lift_anchor_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Data/Traversal/EmeraldJungle_LiftPlatformManifest.csv"
            with path.open("r", encoding="utf-8", newline="") as handle:
                rows = list(csv.DictReader(handle))
                columns = list(rows[0].keys())
            rows[0]["UpperX_cm"] = str(float(rows[0]["UpperX_cm"]) + 250.0)
            with path.open("w", encoding="utf-8", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=columns)
                writer.writeheader()
                writer.writerows(rows)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("link entry/exit anchors must match" in error for error in errors))

    def test_unknown_region_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Data/Traversal/EmeraldJungle_TraversalPlacement.csv"
            with path.open("r", encoding="utf-8", newline="") as handle:
                rows = list(csv.DictReader(handle))
                columns = list(rows[0].keys())
            rows[0]["RegionId"] = "REGION_NOT_REAL"
            with path.open("w", encoding="utf-8", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=columns)
                writer.writeheader()
                writer.writerows(rows)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("unknown RegionId" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
