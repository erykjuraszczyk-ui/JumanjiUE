from __future__ import annotations

import csv
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_emerald_jungle_boss_telegraphs as MODULE


class BossTelegraphManifestTests(unittest.TestCase):
    def _fixture(self, temp_dir: str) -> Path:
        source_root = Path(__file__).resolve().parents[2]
        target_root = Path(temp_dir)
        relative = "Data/Encounter/EmeraldJungleBossTelegraphs.csv"
        destination = target_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_root / relative, destination)
        return target_root

    def _read(self, root: Path) -> tuple[Path, list[dict[str, str]], list[str]]:
        path = root / "Data/Encounter/EmeraldJungleBossTelegraphs.csv"
        with path.open("r", encoding="utf-8", newline="") as handle:
            reader = csv.DictReader(handle)
            columns = list(reader.fieldnames or [])
            return path, list(reader), columns

    def _write(self, path: Path, rows: list[dict[str, str]], columns: list[str]) -> None:
        with path.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(rows)

    def test_repository_manifest_passes(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(project_root), [])

    def test_duplicate_attack_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows[1]["AttackId"] = rows[0]["AttackId"]
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("duplicate AttackId" in error for error in errors))

    def test_invalid_pattern_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows[0]["Pattern"] = "UnsupportedShape"
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("Pattern must be one of" in error for error in errors))

    def test_out_of_range_warning_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows[0]["WarningSeconds"] = "9.0"
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("WarningSeconds must be within" in error for error in errors))

    def test_line_requires_nonzero_length_and_width(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows[1]["WidthCm"] = "0"
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("Line requires LengthCm and WidthCm" in error for error in errors))

    def test_missing_boss_phase_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows = [row for row in rows if row["Phase"] != "3"]
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("missing phase(s): 3" in error for error in errors))

    def test_missing_column_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            columns.remove("WidthCm")
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("WidthCm" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
