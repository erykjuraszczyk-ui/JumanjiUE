from __future__ import annotations

import csv
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_emerald_jungle_encounter_rewards as MODULE


class WorldEncounterRewardManifestTests(unittest.TestCase):
    def _fixture(self, temp_dir: str) -> Path:
        source_root = Path(__file__).resolve().parents[2]
        target_root = Path(temp_dir)
        relative = "Data/Progression/WorldEncounterRewards.csv"
        destination = target_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_root / relative, destination)
        return target_root

    def _read(self, root: Path) -> tuple[Path, list[dict[str, str]], list[str]]:
        path = root / "Data/Progression/WorldEncounterRewards.csv"
        with path.open("r", encoding="utf-8", newline="") as handle:
            reader = csv.DictReader(handle)
            return path, list(reader), list(reader.fieldnames or [])

    def _write(self, path: Path, rows: list[dict[str, str]], columns: list[str]) -> None:
        with path.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=columns)
            writer.writeheader()
            writer.writerows(rows)

    def test_repository_manifest_passes(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(project_root), [])

    def test_wrong_chapter_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows[0]["RequiredChapterId"] = "CHAPTER_NOT_CONFIGURED"
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("RequiredChapterId" in error for error in errors))

    def test_duplicate_encounter_row_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows.append(dict(rows[0]))
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("exactly one encounter reward row" in error for error in errors))

    def test_negative_currency_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows[0]["CurrencyAmount"] = "-1"
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("CurrencyAmount must be within" in error for error in errors))

    def test_unknown_item_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows[0]["ItemId"] = "MAT_NOT_REAL"
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("unsupported ItemId" in error for error in errors))

    def test_empty_reward_package_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._fixture(temp_dir)
            path, rows, columns = self._read(root)
            rows[0]["AccountExperienceReward"] = "0"
            rows[0]["ActiveCharacterExperienceReward"] = "0"
            rows[0]["CurrencyId"] = ""
            rows[0]["CurrencyAmount"] = "0"
            rows[0]["ItemId"] = ""
            rows[0]["ItemAmount"] = "0"
            self._write(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("at least one positive reward" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
