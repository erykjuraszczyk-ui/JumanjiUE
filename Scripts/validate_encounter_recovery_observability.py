#!/usr/bin/env python3
"""Static contract checks for JUMANJI Stage 37 recovery observability."""
from __future__ import annotations

import sys
from pathlib import Path

RUNTIME_HEADER = "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h"
RUNTIME_CPP = "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp"
DOC = "Docs/STAGE_37_ENCOUNTER_RECOVERY_OBSERVABILITY_OPERATOR_CONTROLS.md"

REQUIRED = {
    RUNTIME_HEADER: (
        "enum class EJumanjiEncounterRecoveryOutcome",
        "enum class EJumanjiEncounterRecoveryAbortReason",
        "struct FJumanjiEncounterRecoveryDiagnostics",
        "FGuid CorrelationId",
        "FName LastErrorCode",
        "uint64 RestoreCommitAttempts",
        "GetRecoveryDiagnostics() const",
        "RequestOperatorRetry(",
        "RequestOperatorAbort(",
    ),
    RUNTIME_CPP: (
        "UJumanjiEncounterCheckpointRuntimeSubsystem::GetRecoveryDiagnostics() const",
        "UJumanjiEncounterCheckpointRuntimeSubsystem::RequestOperatorRetry(",
        "UJumanjiEncounterCheckpointRuntimeSubsystem::RequestOperatorAbort(",
        "if (!IsServerRuntime()",
        "EncounterDirector->HasAuthority()",
        "OPERATOR_RETRY_NOT_A_STAGED_OR_IDLE_RECOVERY",
        "EncounterDirector->GetLevel() != OperatorWorld->GetPersistentLevel()",
        "bHasStagedRestoreCandidate && bWaitingForRestoreParticipants",
        "JUMANJI_RECOVERY correlation=%s outcome=%s encounter=%s sequence=%lld epoch=%lld http=%d error=%s operatorReason=%s",
        "OPERATOR_ABORTED_STAGED_RESTORE",
        "LastKnownCheckpointSequence",
        "LastKnownAuthorityEpoch",
        "LeaseRenewalFailures",
        "CheckpointWriteFailures",
        "RestoreReadFailures",
        "RestoreCommitSuccesses",
        "ParticipantWaitTimeouts",
    ),
    DOC: (
        "Recovery Observability & Operator Controls",
        "GetRecoveryDiagnostics()",
        "RequestOperatorRetry",
        "RequestOperatorAbort",
        "LeaseToken",
        "ProfileId",
        "participantKey",
        "nie są oznaczone UFUNCTION",
        "180 sekund",
        "Stage 38",
    ),
}

FORBIDDEN_DIAGNOSTICS = (
    "UPROPERTY(",
    "UFUNCTION(",
    "LeaseToken",
    "ProfileId",
    "ParticipantKey",
    "Bearer",
    "PrivateKey",
    "ResponseBody",
    "FString ",
    "TObjectPtr<",
    "AActor*",
    "FTransform",
    "FVector",
)

def read(root: Path, path: str, errors: list[str]) -> str:
    file_path = root / path
    try:
        return file_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"{path}: cannot read: {exc}")
        return ""

def slice_between(source: str, start: str, end: str) -> str:
    first = source.find(start)
    if first < 0:
        return ""
    last = source.find(end, first + len(start))
    return source[first:] if last < 0 else source[first:last]

def validate_project(root: Path) -> list[str]:
    errors: list[str] = []
    content = {path: read(root, path, errors) for path in REQUIRED}

    for path, tokens in REQUIRED.items():
        for token in tokens:
            if token not in content[path]:
                errors.append(f"{path}: missing required contract {token!r}")

    header = content[RUNTIME_HEADER]
    diagnostics = slice_between(
        header,
        "struct FJumanjiEncounterRecoveryDiagnostics",
        "/**\n * Private, validated logical restore plan.",
    )
    if not diagnostics:
        errors.append("Diagnostics struct cannot be isolated")
    else:
        for token in FORBIDDEN_DIAGNOSTICS:
            if token in diagnostics:
                errors.append(f"Diagnostics struct contains forbidden field/marker {token!r}")

    if "UFUNCTION(" in header:
        errors.append("Checkpoint runtime header must remain native-only; no UFUNCTION is allowed")

    cpp = content[RUNTIME_CPP]
    retry = slice_between(
        cpp,
        "bool UJumanjiEncounterCheckpointRuntimeSubsystem::RequestOperatorRetry(",
        "bool UJumanjiEncounterCheckpointRuntimeSubsystem::RequestOperatorAbort(",
    )
    for token in (
        "!IsServerRuntime()",
        "!EncounterDirector->HasAuthority()",
        "EncounterDirector->GetLevel() != OperatorWorld->GetPersistentLevel()",
        "bLeaseAcquiring || bReadInFlight || bWriteInFlight || bRestoreCommitInProgress",
        "OPERATOR_RETRY_NOT_A_STAGED_OR_IDLE_RECOVERY",
        "TryCommitStagedRestore(EncounterDirector)",
        "BeginRestoreStaging(EncounterDirector)",
    ):
        if token not in retry:
            errors.append(f"Operator retry API is missing guard/operation {token!r}")

    abort = slice_between(
        cpp,
        "bool UJumanjiEncounterCheckpointRuntimeSubsystem::RequestOperatorAbort(",
        "void UJumanjiEncounterCheckpointRuntimeSubsystem::Initialize(",
    )
    for token in (
        "!IsServerRuntime()",
        "!IsValidOperatorAbortReason(Reason)",
        "!bHasStagedRestoreCandidate || !bWaitingForRestoreParticipants",
        "bReadInFlight || bWriteInFlight",
        "World->GetTimerManager().ClearTimer(RestoreParticipantWaitTimerHandle)",
        "OPERATOR_ABORTED_STAGED_RESTORE",
        "EndEncounterCheckpointSession(ActiveEncounterId)",
    ):
        if token not in abort:
            errors.append(f"Operator abort API is missing guard/operation {token!r}")

    # Operator APIs must not be UFUNCTIONs in the public header.
    for signature in ("RequestOperatorRetry(", "RequestOperatorAbort("):
        pos = header.find(signature)
        preceding = header[max(0, pos - 100):pos]
        if "UFUNCTION(" in preceding:
            errors.append(f"{signature} must not be Blueprint/client reflected")

    # Prevent accidental client-facing or live-gameplay-abort hooks.
    if "ServerRequestOperator" in header or "ServerRequestOperator" in cpp:
        errors.append("Operator controls must not be exposed through a client RPC")
    if "ApplyEncounterCheckpointRestore" in abort or "DestroyActor(" in abort:
        errors.append("Operator abort must not materialize/reset gameplay or destroy encounter actors")

    return errors

def main() -> int:
    root = Path(__file__).resolve().parents[1]
    errors = validate_project(root)
    if errors:
        print(f"Encounter recovery observability validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Encounter recovery observability validation: PASS")
    print("Diagnostics are secret-free and process-local; operator controls are native server-only.")
    print("Static validation is not Unreal compilation, IAM integration or a live recovery test.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
