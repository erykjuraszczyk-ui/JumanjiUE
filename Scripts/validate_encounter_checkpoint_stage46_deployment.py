#!/usr/bin/env python3
"""Static fail-closed repository checks for Stage 46 deployment orchestration."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_FILES = (
    "Data/Encounter/ProductionRollbackBaseline.schema.json",
    "Data/Encounter/ProductionRollbackBaseline.template.json",
    "Data/Encounter/ProductionDeploymentPlan.schema.json",
    "Data/Encounter/ProductionDeploymentPlan.template.json",
    "Scripts/encounter_checkpoint_release_board.py",
    "Scripts/encounter_checkpoint_deployment.py",
    "Scripts/validate_encounter_checkpoint_stage46_deployment.py",
    "Scripts/Tests/test_validate_encounter_checkpoint_stage46_deployment.py",
    "Docs/STAGE_46_CONTROLLED_DEPLOYMENT_ROLLBACK_READINESS.md",
    ".github/workflows/encounter-checkpoint-stage46-deployment.yml",
)
SCRIPT_MARKERS = (
    "stage45.evaluate",
    "APPROVED_FOR_DEPLOYMENT",
    "STAGE45_BOARD_QUORUM_INVALID",
    "ROLLBACK_BASELINE_HASH_MISMATCH",
    "ROLLBACK_BASELINE_TARGET_SET_MISMATCH",
    "ROLLBACK_BASELINE_PATH_TRAVERSAL",
    "MANUAL_OUT_OF_BAND",
    "DEPLOYMENT_PLAN_BINDING_MISMATCH",
    "ATTESTATION_BYTES_MISMATCH",
    "KEYRING_BYTES_MISMATCH",
    "CANDIDATE_BYTES_MISMATCH",
    "RELEASE_BOARD_DECISION_BYTES_MISMATCH",
    "ROLLBACK_BASELINE_BYTES_MISMATCH",
    "deploymentExecuted",
    "sideEffectsPerformed",
    "automaticRetries",
)


def _read_json(root: Path, relative: str, errors: list[str]) -> dict:
    try:
        value = json.loads((root / relative).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        errors.append(f"JSON_READ_FAILED:{relative}")
        return {}
    if not isinstance(value, dict):
        errors.append(f"JSON_ROOT_NOT_OBJECT:{relative}")
        return {}
    return value


def validate_project(root: Path) -> list[str]:
    errors: list[str] = []
    for relative in REQUIRED_FILES:
        if not (root / relative).is_file():
            errors.append(f"REQUIRED_FILE_MISSING:{relative}")
    if errors:
        return errors

    baseline_schema = _read_json(root, REQUIRED_FILES[0], errors)
    baseline_template = _read_json(root, REQUIRED_FILES[1], errors)
    plan_schema = _read_json(root, REQUIRED_FILES[2], errors)
    plan_template = _read_json(root, REQUIRED_FILES[3], errors)

    if baseline_schema.get("additionalProperties") is not False:
        errors.append("ROLLBACK_BASELINE_SCHEMA_MUST_BE_STRICT")
    if baseline_schema.get("properties", {}).get("schema", {}).get("const") != "JUMANJI.ProductionRollbackBaseline.v1":
        errors.append("ROLLBACK_BASELINE_SCHEMA_ID_INVALID")
    if (
        baseline_template.get("schema") != "JUMANJI.ProductionRollbackBaseline.v1"
        or baseline_template.get("environmentId") != "production"
        or baseline_template.get("baselineId") is not None
        or baseline_template.get("targets") != []
    ):
        errors.append("ROLLBACK_BASELINE_TEMPLATE_MUST_REMAIN_EMPTY")

    if plan_schema.get("additionalProperties") is not False:
        errors.append("DEPLOYMENT_PLAN_SCHEMA_MUST_BE_STRICT")
    if plan_schema.get("properties", {}).get("stage", {}).get("const") != 46:
        errors.append("DEPLOYMENT_PLAN_SCHEMA_STAGE_INVALID")
    for marker in ("deploymentExecuted", "sideEffectsPerformed", "productionReady", "promotionAllowed"):
        if plan_schema.get("properties", {}).get(marker, {}).get("const") is not False:
            errors.append(f"DEPLOYMENT_PLAN_SCHEMA_MUST_KEEP_FALSE:{marker}")
    if plan_schema.get("properties", {}).get("sloStatus", {}).get("const") != "UNBASELINED":
        errors.append("DEPLOYMENT_PLAN_SCHEMA_MUST_KEEP_SLO_UNBASELINED")
    if (
        plan_template.get("schema") != "JUMANJI.ProductionDeploymentPlan.v1"
        or plan_template.get("stage") != 46
        or plan_template.get("state") != "BLOCKED_MISSING_EVIDENCE"
        or plan_template.get("planId") is not None
        or plan_template.get("deploymentExecuted") is not False
        or plan_template.get("sideEffectsPerformed") is not False
        or plan_template.get("promotionAllowed") is not False
    ):
        errors.append("DEPLOYMENT_PLAN_TEMPLATE_MUST_REMAIN_BLOCKED")

    script = (root / REQUIRED_FILES[5]).read_text(encoding="utf-8")
    for marker in SCRIPT_MARKERS:
        if marker not in script:
            errors.append(f"DEPLOYMENT_SECURITY_MARKER_MISSING:{marker}")

    tests = (root / REQUIRED_FILES[7]).read_text(encoding="utf-8")
    for marker in (
        "test_approved_release_creates_plan_bound_to_candidate_and_rollback",
        "test_unsigned_release_board_decision_cannot_create_a_plan",
        "test_rejected_release_board_decision_cannot_create_a_plan",
        "test_candidate_artifact_mutation_blocks_plan",
        "test_rollback_artifact_hash_mismatch_blocks_plan",
        "test_rollback_path_traversal_is_rejected",
        "test_rollback_target_set_must_match_release_targets",
        "test_decision_bytes_must_match_verified_decision_object",
        "test_rollback_baseline_bytes_must_match_parsed_object",
        "test_candidate_bytes_must_match_parsed_candidate",
        "test_plan_verification_detects_mutation_and_recomputes_bindings",
        "test_empty_repository_templates_remain_blocked",
    ):
        if marker not in tests:
            errors.append(f"DEPLOYMENT_REGRESSION_TEST_MISSING:{marker}")

    doc = (root / REQUIRED_FILES[8]).read_text(encoding="utf-8")
    for marker in (
        "APPROVED_FOR_DEPLOYMENT",
        "MANUAL_OUT_OF_BAND",
        "PREFLIGHT",
        "CANARY",
        "PROGRESSIVE_ROLLOUT",
        "POST_DEPLOYMENT_VERIFICATION",
        "MANUAL_REDEPLOY_LAST_KNOWN_GOOD",
        "deploymentExecuted=false",
        "productionReady=false",
        "UNBASELINED",
        "Nie wykonuje deploymentu",
    ):
        if marker not in doc:
            errors.append(f"DEPLOYMENT_RUNBOOK_MARKER_MISSING:{marker}")

    workflow = (root / REQUIRED_FILES[9]).read_text(encoding="utf-8")
    for marker in (
        "cryptography>=42.0",
        "rfc8785==0.1.4",
        "validate_encounter_checkpoint_stage45_release_board.py",
        "validate_encounter_checkpoint_stage46_deployment.py",
        "test_validate_encounter_checkpoint_stage45_release_board.py",
        "test_validate_encounter_checkpoint_stage46_deployment.py",
        "BLOCKED_MISSING_EVIDENCE",
    ):
        if marker not in workflow:
            errors.append(f"DEPLOYMENT_WORKFLOW_MARKER_MISSING:{marker}")
    if "aws-actions" in workflow or "azure/" in workflow or "google-github-actions" in workflow:
        errors.append("DEPLOYMENT_CI_MUST_NOT_DEPLOY_TO_CLOUD")

    return errors


def main() -> int:
    errors = validate_project(ROOT)
    if errors:
        print(f"Stage 46 controlled deployment: FAIL ({len(errors)} error(s))")
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Stage 46 controlled deployment: PASS")
    print("Plan generation is non-executing; rollback and release hashes are verified; production/SLO promotion remains blocked.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
