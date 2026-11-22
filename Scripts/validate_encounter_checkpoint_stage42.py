#!/usr/bin/env python3
"""Static governance checks for Stage 42 staging validation and DR tooling.

This script does not create infrastructure, deploy staging, execute PITR or
prove an Unreal Engine build. It verifies that the required tools and explicit
production-readiness boundaries remain present in the repository.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REQUIRED_FILES = (
    "Scripts/run_encounter_checkpoint_staging_drill.py",
    "Scripts/fence_checkpoint_leases_after_restore.py",
    "Scripts/validate_encounter_checkpoint_staging_report.py",
    "Scripts/Tests/test_encounter_checkpoint_stage42.py",
    "Scripts/Tests/test_checkpoint_postgres_restore_fencing.py",
    "Data/Encounter/EncounterCheckpointStagingDrillReport.schema.json",
    "Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json",
    "Docs/STAGE_42_PRODUCTION_STAGING_VALIDATION_RECOVERY_DR_DRILLS.md",
    "Scripts/validate_encounter_checkpoint_production.py",
    "Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json",
)

STAGING_MARKERS = (
    "JUMANJI_STAGING_ENVIRONMENT_CLASS",
    "ephemeral-ci",
    "staging",
    "DRILL_STAGE42_",
    "--confirm-staging-mutations",
    "https",
    "load_cert_chain",
    "request_without_client_certificate",
    "postgres_readiness",
    "lease_exclusion",
    "checkpoint_cas_conflict",
    "verify_signed_envelope",
    "stale_lease_fenced",
    "durable_readback_after_reacquire",
    "JUMANJI_STAGING_VERIFY_KEYRING_FILE",
)

FENCING_MARKERS = (
    "--apply",
    "--confirm-restored-target",
    "dr-rehearsal",
    "LOCK TABLE encounter_checkpoint_state IN ACCESS EXCLUSIVE MODE",
    "authority_epoch = authority_epoch + 1",
    "lease_token_hash = NULL",
    "checkpoint_sequence",
    "AUTHORITY_EPOCH_OVERFLOW",
)

TEST_MARKERS = (
    "test_dry_run_does_not_change_lease_or_checkpoint",
    "test_apply_invalidates_old_lease_and_preserves_signed_checkpoint",
    "test_authority_epoch_overflow_aborts_without_partial_fencing",
    "test_signed_readback_signature_is_verified",
    "test_tampered_signed_payload_is_rejected",
    "test_report_with_lease_token_is_rejected",
)

FORBIDDEN_EXAMPLES = (
    re.compile(r"(?i)BEGIN\s+PRODUCTION\s+DEPLOYMENT"),
    re.compile(r"(?i)PRODUCTION_READY\s*=\s*true"),
)


def _read(root: Path, relative: str, errors: list[str]) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read {relative}: {type(error).__name__}")
        return ""


def validate_project(root: Path) -> list[str]:
    errors: list[str] = []
    for relative in REQUIRED_FILES:
        if not (root / relative).is_file():
            errors.append(f"required file missing: {relative}")
    if errors:
        return errors

    runner = _read(root, "Scripts/run_encounter_checkpoint_staging_drill.py", errors)
    for marker in STAGING_MARKERS:
        if marker not in runner:
            errors.append(f"staging drill runner missing marker: {marker}")

    fence = _read(root, "Scripts/fence_checkpoint_leases_after_restore.py", errors)
    for marker in FENCING_MARKERS:
        if marker not in fence:
            errors.append(f"restore fencing tool missing marker: {marker}")
    if fence.find('parser.add_argument("--apply"') < fence.find('parser.add_argument("--confirm-restored-target"'):
        pass
    else:
        errors.append("restore fencing tool must expose explicit apply and restored-target confirmation flags")

    tests = _read(root, "Scripts/Tests/test_checkpoint_postgres_restore_fencing.py", errors)
    tests += _read(root, "Scripts/Tests/test_encounter_checkpoint_stage42.py", errors)
    for marker in TEST_MARKERS:
        if marker not in tests:
            errors.append(f"Stage 42 tests missing marker: {marker}")

    report_schema = _read(root, "Data/Encounter/EncounterCheckpointStagingDrillReport.schema.json", errors)
    try:
        schema = json.loads(report_schema)
    except json.JSONDecodeError:
        schema = {}
        errors.append("Stage 42 report schema is invalid JSON")
    if schema.get("properties", {}).get("stage", {}).get("const") != 42:
        errors.append("Stage 42 report schema must use stage=42")
    if schema.get("additionalProperties") is not False:
        errors.append("Stage 42 report schema must reject unsupported fields")

    readiness_path = root / "Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json"
    try:
        manifest = json.loads(readiness_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        manifest = {}
        errors.append("Stage 42 readiness manifest is invalid JSON")
    if manifest.get("stage") != 42:
        errors.append("Stage 42 readiness manifest must identify stage=42")
    evidence = manifest.get("evidence", {})
    required_false_fields = {
        "stagingDeploymentVerified": "realStagingDeploymentVerified",
        "postgresFailoverDrillCompleted": "postgresPrimaryFailoverDrillCompleted",
        "pitrRestoreDrillCompleted": "postgresPitrRestoreDrillCompleted",
        "keyRotationOverlapDrillCompleted": "signingKeyOverlapRotationDrillCompleted",
        "unrealEngineEndpointSmokeCompleted": "liveUnrealDedicatedServerRecoveryVerified",
        "androidPackageTestCompleted": "androidDevicePackageVerified",
    }
    for key in ("productionReady", "highAvailabilityApproved"):
        if manifest.get(key) is not False:
            errors.append(f"Stage 42 manifest must keep {key}=false until evidence is attached")
    for public_key, stored_key in required_false_fields.items():
        if evidence.get(stored_key) is not False:
            errors.append(f"Stage 42 manifest must keep evidence.{stored_key}=false ({public_key}) until evidence is attached")
    if manifest.get("sloStatus") != "UNBASELINED":
        errors.append("Stage 42 must retain Stage 40 UNBASELINED SLO governance")

    stage41 = _read(root, "Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json", errors)
    try:
        stage41_manifest = json.loads(stage41)
    except json.JSONDecodeError:
        stage41_manifest = {}
        errors.append("Stage 41 deployment manifest is invalid JSON")
    if stage41_manifest.get("productionReady") is not False:
        errors.append("Stage 42 must not silently upgrade Stage 41 productionReady")
    if stage41_manifest.get("highAvailabilityApproved") is not False:
        errors.append("Stage 42 must not silently approve Stage 41 HA")
    if stage41_manifest.get("sloStatus") != "UNBASELINED":
        errors.append("Stage 42 must not fabricate Stage 40 SLO approval")

    doc = _read(root, "Docs/STAGE_42_PRODUCTION_STAGING_VALIDATION_RECOVERY_DR_DRILLS.md", errors)
    for marker in (
        "CI",
        "ephemeral",
        "PITR",
        "mTLS",
        "DR",
        "productionReady=false",
        "UNBASELINED",
        "Unreal Engine 5.8",
        "Nie wykonano",
    ):
        if marker not in doc:
            errors.append(f"Stage 42 runbook missing limitation/evidence marker: {marker}")

    for relative in (
        "Scripts/run_encounter_checkpoint_staging_drill.py",
        "Scripts/fence_checkpoint_leases_after_restore.py",
    ):
        source = _read(root, relative, errors)
        for pattern in FORBIDDEN_EXAMPLES:
            if pattern.search(source):
                errors.append(f"{relative}: forbidden production readiness claim")
        if re.search(r"(?im)^\s*(?:JUMANJI_[A-Z0-9_]*(?:TOKEN|SECRET|PRIVATE_KEY))\s*=\s*['\"][^$'\"]{8,}['\"]", source):
            errors.append(f"{relative}: possible hard-coded deployment secret")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())
    if errors:
        print(f"Stage 42 staging/DR validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Stage 42 staging/DR validation: PASS")
    print("Readiness manifest remains fail-closed; static validation is not staging/DR evidence.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
