#!/usr/bin/env python3
"""Validate server-side encounter replay/recovery source contracts.

This is a static source guard. It does not compile Unreal C++, run PIE, or
prove runtime replication and reward behavior.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REQUIRED_TOKENS = {
    "Source/JumanjiUE/Private/JumanjiGameState.cpp": (
        "WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Failed",
        "WorldEncounterState.EncounterId == EncounterId",
        "ResetAdventureChapterForEncounterRetry",
        "AdventureObjectiveSet.Objectives.Reset()",
        "ProcessedTransactionIds",
    ),
    "Source/JumanjiUE/Private/JumanjiTrialActor.cpp": (
        "CanResetForEncounterRetry",
        "TrialState = EJumanjiTrialState::Dormant",
        "CurrentStepIndex = 0",
        "TrialRevision = TrialRevision < MAX_int32 ? TrialRevision + 1 : 1",
    ),
    "Source/JumanjiUE/Private/JumanjiAdventureChapterDirector.cpp": (
        "ResetChapterForEncounterRetry",
        "EJumanjiWorldEncounterPhase::Failed",
        "BoardSubsystem->EndBoardEvent(BoardEventId)",
        "TrialActor->ResetForEncounterRetry()",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp": (
        "PreviousEncounterState.Phase == EJumanjiWorldEncounterPhase::Failed",
        "AdventureChapterDirector->ResetChapterForEncounterRetry(EncounterId)",
        "SpawnedEncounterActors.Add(Enemy)",
        "SpawnedEncounterActors.Add(ActiveBoss)",
        "for (AActor* SpawnedActor : SpawnedEncounterActors)",
        "SpawnedActor->Destroy()",
        "PreviousEncounterState.Phase != EJumanjiWorldEncounterPhase::Inactive",
    ),
    "Source/JumanjiUE/Public/JumanjiWorldEncounterDirector.h": (
        "bool CanRetryEncounter() const;",
        "TArray<TObjectPtr<AActor>> SpawnedEncounterActors;",
    ),
    "Docs/STAGE_29_ENCOUNTER_REPLAY_RECOVERY.md": (
        "Completed",
        "ProcessedTransactionIds",
        "Persistent profile",
        "Multiplayer",
    ),
}


def validate_sources(project_root: Path) -> list[str]:
    errors: list[str] = []
    loaded: dict[str, str] = {}

    for relative_path, required in REQUIRED_TOKENS.items():
        path = project_root / relative_path
        if not path.is_file():
            errors.append(f"Missing required file: {relative_path}")
            continue
        try:
            content = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            errors.append(f"Cannot read {relative_path}: {exc}")
            continue
        loaded[relative_path] = content
        for token in required:
            if token not in content:
                errors.append(f"{relative_path}: missing required recovery contract: {token}")

    game_state = loaded.get("Source/JumanjiUE/Private/JumanjiGameState.cpp", "")
    reset_start = game_state.find("bool AJumanjiGameState::ResetAdventureChapterForEncounterRetry(")
    reset_end = game_state.find("bool AJumanjiGameState::InitializeAdventureObjectives(", reset_start)
    if reset_start < 0 or reset_end < 0:
        errors.append("GameState: cannot isolate ResetAdventureChapterForEncounterRetry body")
    else:
        reset_body = game_state[reset_start:reset_end]
        forbidden = (
            "MarkAdventureChapterCompleted",
            "ApplyProfileTransaction",
            "ProcessedTransactionIds.Add",
            "PersistProfileSnapshot",
            "GrantChapterCompletionReward",
            "GrantWorldEncounterCompletionReward",
        )
        for token in forbidden:
            if token in reset_body:
                errors.append(f"GameState retry reset must not modify persistent progression: {token}")

    director = loaded.get("Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp", "")
    if "CompleteWorldEncounter(EncounterId)" in director:
        # Retry must not be reachable from the completion path.
        start = director.find("bool AJumanjiWorldEncounterDirector::StartEncounter(")
        end = director.find("void AJumanjiWorldEncounterDirector::HandleAdventureChapterChanged(", start)
        start_body = director[start:end]
        if "PreviousEncounterState.Phase != EJumanjiWorldEncounterPhase::Inactive" not in start_body:
            errors.append("Encounter start must keep Completed/other terminal states non-retryable")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_sources(args.project_root.resolve())

    if errors:
        print(f"Emerald Jungle encounter recovery validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Emerald Jungle encounter recovery validation: PASS")
    print("Static retry, terminal-state, trial-reset and persistent-progression guards are present.")
    print("Note: PASS does not replace Unreal compilation, multiplayer PIE or device tests.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
