from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_emerald_jungle_encounter_recovery as MODULE


class EncounterRecoverySourceTests(unittest.TestCase):
    def _write_fixture(self, root: Path, remove_token: tuple[str, str] | None = None) -> Path:
        for relative, tokens in MODULE.REQUIRED_TOKENS.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            content = "\n".join(tokens) + "\n"
            if remove_token and relative == remove_token[0]:
                content = content.replace(remove_token[1], "")
            if relative == "Source/JumanjiUE/Private/JumanjiGameState.cpp":
                content += (
                    "bool AJumanjiGameState::ResetAdventureChapterForEncounterRetry(FName EncounterId) {\n"
                    "    AdventureObjectiveSet.Objectives.Reset();\n"
                    "}\n"
                    "bool AJumanjiGameState::InitializeAdventureObjectives() { }\n"
                )
            if relative == "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp":
                content += (
                    "bool AJumanjiWorldEncounterDirector::StartEncounter() {\n"
                    "    if (PreviousEncounterState.Phase != EJumanjiWorldEncounterPhase::Inactive) return false;\n"
                    "}\n"
                )
            path.write_text(content, encoding="utf-8")
        return root

    def test_repository_sources_pass(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_sources(project_root), [])

    def test_missing_actor_tracking_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._write_fixture(
                Path(temp_dir),
                ("Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp",
                 "SpawnedEncounterActors.Add(ActiveBoss)"),
            )
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("SpawnedEncounterActors.Add(ActiveBoss)" in error for error in errors))

    def test_profile_mutation_inside_reset_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._write_fixture(Path(temp_dir))
            path = root / "Source/JumanjiUE/Private/JumanjiGameState.cpp"
            content = path.read_text(encoding="utf-8")
            content = content.replace(
                "AdventureObjectiveSet.Objectives.Reset();",
                "AdventureObjectiveSet.Objectives.Reset();\n    ApplyProfileTransaction(Transaction);",
            )
            path.write_text(content, encoding="utf-8")
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("must not modify persistent progression" in error for error in errors))

    def test_missing_retry_phase_guard_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = self._write_fixture(Path(temp_dir))
            path = root / "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp"
            path.write_text("bool AJumanjiWorldEncounterDirector::StartEncounter() { }", encoding="utf-8")
            errors = MODULE.validate_sources(root)
            self.assertTrue(any("PreviousEncounterState.Phase != EJumanjiWorldEncounterPhase::Inactive" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
