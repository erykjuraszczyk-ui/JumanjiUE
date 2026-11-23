#!/usr/bin/env python3
"""Static fail-closed repository checks for Stage 45 Release Board authorization."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_FILES = (
    "Data/Encounter/ProductionReleaseCandidate.schema.json",
    "Data/Encounter/ProductionReleaseCandidate.template.json",
    "Data/Encounter/ProductionReleaseBoardDecision.schema.json",
    "Data/Encounter/ProductionReleaseBoardDecision.template.json",
    "Scripts/encounter_checkpoint_evidence_attestation.py",
    "Scripts/encounter_checkpoint_release_board.py",
    "Scripts/validate_encounter_checkpoint_stage43_evidence.py",
    "Scripts/validate_encounter_checkpoint_stage44_attestation.py",
    "Scripts/validate_encounter_checkpoint_stage45_release_board.py",
    "Scripts/Tests/test_validate_encounter_checkpoint_stage45_release_board.py",
    "Docs/STAGE_45_RELEASE_BOARD_DEPLOYMENT_AUTHORIZATION.md",
    ".github/workflows/encounter-checkpoint-stage45-release-board.yml",
)
SCRIPT_MARKERS = (
    "RFC8785",
    "UPSTREAM_ATTESTATION_NOT_ELIGIBLE",
    "BOARD_KEY_REUSED_UPSTREAM",
    "CANDIDATE_ARTIFACT_HASH_MISMATCH",
    "CANDIDATE_ANDROID_PACKAGE_NOT_BOUND_TO_STAGE43_EVIDENCE",
    "RELEASE_BOARD_SIGNATURE_VERIFICATION_FAILED",
    "deploymentAuthorizationGranted",
    "deploymentExecuted",
    "productionReady",
    "promotionAllowed",
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

    candidate_schema = _read_json(root, REQUIRED_FILES[0], errors)
    candidate_template = _read_json(root, REQUIRED_FILES[1], errors)
    decision_schema = _read_json(root, REQUIRED_FILES[2], errors)
    decision_template = _read_json(root, REQUIRED_FILES[3], errors)

    if candidate_schema.get("additionalProperties") is not False:
        errors.append("CANDIDATE_SCHEMA_MUST_BE_STRICT")
    if candidate_schema.get("properties", {}).get("schema", {}).get("const") != "JUMANJI.ProductionReleaseCandidate.v1":
        errors.append("CANDIDATE_SCHEMA_ID_INVALID")
    if (
        candidate_template.get("schema") != "JUMANJI.ProductionReleaseCandidate.v1"
        or candidate_template.get("candidateId") is not None
        or candidate_template.get("packetSha256") is not None
        or candidate_template.get("targets") != []
    ):
        errors.append("CANDIDATE_TEMPLATE_MUST_REMAIN_EMPTY")

    if decision_schema.get("additionalProperties") is not False:
        errors.append("DECISION_SCHEMA_MUST_BE_STRICT")
    if decision_schema.get("properties", {}).get("stage", {}).get("const") != 45:
        errors.append("DECISION_SCHEMA_STAGE_INVALID")
    if (
        decision_template.get("schema") != "JUMANJI.ProductionReleaseBoardDecision.v1"
        or decision_template.get("stage") != 45
        or decision_template.get("decision") != "NOT_DECIDED"
        or decision_template.get("boardSignatures") != []
        or decision_template.get("decisionId") is not None
    ):
        errors.append("DECISION_TEMPLATE_MUST_REMAIN_UNSIGNED")

    script = (root / REQUIRED_FILES[5]).read_text(encoding="utf-8")
    for marker in SCRIPT_MARKERS:
        if marker not in script:
            errors.append(f"RELEASE_BOARD_SECURITY_MARKER_MISSING:{marker}")

    tests = (root / REQUIRED_FILES[9]).read_text(encoding="utf-8")
    for marker in (
        "test_unsigned_decision_waits_for_all_three_board_roles",
        "test_three_independent_signatures_authorize_exact_candidate_only",
        "test_stage44_signer_cannot_be_reused_as_release_board_member",
        "test_mutating_android_artifact_invalidates_candidate",
        "test_candidate_manifest_mutation_invalidates_decision_binding",
        "test_rejected_board_decision_never_authorizes_deployment",
        "test_changed_trust_keyring_breaks_decision_binding",
        "test_empty_repository_templates_remain_blocked",
    ):
        if marker not in tests:
            errors.append(f"RELEASE_BOARD_REGRESSION_TEST_MISSING:{marker}")

    doc = (root / REQUIRED_FILES[10]).read_text(encoding="utf-8")
    for marker in (
        "Stage 44",
        "SERVICE_OWNER",
        "SECURITY_REVIEWER",
        "PLATFORM_REVIEWER",
        "BOARD_KEY_REUSED_UPSTREAM",
        "APPROVED_FOR_DEPLOYMENT",
        "deploymentExecuted=false",
        "productionReady=false",
        "promotionAllowed=false",
        "Nie wykonuje wdrożenia",
    ):
        if marker not in doc:
            errors.append(f"RELEASE_BOARD_RUNBOOK_MARKER_MISSING:{marker}")

    workflow = (root / REQUIRED_FILES[11]).read_text(encoding="utf-8")
    for marker in (
        "cryptography>=42.0",
        "rfc8785==0.1.4",
        "validate_encounter_checkpoint_stage45_release_board.py",
        "test_validate_encounter_checkpoint_stage45_release_board.py",
        "BLOCKED_UPSTREAM_NOT_ELIGIBLE",
    ):
        if marker not in workflow:
            errors.append(f"RELEASE_BOARD_WORKFLOW_MARKER_MISSING:{marker}")

    return errors


def main() -> int:
    errors = validate_project(ROOT)
    if errors:
        print(f"Stage 45 Release Board authorization: FAIL ({len(errors)} error(s))")
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Stage 45 Release Board authorization: PASS")
    print("Unsigned templates remain blocked; deployment, production readiness and promotion remain false.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
