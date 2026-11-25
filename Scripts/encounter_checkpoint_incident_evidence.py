#!/usr/bin/env python3
"""Offline Stage 48 incident evidence verifier and dual-signature tool.

It verifies an operator-created incident record against a Stage 46 plan,
sanitized evidence attachments and the pinned Stage 44 public trust keyring.
It never deploys, rolls back, restarts services or changes traffic.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import sys
import tempfile
from datetime import datetime, timezone
from getpass import getpass
from pathlib import Path, PurePosixPath
from typing import Any
from uuid import UUID

import rfc8785
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey

INCIDENT_SCHEMA = "JUMANJI.ProductionDeploymentIncidentRecord.v1"
ATTESTATION_SCHEMA = "JUMANJI.ProductionDeploymentIncidentAttestation.v1"
REPORT_SCHEMA = "JUMANJI.ProductionDeploymentIncidentEvidenceReport.v1"
PLAN_SCHEMA = "JUMANJI.ProductionDeploymentPlan.v1"
KEYRING_SCHEMA = "JUMANJI.ProductionEvidenceTrustKeyring.v1"
SIGNATURE_SCHEMA = "JUMANJI.ProductionDeploymentIncidentSignature.v1"
CANONICALIZATION = "RFC8785"
SHA256 = re.compile(r"^[a-f0-9]{64}$")
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{2,95}$")
MAX_EVIDENCE_BYTES = 25 * 1024 * 1024
ALLOWED_MEDIA_TYPES = {
    "application/json", "text/plain", "application/octet-stream",
    "image/png", "image/jpeg", "text/csv",
}
SIGNER_ROLES = {"SERVICE_OPERATOR", "INDEPENDENT_REVIEWER"}
KEY_ROLES = {
    "SERVICE_OPERATOR", "DATABASE_OPERATOR", "SECURITY_OPERATOR",
    "UNREAL_QA_OPERATOR", "ANDROID_QA_OPERATOR", "SLO_REVIEWER",
    "INDEPENDENT_REVIEWER", "SECURITY_REVIEWER", "DATABASE_REVIEWER",
    "PLATFORM_REVIEWER", "QA_REVIEWER", "SERVICE_OWNER",
}
BASE_EVENT_ORDER = (
    "DETECTED", "DECLARED", "IMPACT_ASSESSED", "CONTAINED",
    "RECOVERY_DECISION",
)
TERMINAL_EVENT_ORDER = (
    "SERVICE_RESTORED", "DATA_INTEGRITY_VERIFIED", "POST_INCIDENT_REVIEW",
)
ACTOR_ROLES = KEY_ROLES
SENSITIVE_KEY = re.compile(
    r"(?:password|passwd|secret|token|private.?key|authorization|api.?key|"
    r"access.?key|refresh.?token|email|phone|player.?id|account.?id|steam.?id)",
    re.IGNORECASE,
)
SENSITIVE_TEXT = (
    re.compile(r"(?i)\bBearer\s+[A-Za-z0-9._~+/-]+=*"),
    re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    re.compile(r"\bAKIA[0-9A-Z]{16}\b"),
    re.compile(r"\beyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\b"),
    re.compile(r"\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b", re.IGNORECASE),
)


class IncidentEvidenceError(ValueError):
    """Fail-closed incident, evidence, signature or binding error."""


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
            raise IncidentEvidenceError("JSON_DUPLICATE_KEY")
        result[key] = value
    return result


def load_json_bytes(raw: bytes, label: str) -> object:
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=json_no_duplicates)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise IncidentEvidenceError(f"{label}_JSON_INVALID") from exc


def read_regular_file(path: Path, label: str, max_bytes: int = 8 * 1024 * 1024) -> bytes:
    if path.is_symlink():
        raise IncidentEvidenceError(f"{label}_SYMLINK_REJECTED")
    try:
        if not path.is_file():
            raise IncidentEvidenceError(f"{label}_NOT_A_FILE")
        size = path.stat().st_size
        if size < 1 or size > max_bytes:
            raise IncidentEvidenceError(f"{label}_SIZE_INVALID")
        return path.read_bytes()
    except OSError as exc:
        raise IncidentEvidenceError(f"{label}_READ_FAILED") from exc


def sha256_bytes(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def safe_evidence_path(root: Path, relative: object) -> Path:
    if not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative:
        raise IncidentEvidenceError("INCIDENT_EVIDENCE_PATH_INVALID")
    posix = PurePosixPath(relative)
    if posix.is_absolute() or any(part in {"", ".", ".."} for part in relative.split("/")):
        raise IncidentEvidenceError("INCIDENT_EVIDENCE_PATH_TRAVERSAL")
    if not re.fullmatch(r"[A-Za-z0-9._/-]{1,240}", relative):
        raise IncidentEvidenceError("INCIDENT_EVIDENCE_PATH_INVALID")
    try:
        resolved_root = root.resolve(strict=True)
    except OSError as exc:
        raise IncidentEvidenceError("INCIDENT_EVIDENCE_ROOT_MISSING") from exc
    if root.is_symlink() or not resolved_root.is_dir():
        raise IncidentEvidenceError("INCIDENT_EVIDENCE_ROOT_INVALID")
    current = resolved_root
    for part in posix.parts:
        current = current / part
        if current.is_symlink():
            raise IncidentEvidenceError("INCIDENT_EVIDENCE_SYMLINK_REJECTED")
    try:
        resolved = current.resolve(strict=True)
        resolved.relative_to(resolved_root)
    except (OSError, ValueError) as exc:
        raise IncidentEvidenceError("INCIDENT_EVIDENCE_FILE_MISSING_OR_OUTSIDE_ROOT") from exc
    if not resolved.is_file():
        raise IncidentEvidenceError("INCIDENT_EVIDENCE_NOT_A_FILE")
    return resolved


def inspect_sanitized_attachment(raw: bytes, media_type: str) -> None:
    if media_type in {"application/json", "text/plain", "text/csv"}:
        try:
            text = raw.decode("utf-8")
        except UnicodeError as exc:
            raise IncidentEvidenceError("INCIDENT_EVIDENCE_TEXT_ENCODING_INVALID") from exc
        for pattern in SENSITIVE_TEXT:
            if pattern.search(text):
                raise IncidentEvidenceError("INCIDENT_EVIDENCE_SENSITIVE_TEXT_REJECTED")
        if media_type == "application/json":
            parsed = load_json_bytes(raw, "INCIDENT_EVIDENCE")
            def walk(value: object) -> None:
                if isinstance(value, dict):
                    for key, child in value.items():
                        if SENSITIVE_KEY.search(str(key)):
                            raise IncidentEvidenceError("INCIDENT_EVIDENCE_SENSITIVE_FIELD_REJECTED")
                        walk(child)
                elif isinstance(value, list):
                    for child in value:
                        walk(child)
            walk(parsed)


def validate_plan(plan: object, plan_raw: bytes) -> dict[str, Any]:
    if load_json_bytes(plan_raw, "DEPLOYMENT_PLAN") != plan or not isinstance(plan, dict):
        raise IncidentEvidenceError("DEPLOYMENT_PLAN_BYTES_MISMATCH")
    if plan.get("schema") != PLAN_SCHEMA or plan.get("stage") != 46:
        raise IncidentEvidenceError("DEPLOYMENT_PLAN_SCHEMA_INVALID")
    if (
        plan.get("targetEnvironment") != "production"
        or plan.get("executionMode") != "MANUAL_OUT_OF_BAND"
        or plan.get("state") != "READY_FOR_OPERATOR_REVIEW"
    ):
        raise IncidentEvidenceError("DEPLOYMENT_PLAN_NOT_OPERATOR_READY")
    for field in (
        "sideEffectsPerformed", "deploymentExecuted", "postDeploymentVerified",
        "rollbackExecuted", "productionReady", "highAvailabilityApproved", "promotionAllowed",
    ):
        if plan.get(field) is not False:
            raise IncidentEvidenceError(f"DEPLOYMENT_PLAN_BOUNDARY_INVALID:{field}")
    if plan.get("sloStatus") != "UNBASELINED":
        raise IncidentEvidenceError("DEPLOYMENT_PLAN_SLO_STATUS_INVALID")
    try:
        UUID(str(plan.get("planId")))
    except (ValueError, TypeError, AttributeError) as exc:
        raise IncidentEvidenceError("DEPLOYMENT_PLAN_ID_INVALID") from exc
    candidate = plan.get("candidate")
    rollback = plan.get("rollbackBaseline")
    if not isinstance(candidate, dict) or not isinstance(rollback, dict):
        raise IncidentEvidenceError("DEPLOYMENT_PLAN_BINDINGS_INVALID")
    candidate_sha = candidate.get("candidateSha256")
    rollback_sha = rollback.get("baselineSha256")
    if not isinstance(candidate_sha, str) or not SHA256.fullmatch(candidate_sha):
        raise IncidentEvidenceError("DEPLOYMENT_CANDIDATE_SHA_INVALID")
    if not isinstance(rollback_sha, str) or not SHA256.fullmatch(rollback_sha):
        raise IncidentEvidenceError("DEPLOYMENT_ROLLBACK_BASELINE_SHA_INVALID")
    return {
        "planId": plan["planId"],
        "planSha256": sha256_bytes(plan_raw),
        "candidateSha256": candidate_sha,
        "rollbackBaselineSha256": rollback_sha,
    }


def validate_incident(
    plan: object,
    plan_raw: bytes,
    incident: object,
    incident_raw: bytes,
    evidence_root: Path,
) -> dict[str, Any]:
    material = validate_plan(plan, plan_raw)
    if load_json_bytes(incident_raw, "INCIDENT_RECORD") != incident or not isinstance(incident, dict):
        raise IncidentEvidenceError("INCIDENT_RECORD_BYTES_MISMATCH")
    required_fields = {
        "schema", "stage", "incidentId", "planId", "planSha256",
        "candidateSha256", "rollbackBaselineSha256", "environmentId",
        "executionMode", "status", "severity", "deploymentOutcome",
        "rollbackOutcome", "resolutionAction", "serviceState",
        "dataIntegrityState", "releaseDisposition", "startedAtUtc",
        "resolvedAtUtc", "events",
    }
    if set(incident) != required_fields:
        raise IncidentEvidenceError("INCIDENT_RECORD_FIELDS_INVALID")
    if incident.get("schema") != INCIDENT_SCHEMA or incident.get("stage") != 48:
        raise IncidentEvidenceError("INCIDENT_RECORD_SCHEMA_INVALID")
    if incident.get("status") != "RESOLVED_FOR_REVIEW":
        raise IncidentEvidenceError("INCIDENT_NOT_RESOLVED_FOR_REVIEW")
    if incident.get("environmentId") != "production" or incident.get("executionMode") != "MANUAL_OUT_OF_BAND":
        raise IncidentEvidenceError("INCIDENT_ENVIRONMENT_INVALID")
    if incident.get("severity") not in {"SEV1", "SEV2", "SEV3", "SEV4"}:
        raise IncidentEvidenceError("INCIDENT_SEVERITY_INVALID")
    if incident.get("deploymentOutcome") not in {"ABORTED", "FAILED", "PARTIALLY_APPLIED"}:
        raise IncidentEvidenceError("INCIDENT_DEPLOYMENT_OUTCOME_NOT_INCIDENT")
    rollback_outcome = incident.get("rollbackOutcome")
    action = incident.get("resolutionAction")
    if rollback_outcome not in {"NOT_REQUIRED", "SUCCEEDED", "FAILED"}:
        raise IncidentEvidenceError("INCIDENT_ROLLBACK_OUTCOME_INVALID")
    if rollback_outcome == "SUCCEEDED" and action != "ROLLBACK":
        raise IncidentEvidenceError("INCIDENT_ROLLBACK_ACTION_MISMATCH")
    if rollback_outcome == "NOT_REQUIRED" and action == "ROLLBACK":
        raise IncidentEvidenceError("INCIDENT_ROLLBACK_ACTION_MISMATCH")
    if rollback_outcome == "FAILED" and action == "ROLLBACK":
        raise IncidentEvidenceError("INCIDENT_FAILED_ROLLBACK_REQUIRES_ALTERNATE_RECOVERY")
    if action not in {"ROLLBACK", "FORWARD_FIX", "TRAFFIC_ISOLATION"}:
        raise IncidentEvidenceError("INCIDENT_RESOLUTION_ACTION_INVALID")
    if incident.get("serviceState") != "RESTORED" or incident.get("dataIntegrityState") != "VERIFIED":
        raise IncidentEvidenceError("INCIDENT_SERVICE_OR_DATA_NOT_RESTORED")
    if incident.get("releaseDisposition") != "HOLD":
        raise IncidentEvidenceError("INCIDENT_RELEASE_MUST_REMAIN_ON_HOLD")
    try:
        UUID(str(incident.get("incidentId")))
    except (ValueError, TypeError, AttributeError) as exc:
        raise IncidentEvidenceError("INCIDENT_ID_INVALID") from exc
    if incident.get("planId") != material["planId"] or incident.get("planSha256") != material["planSha256"]:
        raise IncidentEvidenceError("INCIDENT_PLAN_BINDING_MISMATCH")
    if incident.get("candidateSha256") != material["candidateSha256"]:
        raise IncidentEvidenceError("INCIDENT_CANDIDATE_BINDING_MISMATCH")
    if incident.get("rollbackBaselineSha256") != material["rollbackBaselineSha256"]:
        raise IncidentEvidenceError("INCIDENT_ROLLBACK_BASELINE_BINDING_MISMATCH")

    started = parse_utc(incident.get("startedAtUtc"))
    resolved = parse_utc(incident.get("resolvedAtUtc"))
    now = datetime.now(timezone.utc)
    if started is None or resolved is None or resolved < started or resolved > now:
        raise IncidentEvidenceError("INCIDENT_TIMESTAMPS_INVALID")

    events = incident.get("events")
    expected = list(BASE_EVENT_ORDER)
    if rollback_outcome != "NOT_REQUIRED":
        expected.append("ROLLBACK_EXECUTED")
    expected.extend(TERMINAL_EVENT_ORDER)
    if not isinstance(events, list) or len(events) != len(expected):
        raise IncidentEvidenceError("INCIDENT_EVENT_COUNT_INVALID")

    previous = started
    seen_event_ids: set[str] = set()
    seen_evidence_ids: set[str] = set()
    seen_paths: set[str] = set()
    event_results: list[dict[str, str]] = []
    for index, (event, expected_type) in enumerate(zip(events, expected)):
        if not isinstance(event, dict) or set(event) != {
            "eventId", "eventType", "occurredAtUtc", "actorRole", "summary", "evidence"
        }:
            raise IncidentEvidenceError("INCIDENT_EVENT_FIELDS_INVALID")
        if event.get("eventType") != expected_type:
            raise IncidentEvidenceError(f"INCIDENT_EVENT_ORDER_INVALID:{expected_type}")
        try:
            event_id = str(UUID(str(event.get("eventId"))))
        except (ValueError, TypeError, AttributeError) as exc:
            raise IncidentEvidenceError("INCIDENT_EVENT_ID_INVALID") from exc
        if event_id in seen_event_ids:
            raise IncidentEvidenceError("INCIDENT_EVENT_ID_DUPLICATE")
        seen_event_ids.add(event_id)
        occurred = parse_utc(event.get("occurredAtUtc"))
        if occurred is None or occurred < previous or occurred > resolved:
            raise IncidentEvidenceError(f"INCIDENT_EVENT_TIMESTAMP_INVALID:{expected_type}")
        if index == 0 and occurred != started:
            raise IncidentEvidenceError("INCIDENT_START_MUST_MATCH_DETECTION")
        if index == len(expected) - 1 and occurred != resolved:
            raise IncidentEvidenceError("INCIDENT_RESOLUTION_MUST_MATCH_FINAL_REVIEW")
        previous = occurred
        if event.get("actorRole") not in ACTOR_ROLES:
            raise IncidentEvidenceError("INCIDENT_EVENT_ACTOR_ROLE_INVALID")
        summary = event.get("summary")
        if not isinstance(summary, str) or not 8 <= len(summary) <= 512 or any(ord(ch) < 32 and ch not in "\t" for ch in summary):
            raise IncidentEvidenceError("INCIDENT_EVENT_SUMMARY_INVALID")
        for pattern in SENSITIVE_TEXT:
            if pattern.search(summary):
                raise IncidentEvidenceError("INCIDENT_EVENT_SENSITIVE_SUMMARY_REJECTED")

        evidence = event.get("evidence")
        if not isinstance(evidence, dict) or set(evidence) != {
            "evidenceId", "path", "sha256", "sizeBytes", "mediaType"
        }:
            raise IncidentEvidenceError("INCIDENT_EVENT_EVIDENCE_FIELDS_INVALID")
        try:
            evidence_id = str(UUID(str(evidence.get("evidenceId"))))
        except (ValueError, TypeError, AttributeError) as exc:
            raise IncidentEvidenceError("INCIDENT_EVIDENCE_ID_INVALID") from exc
        if evidence_id in seen_evidence_ids:
            raise IncidentEvidenceError("INCIDENT_EVIDENCE_ID_DUPLICATE")
        seen_evidence_ids.add(evidence_id)
        relative = evidence.get("path")
        if not isinstance(relative, str) or relative in seen_paths:
            raise IncidentEvidenceError("INCIDENT_EVIDENCE_PATH_DUPLICATE_OR_INVALID")
        seen_paths.add(relative)
        digest = evidence.get("sha256")
        size = evidence.get("sizeBytes")
        media = evidence.get("mediaType")
        if not isinstance(digest, str) or not SHA256.fullmatch(digest):
            raise IncidentEvidenceError("INCIDENT_EVIDENCE_SHA_INVALID")
        if type(size) is not int or size < 1 or size > MAX_EVIDENCE_BYTES:
            raise IncidentEvidenceError("INCIDENT_EVIDENCE_SIZE_INVALID")
        if media not in ALLOWED_MEDIA_TYPES:
            raise IncidentEvidenceError("INCIDENT_EVIDENCE_MEDIA_TYPE_INVALID")
        attachment = safe_evidence_path(evidence_root, relative)
        if attachment.stat().st_size != size:
            raise IncidentEvidenceError(f"INCIDENT_EVIDENCE_SIZE_MISMATCH:{expected_type}")
        raw = read_regular_file(attachment, "INCIDENT_EVIDENCE", MAX_EVIDENCE_BYTES)
        if sha256_bytes(raw) != digest:
            raise IncidentEvidenceError(f"INCIDENT_EVIDENCE_HASH_MISMATCH:{expected_type}")
        inspect_sanitized_attachment(raw, media)
        event_results.append({
            "eventType": expected_type,
            "occurredAtUtc": event["occurredAtUtc"],
            "evidenceSha256": digest,
        })

    return {
        "incidentId": incident["incidentId"],
        "planId": material["planId"],
        "incidentSha256": sha256_bytes(incident_raw),
        "planSha256": material["planSha256"],
        "candidateSha256": material["candidateSha256"],
        "rollbackBaselineSha256": material["rollbackBaselineSha256"],
        "severity": incident["severity"],
        "deploymentOutcome": incident["deploymentOutcome"],
        "rollbackOutcome": rollback_outcome,
        "resolutionAction": action,
        "resolvedAtUtc": incident["resolvedAtUtc"],
        "verifiedEvidenceCount": len(event_results),
        "events": event_results,
    }


def signature_message(
    incident_sha: str, plan_sha: str, role: str, key_id: str, signed_at: str
) -> bytes:
    return rfc8785.dumps({
        "schema": SIGNATURE_SCHEMA,
        "incidentSha256": incident_sha,
        "planSha256": plan_sha,
        "role": role,
        "keyId": key_id,
        "signedAtUtc": signed_at,
    })


def load_keyring(keyring: object, keyring_raw: bytes, trusted_sha256: str) -> dict[str, dict[str, Any]]:
    if sha256_bytes(keyring_raw) != trusted_sha256:
        raise IncidentEvidenceError("TRUST_KEYRING_FINGERPRINT_MISMATCH")
    if not isinstance(keyring, dict) or keyring.get("schema") != KEYRING_SCHEMA:
        raise IncidentEvidenceError("TRUST_KEYRING_SCHEMA_INVALID")
    if set(keyring) != {"schema", "keys"} or not isinstance(keyring.get("keys"), list):
        raise IncidentEvidenceError("TRUST_KEYRING_FIELDS_INVALID")
    indexed: dict[str, dict[str, Any]] = {}
    for item in keyring["keys"]:
        if not isinstance(item, dict) or set(item) != {
            "keyId", "publicKeyBase64", "roles", "status", "validFromUtc", "validUntilUtc"
        }:
            raise IncidentEvidenceError("TRUST_KEY_FIELDS_INVALID")
        key_id = item.get("keyId")
        if not isinstance(key_id, str) or not SAFE_ID.fullmatch(key_id) or key_id in indexed:
            raise IncidentEvidenceError("TRUST_KEY_ID_INVALID_OR_DUPLICATE")
        roles = item.get("roles")
        if not isinstance(roles, list) or len(roles) != len(set(roles)) or not set(roles).issubset(KEY_ROLES):
            raise IncidentEvidenceError("TRUST_KEY_ROLES_INVALID")
        if item.get("status") not in {"ACTIVE", "REVOKED"}:
            raise IncidentEvidenceError("TRUST_KEY_STATUS_INVALID")
        try:
            public_raw = base64.b64decode(item.get("publicKeyBase64", ""), validate=True)
            if len(public_raw) != 32:
                raise ValueError("wrong Ed25519 key length")
            item["_publicKey"] = Ed25519PublicKey.from_public_bytes(public_raw)
        except (ValueError, TypeError) as exc:
            raise IncidentEvidenceError("TRUST_KEY_PUBLIC_KEY_INVALID") from exc
        if parse_utc(item.get("validFromUtc")) is None:
            raise IncidentEvidenceError("TRUST_KEY_VALID_FROM_INVALID")
        if item.get("validUntilUtc") is not None and parse_utc(item.get("validUntilUtc")) is None:
            raise IncidentEvidenceError("TRUST_KEY_VALID_UNTIL_INVALID")
        indexed[key_id] = item
    return indexed


def verify_attestation(
    attestation: object, attestation_raw: bytes, material: dict[str, Any],
    keyring: object, keyring_raw: bytes, trusted_keyring_sha256: str,
) -> None:
    if load_json_bytes(attestation_raw, "INCIDENT_ATTESTATION") != attestation:
        raise IncidentEvidenceError("INCIDENT_ATTESTATION_BYTES_MISMATCH")
    expected_fields = {
        "schema", "incidentId", "incidentSha256", "planSha256",
        "canonicalization", "signatures",
    }
    if not isinstance(attestation, dict) or set(attestation) != expected_fields:
        raise IncidentEvidenceError("INCIDENT_ATTESTATION_FIELDS_INVALID")
    if attestation.get("schema") != ATTESTATION_SCHEMA or attestation.get("canonicalization") != CANONICALIZATION:
        raise IncidentEvidenceError("INCIDENT_ATTESTATION_SCHEMA_INVALID")
    if (
        attestation.get("incidentId") != material["incidentId"]
        or attestation.get("incidentSha256") != material["incidentSha256"]
        or attestation.get("planSha256") != material["planSha256"]
    ):
        raise IncidentEvidenceError("INCIDENT_ATTESTATION_BINDING_MISMATCH")
    signatures = attestation.get("signatures")
    if not isinstance(signatures, list) or len(signatures) != 2:
        raise IncidentEvidenceError("INCIDENT_ATTESTATION_REQUIRES_TWO_SIGNERS")
    indexed = load_keyring(keyring, keyring_raw, trusted_keyring_sha256)
    seen_roles: set[str] = set()
    seen_keys: set[str] = set()
    resolved = parse_utc(material["resolvedAtUtc"])
    now = datetime.now(timezone.utc)
    for signature in signatures:
        if not isinstance(signature, dict) or set(signature) != {
            "role", "keyId", "signedAtUtc", "signatureBase64"
        }:
            raise IncidentEvidenceError("INCIDENT_SIGNATURE_FIELDS_INVALID")
        role = signature.get("role")
        key_id = signature.get("keyId")
        signed_at_text = signature.get("signedAtUtc")
        if role not in SIGNER_ROLES or role in seen_roles:
            raise IncidentEvidenceError("INCIDENT_SIGNER_ROLE_INVALID_OR_DUPLICATE")
        if not isinstance(key_id, str) or key_id in seen_keys:
            raise IncidentEvidenceError("INCIDENT_SIGNER_KEY_REUSED")
        key = indexed.get(key_id)
        if key is None or key.get("status") != "ACTIVE" or role not in key.get("roles", []):
            raise IncidentEvidenceError("INCIDENT_SIGNER_NOT_TRUSTED_FOR_ROLE")
        signed_at = parse_utc(signed_at_text)
        valid_from = parse_utc(key.get("validFromUtc"))
        valid_until = parse_utc(key.get("validUntilUtc")) if key.get("validUntilUtc") else None
        if signed_at is None or resolved is None or signed_at < resolved or signed_at > now:
            raise IncidentEvidenceError("INCIDENT_SIGNATURE_TIMESTAMP_INVALID")
        if signed_at < valid_from or (valid_until is not None and signed_at > valid_until):
            raise IncidentEvidenceError("INCIDENT_SIGNER_KEY_OUTSIDE_VALIDITY")
        try:
            raw_signature = base64.b64decode(signature.get("signatureBase64", ""), validate=True)
            if len(raw_signature) != 64:
                raise ValueError("wrong signature length")
            key["_publicKey"].verify(
                raw_signature,
                signature_message(
                    material["incidentSha256"], material["planSha256"],
                    role, key_id, signed_at_text,
                ),
            )
        except (ValueError, InvalidSignature, TypeError) as exc:
            raise IncidentEvidenceError("INCIDENT_SIGNATURE_INVALID") from exc
        seen_roles.add(role)
        seen_keys.add(key_id)
    if seen_roles != SIGNER_ROLES:
        raise IncidentEvidenceError("INCIDENT_SIGNER_QUORUM_INCOMPLETE")


def verify_incident(
    plan: object, plan_raw: bytes, incident: object, incident_raw: bytes,
    evidence_root: Path, attestation: object, attestation_raw: bytes,
    keyring: object, keyring_raw: bytes, trusted_keyring_sha256: str,
) -> dict[str, Any]:
    material = validate_incident(plan, plan_raw, incident, incident_raw, evidence_root)
    verify_attestation(
        attestation, attestation_raw, material,
        keyring, keyring_raw, trusted_keyring_sha256,
    )
    return {
        "schema": REPORT_SCHEMA,
        "outcome": "INCIDENT_EVIDENCE_VERIFIED",
        "incidentId": material["incidentId"],
        "planId": material["planId"],
        "incidentSha256": material["incidentSha256"],
        "planSha256": material["planSha256"],
        "candidateSha256": material["candidateSha256"],
        "rollbackBaselineSha256": material["rollbackBaselineSha256"],
        "severity": material["severity"],
        "deploymentOutcome": material["deploymentOutcome"],
        "rollbackOutcome": material["rollbackOutcome"],
        "rollbackExecuted": material["rollbackOutcome"] != "NOT_REQUIRED",
        "resolutionAction": material["resolutionAction"],
        "resolvedAtUtc": material["resolvedAtUtc"],
        "verifiedEvidenceCount": material["verifiedEvidenceCount"],
        "events": material["events"],
        "signersVerified": ["INDEPENDENT_REVIEWER", "SERVICE_OPERATOR"],
        "incidentClosedForAudit": True,
        "releaseDisposition": "HOLD",
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "promotionAllowed": False,
        "sideEffectsPerformedByVerifier": False,
    }


def write_json_atomic(path: Path, value: object) -> None:
    if path.is_symlink():
        raise IncidentEvidenceError("OUTPUT_SYMLINK_REJECTED")
    path.parent.mkdir(parents=True, exist_ok=True)
    data = (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=path.name + ".", suffix=".tmp",
            dir=path.parent, delete=False,
        ) as handle:
            temporary_name = handle.name
            try:
                os.chmod(temporary_name, 0o600)
            except OSError:
                pass
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, path)
    except OSError as exc:
        if temporary_name:
            try:
                Path(temporary_name).unlink(missing_ok=True)
            except OSError:
                pass
        raise IncidentEvidenceError("OUTPUT_WRITE_FAILED") from exc


def sign_incident(
    plan: object, plan_raw: bytes, incident: object, incident_raw: bytes,
    evidence_root: Path, keyring: object, keyring_raw: bytes,
    trusted_keyring_sha256: str, role: str, key_id: str,
    private_key_path: Path, attestation_path: Path,
) -> dict[str, Any]:
    material = validate_incident(plan, plan_raw, incident, incident_raw, evidence_root)
    indexed = load_keyring(keyring, keyring_raw, trusted_keyring_sha256)
    if role not in SIGNER_ROLES:
        raise IncidentEvidenceError("INCIDENT_SIGNER_ROLE_INVALID")
    key = indexed.get(key_id)
    if key is None or key.get("status") != "ACTIVE" or role not in key.get("roles", []):
        raise IncidentEvidenceError("INCIDENT_SIGNER_NOT_TRUSTED_FOR_ROLE")
    private_raw = read_regular_file(private_key_path, "PRIVATE_KEY", 1024 * 1024)
    password_text = getpass("Hasło klucza prywatnego Ed25519: ")
    password = password_text.encode("utf-8") if password_text else None
    try:
        private_key = serialization.load_pem_private_key(private_raw, password=password)
    except (ValueError, TypeError) as exc:
        raise IncidentEvidenceError("PRIVATE_KEY_LOAD_FAILED") from exc
    if not isinstance(private_key, Ed25519PrivateKey):
        raise IncidentEvidenceError("PRIVATE_KEY_NOT_ED25519")
    public_raw = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )
    trusted_raw = base64.b64decode(key["publicKeyBase64"], validate=True)
    if public_raw != trusted_raw:
        raise IncidentEvidenceError("PRIVATE_KEY_DOES_NOT_MATCH_TRUSTED_KEY")

    envelope = {
        "schema": ATTESTATION_SCHEMA,
        "incidentId": material["incidentId"],
        "incidentSha256": material["incidentSha256"],
        "planSha256": material["planSha256"],
        "canonicalization": CANONICALIZATION,
        "signatures": [],
    }
    if attestation_path.exists():
        if attestation_path.is_symlink():
            raise IncidentEvidenceError("ATTESTATION_OUTPUT_SYMLINK_REJECTED")
        existing = load_json_bytes(
            read_regular_file(attestation_path, "ATTESTATION_OUTPUT"),
            "ATTESTATION_OUTPUT",
        )
        if not isinstance(existing, dict) or set(existing) != set(envelope):
            raise IncidentEvidenceError("ATTESTATION_OUTPUT_FIELDS_INVALID")
        for field in ("schema", "incidentId", "incidentSha256", "planSha256", "canonicalization"):
            if existing.get(field) != envelope[field]:
                raise IncidentEvidenceError("ATTESTATION_OUTPUT_BINDING_MISMATCH")
        if not isinstance(existing.get("signatures"), list):
            raise IncidentEvidenceError("ATTESTATION_OUTPUT_SIGNATURES_INVALID")
        envelope = existing

    signatures = envelope["signatures"]
    if any(item.get("role") == role for item in signatures if isinstance(item, dict)):
        raise IncidentEvidenceError("INCIDENT_SIGNER_ROLE_ALREADY_PRESENT")
    if any(item.get("keyId") == key_id for item in signatures if isinstance(item, dict)):
        raise IncidentEvidenceError("INCIDENT_SIGNER_KEY_REUSED")
    if len(signatures) >= 2:
        raise IncidentEvidenceError("INCIDENT_ATTESTATION_ALREADY_COMPLETE")

    signed_at = utc_now()
    signed_at_dt = parse_utc(signed_at)
    resolved_dt = parse_utc(material["resolvedAtUtc"])
    if signed_at_dt is None or resolved_dt is None or signed_at_dt < resolved_dt:
        raise IncidentEvidenceError("INCIDENT_SIGNATURE_TIMESTAMP_INVALID")
    raw_signature = private_key.sign(signature_message(
        material["incidentSha256"], material["planSha256"], role, key_id, signed_at
    ))
    signatures.append({
        "role": role,
        "keyId": key_id,
        "signedAtUtc": signed_at,
        "signatureBase64": base64.b64encode(raw_signature).decode("ascii"),
    })
    write_json_atomic(attestation_path, envelope)
    return {
        "outcome": "INCIDENT_SIGNATURE_ADDED",
        "role": role,
        "keyId": key_id,
        "signaturesPresent": len(signatures),
        "attestationPath": str(attestation_path),
    }


def load_input(path_text: str, label: str, max_bytes: int = 8 * 1024 * 1024) -> tuple[object, bytes]:
    raw = read_regular_file(Path(path_text), label, max_bytes)
    return load_json_bytes(raw, label), raw


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Offline Stage 48 production incident evidence tool.")
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("sign", "verify"):
        sub = subparsers.add_parser(command)
        sub.add_argument("--plan", required=True, help="Exact Stage 46 deployment plan JSON")
        sub.add_argument("--incident", required=True, help="Stage 48 incident record JSON")
        sub.add_argument("--evidence-root", required=True, help="Root of sanitized incident attachments")
        sub.add_argument("--keyring", required=True, help="Stage 44 public trust keyring")
        sub.add_argument("--trusted-keyring-sha256", required=True, help="Out-of-band SHA-256 of the keyring")
        sub.add_argument("--attestation", required=True, help="Stage 48 incident attestation JSON")
        if command == "sign":
            sub.add_argument("--role", required=True, choices=sorted(SIGNER_ROLES))
            sub.add_argument("--key-id", required=True)
            sub.add_argument("--private-key", required=True, help="Encrypted Ed25519 PEM; password is prompted")
        else:
            sub.add_argument("--report", required=True, help="Output audit report JSON")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        plan, plan_raw = load_input(args.plan, "DEPLOYMENT_PLAN")
        incident, incident_raw = load_input(args.incident, "INCIDENT_RECORD")
        keyring, keyring_raw = load_input(args.keyring, "TRUST_KEYRING")
        if args.command == "sign":
            result = sign_incident(
                plan, plan_raw, incident, incident_raw, Path(args.evidence_root),
                keyring, keyring_raw, args.trusted_keyring_sha256,
                args.role, args.key_id, Path(args.private_key), Path(args.attestation),
            )
            print(json.dumps(result, ensure_ascii=False, indent=2))
            return 0
        attestation, attestation_raw = load_input(args.attestation, "INCIDENT_ATTESTATION")
        report = verify_incident(
            plan, plan_raw, incident, incident_raw, Path(args.evidence_root),
            attestation, attestation_raw, keyring, keyring_raw,
            args.trusted_keyring_sha256,
        )
        write_json_atomic(Path(args.report), report)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0
    except (IncidentEvidenceError, OSError) as exc:
        print(f"Stage 48 incident evidence rejected: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())