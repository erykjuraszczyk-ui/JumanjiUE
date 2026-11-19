from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_encounter_checkpoint_runtime as MODULE


class EncounterCheckpointRuntimeTests(unittest.TestCase):
    def _copy_fixture(self, temp_dir: str) -> Path:
        source_root = Path(__file__).resolve().parents[2]
        target_root = Path(temp_dir)
        paths = set(MODULE.REQUIRED) | set(MODULE.FORBIDDEN) | set(MODULE.REQUIRED_WORKFLOW_FILES)
        for relative in paths:
            source = source_root / relative
            destination = target_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        return target_root

    def test_repository_runtime_contract_passes(self) -> None:
        root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(root), [])

    def test_missing_safe_point_hook_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace("EJumanjiEncounterCheckpointSafePoint::WaveBoundary", "EJumanjiEncounterCheckpointSafePoint::Missing")
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("WaveBoundary" in error for error in errors))

    def test_raw_profile_id_serialization_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
            source = path.read_text(encoding="utf-8") + '\nPayload->SetStringField(TEXT("profileId"), RawProfileId);\n'
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("profileId" in error for error in errors))

    def test_actor_transform_serialization_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
            source = path.read_text(encoding="utf-8") + "\nconst FTransform Bad = Actor->GetActorTransform();\n"
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("GetActorTransform" in error for error in errors))

    def test_restore_candidate_cannot_contain_actor_references(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h"
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "FName EncounterId = NAME_None;",
                "FName EncounterId = NAME_None;\\n    TObjectPtr<AActor> ForbiddenActor = nullptr;",
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("forbidden runtime field" in error for error in errors))

    def test_blueprint_callable_checkpoint_api_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h"
            source = path.read_text(encoding="utf-8")
            source = source.replace("bool BeginEncounterCheckpointSession", "UFUNCTION(BlueprintCallable)\\n    bool BeginEncounterCheckpointSession", 1)
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("native-only" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
