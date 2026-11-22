#!/usr/bin/env python3
"""Static fail-closed repository checks for Stage 44 evidence attestation."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_FILES = (
    "Data/Encounter/ProductionReadinessEvidencePacket.schema.json",
    "Data/Encounter/ProductionReadinessEvidencePacket.template.json",
    "Data/Encounter/ProductionEvidenceTrustKeyring.schema.json",
    "Data/Encounter/ProductionEvidenceTrustKeyring.template.json",
    "Data/Encounter/ProductionEvidenceAttestation.schema.json",
    "Data/Encounter/ProductionEvidenceAttestation.template.json",
    "Scripts/encounter_checkpoint_evidence_attestation.py",
    "Scripts/validate_encounter_checkpoint_stage43_evidence.py",
    "Scripts/validate_encounter_checkpoint_stage44_attestation.py",
    "Scripts/Tests/test_validate_encounter_checkpoint_stage44_attestation.py",
    "Docs/STAGE_44_CRYPTOGRAPHIC_EVIDENCE_ATTESTATION.md",
    ".github/workflows/encounter-checkpoint-stage44-attestation.yml",
)
REQUIRED_MARKERS = (
    "RFC8785",
    "ATTESTATION_PACKET_BINDING_MISMATCH",
    "TRUST_ANCHOR_HASH_REQUIRED",
    "FINAL_REVIEWER_WAS_EVIDENCE_OPERATOR",
    "ENTRY_SIGNERS_NOT_INDEPENDENT",
    "TRUSTED_KEY_REVOKED",
    "promotionAllowed",
    "productionReady",
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

    key_schema = _read_json(root, REQUIRED_FILES[2], errors)
    key_template = _read_json(root, REQUIRED_FILES[3], errors)
    attestation_schema = _read_json(root, REQUIRED_FILES[4], errors)
    attestation_template = _read_json(root, REQUIRED_FILES[5], errors)
    packet_template = _read_json(root, REQUIRED_FILES[1], errors)

    if key_schema.get("additionalProperties") is not False:
        errors.append("TRUST_KEYRING_SCHEMA_MUST_BE_STRICT")
    if key_template.get("schema") != "JUMANJI.ProductionEvidenceTrustKeyring.v1" or key_template.get("keys") != []:
        errors.append("TRUST_KEYRING_TEMPLATE_MUST_BE_EMPTY")
    if attestation_schema.get("additionalProperties") is not False:
        errors.append("ATTESTATION_SCHEMA_MUST_BE_STRICT")
    if (
        attestation_template.get("schema") != "JUMANJI.ProductionEvidenceAttestation.v1"
        or attestation_template.get("packetId") is not None
        or attestation_template.get("packetSha256") is not None
        or attestation_template.get("entryAttestations") != []
        or attestation_template.get("finalReviewSignatures") != []
    ):
        errors.append("ATTESTATION_TEMPLATE_MUST_START_EMPTY")
    if (
        packet_template.get("stage") != 43
        or packet_template.get("entries") != []
        or packet_template.get("submissionStatus") != "AWAITING_OPERATOR_EVIDENCE"
        or packet_template.get("review", {}).get("decision") != "NOT_REVIEWED"
    ):
        errors.append("STAGE43_PACKET_TEMPLATE_MUST_REMAIN_BLOCKED")

    tool = (root / REQUIRED_FILES[6]).read_text(encoding="utf-8")
    for marker in REQUIRED_MARKERS:
        if marker not in tool:
            errors.append(f"ATTESTATION_SECURITY_MARKER_MISSING:{marker}")

    tests = (root / REQUIRED_FILES[9]).read_text(encoding="utf-8")
    for marker in (
        "test_empty_repository_templates_remain_blocked",
        "test_complete_packet_with_all_signatures_is_only_release_board_eligible",
        "test_missing_out_of_band_keyring_hash_blocks_trust",
        "test_packet_mutation_invalidates_every_packet_bound_signature",
        "test_revoked_key_invalidates_existing_signature",
        "test_final_reviewer_cannot_be_an_evidence_operator",
    ):
        if marker not in tests:
            errors.append(f"ATTESTATION_REGRESSION_TEST_MISSING:{marker}")

    doc = (root / REQUIRED_FILES[10]).read_text(encoding="utf-8")
    for marker in (
        "Stage 43", "Ed25519", "RFC 8785", "poza tym samym repozytorium",
        "keyring", "Prywatne klucze", "BLOCKED_MISSING_EVIDENCE",
        "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW",
        "productionReady=false", "nie wykonano zewnętrznych testów",
    ):
        if marker not in doc:
            errors.append(f"ATTESTATION_RUNBOOK_MARKER_MISSING:{marker}")

    return errors


def main() -> int:
    errors = validate_project(ROOT)
    if errors:
        print(f"Stage 44 evidence attestation governance: FAIL ({len(errors)} error(s))")
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Stage 44 evidence attestation governance: PASS")
    print("Empty evidence/key templates remain blocked; production, HA and SLO are not approved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
