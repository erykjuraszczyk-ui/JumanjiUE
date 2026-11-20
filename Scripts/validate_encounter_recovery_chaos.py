from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MATRIX_PATH = Path("Data/Encounter/EncounterRecoveryChaosMatrix.csv")

MATRIX_COLUMNS = (
    "CaseId",
    "InjectionPoint",
    "InjectedFault",
    "ExpectedOutcome",
    "MustRemainUnchanged",
    "ValidationLayer",
)

REQUIRED_CASES = {
    "ACK_LOST_AFTER_COMMIT": ("RECONCILE_BEFORE_RETRY", "BackendIntegration"),
    "BACKEND_RESTART_DURABILITY": ("CHECKPOINT_SURVIVES_RESTART", "BackendIntegration"),
    "SQLITE_ABORT_WRITE": ("NO_PARTIAL_CHECKPOINT_COMMIT", "BackendIntegration"),
    "SAME_SEQUENCE_CAS_RACE": ("EXACTLY_ONE_WRITER_WINS", "BackendIntegration"),
    "SIGNATURE_TAMPER": ("RESTORE_REJECTED", "BackendIntegration"),
    "CHECKPOINT_EXPIRED": ("RESTORE_REJECTED", "BackendIntegration"),
    "BACKEND_UNREACHABLE": ("FAIL_CLOSED_NO_WORLD_MUTATION", "BackendIntegration"),
    "LEASE_RENEWAL_LOSS": ("BLOCK_WRITES_UNTIL_RESYNC", "StaticSourceAndPIE"),
    "MISSING_PARTICIPANT": ("WAIT_WITHOUT_PUBLISH", "StaticSourceAndPIE"),
    "PROFILE_REVISION_ROLLBACK": ("REJECT_WITHOUT_PUBLISH", "StaticSourceAndPIE"),
    "REWARD_ALREADY_COMMITTED": ("NO_DUPLICATE_GRANT", "StaticSourceAndServerTest"),
    "REWARD_LEDGER_UNAVAILABLE": ("NO_PUBLISH", "ManualServerTest"),
    "ACTOR_STAGING_FAILURE": ("DESTROY_STAGED_ACTORS_NO_PUBLISH", "ManualPIE"),
    "OPERATOR_ABORT_DURING_WAIT": ("CLEAR_CANDIDATE_ONLY", "StaticSourceAndServerTest"),
}

BACKEND_TEST_METHODS = {
    "ACK_LOST_AFTER_COMMIT": "test_lost_write_ack_is_reconciled_without_duplicate_sequence",
    "BACKEND_RESTART_DURABILITY": "test_backend_restart_preserves_checkpoint_and_fences_old_owner",
    "SQLITE_ABORT_WRITE": "test_sqlite_abort_keeps_checkpoint_head_unchanged",
    "SAME_SEQUENCE_CAS_RACE": "test_concurrent_same_sequence_writes_have_one_winner",
    "SIGNATURE_TAMPER": "test_signature_tamper_fails_closed",
    "CHECKPOINT_EXPIRED": "test_expired_checkpoint_fails_closed",
    "BACKEND_UNREACHABLE": "test_transport_outage_is_reported_as_unavailable",
}

SOURCE_TOKENS = {
    "Backend/EncounterCheckpointService/service.py": (
        "BEGIN IMMEDIATE",
        "connection.rollback()",
        "SEQUENCE_CONFLICT",
        "SIGNATURE_INVALID",
        "CHECKPOINT_EXPIRED",
    ),
    "Scripts/Tests/test_encounter_checkpoint_chaos.py": tuple(BACKEND_TEST_METHODS.values()),
    "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp": (
        "bLeaseReady = false;",
        "bCheckpointWriteBlockedUntilResync = true;",
        "TryCommitStagedRestore(",
        "CommitStagedRestore(",
        "HasProcessedProfileTransaction",
        "RESTORE_PARTICIPANT_NOT_CONNECTED_OR_AUTHENTICATED",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp": (
        "SetReplicates(false)",
        "SetReplicates(true)",
        "DestroyStagedActors",
        "ApplyEncounterCheckpointRestore(",
    ),
    "Source/JumanjiUE/Private/JumanjiGameState.cpp": (
        "ApplyEncounterCheckpointRestore(",
        "OnWorldEncounterChanged.Broadcast",
    ),
    "Source/JumanjiUE/Private/JumanjiGameMode.cpp": (
        "TryStartEncounterCheckpointRecovery();",
        "TryCommitStagedRestore(EncounterDirector)",
    ),
    "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h": (
        "FJumanjiEncounterRecoveryDiagnostics",
        "GetRecoveryDiagnostics() const",
    ),
    "Docs/STAGE_37_ENCOUNTER_RECOVERY_OBSERVABILITY_OPERATOR_CONTROLS.md": (
        "RequestOperatorAbort",
        "GetRecoveryDiagnostics",
        "nie może przerwać aktywnego gameplay encounteru",
    ),
    "Docs/STAGE_38_RECOVERY_CHAOS_TESTING_FAILURE_INJECTION.md": (
        "BackendIntegration",
        "ManualPIE",
        "Nie wykonano kompilacji Unreal Engine 5.8",
        "Utracona odpowiedź po commit",
    ),
    ".github/workflows/encounter-recovery-chaos.yml": (
        "validate_encounter_recovery_chaos.py",
        "test_encounter_checkpoint_chaos.py",
        "test_validate_encounter_recovery_chaos.py",
        "test_encounter_checkpoint_service.py",
    ),
}

WORKFLOW_PATH = ".github/workflows/encounter-recovery-chaos.yml"


def read_source(root: Path, relative: str, errors: list[str]) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"{relative}: cannot read file: {exc}")
        return ""


def validate_matrix(path: Path) -> list[str]:
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            columns = reader.fieldnames or []
            missing_columns = [column for column in MATRIX_COLUMNS if column not in columns]
            if missing_columns:
                return [f"matrix missing columns: {', '.join(missing_columns)}"]
            rows = list(reader)
    except (OSError, UnicodeError, csv.Error) as exc:
        return [f"matrix cannot be read: {exc}"]

    errors: list[str] = []
    by_id: dict[str, dict[str, str]] = {}
    for line_number, row in enumerate(rows, start=2):
        case_id = row.get("CaseId", "").strip()
        if not case_id:
            errors.append(f"row {line_number}: CaseId is empty")
            continue
        if case_id in by_id:
            errors.append(f"row {line_number}: duplicate CaseId {case_id}")
            continue
        by_id[case_id] = row

        for field in MATRIX_COLUMNS[1:]:
            if not row.get(field, "").strip():
                errors.append(f"{case_id}: {field} is required")

    for case_id, (expected_outcome, expected_layer) in REQUIRED_CASES.items():
        row = by_id.get(case_id)
        if row is None:
            errors.append(f"matrix missing required case {case_id}")
            continue
        if row.get("ExpectedOutcome", "").strip() != expected_outcome:
            errors.append(f"{case_id}: expected outcome {expected_outcome}")
        if row.get("ValidationLayer", "").strip() != expected_layer:
            errors.append(f"{case_id}: expected validation layer {expected_layer}")

    return errors


def validate_sources(root: Path) -> list[str]:
    errors: list[str] = []
    sources = {
        relative: read_source(root, relative, errors)
        for relative in SOURCE_TOKENS
    }

    for relative, markers in SOURCE_TOKENS.items():
        source = sources[relative]
        for marker in markers:
            if marker not in source:
                errors.append(f"{relative}: missing required failure-recovery contract {marker!r}")

    workflow = sources[WORKFLOW_PATH]
    for required_command in (
        "python Scripts/validate_encounter_checkpoint_contract.py",
        "python Scripts/validate_encounter_checkpoint_backend.py",
        "python Scripts/validate_encounter_checkpoint_runtime.py",
        "python Scripts/validate_encounter_restore_materialization.py",
        "python Scripts/validate_encounter_startup_coordinator.py",
        "python Scripts/validate_encounter_recovery_observability.py",
    ):
        if required_command not in workflow:
            errors.append(f"{WORKFLOW_PATH}: missing regression command {required_command!r}")

    return errors


def validate_project(root: Path) -> list[str]:
    return validate_sources(root) + validate_matrix(root / MATRIX_PATH)


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate Stage 38 encounter recovery chaos tests and QA matrix.")
    parser.add_argument("--project-root", type=Path, default=ROOT)
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())

    if errors:
        print(f"Encounter recovery chaos validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Encounter recovery chaos validation: PASS")
    print(f"Validated {len(REQUIRED_CASES)} required failure scenarios and {len(SOURCE_TOKENS)} source/workflow contracts.")
    print("Backend integration tests exercise the Python SQLite service; Unreal PIE and process-crash tests remain manual.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
