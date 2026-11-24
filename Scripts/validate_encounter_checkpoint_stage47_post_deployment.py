#!/usr/bin/env python3
"""Static, fail-closed contract checks for Stage 47 deployment evidence."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_FILES = (
    "Data/Encounter/ProductionDeploymentEvidenceJournal.schema.json",
    "Data/Encounter/ProductionDeploymentEvidenceJournal.template.json",
    "Data/Encounter/ProductionDeploymentEvidenceAttestation.schema.json",
    "Data/Encounter/ProductionDeploymentEvidenceAttestation.template.json",
    "Scripts/encounter_checkpoint_deployment_evidence.py",
    "Scripts/validate_encounter_checkpoint_stage47_post_deployment.py",
    "Scripts/Tests/test_validate_encounter_checkpoint_stage47_post_deployment.py",
    "Docs/STAGE_47_POST_DEPLOYMENT_VERIFICATION_EVIDENCE_RECONCILIATION.md",
    ".github/workflows/encounter-checkpoint-stage47-post-deployment.yml",
    "Scripts/encounter_checkpoint_deployment.py",
    "Data/Encounter/ProductionDeploymentPlan.schema.json",
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

    journal_schema = _read_json(root, REQUIRED_FILES[0], errors)
    journal_template = _read_json(root, REQUIRED_FILES[1], errors)
    attestation_schema = _read_json(root, REQUIRED_FILES[2], errors)
    attestation_template = _read_json(root, REQUIRED_FILES[3], errors)
    plan_schema = _read_json(root, REQUIRED_FILES[10], errors)

    if journal_schema.get("additionalProperties") is not False:
        errors.append("JOURNAL_SCHEMA_MUST_BE_STRICT")
    if journal_schema.get("properties", {}).get("schema", {}).get("const") != "JUMANJI.ProductionDeploymentEvidenceJournal.v1":
        errors.append("JOURNAL_SCHEMA_ID_INVALID")
    phases = journal_schema.get("$defs", {}).get("phase", {}).get("properties", {}).get("phase", {}).get("enum", [])
    if phases != ["PREFLIGHT", "CANARY", "PROGRESSIVE_ROLLOUT", "POST_DEPLOYMENT_VERIFICATION"]:
        errors.append("JOURNAL_PHASE_ORDER_CONTRACT_INVALID")
    if (
        journal_template.get("schema") != "JUMANJI.ProductionDeploymentEvidenceJournal.v1"
        or journal_template.get("journalId") is not None
        or journal_template.get("outcome") != "NOT_RECORDED"
        or journal_template.get("rollbackOutcome") != "NOT_RECORDED"
        or journal_template.get("phases") != []
    ):
        errors.append("JOURNAL_TEMPLATE_MUST_REMAIN_EMPTY")

    if attestation_schema.get("additionalProperties") is not False:
        errors.append("DEPLOYMENT_ATTESTATION_SCHEMA_MUST_BE_STRICT")
    if attestation_schema.get("properties", {}).get("schema", {}).get("const") != "JUMANJI.ProductionDeploymentEvidenceAttestation.v1":
        errors.append("DEPLOYMENT_ATTESTATION_SCHEMA_ID_INVALID")
    roles = attestation_schema.get("$defs", {}).get("signature", {}).get("properties", {}).get("role", {}).get("enum", [])
    if roles != ["SERVICE_OPERATOR", "INDEPENDENT_REVIEWER"]:
        errors.append("DEPLOYMENT_ATTESTATION_REQUIRES_TWO_DISTINCT_ROLES")
    if (
        attestation_template.get("schema") != "JUMANJI.ProductionDeploymentEvidenceAttestation.v1"
        or attestation_template.get("journalId") is not None
        or attestation_template.get("signatures") != []
    ):
        errors.append("DEPLOYMENT_ATTESTATION_TEMPLATE_MUST_REMAIN_UNSIGNED")

    if plan_schema.get("properties", {}).get("stage", {}).get("const") != 46:
        errors.append("STAGE46_PLAN_SCHEMA_CONTRACT_INVALID")

    script = (root / REQUIRED_FILES[4]).read_text(encoding="utf-8")
    for marker in (
        "PHASE_REQUIREMENTS",
        "TRUST_KEYRING_FINGERPRINT_MISMATCH",
        "DEPLOYMENT_PLAN_BINDING_MISMATCH",
        "EVIDENCE_PATH_TRAVERSAL",
        "DEPLOYMENT_EVIDENCE_HASH_MISMATCH",
        "DEPLOYMENT_SIGNER_KEY_REUSED",
        "DEPLOYMENT_SIGNATURE_INVALID",
        "POST_DEPLOYMENT_EVIDENCE_VERIFIED",
        "productionReady",
        "highAvailabilityApproved",
        "UNBASELINED",
        "promotionAllowed",
        "sideEffectsPerformedByVerifier",
        "MANUAL_OUT_OF_BAND",
    ):
        if marker not in script:
            errors.append(f"STAGE47_SECURITY_MARKER_MISSING:{marker}")
    if "kubectl" in script or "subprocess.run" in script or "os.system(" in script:
        errors.append("STAGE47_VERIFIER_MUST_NOT_EXECUTE_DEPLOYMENT")

    tests = (root / REQUIRED_FILES[6]).read_text(encoding="utf-8")
    for marker in (
        "test_signed_complete_journal_verifies_without_promoting_production",
        "test_missing_phase_evidence_blocks_verification",
        "test_evidence_file_tampering_is_rejected",
        "test_plan_byte_change_breaks_journal_binding",
        "test_signer_key_cannot_be_reused_for_both_roles",
        "test_revoked_operator_key_blocks_verification",
        "test_keyring_fingerprint_mismatch_blocks_verification",
        "test_path_traversal_is_rejected",
        "test_unsuccessful_rollout_or_executed_rollback_is_not_closable",
        "test_invalid_ed25519_signature_is_rejected",
        "test_empty_repository_templates_remain_blocked",
    ):
        if marker not in tests:
            errors.append(f"STAGE47_REGRESSION_TEST_MISSING:{marker}")

    doc = (root / REQUIRED_FILES[7]).read_text(encoding="utf-8")
    for marker in (
        "PREFLIGHT",
        "CANARY",
        "PROGRESSIVE_ROLLOUT",
        "POST_DEPLOYMENT_VERIFICATION",
        "SERVICE_OPERATOR",
        "INDEPENDENT_REVIEWER",
        "RFC 8785",
        "SHA-256",
        "NOT_REQUIRED",
        "promotionAllowed=false",
        "UNBASELINED",
        "Nie wykonuje deploymentu",
    ):
        if marker not in doc:
            errors.append(f"STAGE47_RUNBOOK_MARKER_MISSING:{marker}")

    workflow = (root / REQUIRED_FILES[8]).read_text(encoding="utf-8")
    for marker in (
        "cryptography>=42.0",
        "rfc8785==0.1.4",
        "validate_encounter_checkpoint_stage46_deployment.py",
        "validate_encounter_checkpoint_stage47_post_deployment.py",
        "test_validate_encounter_checkpoint_stage46_deployment.py",
        "test_validate_encounter_checkpoint_stage47_post_deployment.py",
    ):
        if marker not in workflow:
            errors.append(f"STAGE47_WORKFLOW_MARKER_MISSING:{marker}")
    if "aws-actions" in workflow or "azure/" in workflow or "google-github-actions" in workflow:
        errors.append("STAGE47_CI_MUST_NOT_DEPLOY_TO_CLOUD")

    return errors


def main() -> int:
    errors = validate_project(ROOT)
    if errors:
        print(f"Stage 47 post-deployment verification: FAIL ({len(errors)} error(s))")
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Stage 47 post-deployment verification: PASS")
    print("Signed evidence reconciliation is offline; deployment execution and production/HA/SLO promotion remain independently controlled.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
