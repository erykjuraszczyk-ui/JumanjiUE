import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))

from validate_traversal_presentation import validate_manifest


class TraversalPresentationManifestTests(unittest.TestCase):
    def test_repository_manifest_is_valid(self):
        errors = validate_manifest(ROOT / "Data/Traversal/TraversalPresentationManifest.csv")
        self.assertEqual(errors, [])

    def test_missing_manifest_is_reported(self):
        errors = validate_manifest(ROOT / "Data/Traversal/does-not-exist.csv")
        self.assertTrue(errors)
        self.assertIn("missing manifest", errors[0])


if __name__ == "__main__":
    unittest.main()
