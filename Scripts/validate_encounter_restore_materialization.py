#!/usr/bin/env python3
"""Static source-contract validator for Stage 35 checkpoint materialization.

This check does not compile Unreal C++, run UHT, start a backend or perform PIE.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REQUIRED: dict[str, tuple[str, ...]] = {
    "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h": (
        "CommitStagedRestore",
        "bRestoreCommitInProgress",
        "FJumanjiEncounterCheckpointRestoreCandidate",
        "ParticipantKeys",
        "ParticipantProfileRevisions",
    ),
    "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp": (
        "CommitStagedRestore(",
        "BuildOpaqueParticipantKey(ProfileId, ParticipantSalt, ParticipantKey)",
        "CurrentProfileRevision < Candidate.ParticipantProfileRevisions[Index]",
        "HasCompletedAdventureChapter(Candidate.ChapterId)",
        "HasProcessedProfileTransaction(Candidate.RewardTransactionId)",
        "GrantWorldEncounterCompletionReward(",
        "MaterializeCheckpointRestore(Candidate, Participants)",
        "ActiveSessionId = Candidate.SessionId",
        "RESTORE_REWARD_RECEIPT_NOT_COMMITTED",
        "bRestoreCommitInProgress",
        "!PlayerState->IsProfilePersistenceEnabled()",
        "ProfileRevision > MAX_int32",
        "EncounterRevision > MAX_int32",
    ),
    "Source/JumanjiUE/Public/JumanjiPlayerState.h": (
        "IsProfilePersistenceEnabled() const",
    ),
    "Source/JumanjiUE/Public/JumanjiWorldEncounterDirector.h": (
        "BeginCheckpointRestore()",
        "MaterializeCheckpointRestore(",
        "FJumanjiEncounterCheckpointRestoreCandidate",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp": (
        "BeginCheckpointRestore()",
        "BeginRestoreStaging(this)",
        "MaterializeCheckpointRestore(",
        "BeginRestoredEncounterSession(",
        "ApplyCheckpointTrialStates(",
        "ApplyEncounterCheckpointRestore(",
        "SetReplicates(false)",
        "SetReplicates(true)",
        "DestroyStagedActors",
        "BOSS_PHASE_02",
        "BOSS_PHASE_03",
    ),
    "Source/JumanjiUE/Public/JumanjiWorldEncounterSessionSubsystem.h": (
        "BeginRestoredEncounterSession(",
        "RestoredSessionId",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp": (
        "BeginRestoredEncounterSession(",
        "RestoredSessionId.IsValid()",
        "ActiveSessionId = RestoredSessionId",
        "UniqueProfileIds.Contains",
    ),
    "Source/JumanjiUE/Public/JumanjiTrialActor.h": (
        "CanApplyCheckpointRestoreState() const",
        "ApplyCheckpointRestoreState(bool bCompleted)",
        "PublishCheckpointRestoreState()",
    ),
    "Source/JumanjiUE/Private/JumanjiTrialActor.cpp": (
        "CanApplyCheckpointRestoreState() const",
        "TrialState = bCompleted ? EJumanjiTrialState::Completed : EJumanjiTrialState::Available",
        "CurrentStepIndex = bCompleted ? Steps.Num() : 0",
        "PublishCheckpointRestoreState()",
    ),
    "Source/JumanjiUE/Public/JumanjiAdventureChapterDirector.h": (
        "BuildCheckpointRestoreObjectives(",
        "CanApplyCheckpointTrialStates(",
        "ApplyCheckpointTrialStates(",
        "BindForCheckpointRestoredActiveChapter()",
    ),
    "Source/JumanjiUE/Private/JumanjiAdventureChapterDirector.cpp": (
        "BuildCheckpointRestoreObjectives(",
        "CanApplyCheckpointTrialStates(",
        "ApplyCheckpointTrialStates(",
        "PublishCheckpointTrialStates()",
        "BindForCheckpointRestoredActiveChapter()",
        "CompletedTrialSet.Num() == CompletedObjectiveSet.Num()",
        "TrialObjectiveIds.Contains(ObjectiveId)",
        "MatchingObjective->RequiredProgress != Trial->GetStepCount()",
        "partial TrialBoundary",
    ),
    "Source/JumanjiUE/Public/JumanjiGameState.h": (
        "ApplyEncounterCheckpointRestore(",
    ),
    "Source/JumanjiUE/Private/JumanjiGameState.cpp": (
        "ApplyEncounterCheckpointRestore(",
        "ActiveBoardEventId = bTrials ? BoardEventId : NAME_None",
        "AdventureChapterState = MoveTemp(NewChapterState)",
        "AdventureObjectiveSet = MoveTemp(NewObjectiveSet)",
        "WorldEncounterState = MoveTemp(NewEncounterState)",
        "OnBoardEventStateChanged.Broadcast",
        "OnWorldEncounterChanged.Broadcast",
    ),
    "Source/JumanjiUE/Public/JumanjiBossCharacter.h": (
        "ApplyCheckpointRestoreState(",
        "bApplyingCheckpointRestoreState",
    ),
    "Source/JumanjiUE/Private/JumanjiBossCharacter.cpp": (
        "ApplyCheckpointRestoreState(",
        "Attributes->GetMaxHealth() * HealthFraction",
        "BossPhase = RestoredPhase",
        "BossPhaseRevision = RestoredPhaseRevision",
        "HealthFraction > PhaseTwoHealthThreshold",
        "bApplyingCheckpointRestoreState",
    ),
    "Docs/STAGE_35_ENCOUNTER_RESTORE_MATERIALIZATION_ATOMIC_PUBLISH.md": (
        "CommitStagedRestore",
        "profileRevision",
        "RewardPending",
        "BeginCheckpointRestore",
        "nie jest gwarancją jednoczesnego otrzymania pakietu",
        "SQLite WAL",
    ),
}

FORBIDDEN: dict[str, tuple[str, ...]] = {
    "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp": (
        'Payload->SetStringField(TEXT("profileId")',
        'Payload->SetStringField(TEXT("rawProfileId")',
        'Payload->SetObjectField(TEXT("inventory")',
        'Payload->SetObjectField(TEXT("wallet")',
        'SetStringField(TEXT("participantProfileId")',
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp": (
        "UFUNCTION(Server",
        "ServerBeginCheckpointRestore",
    ),
}

WORKFLOW_FILES = (
    ".github/workflows/encounter-restore-materialization.yml",
    "Scripts/Tests/test_validate_encounter_restore_materialization.py",
)


def read_source(root: Path, relative: str, errors: list[str]) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"{relative}: cannot read file: {exc}")
        return ""


def extract_function(source: str, signature: str) -> str:
    """Return a C++ function body using brace depth, or an empty string."""
    start = source.find(signature)
    if start < 0:
        return ""
    opening = source.find("{", start + len(signature))
    if opening < 0:
        return ""
    depth = 0
    for index in range(opening, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    return ""


def validate_project(root: Path) -> list[str]:
    errors: list[str] = []
    paths = set(REQUIRED) | set(FORBIDDEN) | set(WORKFLOW_FILES)
    sources = {path: read_source(root, path, errors) for path in sorted(paths)}

    for path, markers in REQUIRED.items():
        for marker in markers:
            if marker not in sources[path]:
                errors.append(f"{path}: missing required marker {marker!r}")

    for path, markers in FORBIDDEN.items():
        for marker in markers:
            if marker in sources[path]:
                errors.append(f"{path}: forbidden client/identity serialization marker {marker!r}")

    runtime_header = sources["Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h"]
    candidate_start = runtime_header.find(
        "struct JUMANJIUE_API FJumanjiEncounterCheckpointRestoreCandidate"
    )
    candidate_end = runtime_header.find("/**", candidate_start + 1)
    if candidate_start < 0:
        errors.append("Restore candidate struct is missing")
    else:
        candidate = runtime_header[candidate_start:candidate_end if candidate_end > candidate_start else len(runtime_header)]
        for forbidden in ("UPROPERTY(", "TObjectPtr<", "AActor*", "APawn*", "FTransform", "FVector"):
            if forbidden in candidate:
                errors.append(f"Restore candidate contains forbidden live-world field {forbidden!r}")

    director_header = sources["Source/JumanjiUE/Public/JumanjiWorldEncounterDirector.h"]
    for name in ("BeginCheckpointRestore", "MaterializeCheckpointRestore"):
        if f"bool {name}" not in director_header:
            errors.append(f"{name} declaration is missing")
        reflected_declaration = re.search(
            rf"UFUNCTION\s*\([^)]*\)\s*bool\s+{re.escape(name)}\s*\(",
            director_header,
        )
        if reflected_declaration:
            errors.append(f"{name} must remain a native server-only API, not a Blueprint/RPC entry point")

    runtime = sources["Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"]
    commit = extract_function(
        runtime,
        "bool UJumanjiEncounterCheckpointRuntimeSubsystem::CommitStagedRestore(",
    )
    for marker in (
        "BuildOpaqueParticipantKey",
        "ParticipantKeys",
        "ParticipantProfileRevisions",
        "CurrentProfileRevision < Candidate.ParticipantProfileRevisions[Index]",
        "HasCompletedAdventureChapter",
        "HasProcessedProfileTransaction",
        "GrantWorldEncounterCompletionReward",
        "MaterializeCheckpointRestore",
        "ActiveSessionId = Candidate.SessionId",
    ):
        if marker not in commit:
            errors.append(f"CommitStagedRestore: missing required invariant {marker!r}")
    if "SetPersistentProfileId(" in commit or "ApplyProfileSnapshot(" in commit:
        errors.append("CommitStagedRestore must not synthesize or roll back profile identity/snapshots")

    director = sources["Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp"]
    materialize = extract_function(
        director,
        "bool AJumanjiWorldEncounterDirector::MaterializeCheckpointRestore(",
    )
    ordered_markers = (
        "SetReplicates(false)",
        "BeginRestoredEncounterSession(",
        "ApplyCheckpointTrialStates(",
        "ApplyEncounterCheckpointRestore(",
        "SetReplicates(true)",
    )
    positions = [materialize.find(marker) for marker in ordered_markers]
    if any(position < 0 for position in positions):
        errors.append("MaterializeCheckpointRestore is missing a required staging/commit/replication step")
    elif positions != sorted(positions):
        errors.append("MaterializeCheckpointRestore order must be: hide actors, create session, apply trials, commit GameState, replicate actors")
    if "DestroyStagedActors" not in materialize:
        errors.append("MaterializeCheckpointRestore must destroy staged actors on pre-commit failure")

    game_state = sources["Source/JumanjiUE/Private/JumanjiGameState.cpp"]
    commit_state = extract_function(game_state, "bool AJumanjiGameState::ApplyEncounterCheckpointRestore(")
    assignment_markers = (
        "ActiveBoardEventId = bTrials ? BoardEventId : NAME_None",
        "AdventureChapterState = MoveTemp(NewChapterState)",
        "AdventureObjectiveSet = MoveTemp(NewObjectiveSet)",
        "WorldEncounterState = MoveTemp(NewEncounterState)",
    )
    broadcast_at = commit_state.find("OnBoardEventStateChanged.Broadcast")
    if not commit_state or broadcast_at < 0:
        errors.append("GameState restore commit function/broadcast is missing")
    else:
        for marker in assignment_markers:
            if commit_state.find(marker) < 0 or commit_state.find(marker) > broadcast_at:
                errors.append(f"GameState must assign {marker!r} before the first delegate broadcast")

    session = extract_function(
        sources["Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp"],
        "bool UJumanjiWorldEncounterSessionSubsystem::BeginRestoredEncounterSession(",
    )
    if "ActiveSessionId = RestoredSessionId" not in session or "PlayerState->HasAuthority()" not in session:
        errors.append("Restored session must adopt the verified session UUID and accept only server PlayerStates")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())
    if errors:
        print(f"Encounter restore materialization validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Encounter restore materialization validation: PASS")
    print(f"Validated {len(REQUIRED)} source/doc contracts and {len(FORBIDDEN)} forbidden-marker groups.")
    print("Note: static source validation only; Unreal build/UHT/PIE and real backend restore are not executed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
