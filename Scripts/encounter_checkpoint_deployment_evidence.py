#!/usr/bin/env python3
"""Offline Stage 47 verifier for externally executed deployment evidence.

This program never deploys, changes traffic, restarts services or performs rollback.
It verifies a signed journal and the exact bytes of its evidence attachments.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import re
import sys
from datetime import datetime, timezone
from getpass import getpass
from pathlib import Path, PurePosixPath
from typing import Any
from uuid import UUID

import rfc8785
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey

JOURNAL_SCHEMA = "JUMANJI.ProductionDeploymentEvidenceJournal.v1"
ATTESTATION_SCHEMA = "JUMANJI.ProductionDeploymentEvidenceAttestation.v1"
REPORT_SCHEMA = "JUMANJI.ProductionDeploymentEvidenceReport.v1"
KEYRING_SCHEMA = "JUMANJI.ProductionEvidenceTrustKeyring.v1"
SIGNATURE_SCHEMA = "JUMANJI.ProductionDeploymentEvidenceSignature.v1"
CANONICALIZATION = "RFC8785"
SHA256 = re.compile(r"^[a-f0-9]{64}$")
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{2,95}$")
MAX_EVIDENCE_BYTES = 25 * 1024 * 1024
ALLOWED_MEDIA_TYPES = {
    "application/json", "text/plain", "application/octet-stream",
    "image/png", "image/jpeg", "text/csv",
}
SIGNER_ROLES = {"SERVICE_OPERATOR", "INDEPENDENT_REVIEWER"}

PHASE_REQUIREMENTS = (
    ("PREFLIGHT", (
        "approved_change_window",
        "production_backup_and_restore_point",
        "operator_and_observer_identity",
        "pre_change_service_health",
        "rollback_baseline_retrieval_check",
    )),
    ("CANARY", (
        "canary_target_identity",
        "canary_service_health",
        "checkpoint_write_read_and_signature_smoke",
        "operator_canary_observation",
    )),
    ("PROGRESSIVE_ROLLOUT", (
        "operator_approval_for_each_batch",
        "per_batch_service_and_database_health",
        "lease_fencing_and_recovery_smoke",
        "client_compatibility_observation",
    )),
    ("POST_DEPLOYMENT_VERIFICATION", (
        "dedicated_server_startup_and_health",
        "encounter_join_and_checkpoint_resume_smoke",
        "android_install_launch_and_network_smoke",
        "post_change_backup_and_restore_point",
        "operator_and_independent_observer_acceptance",
    )),
)


class DeploymentEvidenceError(ValueError):
    """Fail-closed Stage 47 input, evidence, signature or binding error."""


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_utc(value: object) -> datetime | None:
    if not isinstance(value, str):
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
    if parsed.tzinfo is None or parsed.utcoffset() is None or parsed.utcoffset().total_seconds() != 0:
        return None
    return parsed.astimezone(timezone.utc)


def json_no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DeploymentEvidenceError("JSON_DUPLICATE_KEY")
        result[key] = value
    return result


def load_json_bytes(raw: bytes, label: str) -> object:
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=json_no_duplicates)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise DeploymentEvidenceError(f"{label}_JSON_INVALID") from exc


def read_regular_file(path: Path, label: str, max_bytes: int = 8 * 1024 * 1024) -> bytes:
    if path.is_symlink():
        raise DeploymentEvidenceError(f"{label}_SYMLINK_REJECTED")
    try:
        if not path.is_file():
            raise DeploymentEvidenceError(f"{label}_NOT_A_FILE")
        size = path.stat().st_size
        if size < 1 or size > max_bytes:
            raise DeploymentEvidenceError(f"{label}_SIZE_INVALID")
        return path.read_bytes()
    except OSError as exc:
        raise DeploymentEvidenceError(f"{label}_READ_FAILED") from exc


def sha256_bytes(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def safe_evidence_path(root: Path, relative: object) -> Path:
    if not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative:
        raise DeploymentEvidenceError("EVIDENCE_PATH_INVALID")
    posix = PurePosixPath(relative)
    if posix.is_absolute() or any(part in {"", ".", ".."} for part in posix.parts):
        raise DeploymentEvidenceError("EVIDENCE_PATH_TRAVERSAL")
    if not re.fullmatch(r"[A-Za-z0-9._/-]{1,240}", relative):
        raise DeploymentEvidenceError("EVIDENCE_PATH_INVALID")
    try:
        resolved_root = root.resolve(strict=True)
    except OSError as exc:
        raise DeploymentEvidenceError("EVIDENCE_ROOT_MISSING") from exc
    if root.is_symlink() or not resolved_root.is_dir():
        raise DeploymentEvidenceError("EVIDENCE_ROOT_INVALID")
    current = resolved_root
    for part in posix.parts:
        current = current / part
        if current.is_symlink():
            raise DeploymentEvidenceError("EVIDENCE_SYMLINK_REJECTED")
    try:
        resolved = current.resolve(strict=True)
        resolved.relative_to(resolved_root)
    except (OSError, ValueError) as exc:
        raise DeploymentEvidenceError("EVIDENCE_FILE_MISSING_OR_OUTSIDE_ROOT") from exc
    if not resolved.is_file():
        raise DeploymentEvidenceError("EVIDENCE_NOT_A_FILE")
    return resolved


def verify_plan_and_journal(
    plan: object,
    plan_raw: bytes,
    journal: object,
    journal_raw: bytes,
    evidence_root: Path,
) -> dict[str, Any]:
    if load_json_bytes(plan_raw, "DEPLOYMENT_PLAN") != plan:
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_BYTES_MISMATCH")
    if load_json_bytes(journal_raw, "DEPLOYMENT_JOURNAL") != journal:
        raise DeploymentEvidenceError("DEPLOYMENT_JOURNAL_BYTES_MISMATCH")
    if not isinstance(plan, dict) or not isinstance(journal, dict):
        raise DeploymentEvidenceError("INPUT_ROOT_INVALID")
    plan_fields = {
        "schema", "stage", "planId", "targetEnvironment", "executionMode",
        "state", "candidate", "rollbackBaseline", "sideEffectsPerformed",
        "deploymentExecuted", "postDeploymentVerified", "rollbackExecuted",
        "productionReady", "highAvailabilityApproved", "sloStatus", "promotionAllowed",
    }
    if not plan_fields.issubset(plan):
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_FIELDS_MISSING")
    if plan.get("schema") != "JUMANJI.ProductionDeploymentPlan.v1" or plan.get("stage") != 46:
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_SCHEMA_INVALID")
    if plan.get("targetEnvironment") != "production" or plan.get("executionMode") != "MANUAL_OUT_OF_BAND":
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_EXECUTION_MODE_INVALID")
    if plan.get("state") != "READY_FOR_OPERATOR_REVIEW":
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_NOT_READY_FOR_REVIEW")
    for flag in (
        "sideEffectsPerformed", "deploymentExecuted", "postDeploymentVerified",
        "rollbackExecuted", "productionReady", "highAvailabilityApproved", "promotionAllowed",
    ):
        if plan.get(flag) is not False:
            raise DeploymentEvidenceError(f"DEPLOYMENT_PLAN_BOUNDARY_INVALID:{flag}")
    if plan.get("sloStatus") != "UNBASELINED":
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_SLO_STATUS_INVALID")

    plan_id = plan.get("planId")
    try:
        UUID(str(plan_id))
    except (ValueError, TypeError, AttributeError) as exc:
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_ID_INVALID") from exc
    candidate = plan.get("candidate")
    rollback = plan.get("rollbackBaseline")
    if not isinstance(candidate, dict) or not isinstance(rollback, dict):
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_BINDINGS_INVALID")
    candidate_sha = candidate.get("candidateSha256")
    rollback_sha = rollback.get("baselineSha256")
    if not isinstance(candidate_sha, str) or not SHA256.fullmatch(candidate_sha):
        raise DeploymentEvidenceError("DEPLOYMENT_CANDIDATE_SHA_INVALID")
    if not isinstance(rollback_sha, str) or not SHA256.fullmatch(rollback_sha):
        raise DeploymentEvidenceError("ROLLBACK_BASELINE_SHA_INVALID")

    required_journal_fields = {
        "schema", "journalId", "planId", "planSha256", "candidateSha256",
        "rollbackBaselineSha256", "environmentId", "executionMode", "outcome",
        "rollbackOutcome", "startedAtUtc", "completedAtUtc", "phases",
    }
    if set(journal) != required_journal_fields:
        raise DeploymentEvidenceError("DEPLOYMENT_JOURNAL_FIELDS_INVALID")
    if journal.get("schema") != JOURNAL_SCHEMA:
        raise DeploymentEvidenceError("DEPLOYMENT_JOURNAL_SCHEMA_INVALID")
    try:
        UUID(str(journal.get("journalId")))
    except (ValueError, TypeError, AttributeError) as exc:
        raise DeploymentEvidenceError("DEPLOYMENT_JOURNAL_ID_INVALID") from exc
    expected_plan_sha = sha256_bytes(plan_raw)
    if journal.get("planId") != plan_id or journal.get("planSha256") != expected_plan_sha:
        raise DeploymentEvidenceError("DEPLOYMENT_PLAN_BINDING_MISMATCH")
    if journal.get("candidateSha256") != candidate_sha:
        raise DeploymentEvidenceError("DEPLOYMENT_CANDIDATE_BINDING_MISMATCH")
    if journal.get("rollbackBaselineSha256") != rollback_sha:
        raise DeploymentEvidenceError("DEPLOYMENT_ROLLBACK_BINDING_MISMATCH")
    if journal.get("environmentId") != "production" or journal.get("executionMode") != "MANUAL_OUT_OF_BAND":
        raise DeploymentEvidenceError("DEPLOYMENT_JOURNAL_ENVIRONMENT_INVALID")
    if journal.get("outcome") != "SUCCEEDED" or journal.get("rollbackOutcome") != "NOT_REQUIRED":
        raise DeploymentEvidenceError("DEPLOYMENT_JOURNAL_OUTCOME_NOT_CLOSABLE")

    started = parse_utc(journal.get("startedAtUtc"))
    completed = parse_utc(journal.get("completedAtUtc"))
    now = datetime.now(timezone.utc)
    if started is None or completed is None or completed < started or completed > now:
        raise DeploymentEvidenceError("DEPLOYMENT_JOURNAL_TIMESTAMPS_INVALID")

    phases = journal.get("phases")
    if not isinstance(phases, list) or len(phases) != len(PHASE_REQUIREMENTS):
        raise DeploymentEvidenceError("DEPLOYMENT_PHASE_COUNT_INVALID")
    seen_evidence_ids: set[str] = set()
    seen_paths: set[str] = set()
    evidence_count = 0
    previous_end = started
    phase_results: list[dict[str, Any]] = []
    for item, (expected_phase, required_categories) in zip(phases, PHASE_REQUIREMENTS):
        if not isinstance(item, dict) or set(item) != {
            "phase", "status", "startedAtUtc", "completedAtUtc", "evidence"
        }:
            raise DeploymentEvidenceError("DEPLOYMENT_PHASE_FIELDS_INVALID")
        if item.get("phase") != expected_phase or item.get("status") != "PASSED":
            raise DeploymentEvidenceError(f"DEPLOYMENT_PHASE_NOT_PASSED:{expected_phase}")
        phase_start = parse_utc(item.get("startedAtUtc"))
        phase_end = parse_utc(item.get("completedAtUtc"))
        if (
            phase_start is None or phase_end is None or phase_end < phase_start
            or phase_start < previous_end or phase_end > completed
        ):
            raise DeploymentEvidenceError(f"DEPLOYMENT_PHASE_TIMESTAMPS_INVALID:{expected_phase}")
        previous_end = phase_end
        records = item.get("evidence")
        if not isinstance(records, list) or len(records) != len(required_categories):
            raise DeploymentEvidenceError(f"DEPLOYMENT_PHASE_EVIDENCE_COUNT_INVALID:{expected_phase}")
        categories: set[str] = set()
        for record in records:
            if not isinstance(record, dict) or set(record) != {
                "evidenceId", "category", "path", "sha256", "sizeBytes", "mediaType"
            }:
                raise DeploymentEvidenceError("DEPLOYMENT_EVIDENCE_FIELDS_INVALID")
            category = record.get("category")
            if not isinstance(category, str) or category not in required_categories or category in categories:
                raise DeploymentEvidenceError(f"DEPLOYMENT_EVIDENCE_CATEGORY_INVALID:{expected_phase}")
            categories.add(category)
            evidence_id = record.get("evidenceId")
            try:
                UUID(str(evidence_id))
            except (ValueError, TypeError, AttributeError) as exc:
                raise DeploymentEvidenceError("DEPLOYMENT_EVIDENCE_ID_INVALID") from exc
            if evidence_id in seen_evidence_ids:
                raise DeploymentEvidenceError("DEPLOYMENT_EVIDENCE_ID_DUPLICATE")
            seen_evidence_ids.add(evidence_id)
            relative = record.get("path")
            if not isinstance(relative, str) or relative in seen_paths:
                raise DeploymentEvidenceError("DEPLOYMENT_EVIDENCE_PATH_DUPLICATE_OR_INVALID")
            seen_paths.add(relative)
            digest = record.get("sha256")
            size = record.get("sizeBytes")
            media = record.get("mediaType")
            if not isinstance(digest, str) or not SHA256.fullmatch(digest):
                raise DeploymentEvidenceError("DEPLOYMENT_EVIDENCE_SHA_INVALID")
            if type(size) is not int or size < 1 or size > MAX_EVIDENCE_BYTES:
                raise DeploymentEvidenceError("DEPLOYMENT_EVIDENCE_SIZE_INVALID")
            if media not in ALLOWED_MEDIA_TYPES:
                raise DeploymentEvidenceError("DEPLOYMENT_EVIDENCE_MEDIA_TYPE_INVALID")
            attachment = safe_evidence_path(evidence_root, relative)
            if attachment.stat().st_size != size:
                raise DeploymentEvidenceError(f"DEPLOYMENT_EVIDENCE_SIZE_MISMATCH:{category}")
            digest_state = hashlib.sha256()
            with attachment.open("rb") as handle:
                for block in iter(lambda: handle.read(1024 * 1024), b""):
                    digest_state.update(block)
            if digest_state.hexdigest() != digest:
                raise DeploymentEvidenceError(f"DEPLOYMENT_EVIDENCE_HASH_MISMATCH:{category}")
            evidence_count += 1
        if categories != set(required_categories):
            raise DeploymentEvidenceError(f"DEPLOYMENT_EVIDENCE_SET_INCOMPLETE:{expected_phase}")
        phase_results.append({
            "phase": expected_phase,
            "status": "VERIFIED",
            "evidenceCount": len(categories),
        })

    if previous_end > completed:
        raise DeploymentEvidenceError("DEPLOYMENT_PHASE_ORDER_INVALID")
    return {
        "journalId": journal["journalId"],
        "planId": plan_id,
        "journalSha256": sha256_bytes(journal_raw),
        "planSha256": expected_plan_sha,
        "candidateSha256": candidate_sha,
        "rollbackBaselineSha256": rollback_sha,
        "completedAtUtc": journal["completedAtUtc"],
        "verifiedEvidenceCount": evidence_count,
        "phases": phase_results,
    }


def signature_message(journal_sha: str, plan_sha: str, role: str, key_id: str, signed_at: str) -> bytes:
    return rfc8785.dumps({
        "schema": SIGNATURE_SCHEMA,
        "journalSha256": journal_sha,
        "planSha256": plan_sha,
        "role": role,
        "keyId": key_id,
        "signedAtUtc": signed_at,
    })


def load_keyring(keyring: object, keyring_raw: bytes, trusted_sha256: str) -> dict[str, dict[str, Any]]:
    if sha256_bytes(keyring_raw) != trusted_sha256:
        raise DeploymentEvidenceError("TRUST_KEYRING_FINGERPRINT_MISMATCH")
    if not isinstance(keyring, dict) or keyring.get("schema") != KEYRING_SCHEMA:
        raise DeploymentEvidenceError("TRUST_KEYRING_SCHEMA_INVALID")
    if set(keyring) != {"schema", "keys"} or not isinstance(keyring.get("keys"), list):
        raise DeploymentEvidenceError("TRUST_KEYRING_FIELDS_INVALID")
    indexed: dict[str, dict[str, Any]] = {}
    for item in keyring["keys"]:
        if not isinstance(item, dict) or set(item) != {
            "keyId", "publicKeyBase64", "roles", "status", "validFromUtc", "validUntilUtc"
        }:
            raise DeploymentEvidenceError("TRUST_KEY_FIELDS_INVALID")
        key_id = item.get("keyId")
        if not isinstance(key_id, str) or not SAFE_ID.fullmatch(key_id) or key_id in indexed:
            raise DeploymentEvidenceError("TRUST_KEY_ID_INVALID_OR_DUPLICATE")
        roles = item.get("roles")
        if not isinstance(roles, list) or len(roles) != len(set(roles)) or not set(roles).issubset({
            "SERVICE_OPERATOR", "DATABASE_OPERATOR", "SECURITY_OPERATOR",
            "UNREAL_QA_OPERATOR", "ANDROID_QA_OPERATOR", "SLO_REVIEWER",
            "INDEPENDENT_REVIEWER", "SECURITY_REVIEWER", "DATABASE_REVIEWER",
            "PLATFORM_REVIEWER", "QA_REVIEWER", "SERVICE_OWNER",
        }):
            raise DeploymentEvidenceError("TRUST_KEY_ROLES_INVALID")
        if item.get("status") not in {"ACTIVE", "REVOKED"}:
            raise DeploymentEvidenceError("TRUST_KEY_STATUS_INVALID")
        try:
            public_bytes = base64.b64decode(item.get("publicKeyBase64", ""), validate=True)
            if len(public_bytes) != 32:
                raise ValueError("wrong key length")
            item["_publicKey"] = Ed25519PublicKey.from_public_bytes(public_bytes)
        except (ValueError, TypeError) as exc:
            raise DeploymentEvidenceError("TRUST_KEY_PUBLIC_KEY_INVALID") from exc
        if parse_utc(item.get("validFromUtc")) is None:
            raise DeploymentEvidenceError("TRUST_KEY_VALID_FROM_INVALID")
        if item.get("validUntilUtc") is not None and parse_utc(item.get("validUntilUtc")) is None:
            raise DeploymentEvidenceError("TRUST_KEY_VALID_UNTIL_INVALID")
        indexed[key_id] = item
    return indexed


def verify_attestation(
    attestation: object,
    attestation_raw: bytes,
    material: dict[str, Any],
    keyring: object,
    keyring_raw: bytes,
    trusted_keyring_sha256: str,
) -> None:
    if load_json_bytes(attestation_raw, "DEPLOYMENT_ATTESTATION") != attestation:
        raise DeploymentEvidenceError("DEPLOYMENT_ATTESTATION_BYTES_MISMATCH")
    if not isinstance(attestation, dict) or set(attestation) != {
        "schema", "journalId", "journalSha256", "planSha256", "canonicalization", "signatures"
    }:
        raise DeploymentEvidenceError("DEPLOYMENT_ATTESTATION_FIELDS_INVALID")
    if attestation.get("schema") != ATTESTATION_SCHEMA or attestation.get("canonicalization") != CANONICALIZATION:
        raise DeploymentEvidenceError("DEPLOYMENT_ATTESTATION_SCHEMA_INVALID")
    if (
        attestation.get("journalId") != material["journalId"]
        or attestation.get("journalSha256") != material["journalSha256"]
        or attestation.get("planSha256") != material["planSha256"]
    ):
        raise DeploymentEvidenceError("DEPLOYMENT_ATTESTATION_BINDING_MISMATCH")
    signatures = attestation.get("signatures")
    if not isinstance(signatures, list) or len(signatures) != 2:
        raise DeploymentEvidenceError("DEPLOYMENT_ATTESTATION_REQUIRES_TWO_SIGNERS")
    indexed = load_keyring(keyring, keyring_raw, trusted_keyring_sha256)
    seen_roles: set[str] = set()
    seen_keys: set[str] = set()
    completed = parse_utc(material["completedAtUtc"])
    now = datetime.now(timezone.utc)
    for signature in signatures:
        if not isinstance(signature, dict) or set(signature) != {
            "role", "keyId", "signedAtUtc", "signatureBase64"
        }:
            raise DeploymentEvidenceError("DEPLOYMENT_SIGNATURE_FIELDS_INVALID")
        role = signature.get("role")
        key_id = signature.get("keyId")
        signed_at_text = signature.get("signedAtUtc")
        if role not in SIGNER_ROLES or role in seen_roles:
            raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_ROLE_INVALID_OR_DUPLICATE")
        if not isinstance(key_id, str) or key_id in seen_keys:
            raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_KEY_REUSED")
        key = indexed.get(key_id)
        if key is None or key.get("status") != "ACTIVE" or role not in key.get("roles", []):
            raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_NOT_TRUSTED_FOR_ROLE")
        signed_at = parse_utc(signed_at_text)
        valid_from = parse_utc(key.get("validFromUtc"))
        valid_until = parse_utc(key.get("validUntilUtc")) if key.get("validUntilUtc") else None
        if signed_at is None or completed is None or signed_at < completed or signed_at > now:
            raise DeploymentEvidenceError("DEPLOYMENT_SIGNATURE_TIMESTAMP_INVALID")
        if signed_at < valid_from or (valid_until is not None and signed_at > valid_until):
            raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_KEY_OUTSIDE_VALIDITY")
        try:
            raw_signature = base64.b64decode(signature.get("signatureBase64", ""), validate=True)
            if len(raw_signature) != 64:
                raise ValueError("wrong signature length")
            key["_publicKey"].verify(
                raw_signature,
                signature_message(
                    material["journalSha256"], material["planSha256"],
                    role, key_id, signed_at_text,
                ),
            )
        except (ValueError, InvalidSignature, TypeError) as exc:
            raise DeploymentEvidenceError("DEPLOYMENT_SIGNATURE_INVALID") from exc
        seen_roles.add(role)
        seen_keys.add(key_id)
    if seen_roles != SIGNER_ROLES:
        raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_QUORUM_INCOMPLETE")


def verify_deployment(
    plan: object,
    plan_raw: bytes,
    journal: object,
    journal_raw: bytes,
    evidence_root: Path,
    attestation: object,
    attestation_raw: bytes,
    keyring: object,
    keyring_raw: bytes,
    trusted_keyring_sha256: str,
) -> dict[str, Any]:
    material = verify_plan_and_journal(plan, plan_raw, journal, journal_raw, evidence_root)
    verify_attestation(
        attestation, attestation_raw, material,
        keyring, keyring_raw, trusted_keyring_sha256,
    )
    return {
        "schema": REPORT_SCHEMA,
        "outcome": "POST_DEPLOYMENT_EVIDENCE_VERIFIED",
        "journalId": material["journalId"],
        "planId": material["planId"],
        "journalSha256": material["journalSha256"],
        "planSha256": material["planSha256"],
        "candidateSha256": material["candidateSha256"],
        "rollbackBaselineSha256": material["rollbackBaselineSha256"],
        "completedAtUtc": material["completedAtUtc"],
        "verifiedEvidenceCount": material["verifiedEvidenceCount"],
        "phases": material["phases"],
        "signersVerified": ["INDEPENDENT_REVIEWER", "SERVICE_OPERATOR"],
        "deploymentExecuted": True,
        "postDeploymentVerified": True,
        "rollbackExecuted": False,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "promotionAllowed": False,
        "sideEffectsPerformedByVerifier": False,
    }


def sign_journal(
    plan: object,
    plan_raw: bytes,
    journal: object,
    journal_raw: bytes,
    evidence_root: Path,
    keyring: object,
    keyring_raw: bytes,
    trusted_keyring_sha256: str,
    role: str,
    key_id: str,
    private_key_path: Path,
    attestation_path: Path,
) -> dict[str, Any]:
    material = verify_plan_and_journal(plan, plan_raw, journal, journal_raw, evidence_root)
    indexed = load_keyring(keyring, keyring_raw, trusted_keyring_sha256)
    if role not in SIGNER_ROLES:
        raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_ROLE_INVALID")
    key = indexed.get(key_id)
    if key is None or key.get("status") != "ACTIVE" or role not in key.get("roles", []):
        raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_NOT_TRUSTED_FOR_ROLE")
    if private_key_path.is_symlink():
        raise DeploymentEvidenceError("PRIVATE_KEY_SYMLINK_REJECTED")
    try:
        private_raw = read_regular_file(private_key_path, "PRIVATE_KEY", 1024 * 1024)
    except DeploymentEvidenceError:
        raise
    password_text = getpass("Hasło klucza prywatnego Ed25519: ")
    password = password_text.encode("utf-8") if password_text else None
    try:
        private_key = serialization.load_pem_private_key(private_raw, password=password)
    except (ValueError, TypeError) as exc:
        raise DeploymentEvidenceError("PRIVATE_KEY_LOAD_FAILED") from exc
    if not isinstance(private_key, Ed25519PrivateKey):
        raise DeploymentEvidenceError("PRIVATE_KEY_NOT_ED25519")
    public_raw = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )
    trusted_raw = base64.b64decode(key["publicKeyBase64"], validate=True)
    if public_raw != trusted_raw:
        raise DeploymentEvidenceError("PRIVATE_KEY_DOES_NOT_MATCH_TRUSTED_KEY")

    if attestation_path.exists():
        if attestation_path.is_symlink():
            raise DeploymentEvidenceError("ATTESTATION_OUTPUT_SYMLINK_REJECTED")
        existing_raw = read_regular_file(attestation_path, "ATTESTATION_OUTPUT")
        existing = load_json_bytes(existing_raw, "ATTESTATION_OUTPUT")
        if not isinstance(existing, dict) or set(existing) != {
            "schema", "journalId", "journalSha256", "planSha256", "canonicalization", "signatures"
        }:
            raise DeploymentEvidenceError("ATTESTATION_OUTPUT_FIELDS_INVALID")
        if (
            existing.get("schema") != ATTESTATION_SCHEMA
            or existing.get("journalId") != material["journalId"]
            or existing.get("journalSha256") != material["journalSha256"]
            or existing.get("planSha256") != material["planSha256"]
            or existing.get("canonicalization") != CANONICALIZATION
            or not isinstance(existing.get("signatures"), list)
        ):
            raise DeploymentEvidenceError("ATTESTATION_OUTPUT_BINDING_MISMATCH")
        envelope = existing
    else:
        envelope = {
            "schema": ATTESTATION_SCHEMA,
            "journalId": material["journalId"],
            "journalSha256": material["journalSha256"],
            "planSha256": material["planSha256"],
            "canonicalization": CANONICALIZATION,
            "signatures": [],
        }
    if any(item.get("role") == role or item.get("keyId") == key_id for item in envelope["signatures"] if isinstance(item, dict)):
        raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_ROLE_OR_KEY_ALREADY_PRESENT")
    signed_at = utc_now()
    valid_from = parse_utc(key.get("validFromUtc"))
    valid_until = parse_utc(key.get("validUntilUtc")) if key.get("validUntilUtc") else None
    signed_at_dt = parse_utc(signed_at)
    completed = parse_utc(material["completedAtUtc"])
    if signed_at_dt is None or completed is None or signed_at_dt < completed or signed_at_dt < valid_from:
        raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_KEY_NOT_YET_VALID")
    if valid_until is not None and signed_at_dt > valid_until:
        raise DeploymentEvidenceError("DEPLOYMENT_SIGNER_KEY_EXPIRED")
    message = signature_message(material["journalSha256"], material["planSha256"], role, key_id, signed_at)
    signature = private_key.sign(message)
    envelope["signatures"].append({
        "role": role,
        "keyId": key_id,
        "signedAtUtc": signed_at,
        "signatureBase64": base64.b64encode(signature).decode("ascii"),
    })
    attestation_path.parent.mkdir(parents=True, exist_ok=True)
    if attestation_path.is_symlink():
        raise DeploymentEvidenceError("ATTESTATION_OUTPUT_SYMLINK_REJECTED")
    attestation_path.write_text(
        json.dumps(envelope, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    return {"outcome": "SIGNATURE_ADDED", "role": role, "keyId": key_id, "signatureCount": len(envelope["signatures"])}


def verify_command(args: argparse.Namespace) -> int:
    plan_raw = read_regular_file(args.plan, "DEPLOYMENT_PLAN")
    journal_raw = read_regular_file(args.journal, "DEPLOYMENT_JOURNAL")
    attestation_raw = read_regular_file(args.attestation, "DEPLOYMENT_ATTESTATION")
    keyring_raw = read_regular_file(args.keyring, "TRUST_KEYRING")
    plan = load_json_bytes(plan_raw, "DEPLOYMENT_PLAN")
    journal = load_json_bytes(journal_raw, "DEPLOYMENT_JOURNAL")
    attestation = load_json_bytes(attestation_raw, "DEPLOYMENT_ATTESTATION")
    keyring = load_json_bytes(keyring_raw, "TRUST_KEYRING")
    report = verify_deployment(
        plan, plan_raw, journal, journal_raw, args.evidence_root,
        attestation, attestation_raw, keyring, keyring_raw, args.trusted_keyring_sha256,
    )
    rendered = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if args.output:
        if args.output.exists() or args.output.is_symlink():
            raise DeploymentEvidenceError("REPORT_OUTPUT_ALREADY_EXISTS")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


def sign_command(args: argparse.Namespace) -> int:
    plan_raw = read_regular_file(args.plan, "DEPLOYMENT_PLAN")
    journal_raw = read_regular_file(args.journal, "DEPLOYMENT_JOURNAL")
    keyring_raw = read_regular_file(args.keyring, "TRUST_KEYRING")
    plan = load_json_bytes(plan_raw, "DEPLOYMENT_PLAN")
    journal = load_json_bytes(journal_raw, "DEPLOYMENT_JOURNAL")
    keyring = load_json_bytes(keyring_raw, "TRUST_KEYRING")
    result = sign_journal(
        plan, plan_raw, journal, journal_raw, args.evidence_root,
        keyring, keyring_raw, args.trusted_keyring_sha256,
        args.role, args.key_id, args.private_key, args.attestation,
    )
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Stage 47 offline post-deployment evidence verifier.")
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("sign", "verify"):
        command_parser = subparsers.add_parser(command)
        command_parser.add_argument("--plan", type=Path, required=True)
        command_parser.add_argument("--journal", type=Path, required=True)
        command_parser.add_argument("--evidence-root", type=Path, required=True)
        command_parser.add_argument("--keyring", type=Path, required=True)
        command_parser.add_argument("--trusted-keyring-sha256", required=True)
        if command == "sign":
            command_parser.add_argument("--role", choices=sorted(SIGNER_ROLES), required=True)
            command_parser.add_argument("--key-id", required=True)
            command_parser.add_argument("--private-key", type=Path, required=True)
            command_parser.add_argument("--attestation", type=Path, required=True)
        else:
            command_parser.add_argument("--attestation", type=Path, required=True)
            command_parser.add_argument("--output", type=Path)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        return sign_command(args) if args.command == "sign" else verify_command(args)
    except DeploymentEvidenceError as exc:
        print(f"Stage 47 blocked: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
