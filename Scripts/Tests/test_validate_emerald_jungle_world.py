from __future__ import annotations

import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "validate_emerald_jungle_world.py"
SPEC = importlib.util.spec_from_file_location("validate_emerald_jungle_world", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


class VectorParsingTests(unittest.TestCase):
    def test_parse_vector_2d(self) -> None:
        self.assertEqual(
            MODULE.parse_vector("(X=0.30,Y=0.22)", ("X", "Y")),
            {"X": 0.30, "Y": 0.22},
        )

    def test_parse_vector_3d(self) -> None:
        self.assertEqual(
            MODULE.parse_vector("(X=-80000,Y=-112000,Z=500)", ("X", "Y", "Z")),
            {"X": -80000.0, "Y": -112000.0, "Z": 500.0},
        )

    def test_parse_vector_rejects_missing_axis(self) -> None:
        with self.assertRaises(ValueError):
            MODULE.parse_vector("(X=1,Y=2)", ("X", "Y", "Z"))

    def test_parse_bool(self) -> None:
        self.assertTrue(MODULE.parse_bool("True"))
        self.assertFalse(MODULE.parse_bool("false"))
        with self.assertRaises(ValueError):
            MODULE.parse_bool("maybe")


class RepositoryDataTests(unittest.TestCase):
    def test_current_repository_world_data_passes(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        report = MODULE.validate_project(project_root)
        self.assertTrue(report.passed, "\\n".join(report.errors))

    def test_invalid_map_coordinate_is_rejected(self) -> None:
        source_root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            target_data = temp_root / "Data" / "World"
            target_data.mkdir(parents=True)
            for filename in (
                "DT_Jumanji_WorldRegions.csv",
                "DT_Jumanji_WorldLandmarks.csv",
                "EmeraldJungle_GreyboxPlacement.csv",
                "EmeraldJungle_WorldManifest.json",
            ):
                shutil.copy2(source_root / "Data" / "World" / filename, target_data / filename)

            regions_path = target_data / "DT_Jumanji_WorldRegions.csv"
            text = regions_path.read_text(encoding="utf-8")
            regions_path.write_text(text.replace("(X=0.30,Y=0.22)", "(X=1.30,Y=0.22)", 1), encoding="utf-8")

            report = MODULE.validate_project(temp_root)
            self.assertFalse(report.passed)
            self.assertTrue(any("Map coordinate range REGION_ROOT_GROVE" in error for error in report.errors))


if __name__ == "__main__":
    unittest.main()
