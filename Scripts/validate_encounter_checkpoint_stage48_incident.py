#!/usr/bin/env python3
"""Static, fail-closed repository contract checks for Stage 48."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_FILES = (
    "Data/Encounter/ProductionDeploymentIncidentRecord.schema.json",
    "Data/Encounter/ProductionDeploymentIncidentRecord.template.json",
    "Data/Encounter/ProductionDeploymentIncidentAttestation.schema.json",
    "Data/Encounter/ProductionDeploymentIncidentAttestation.template.json",
    "Scripts/encounter_checkpoint_incident_evidence.py",
    "Scripts/validate_encounter_checkpoint_stage48_incident.py",
    "Scripts/Tests/test_validate_encounter_checkpoint_stage48_incident.py",
    "Docs/STAGE_48_INCIDENT_RESPONSE_ROLLBACK_EVIDENCE_RECONCILIATION.md",
    ".github/workflows/encounter-checkpoint-stage48-incident.yml",
    "Data/Encounter/ProductionDeploymentPlan.schema.json",
    "Data/Encounter/ProductionEvidenceTrustKeyring.schema.json",
)


def read_json(relative: str, errors: list[str]) -> dict:
    try:
        value = json.loads((ROOT / relative).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        errors.append(f"JSON_READ_FAILED:{relative}")
        return {}
    if not isinstance(value, dict):
        errors.append(f"JSON_ROOT_NOT_OBJECT:{relative}")
        return {}
    return value


def validate_project() -> list[str]:
    errors: list[str] = []
    for relative in REQUIRED_FILES:
        if not (ROOT / relative).is_file():
            errors.append(f"REQUIRED_FILE_MISSING:{relative}")
    if errors:
        return errors

    schema = read_json(REQUIRED_FILES[0], errors)
    template = read_json(REQUIRED_FILES[1], errors)
    attestation_schema = read_json(REQUIRED_FILES[2], errors)
    attestation_template = read_json(REQUIRED_FILES[3], errors)
    plan_schema = read_json(REQUIRED_FILES[9], errors)
    keyring_schema = read_json(REQUIRED_FILES[10], errors)

    if schema.get("additionalProperties") is not False:
        errors.append("INCIDENT_RECORD_SCHEMA_MUST_BE_STRICT")
    if schema.get("properties", {}).get("stage", {}).get("const") != 48:
        errors.append("INCIDENT_RECORD_STAGE_INVALID")
    if schema.get("properties", {}).get("releaseDisposition", {}).get("const") != "HOLD":
        errors.append("INCIDENT_RELEASE_MUST_REMAIN_ON_HOLD")
    if schema.get("properties", {}).get("serviceState", {}).get("const") != "RESTORED":
        errors.append("INCIDENT_REQUIRES_SERVICE_RESTORATION")
    if schema.get("properties", {}).get("dataIntegrityState", {}).get("const") != "VERIFIED":
        errors.append("INCIDENT_REQUIRES_DATA_INTEGRITY_VERIFICATION")
    if (
        template.get("schema") != "JUMANJI.ProductionDeploymentIncidentRecord.v1"
        or template.get("status") != "BLOCKED_MISSING_EVIDENCE"
        or template.get("events") != []
        or template.get("incidentId") is not None
        or template.get("releaseDisposition") != "HOLD"
    ):
        errors.append("INCIDENT_RECORD_TEMPLATE_MUST_REMAIN_BLOCKED")

    if attestation_schema.get("additionalProperties") is not False:
        errors.append("INCIDENT_ATTESTATION_SCHEMA_MUST_BE_STRICT")
    roles = attestation_schema.get("$defs", {}).get("signature", {}).get("properties", {}).get("role", {}).get("enum", [])
    if roles != ["SERVICE_OPERATOR", "INDEPENDENT_REVIEWER"]:
        errors.append("INCIDENT_ATTESTATION_REQUIRES_TWO_ROLES")
    if (
        attestation_template.get("schema") != "JUMANJI.ProductionDeploymentIncidentAttestation.v1"
        or attestation_template.get("incidentId") is not None
        or attestation_template.get("signatures") != []
    ):
        errors.append("INCIDENT_ATTESTATION_TEMPLATE_MUST_REMAIN_UNSIGNED")

    if plan_schema.get("properties", {}).get("stage", {}).get("const") != 46:
        errors.append("INCIDENT_MUST_BIND_STAGE46_PLAN")
    if keyring_schema.get("properties", {}).get("schema", {}).get("const") != "JUMANJI.ProductionEvidenceTrustKeyring.v1":
        errors.append("INCIDENT_MUST_REUSE_STAGE44_TRUST_ROOT")

    script = (ROOT / REQUIRED_FILES[4]).read_text(encoding="utf-8")
    for marker in (
        "INCIDENT_PLAN_BINDING_MISMATCH",
        "INCIDENT_CANDIDATE_BINDING_MISMATCH",
        "INCIDENT_ROLLBACK_BASELINE_BINDING_MISMATCH",
        "INCIDENT_EVIDENCE_PATH_TRAVERSAL",
        "INCIDENT_EVIDENCE_HASH_MISMATCH",
        "INCIDENT_EVIDENCE_SENSITIVE_FIELD_REJECTED",
        "INCIDENT_SIGNER_KEY_REUSED",
        "INCIDENT_SIGNATURE_INVALID",
        "INCIDENT_FAILED_ROLLBACK_REQUIRES_ALTERNATE_RECOVERY",
        "INCIDENT_SERVICE_OR_DATA_NOT_RESTORED",
        "releaseDisposition",
        "promotionAllowed",
        "sideEffectsPerformedByVerifier",
    ):
        if marker not in script:
            errors.append(f"INCIDENT_SECURITY_MARKER_MISSING:{marker}")
    if "kubectl" in script or "subprocess.run" in script or "os.system(" in script:
        errors.append("INCIDENT_VERIFIER_MUST_NOT_EXECUTE_OPERATIONS")

    tests = (ROOT / REQUIRED_FILES[6]).read_text(encoding="utf-8")
    for marker in (
        "test_signed_rollback_incident_verifies_and_keeps_release_on_hold",
        "test_no_rollback_incident_uses_eight_ordered_evidence_records",
        "test_missing_rollback_event_blocks_rollback_incident",
        "test_attachment_tampering_is_rejected",
        "test_plan_byte_change_breaks_incident_binding",
        "test_path_traversal_is_rejected",
        "test_failed_rollback_requires_alternate_recovery",
        "test_service_and_data_must_be_restored_before_closure",
        "test_sensitive_json_fields_are_rejected",
        "test_revoked_signer_key_blocks_attestation",
        "test_same_key_cannot_sign_both_roles",
        "test_tampered_signature_is_rejected",
        "test_empty_templates_remain_blocked",
    ):
        if marker not in tests:
            errors.append(f"INCIDENT_REGRESSION_TEST_MISSING:{marker}")

    doc = (ROOT / REQUIRED_FILES[7]).read_text(encoding="utf-8")
    for marker in (
        "DETECTED", "DECLARED", "IMPACT_ASSESSED", "CONTAINED",
        "RECOVERY_DECISION", "ROLLBACK_EXECUTED", "SERVICE_RESTORED",
        "DATA_INTEGRITY_VERIFIED", "POST_INCIDENT_REVIEW",
        "RFC 8785", "SHA-256", "releaseDisposition=HOLD",
        "nie wykonuje", "SEV1", "SEV4",
    ):
        if marker not in doc:
            errors.append(f"INCIDENT_RUNBOOK_MARKER_MISSING:{marker}")

    workflow = (ROOT / REQUIRED_FILES[8]).read_text(encoding="utf-8")
    for marker in (
        "cryptography>=42.0", "rfc8785==0.1.4",
        "validate_encounter_checkpoint_stage47_post_deployment.py",
        "validate_encounter_checkpoint_stage48_incident.py",
        "test_validate_encounter_checkpoint_stage47_post_deployment.py",
        "test_validate_encounter_checkpoint_stage48_incident.py",
    ):
        if marker not in workflow:
            errors.append(f"INCIDENT_WORKFLOW_MARKER_MISSING:{marker}")
    if any(token in workflow for token in ("aws-actions", "azure/", "google-github-actions")):
        errors.append("INCIDENT_CI_MUST_NOT_EXECUTE_DEPLOYMENT_OR_ROLLBACK")
    return errors


def main() -> int:
    errors = validate_project()
    if errors:
        print(f"Stage 48 incident governance: FAIL ({len(errors)} error(s))")
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Stage 48 incident governance: PASS")
    print("Incident evidence is offline; release HOLD and production/HA/SLO gates remain closed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())