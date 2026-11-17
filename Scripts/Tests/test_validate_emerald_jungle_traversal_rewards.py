from __future__ import annotations

import csv
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_emerald_jungle_traversal_rewards as MODULE


class TraversalRewardManifestTests(unittest.TestCase):
    def _copy_fixture(self, temp_dir: str) -> Path:
        source_root = Path(__file__).resolve().parents[2]
        target_root = Path(temp_dir)
        for relative in (
            "Data/Traversal/EmeraldJungle_TraversalPlacement.csv",
            "Data/Traversal/EmeraldJungle_TraversalRewards.csv",
        ):
            destination = target_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_root / relative, destination)
        return target_root

    def test_repository_rewards_match_all_traversal_links(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(project_root), [])

    def _read_csv(self, root: Path) -> tuple[Path, list[dict[str, str]], list[str]]:
        path = root / "Data/Traversal/EmeraldJungle_TraversalRewards.csv"
        with path.open("r", encoding="utf-8", newline="") as handle:
            reader = csv.DictReader(handle)
            rows = list(reader)
            columns = list(reader.fieldnames or [])
        return path, rows, columns

    def _write_csv(self, path: Path, rows: list[dict[str, str]], columns: list[str]) -> None:
        with path.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=columns)
            writer.writeheader()
            writer.writerows(rows)

    def test_missing_reward_row_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path, rows, columns = self._read_csv(root)
            rows.pop()
            self._write_csv(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("missing traversal reward row" in error for error in errors))

    def test_duplicate_reward_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path, rows, columns = self._read_csv(root)
            rows[1]["TraversalId"] = rows[0]["TraversalId"]
            self._write_csv(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("Duplicate traversal reward" in error for error in errors))

    def test_reward_for_unknown_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path, rows, columns = self._read_csv(root)
            rows[0]["TraversalId"] = "EJ_NOT_IN_WORLD"
            self._write_csv(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("no matching traversal placement row" in error for error in errors))

    def test_negative_currency_reward_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path, rows, columns = self._read_csv(root)
            rows[0]["CurrencyAmount"] = "-10"
            self._write_csv(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("CurrencyAmount must be within" in error for error in errors))

    def test_unknown_item_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path, rows, columns = self._read_csv(root)
            rows[0]["ItemId"] = "MAT_NOT_REAL"
            rows[0]["ItemAmount"] = "1"
            self._write_csv(path, rows, columns)
            errors = MODULE.validate_project(root)
            self.assertTrue(any("unsupported ItemId" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
