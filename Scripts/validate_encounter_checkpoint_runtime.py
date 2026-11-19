#!/usr/bin/env python3
"""Static validation for Stage 34 encounter checkpoint runtime wiring.

This is a source-contract check. It does not compile Unreal C++, run UHT,
verify Ed25519 cryptography, connect to the backend or restore a live world.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REQUIRED: dict[str, tuple[str, ...]] = {
    "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h": (
        "EJumanjiEncounterCheckpointSafePoint",
        "TrialBoundary",
        "WaveBoundary",
        "BossPhaseBoundary",
        "RewardPending",
        "FJumanjiEncounterCheckpointRestoreCandidate",
        "BeginEncounterCheckpointSession",
        "CaptureSafePoint",
        "BeginRestoreStaging",
        "HasStagedRestoreCandidate",
    ),
    "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp": (
        "BeginEncounterCheckpointSession",
        "BackendSubsystem->AcquireLease",
        "BackendSubsystem->RenewLease",
        "BackendSubsystem->WriteCheckpoint",
        "BackendSubsystem->ReadCheckpoint",
        "BackendSubsystem->ReleaseLease",
        "JUMANJI_SERVER_BUILD_ID",
        "JUMANJI_RULESET_VERSION",
        "JUMANJI_WORLD_CONTENT_VERSION",
        "JUMANJI_CHECKPOINT_PARTICIPANT_SALT",
        "FIoHash::HashBuffer",
        "pk_",
        "Now.ToIso8601()",
        "Expires.ToIso8601()",
        "TrialBoundary",
        "WaveBoundary",
        "BossPhaseBoundary",
        "RewardPending",
        "PreviousEpoch >= AuthorityEpoch",
        "PreviousShardId != ShardId",
        "PreviousBuildId != BuildId",
        "PreviousRuleset != RulesetVersion",
        "PreviousContent != WorldContentVersion",
        "CHECKPOINT_REWARD_RECEIPT_MISMATCH",
        "PendingLedgerCheck",
        "StagedRestoreCandidate = MoveTemp(Candidate)",
        "trusted service has already revalidated the stored envelope",
        "bCheckpointWriteBlockedUntilResync",
    ),
    "Source/JumanjiUE/Public/JumanjiWorldEncounterSessionSubsystem.h": (
        "FGuid GetActiveSessionId() const",
        "FGuid ActiveSessionId",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp": (
        "ActiveSessionId = FGuid::NewGuid()",
        "ActiveSessionId.Invalidate()",
    ),
    "Source/JumanjiUE/Public/JumanjiWorldEncounterDirector.h": (
        "GetBossDefinitionId()",
        "GetSpawnSetId()",
        "GetEncounterSeed()",
        "GetTotalWaveCount()",
        "HandleAdventureObjectivesChanged",
        "HandleBossPhaseChanged",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp": (
        "BeginEncounterCheckpointSession(EncounterId)",
        "CaptureSafePoint(",
        "EJumanjiEncounterCheckpointSafePoint::TrialBoundary",
        "EJumanjiEncounterCheckpointSafePoint::WaveBoundary",
        "EJumanjiEncounterCheckpointSafePoint::BossPhaseBoundary",
        "EJumanjiEncounterCheckpointSafePoint::RewardPending",
        "EndEncounterCheckpointSession(EncounterId)",
    ),
    "Source/JumanjiUE/Public/JumanjiTrialActor.h": (
        "CheckpointTrialId",
        "GetCheckpointTrialId() const",
        "return CheckpointTrialId.IsNone() ? ObjectiveId : CheckpointTrialId;",
    ),
    "Source/JumanjiUE/Private/JumanjiTrialActor.cpp": (
        "const EJumanjiTrialState PreviousTrialState = TrialState;",
        "CurrentStepIndex = PreviousStepIndex;",
        "TrialState = PreviousTrialState;",
    ),
    "Source/JumanjiUE/Private/JumanjiAdventureChapterDirector.cpp": (
        "GetConfiguredTrialIds() const",
        "GetCompletedTrialIds() const",
        "GetCompletedTrialObjectiveIds() const",
        "AreCheckpointTrialIdsConsistent(",
        "HasNoTrialsInProgress() const",
    ),
    "Source/JumanjiUE/Public/JumanjiBossCharacter.h": (
        "BossPhaseRevision",
        "FJumanjiBossPhaseChanged",
        "OnBossPhaseChanged",
        "GetBossHealthFraction() const",
    ),
    "Source/JumanjiUE/Private/JumanjiBossCharacter.cpp": (
        "OnBossPhaseChanged.Broadcast",
        "GetBossHealthFraction() const",
        "DOREPLIFETIME(AJumanjiBossCharacter, BossPhaseRevision)",
    ),
    "Source/JumanjiUE/Public/JumanjiProgressionRewardSubsystem.h": (
        "GetWorldEncounterRewardTransactionId",
    ),
    "Source/JumanjiUE/Private/JumanjiProgressionRewardSubsystem.cpp": (
        "GetWorldEncounterRewardTransactionId(",
        "BuildWorldEncounterTransactionId(*PlayerState, EncounterId)",
    ),
    "Docs/STAGE_34_ENCOUNTER_SAFEPOINT_SNAPSHOT_RESTORE_STAGING.md": (
        "TrialBoundary",
        "WaveBoundary",
        "BossPhaseBoundary",
        "RewardPending",
        "Stage 35",
        "nie zmienia GameState",
    ),
}

FORBIDDEN: dict[str, tuple[str, ...]] = {
    "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp": (
        'SetStringField(TEXT("profileId")',
        'SetStringField(TEXT("rawProfileId")',
        'SetObjectField(TEXT("actor")',
        'SetObjectField(TEXT("inventory")',
        'SetObjectField(TEXT("wallet")',
        'GetActorTransform()',
        'GetActorVelocity()',
        'SpawnActor<',
        'DestroyActor(',
    ),
    "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h": (
        "TObjectPtr<AActor>",
        "TObjectPtr<APawn>",
        "FTransform ",
        "FVector ",
    ),
}

REQUIRED_WORKFLOW_FILES = (
    ".github/workflows/encounter-checkpoint-runtime.yml",
    "Scripts/Tests/test_validate_encounter_checkpoint_runtime.py",
)


def read_source(root: Path, relative: str, errors: list[str]) -> str:
    path = root / relative
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"{relative}: cannot read file: {exc}")
        return ""


def validate_project(root: Path) -> list[str]:
    errors: list[str] = []
    sources: dict[str, str] = {}

    all_paths = set(REQUIRED) | set(FORBIDDEN) | set(REQUIRED_WORKFLOW_FILES)
    for relative in sorted(all_paths):
        sources[relative] = read_source(root, relative, errors)

    for relative, markers in REQUIRED.items():
        source = sources[relative]
        for marker in markers:
            if marker not in source:
                errors.append(f"{relative}: missing required marker {marker!r}")

    for relative, markers in FORBIDDEN.items():
        source = sources[relative]
        for marker in markers:
            if marker in source:
                errors.append(f"{relative}: forbidden serialization/runtime mutation marker {marker!r}")

    header = sources["Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h"]
    if "UFUNCTION(" in header:
        errors.append("Checkpoint runtime subsystem must remain native-only; no Blueprint UFUNCTIONs are allowed")

    candidate_start = header.find("struct JUMANJIUE_API FJumanjiEncounterCheckpointRestoreCandidate")
    candidate_end = header.find("/**", candidate_start + 1)
    if candidate_start < 0:
        errors.append("Restore candidate struct is missing")
    else:
        candidate = header[candidate_start:candidate_end if candidate_end > candidate_start else len(header)]
        for forbidden in ("UPROPERTY(", "TObjectPtr<", "AActor*", "APawn*", "FTransform", "FVector"):
            if forbidden in candidate:
                errors.append(f"Restore candidate contains forbidden runtime field {forbidden!r}")

    runtime = sources["Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"]
    if "FSHA256" in runtime:
        errors.append("Runtime uses an unexpected FSHA256 symbol; use the configured Unreal hash implementation")
    if 'ToString(TEXT("%Y' in runtime:
        errors.append("Runtime timestamp formatting appears to use strftime tokens instead of ISO-8601")
    if "VerifySignature" in runtime or "Ed25519PrivateKey" in runtime:
        errors.append("Unreal runtime must not contain the backend Ed25519 private key/signing implementation")

    director = sources["Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp"]
    for safe_point in ("TrialBoundary", "WaveBoundary", "BossPhaseBoundary", "RewardPending"):
        if safe_point not in director:
            errors.append(f"World encounter director does not wire safe point {safe_point}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())

    if errors:
        print(f"Encounter checkpoint runtime validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Encounter checkpoint runtime validation: PASS")
    print(f"Validated {len(REQUIRED)} source/doc contracts and {len(FORBIDDEN)} forbidden-serialization groups.")
    print("Note: this is static source validation, not UHT/C++ compilation or runtime restore verification.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
