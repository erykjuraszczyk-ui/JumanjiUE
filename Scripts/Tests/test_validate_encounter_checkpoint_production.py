from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_encounter_checkpoint_production as MODULE


class Stage41ManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        root = Path(__file__).resolve().parents[2]
        self.manifest = json.loads(
            (root / "Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json").read_text(encoding="utf-8")
        )

    def test_current_manifest_passes(self) -> None:
        self.assertEqual(MODULE.validate_manifest(self.manifest), [])

    def test_production_readiness_cannot_be_claimed(self) -> None:
        changed = copy.deepcopy(self.manifest)
        changed["productionReady"] = True
        errors = MODULE.validate_manifest(changed)
        self.assertTrue(any("productionReady must remain false" in error for error in errors))

    def test_high_availability_cannot_be_claimed(self) -> None:
        changed = copy.deepcopy(self.manifest)
        changed["highAvailabilityApproved"] = True
        errors = MODULE.validate_manifest(changed)
        self.assertTrue(any("highAvailabilityApproved must remain false" in error for error in errors))

    def test_slo_must_remain_unbaselined(self) -> None:
        changed = copy.deepcopy(self.manifest)
        changed["sloStatus"] = "APPROVED"
        errors = MODULE.validate_manifest(changed)
        self.assertTrue(any("UNBASELINED" in error for error in errors))

    def test_active_lease_must_not_be_migrated(self) -> None:
        changed = copy.deepcopy(self.manifest)
        changed["migration"]["transfersActiveLease"] = True
        errors = MODULE.validate_manifest(changed)
        self.assertTrue(any("never transfer active leases" in error for error in errors))

    def test_mtls_must_be_required_for_external_bind(self) -> None:
        changed = copy.deepcopy(self.manifest)
        changed["transport"]["nonLoopbackBindRequiresClientCertificate"] = False
        errors = MODULE.validate_manifest(changed)
        self.assertTrue(any("require client certificates" in error for error in errors))


class Stage41RepositoryTests(unittest.TestCase):
    def test_current_repository_passes(self) -> None:
        root = Path(__file__).resolve().parents[2]
        errors = MODULE.validate_project(root)
        self.assertEqual(errors, [])

    def test_compose_must_not_publish_service_ports(self) -> None:
        compose = """
        services:
          checkpoint-service:
            ports:
              - "8787:8787"
        """
        import re
        self.assertIsNotNone(re.search(r"(?m)^\s+ports\s*:", compose))


if __name__ == "__main__":
    unittest.main()
