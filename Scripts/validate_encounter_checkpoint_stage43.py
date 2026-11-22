#!/usr/bin/env python3
"""Static repository-governance checks for Stage 43 evidence intake."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_FILES = (
    "Data/Encounter/ProductionReadinessEvidencePacket.schema.json",
    "Data/Encounter/ProductionReadinessEvidencePacket.template.json",
    "Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json",
    "Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json",
    "Scripts/validate_encounter_checkpoint_stage43_evidence.py",
    "Scripts/validate_encounter_checkpoint_staging_report.py",
    "Scripts/validate_encounter_checkpoint_stage43.py",
    "Scripts/Tests/test_validate_encounter_checkpoint_stage43_evidence.py",
    "Docs/STAGE_43_OPERATOR_EVIDENCE_INTAKE_PRODUCTION_READINESS_GATE.md",
    "Docs/STAGE_42_PRODUCTION_STAGING_VALIDATION_RECOVERY_DR_DRILLS.md",
    ".github/workflows/encounter-checkpoint-stage43-evidence.yml",
)
REQUIRED_GATE_MARKERS = (
    "ARTIFACT_SHA256_MISMATCH",
    "ARTIFACT_PATH_TRAVERSAL_REJECTED",
    "ARTIFACT_SYMLINK_REJECTED",
    "EPHEMERAL_CI_IS_NOT_OPERATOR_STAGING",
    "FENCING_DRY_RUN_MUST_NOT_MUTATE",
    "FENCING_APPLY_DID_NOT_FENCE_ALL_ROWS",
    "APPROVAL_REQUIRES_TWO_REVIEWERS_INCLUDING_SECURITY_OR_DATABASE",
    "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW",
    '"productionReady": False',
    '"promotionAllowed": False',
)
REQUIRED_KINDS = (
    "staging_smoke",
    "postgres_primary_failover",
    "postgres_pitr_restore",
    "post_restore_fencing_dry_run",
    "post_restore_fencing_apply",
    "signing_key_overlap_rotation",
    "ue58_dedicated_server_recovery",
    "ue58_pie_multiplayer",
    "android_device_package",
    "stage40_slo_approval",
)


def _load_json(path: Path, label: str, errors: list[str]) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        errors.append(f"{label}: JSON_READ_FAILED")
        return {}
    if not isinstance(value, dict):
        errors.append(f"{label}: ROOT_NOT_OBJECT")
        return {}
    return value


def validate_project(root: Path) -> list[str]:
    errors: list[str] = []
    for relative in REQUIRED_FILES:
        if not (root / relative).is_file():
            errors.append(f"REQUIRED_FILE_MISSING:{relative}")
    if errors:
        return errors

    schema = _load_json(root / REQUIRED_FILES[0], "STAGE43_SCHEMA", errors)
    template = _load_json(root / REQUIRED_FILES[1], "STAGE43_TEMPLATE", errors)
    stage41 = _load_json(root / REQUIRED_FILES[2], "STAGE41_MANIFEST", errors)
    stage42 = _load_json(root / REQUIRED_FILES[3], "STAGE42_MANIFEST", errors)
    if schema.get("properties", {}).get("stage", {}).get("const") != 43:
        errors.append("STAGE43_SCHEMA_STAGE_MISMATCH")
    if schema.get("additionalProperties") is not False:
        errors.append("STAGE43_SCHEMA_MUST_REJECT_UNKNOWN_FIELDS")
    if template.get("stage") != 43 or template.get("schema") != "JUMANJI.ProductionReadinessEvidencePacket.v1":
        errors.append("STAGE43_TEMPLATE_SCHEMA_MISMATCH")
    if template.get("entries") != [] or template.get("submissionStatus") != "AWAITING_OPERATOR_EVIDENCE":
        errors.append("STAGE43_TEMPLATE_MUST_START_EMPTY_AND_BLOCKED")
    if template.get("review", {}).get("decision") != "NOT_REVIEWED":
        errors.append("STAGE43_TEMPLATE_MUST_START_UNREVIEWED")

    stage41_fail_closed = (
        stage41.get("productionReady") is False
        and stage41.get("highAvailabilityApproved") is False
        and stage41.get("sloStatus") == "UNBASELINED"
    )
    if not stage41_fail_closed:
        errors.append("STAGE41_MANIFEST_MUST_REMAIN_FAIL_CLOSED")

    stage42_evidence = stage42.get("evidence", {})
    stage42_false = (
        stage42.get("productionReady") is False
        and stage42.get("highAvailabilityApproved") is False
        and stage42.get("sloStatus") == "UNBASELINED"
        and all(stage42_evidence.get(key) is False for key in (
            "realStagingDeploymentVerified",
            "postgresPrimaryFailoverDrillCompleted",
            "postgresPitrRestoreDrillCompleted",
            "postRestoreLeaseFencingDrillCompleted",
            "signingKeyOverlapRotationDrillCompleted",
            "liveUnrealDedicatedServerRecoveryVerified",
            "unrealEngine58BuildVerified",
            "androidDevicePackageVerified",
        ))
    )
    if not stage42_false:
        errors.append("STAGE42_EXTERNAL_EVIDENCE_MUST_REMAIN_FAIL_CLOSED")

    gate = (root / "Scripts/validate_encounter_checkpoint_stage43_evidence.py").read_text(encoding="utf-8")
    for marker in REQUIRED_GATE_MARKERS:
        if marker not in gate:
            errors.append(f"STAGE43_GATE_MARKER_MISSING:{marker}")
    for kind in REQUIRED_KINDS:
        if f'"{kind}"' not in gate:
            errors.append(f"STAGE43_REQUIRED_EVIDENCE_KIND_MISSING:{kind}")

    doc = (root / "Docs/STAGE_43_OPERATOR_EVIDENCE_INTAKE_PRODUCTION_READINESS_GATE.md").read_text(encoding="utf-8")
    for marker in (
        "Stage 42", "ephemeral-ci", "PITR", "mTLS", "Dedicated Server",
        "Android", "UNBASELINED", "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW",
        "Nie wykonano",
    ):
        if marker not in doc:
            errors.append(f"STAGE43_RUNBOOK_MARKER_MISSING:{marker}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())
    if errors:
        print(f"Stage 43 repository governance: FAIL ({len(errors)} error(s))")
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Stage 43 repository governance: PASS")
    print("External evidence is still pending; Stage41/42 production, HA and SLO flags remain unchanged.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
