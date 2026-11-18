from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_emerald_jungle_encounter_reconnect as MODULE


class EncounterReconnectValidationTests(unittest.TestCase):
    def _fixture(self, root: Path) -> Path:
        for relative, tokens in MODULE.REQUIRED_TOKENS.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            content = "\n".join(tokens) + "\n"
            if relative.endswith("JumanjiPlayerState.cpp"):
                content += (
                    "FJumanjiPlayerProfileSnapshot AJumanjiPlayerState::BuildProfileSnapshot() const {\n"
                    " FJumanjiPlayerProfileSnapshot Snapshot;\n"
                    " Snapshot.ProcessedTransactionIds = ProcessedTransactionIds;\n"
                    " return Snapshot;\n"
                    "}\n"
                    "void AJumanjiPlayerState::PersistProfileSnapshot() { }\n"
                )
            if relative.endswith("JumanjiGameMode.cpp"):
                content += (
                    "void AJumanjiGameMode::PostLogin(APlayerController* NewPlayer) {\n"
                    " ProfileSubsystem->LoadProfile(ProfileId, Snapshot);\n"
                    " SessionSubsystem->TryRejoinPlayer(JumanjiPlayerState, ReconnectState);\n"
                    "}\n"
                    "void AJumanjiGameMode::Logout(AController* Exiting) {\n"
                    " SessionSubsystem->MarkPlayerDisconnected(JumanjiPlayerState);\n"
                    " JumanjiPlayerState->RequestProfileSave();\n"
                    "}\n"
                )
            if relative.endswith("JumanjiWorldEncounterDirector.cpp"):
                content += (
                    "void AJumanjiWorldEncounterDirector::CompleteEncounter() { SessionSubsystem->EndEncounterSession(EncounterId); }\n"
                    "void AJumanjiWorldEncounterDirector::FailEncounter(FName ReasonId) { SessionSubsystem->EndEncounterSession(EncounterId); }\n"
                    "void AJumanjiWorldEncounterDirector::EndPlay() { SessionSubsystem->EndEncounterSession(EncounterId); }\n"
                )
            path.write_text(content, encoding="utf-8")
        return root

    def test_repository_sources_pass(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_sources(project_root), [])

    def test_live_actor_storage_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._fixture(Path(tmp))
            path = root / "Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp"
            path.write_text(
                path.read_text(encoding="utf-8") + "\nTObjectPtr<AActor> CachedBoss;\n",
                encoding="utf-8",
            )
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("must not serialize/store live actors" in e for e in errors))

    def test_profile_snapshot_cannot_contain_reconnect_state(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._fixture(Path(tmp))
            path = root / "Source/JumanjiUE/Private/JumanjiPlayerState.cpp"
            content = path.read_text(encoding="utf-8")
            content = content.replace(
                "Snapshot.ProcessedTransactionIds = ProcessedTransactionIds;",
                "Snapshot.ProcessedTransactionIds = ProcessedTransactionIds;\n Snapshot.EncounterReconnectState = EncounterReconnectState;",
            )
            path.write_text(content, encoding="utf-8")
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("must not be written to the profile snapshot" in e for e in errors))

    def test_logout_must_mark_disconnect_before_profile_save(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._fixture(Path(tmp))
            path = root / "Source/JumanjiUE/Private/JumanjiGameMode.cpp"
            content = path.read_text(encoding="utf-8")
            content = content.replace(
                "SessionSubsystem->MarkPlayerDisconnected(JumanjiPlayerState);\n JumanjiPlayerState->RequestProfileSave();",
                "JumanjiPlayerState->RequestProfileSave();\n SessionSubsystem->MarkPlayerDisconnected(JumanjiPlayerState);",
            )
            path.write_text(content, encoding="utf-8")
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("mark the encounter participant disconnected" in e for e in errors))

    def test_post_login_restores_profile_before_reconnect(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._fixture(Path(tmp))
            path = root / "Source/JumanjiUE/Private/JumanjiGameMode.cpp"
            content = path.read_text(encoding="utf-8")
            content = content.replace(
                "ProfileSubsystem->LoadProfile(ProfileId, Snapshot);\n SessionSubsystem->TryRejoinPlayer",
                "SessionSubsystem->TryRejoinPlayer(JumanjiPlayerState, ReconnectState);\n ProfileSubsystem->LoadProfile(ProfileId, Snapshot);",
            )
            path.write_text(content, encoding="utf-8")
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("restore the persistent profile before attempting encounter rejoin" in e for e in errors))


if __name__ == "__main__":
    unittest.main()
