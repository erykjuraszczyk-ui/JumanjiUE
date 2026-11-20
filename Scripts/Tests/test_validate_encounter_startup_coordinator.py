from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_encounter_startup_coordinator as MODULE


class EncounterStartupCoordinatorTests(unittest.TestCase):
    def _copy_fixture(self, temp_dir: str) -> Path:
        source_root = Path(__file__).resolve().parents[2]
        target_root = Path(temp_dir)
        paths = set(MODULE.REQUIRED) | set(MODULE.FORBIDDEN) | set(MODULE.WORKFLOW_FILES)
        for relative in paths:
            source = source_root / relative
            destination = target_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        return target_root

    def test_repository_startup_contract_passes(self) -> None:
        root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(root), [])

    def test_unbounded_participant_wait_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace("180.0f", "0.0f", 1)
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("bounded 180-second timer" in error for error in errors))

    def test_missing_participant_must_not_discard_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                'if (Error == TEXT("RESTORE_PARTICIPANT_NOT_CONNECTED_OR_AUTHENTICATED"))',
                'if (Error == TEXT("RESTORE_PARTICIPANT_INVALID"))',
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("transient participant-wait branch" in error for error in errors))

    def test_coordinator_requires_persistent_level_director(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiGameMode.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace("It->GetLevel() != PersistentLevel", "false", 1)
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("persistent-level encounter director" in error for error in errors))

    def test_staged_commit_must_precede_one_shot_guard(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiGameMode.cpp"
            source = path.read_text(encoding="utf-8")
            retry = source.index("    if (CheckpointSubsystem->HasStagedRestoreCandidate())")
            guard_block = (
                "    if (bCheckpointRestoreStarted)\\n"
                "    {\\n"
                "        return;\\n"
                "    }\\n\\n"
            )
            source = source.replace(guard_block, "", 1)
            source = source[:retry] + guard_block + source[retry:]
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("before applying the one-shot startup guard" in error for error in errors))

    def test_post_login_coordinator_runs_after_profile_restore(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiGameMode.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "    TryStartEncounterCheckpointRecovery();",
                "",
                1,
            )
            source = source.replace(
                "    JumanjiPlayerState->SetPersistentProfileId(ProfileId);",
                "    TryStartEncounterCheckpointRecovery();\n    JumanjiPlayerState->SetPersistentProfileId(ProfileId);",
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("after profile application" in error for error in errors))

    def test_runtime_restore_api_is_not_blueprint_reflected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h"
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "bool TryCommitStagedRestore(AJumanjiWorldEncounterDirector* EncounterDirector);",
                "UFUNCTION(BlueprintCallable)\n    bool TryCommitStagedRestore(AJumanjiWorldEncounterDirector* EncounterDirector);",
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("forbidden client/reflected" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
