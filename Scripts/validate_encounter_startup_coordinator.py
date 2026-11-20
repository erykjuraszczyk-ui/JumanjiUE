#!/usr/bin/env python3
"""Static source-contract validator for Stage 36 encounter startup recovery.

This verifies server startup wiring and the bounded authenticated-participant
barrier. It does not compile Unreal C++, run UHT, contact the backend or perform PIE.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REQUIRED: dict[str, tuple[str, ...]] = {
    "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h": (
        "TryCommitStagedRestore",
        "IsWaitingForRestoreParticipants",
        "RestoreParticipantWaitTimerHandle",
        "bWaitingForRestoreParticipants",
    ),
    "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp": (
        "TryCommitStagedRestore(",
        "RESTORE_PARTICIPANT_NOT_CONNECTED_OR_AUTHENTICATED",
        "HandleRestoreParticipantWaitExpired",
        "RestoreParticipantWaitTimerHandle",
        "180.0f",
        "StageVerifiedBackendResponse(",
        "BeginRestoreStaging(",
        "EndEncounterCheckpointSession(ActiveEncounterId)",
        "CommitStagedRestore(EncounterDirector, Error)",
    ),
    "Source/JumanjiUE/Public/JumanjiGameMode.h": (
        "TryStartEncounterCheckpointRecovery",
        "bCheckpointRestoreStarted",
    ),
    "Source/JumanjiUE/Private/JumanjiGameMode.cpp": (
        "TryStartEncounterCheckpointRecovery();",
        "TActorIterator<AJumanjiWorldEncounterDirector>",
        "World->GetPersistentLevel()",
        "PersistentDirectorCount != 1",
        "IsProfilePersistenceEnabled()",
        "HasStagedRestoreCandidate()",
        "TryCommitStagedRestore(EncounterDirector)",
        "BeginCheckpointRestore()",
        "ProfileSubsystem->LoadProfile(ProfileId, Snapshot)",
        "ApplyProfileSnapshot(Snapshot)",
    ),
    "Docs/STAGE_36_ENCOUNTER_STARTUP_COORDINATOR_PARTICIPANT_BARRIER.md": (
        "PostLogin",
        "180 sekund",
        "persistent level",
        "participantKey",
        "GameState",
        "lease",
    ),
}

FORBIDDEN: dict[str, tuple[str, ...]] = {
    "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h": (
        "UFUNCTION(",
    ),
    "Source/JumanjiUE/Public/JumanjiGameMode.h": (
        "UFUNCTION(",
    ),
    "Source/JumanjiUE/Private/JumanjiGameMode.cpp": (
        "ServerBeginCheckpointRestore",
        "ServerRequestCheckpointRestore",
        "ApplyProfileSnapshot(Candidate",
    ),
}

WORKFLOW_FILES = (
    ".github/workflows/encounter-startup-coordinator.yml",
    "Scripts/Tests/test_validate_encounter_startup_coordinator.py",
)


def read_source(root: Path, relative: str, errors: list[str]) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"{relative}: cannot read file: {exc}")
        return ""


def extract_function(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        return ""
    opening = source.find("{", start + len(signature))
    if opening < 0:
        return ""
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    return ""


def extract_if_block(source: str, marker: str) -> str:
    start = source.find(marker)
    if start < 0:
        return ""
    opening = source.find("{", start + len(marker))
    if opening < 0:
        return ""
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
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
                errors.append(f"{path}: forbidden client/reflected recovery marker {marker!r}")

    runtime = sources["Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"]
    retry = extract_function(
        runtime,
        "bool UJumanjiEncounterCheckpointRuntimeSubsystem::TryCommitStagedRestore(",
    )
    pending = extract_if_block(
        retry,
        'if (Error == TEXT("RESTORE_PARTICIPANT_NOT_CONNECTED_OR_AUTHENTICATED"))',
    )
    if not pending:
        errors.append("TryCommitStagedRestore: missing transient participant-wait branch")
    else:
        if "SetTimer(" not in pending or "180.0f" not in pending:
            errors.append("Participant wait must use a bounded 180-second timer")
        if "bHasStagedRestoreCandidate = false" in pending:
            errors.append("A missing participant must not discard the verified staged candidate")
        if "EndEncounterCheckpointSession" in pending:
            errors.append("A missing participant must not release the lease before the wait expires")

    read_handler = extract_function(
        runtime,
        "void UJumanjiEncounterCheckpointRuntimeSubsystem::HandleCheckpointRead(",
    )
    read_stage = read_handler.find("StageVerifiedBackendResponse(")
    read_commit = read_handler.find("TryCommitStagedRestore(")
    if read_stage < 0 or read_commit < 0 or read_commit < read_stage:
        errors.append("HandleCheckpointRead must stage and validate the backend candidate before attempting commit")

    timeout = extract_function(
        runtime,
        "void UJumanjiEncounterCheckpointRuntimeSubsystem::HandleRestoreParticipantWaitExpired(",
    )
    for marker in (
        "bWaitingForRestoreParticipants = false",
        "bHasStagedRestoreCandidate = false",
        "StagedRestoreCandidate = FJumanjiEncounterCheckpointRestoreCandidate()",
        "EndEncounterCheckpointSession(ActiveEncounterId)",
    ):
        if marker not in timeout:
            errors.append(f"Restore participant timeout handler is missing {marker!r}")

    game_mode = sources["Source/JumanjiUE/Private/JumanjiGameMode.cpp"]
    post_login = extract_function(game_mode, "void AJumanjiGameMode::PostLogin(")
    load_at = post_login.find("ProfileSubsystem->LoadProfile(ProfileId, Snapshot)")
    apply_at = post_login.find("ApplyProfileSnapshot(Snapshot)")
    reconnect_at = post_login.find("TryRejoinPlayer(")
    coordinator_at = post_login.find("TryStartEncounterCheckpointRecovery();")
    if min(load_at, apply_at, reconnect_at, coordinator_at) < 0:
        errors.append("PostLogin must contain profile restore, reconnect handling and recovery coordination")
    elif not (load_at < apply_at < coordinator_at and reconnect_at < coordinator_at):
        errors.append("Recovery coordinator must run after profile application and reconnect handling")

    coordinator = extract_function(
        game_mode,
        "void AJumanjiGameMode::TryStartEncounterCheckpointRecovery(",
    )
    ordered = (
        "HasStagedRestoreCandidate()",
        "TryCommitStagedRestore(EncounterDirector)",
        "if (bCheckpointRestoreStarted)",
        "BeginCheckpointRestore()",
    )
    positions = [coordinator.find(marker) for marker in ordered]
    if any(position < 0 for position in positions):
        errors.append("GameMode coordinator is missing staged-candidate retry or one-shot startup")
    elif positions != sorted(positions):
        errors.append("GameMode must retry a staged candidate before applying the one-shot startup guard")

    if "TActorIterator<AJumanjiWorldEncounterDirector>" in coordinator:
        if "It->GetLevel() != PersistentLevel" not in coordinator:
            errors.append("Coordinator must accept only the persistent-level encounter director")
        if "PersistentDirectorCount != 1" not in coordinator:
            errors.append("Coordinator must reject ambiguous multiple persistent-level directors")
        if "IsProfilePersistenceEnabled()" not in coordinator or "GetPersistentProfileId().IsEmpty()" not in coordinator:
            errors.append("Coordinator must require at least one identity-bound, persistence-enabled profile")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())

    if errors:
        print(f"Encounter startup coordinator validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Encounter startup coordinator validation: PASS")
    print(f"Validated {len(REQUIRED)} source/doc contracts, {len(FORBIDDEN)} forbidden-marker groups and {len(WORKFLOW_FILES)} workflow/test files.")
    print("Note: static validation only; Unreal compilation, UHT, PIE and live backend recovery are not executed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
