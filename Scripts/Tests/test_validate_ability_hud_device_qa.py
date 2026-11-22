import csv
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS_DIR))

from validate_ability_hud_device_qa import DEFAULT_MATRIX, validate_matrix


class AbilityHUDDeviceQAMatrixTests(unittest.TestCase):
    def test_repository_matrix_is_valid(self):
        self.assertEqual(validate_matrix(DEFAULT_MATRIX), [])

    def test_rejects_out_of_range_scale(self):
        with DEFAULT_MATRIX.open("r", encoding="utf-8-sig", newline="") as source:
            rows = list(csv.DictReader(source))
            columns = list(rows[0].keys())

        rows[0]["HUDScale"] = "2.0"
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "matrix.csv"
            with path.open("w", encoding="utf-8", newline="") as target:
                writer = csv.DictWriter(target, fieldnames=columns)
                writer.writeheader()
                writer.writerows(rows)
            errors = validate_matrix(path)

        self.assertTrue(any("HUDScale" in error for error in errors))

    def test_rejects_unknown_execution_status(self):
        with DEFAULT_MATRIX.open("r", encoding="utf-8-sig", newline="") as source:
            rows = list(csv.DictReader(source))
            columns = list(rows[0].keys())

        rows[0]["ExecutionStatus"] = "DEVICE_TESTED"
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "matrix.csv"
            with path.open("w", encoding="utf-8", newline="") as target:
                writer = csv.DictWriter(target, fieldnames=columns)
                writer.writeheader()
                writer.writerows(rows)
            errors = validate_matrix(path)

        self.assertTrue(any("ExecutionStatus" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
