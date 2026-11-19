from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_encounter_checkpoint_runtime as MODULE


class Stage34ManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        root = Path(__file__).resolve().parents[2]
        path = root / "Data/Encounter/EncounterCheckpointStage34Manifest.json"
        self.manifest = json.loads(path.read_text(encoding="utf-8"))

    def test_current_manifest_passes(self) -> None:
        self.assertEqual(MODULE.validate_manifest_object(self.manifest), [])

    def test_all_four_barriers_are_required(self) -> None:
        mutated = json.loads(json.dumps(self.manifest))
        mutated["safePointBarriers"] = mutated["safePointBarriers"][:3]
        errors = MODULE.validate_manifest_object(mutated)
        self.assertTrue(any("safe-point phase/kind map" in error for error in errors))

    def test_restore_must_not_mutate_world(self) -> None:
        mutated = json.loads(json.dumps(self.manifest))
        mutated["restoreStaging"]["publishReplicatedState"] = True
        errors = MODULE.validate_manifest_object(mutated)
        self.assertTrue(any("publishReplicatedState" in error for error in errors))

    def test_unbound_opaque_identity_must_reject_capture(self) -> None:
        mutated = json.loads(json.dumps(self.manifest))
        mutated["participantIdentity"]["captureBehaviorWhenUnbound"] = "fallback_to_profile_id"
        errors = MODULE.validate_manifest_object(mutated)
        self.assertTrue(any("capture must reject" in error for error in errors))

    def test_signature_verification_boundary_must_be_explicit(self) -> None:
        mutated = json.loads(json.dumps(self.manifest))
        mutated["contract"]["integrity"]["unrealLocalSignatureVerification"] = True
        errors = MODULE.validate_manifest_object(mutated)
        self.assertTrue(any("Unreal does not locally verify" in error for error in errors))


class LogicalCheckpointTypeTests(unittest.TestCase):
    def test_runtime_structs_remain_value_only(self) -> None:
        root = Path(__file__).resolve().parents[2]
        source = (root / "Source/JumanjiUE/Public/JumanjiEncounterCheckpointTypes.h").read_text(encoding="utf-8")
        self.assertEqual(MODULE.validate_checkpoint_type_source(source), [])

    def test_actor_pointer_is_rejected(self) -> None:
        source = """
        USTRUCT()
        struct FTestCheckpoint
        {
            AActor* Actor;
        };
        """
        errors = MODULE.validate_checkpoint_type_source(source)
        self.assertTrue(any("forbidden runtime type" in error for error in errors))


class RepositoryIntegrationTests(unittest.TestCase):
    def test_current_repository_passes(self) -> None:
        root = Path(__file__).resolve().parents[2]
        errors = MODULE.validate_project(root)
        self.assertEqual(errors, "\n".join(errors) if False else [])

    def test_runtime_header_is_native_only(self) -> None:
        root = Path(__file__).resolve().parents[2]
        header = (root / "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h").read_text(encoding="utf-8")
        code = MODULE.strip_cpp_comments(header)
        self.assertNotIn("UFUNCTION(", code)
        self.assertNotIn("UFUNCTION (", code)


if __name__ == "__main__":
    unittest.main()
