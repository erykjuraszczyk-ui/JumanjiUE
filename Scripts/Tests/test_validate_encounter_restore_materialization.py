from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_encounter_restore_materialization as MODULE


class EncounterRestoreMaterializationTests(unittest.TestCase):
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

    def test_repository_restore_contract_passes(self) -> None:
        root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(root), [])

    def test_profile_revision_rollback_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "CurrentProfileRevision < Candidate.ParticipantProfileRevisions[Index]",
                "CurrentProfileRevision > Candidate.ParticipantProfileRevisions[Index]",
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("CurrentProfileRevision" in error for error in errors))

    def test_profile_persistence_must_be_enabled(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace("|| !PlayerState->IsProfilePersistenceEnabled()", "", 1)
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("IsProfilePersistenceEnabled" in error for error in errors))

    def test_reward_ledger_check_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "HasProcessedProfileTransaction(Candidate.RewardTransactionId)",
                "HasSomeOtherReceipt(Candidate.RewardTransactionId)",
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("HasProcessedProfileTransaction" in error for error in errors))

    def test_client_exposed_restore_api_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Public/JumanjiWorldEncounterDirector.h"
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "bool BeginCheckpointRestore();",
                "UFUNCTION(BlueprintCallable)\n    bool BeginCheckpointRestore();",
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("native server-only" in error for error in errors))

    def test_actor_replication_must_follow_game_state_commit(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "    if (!GameState->ApplyEncounterCheckpointRestore(",
                "    for (AJumanjiEnemyCharacter* Enemy : StagedEnemies) { Enemy->SetReplicates(true); }\n"
                "    if (!GameState->ApplyEncounterCheckpointRestore(",
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("order must be" in error for error in errors))

    def test_game_state_fields_must_be_assigned_before_delegate(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiGameState.cpp"
            source = path.read_text(encoding="utf-8")
            first_assignment = "    ActiveBoardEventId = bTrials ? BoardEventId : NAME_None;"
            source = source.replace(first_assignment, "", 1)
            source = source.replace(
                "    OnBoardEventStateChanged.Broadcast",
                first_assignment + "\n    OnBoardEventStateChanged.Broadcast",
                1,
            )
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("before the first delegate" in error for error in errors))

    def test_restored_session_must_adopt_checkpoint_session_id(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._copy_fixture(temp_dir)
            path = root / "Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp"
            source = path.read_text(encoding="utf-8")
            source = source.replace("ActiveSessionId = RestoredSessionId;", "ActiveSessionId = FGuid::NewGuid();", 1)
            path.write_text(source, encoding="utf-8")
            errors = MODULE.validate_project(root)
            self.assertTrue(any("verified session UUID" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
