#!/usr/bin/env python3
"""Ed25519 evidence attestation for the Stage 43 production-readiness packet.

This tool signs packet-bound evidence metadata. It does not prove that a human
performed an operation, authenticate the external key registrar, approve a
release, or change production/HA/SLO state.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone
from getpass import getpass
from pathlib import Path
from typing import Any
from uuid import UUID

import rfc8785
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import validate_encounter_checkpoint_stage43_evidence as stage43  # noqa: E402

KEYRING_SCHEMA = "JUMANJI.ProductionEvidenceTrustKeyring.v1"
ATTESTATION_SCHEMA = "JUMANJI.ProductionEvidenceAttestation.v1"
REPORT_SCHEMA = "JUMANJI.ProductionEvidenceAttestationReport.v1"
CANONICALIZATION = "RFC8785"
SHA256 = re.compile(r"^[a-f0-9]{64}$")
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{2,95}$")
B64_PUBLIC = re.compile(r"^[A-Za-z0-9+/]{43}=$")

OPERATOR_ROLES = {
    "SERVICE_OPERATOR", "DATABASE_OPERATOR", "SECURITY_OPERATOR",
    "UNREAL_QA_OPERATOR", "ANDROID_QA_OPERATOR", "SLO_REVIEWER",
}
EVIDENCE_REVIEWER_ROLES = {
    "INDEPENDENT_REVIEWER", "SECURITY_REVIEWER", "DATABASE_REVIEWER",
    "PLATFORM_REVIEWER", "QA_REVIEWER",
}
FINAL_REVIEW_ROLES = {
    "SERVICE_OWNER", "SECURITY_REVIEWER", "DATABASE_REVIEWER",
    "PLATFORM_REVIEWER", "QA_REVIEWER",
}
ALL_ROLES = OPERATOR_ROLES | EVIDENCE_REVIEWER_ROLES | FINAL_REVIEW_ROLES
KEY_FIELDS = {"keyId", "publicKeyBase64", "roles", "status", "validFromUtc", "validUntilUtc"}
ENVELOPE_FIELDS = {
    "schema", "packetId", "packetSha256", "canonicalization",
    "entryAttestations", "finalReviewSignatures",
}
SIGNATURE_FIELDS = {"keyId", "signedAtUtc", "signatureBase64"}
ENTRY_ATTESTATION_FIELDS = {"evidenceId", "operator", "reviewer"}
FINAL_SIGNATURE_FIELDS = {"reviewerRole", "keyId", "signedAtUtc", "signatureBase64"}


class AttestationError(ValueError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def _parse_utc(value: object) -> datetime | None:
    if not isinstance(value, str):
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
    if parsed.tzinfo is None or parsed.utcoffset() is None or parsed.utcoffset().total_seconds() != 0:
        return None
    return parsed.astimezone(timezone.utc)


def _canonical(value: object) -> bytes:
    return rfc8785.dumps(value)


def packet_digest(packet: dict[str, Any]) -> str:
    return hashlib.sha256(_canonical(packet)).hexdigest()


def _load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise AttestationError("JSON_READ_FAILED") from exc


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink():
        raise AttestationError("OUTPUT_SYMLINK_REJECTED")
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def _keyring_index(keyring: object) -> tuple[dict[str, dict[str, Any]], list[str]]:
    errors: list[str] = []
    index: dict[str, dict[str, Any]] = {}
    seen_public: set[bytes] = set()
    if not isinstance(keyring, dict) or set(keyring) != {"schema", "keys"}:
        return {}, ["KEYRING_FIELDS_INVALID"]
    if keyring.get("schema") != KEYRING_SCHEMA or not isinstance(keyring.get("keys"), list):
        return {}, ["KEYRING_SCHEMA_INVALID"]

    for item in keyring["keys"]:
        if not isinstance(item, dict) or set(item) != KEY_FIELDS:
            errors.append("KEY_RECORD_FIELDS_INVALID")
            continue
        key_id = item.get("keyId")
        if not isinstance(key_id, str) or not SAFE_ID.fullmatch(key_id):
            errors.append("KEY_ID_INVALID")
            continue
        if key_id in index:
            errors.append("DUPLICATE_KEY_ID")
            continue
        roles = item.get("roles")
        if (
            not isinstance(roles, list) or not roles
            or any(not isinstance(role, str) or role not in ALL_ROLES for role in roles)
            or len(set(roles)) != len(roles)
        ):
            errors.append("KEY_ROLES_INVALID")
            continue
        status = item.get("status")
        if not isinstance(status, str) or status not in {"ACTIVE", "REVOKED"}:
            errors.append("KEY_STATUS_INVALID")
            continue
        valid_from = _parse_utc(item.get("validFromUtc"))
        valid_until_value = item.get("validUntilUtc")
        valid_until = None if valid_until_value is None else _parse_utc(valid_until_value)
        if valid_from is None or (valid_until_value is not None and valid_until is None):
            errors.append("KEY_VALIDITY_WINDOW_INVALID")
            continue
        if valid_until is not None and valid_until < valid_from:
            errors.append("KEY_VALIDITY_WINDOW_INVALID")
            continue
        encoded = item.get("publicKeyBase64")
        if not isinstance(encoded, str) or not B64_PUBLIC.fullmatch(encoded):
            errors.append("PUBLIC_KEY_ENCODING_INVALID")
            continue
        try:
            raw = base64.b64decode(encoded, validate=True)
            if len(raw) != 32:
                raise ValueError("bad key length")
            public_key = Ed25519PublicKey.from_public_bytes(raw)
        except (ValueError, TypeError):
            errors.append("PUBLIC_KEY_INVALID")
            continue
        if raw in seen_public:
            errors.append("DUPLICATE_PUBLIC_KEY")
            continue
        seen_public.add(raw)
        index[key_id] = {
            "document": item,
            "public": public_key,
            "raw": raw,
            "roles": set(roles),
            "validFrom": valid_from,
            "validUntil": valid_until,
        }
    return index, errors


def _signature_record_valid(value: object) -> bool:
    if not isinstance(value, dict) or set(value) != SIGNATURE_FIELDS:
        return False
    if not isinstance(value.get("keyId"), str) or not SAFE_ID.fullmatch(value["keyId"]):
        return False
    if _parse_utc(value.get("signedAtUtc")) is None:
        return False
    encoded = value.get("signatureBase64")
    if not isinstance(encoded, str):
        return False
    try:
        raw = base64.b64decode(encoded, validate=True)
    except (ValueError, TypeError):
        return False
    return len(raw) == 64


def _entry_payload(
    packet: dict[str, Any],
    digest: str,
    entry: dict[str, Any],
    signer_type: str,
    signer_role: str,
    signed_at: str,
) -> dict[str, Any]:
    artifact = entry["artifact"]
    return {
        "schema": "JUMANJI.ProductionEvidenceEntryAttestationPayload.v1",
        "canonicalization": CANONICALIZATION,
        "packetId": packet["packetId"],
        "packetSha256": digest,
        "evidenceId": entry["evidenceId"],
        "kind": entry["kind"],
        "sourceClass": entry["sourceClass"],
        "environmentId": entry["environmentId"],
        "capturedAtUtc": entry["capturedAtUtc"],
        "operatorRole": entry["operatorRole"],
        "reviewerRole": entry["reviewerRole"],
        "artifact": {
            "path": artifact["path"],
            "sha256": artifact["sha256"],
            "mediaType": artifact["mediaType"],
        },
        "signerType": signer_type,
        "signerRole": signer_role,
        "signedAtUtc": signed_at,
    }


def _final_review_payload(
    packet: dict[str, Any], digest: str, role: str, signed_at: str
) -> dict[str, Any]:
    review = packet["review"]
    return {
        "schema": "JUMANJI.ProductionEvidenceFinalReviewPayload.v1",
        "canonicalization": CANONICALIZATION,
        "packetId": packet["packetId"],
        "packetSha256": digest,
        "decision": review["decision"],
        "reviewerRoles": review["reviewerRoles"],
        "reviewedAtUtc": review["reviewedAtUtc"],
        "signerRole": role,
        "signedAtUtc": signed_at,
    }


def _resolve_key(
    index: dict[str, dict[str, Any]],
    key_id: object,
    role: str,
    signed_at: str,
) -> tuple[dict[str, Any] | None, str | None]:
    if not isinstance(key_id, str) or key_id not in index:
        return None, "TRUSTED_KEY_NOT_FOUND"
    record = index[key_id]
    doc = record["document"]
    if doc["status"] != "ACTIVE":
        return None, "TRUSTED_KEY_REVOKED"
    if role not in record["roles"]:
        return None, "TRUSTED_KEY_ROLE_MISMATCH"
    timestamp = _parse_utc(signed_at)
    if timestamp is None or timestamp < record["validFrom"]:
        return None, "SIGNATURE_OUTSIDE_KEY_VALIDITY"
    if record["validUntil"] is not None and timestamp > record["validUntil"]:
        return None, "SIGNATURE_OUTSIDE_KEY_VALIDITY"
    return record, None


def _verify_signature(
    signature: object,
    role: str,
    payload: dict[str, Any],
    index: dict[str, dict[str, Any]],
    not_before: str,
) -> tuple[dict[str, Any] | None, str | None]:
    if not _signature_record_valid(signature):
        return None, "SIGNATURE_RECORD_INVALID"
    assert isinstance(signature, dict)
    record, error = _resolve_key(index, signature["keyId"], role, signature["signedAtUtc"])
    if error:
        return None, error
    assert record is not None
    signed_time = _parse_utc(signature["signedAtUtc"])
    boundary_time = _parse_utc(not_before)
    if boundary_time is None:
        return None, "SIGNATURE_TIME_BOUNDARY_INVALID"
    if signed_time is None or signed_time < boundary_time:
        return None, "SIGNATURE_PREDATES_EVIDENCE_OR_REVIEW"
    signed_payload = dict(payload)
    signed_payload["signedAtUtc"] = signature["signedAtUtc"]
    try:
        signature_bytes = base64.b64decode(signature["signatureBase64"], validate=True)
        record["public"].verify(signature_bytes, _canonical(signed_payload))
    except (InvalidSignature, ValueError, TypeError):
        return None, "SIGNATURE_VERIFICATION_FAILED"
    return record, None


def _validate_envelope_shape(envelope: object) -> tuple[dict[str, Any] | None, list[str]]:
    errors: list[str] = []
    if not isinstance(envelope, dict) or set(envelope) != ENVELOPE_FIELDS:
        return None, ["ATTESTATION_ENVELOPE_FIELDS_INVALID"]
    if envelope.get("schema") != ATTESTATION_SCHEMA:
        errors.append("ATTESTATION_SCHEMA_INVALID")
    if envelope.get("canonicalization") != CANONICALIZATION:
        errors.append("ATTESTATION_CANONICALIZATION_INVALID")
    packet_id = envelope.get("packetId")
    digest = envelope.get("packetSha256")
    if packet_id is not None:
        try:
            UUID(str(packet_id))
        except (ValueError, TypeError, AttributeError):
            errors.append("ATTESTATION_PACKET_ID_INVALID")
    if digest is not None and (not isinstance(digest, str) or not SHA256.fullmatch(digest)):
        errors.append("ATTESTATION_PACKET_DIGEST_INVALID")
    if not isinstance(envelope.get("entryAttestations"), list):
        errors.append("ENTRY_ATTESTATIONS_MUST_BE_ARRAY")
    if not isinstance(envelope.get("finalReviewSignatures"), list):
        errors.append("FINAL_REVIEW_SIGNATURES_MUST_BE_ARRAY")
    if errors:
        return None, errors
    return envelope, []


def verify_attestations(
    packet: object,
    envelope: object,
    keyring: object,
    trusted_keyring_sha256: str | None,
    keyring_raw: bytes | None = None,
) -> dict[str, Any]:
    errors: list[str] = []
    missing_entry_signatures = 0
    missing_final_signatures = 0
    verified_operator = 0
    verified_evidence_reviewer = 0
    verified_final_reviewers = 0

    if not isinstance(packet, dict):
        return {"errors": ["PACKET_ROOT_MUST_BE_OBJECT"], "missingEntrySignatures": 0,
                "missingFinalSignatures": 0, "verifiedOperator": 0,
                "verifiedEvidenceReviewer": 0, "verifiedFinalReviewers": 0,
                "trustedKeyCount": 0, "trustAnchorVerified": False}

    key_index, key_errors = _keyring_index(keyring)
    errors.extend(key_errors)
    if keyring_raw is not None:
        actual_keyring_sha = hashlib.sha256(keyring_raw).hexdigest()
    else:
        actual_keyring_sha = ""
    trust_anchor_verified = bool(
        isinstance(trusted_keyring_sha256, str)
        and SHA256.fullmatch(trusted_keyring_sha256)
        and actual_keyring_sha
        and actual_keyring_sha == trusted_keyring_sha256
    )
    trust_anchor_error = None
    if trusted_keyring_sha256 is None:
        trust_anchor_error = "TRUST_ANCHOR_HASH_REQUIRED"
    elif not isinstance(trusted_keyring_sha256, str) or not SHA256.fullmatch(trusted_keyring_sha256):
        trust_anchor_error = "TRUST_ANCHOR_HASH_INVALID"
    elif actual_keyring_sha != trusted_keyring_sha256:
        errors.append("TRUST_ANCHOR_HASH_MISMATCH")

    env, envelope_errors = _validate_envelope_shape(envelope)
    errors.extend(envelope_errors)
    if env is None:
        return {"errors": errors, "missingEntrySignatures": 0, "missingFinalSignatures": 0,
                "verifiedOperator": 0, "verifiedEvidenceReviewer": 0,
                "verifiedFinalReviewers": 0, "trustedKeyCount": len(key_index),
                "trustAnchorVerified": trust_anchor_verified,
                "trustAnchorError": trust_anchor_error}

    try:
        digest = packet_digest(packet)
    except Exception:
        errors.append("PACKET_CANONICALIZATION_FAILED")
        digest = ""
    packet_id = packet.get("packetId")
    records = env["entryAttestations"]
    final_records = env["finalReviewSignatures"]
    signatures_present = bool(records or final_records)

    if env["packetId"] is None and env["packetSha256"] is None and not signatures_present:
        pass
    elif env["packetId"] != packet_id or env["packetSha256"] != digest:
        errors.append("ATTESTATION_PACKET_BINDING_MISMATCH")

    entry_map: dict[str, dict[str, Any]] = {}
    for record in records:
        if not isinstance(record, dict) or set(record) != ENTRY_ATTESTATION_FIELDS:
            errors.append("ENTRY_ATTESTATION_FIELDS_INVALID")
            continue
        evidence_id = record.get("evidenceId")
        if not isinstance(evidence_id, str):
            errors.append("ENTRY_ATTESTATION_ID_INVALID")
            continue
        if evidence_id in entry_map:
            errors.append("DUPLICATE_ENTRY_ATTESTATION")
            continue
        if not (record.get("operator") is None or _signature_record_valid(record.get("operator"))):
            errors.append("OPERATOR_SIGNATURE_RECORD_INVALID")
        if not (record.get("reviewer") is None or _signature_record_valid(record.get("reviewer"))):
            errors.append("EVIDENCE_REVIEWER_SIGNATURE_RECORD_INVALID")
        entry_map[evidence_id] = record

    packet_entries = packet.get("entries") if isinstance(packet.get("entries"), list) else []
    valid_packet_ids: set[str] = set()
    operator_key_ids: set[str] = set()
    operator_public_keys: set[bytes] = set()
    reviewer_keys_by_entry: list[tuple[str, bytes]] = []

    for entry in packet_entries:
        if not isinstance(entry, dict):
            continue
        evidence_id = entry.get("evidenceId")
        if not isinstance(evidence_id, str):
            continue
        valid_packet_ids.add(evidence_id)
        record = entry_map.get(evidence_id)
        if record is None:
            missing_entry_signatures += 2
            continue
        for signer_type, field_name, role_field in (
            ("operator", "operator", "operatorRole"),
            ("reviewer", "reviewer", "reviewerRole"),
        ):
            signature = record.get(field_name)
            if signature is None:
                missing_entry_signatures += 1
                continue
            role = entry.get(role_field)
            if not isinstance(role, str):
                errors.append("PACKET_ENTRY_ROLE_INVALID")
                continue
            signed_at = signature.get("signedAtUtc") if isinstance(signature, dict) else ""
            try:
                payload = _entry_payload(packet, digest, entry, signer_type, role, signed_at)
            except (KeyError, TypeError):
                errors.append("PACKET_ENTRY_PAYLOAD_INVALID")
                continue
            captured_at = entry.get("capturedAtUtc")
            resolved, error = _verify_signature(signature, role, payload, key_index, captured_at)
            if error:
                errors.append(error)
                continue
            assert resolved is not None
            key_id = signature["keyId"]
            if signer_type == "operator":
                operator_key_ids.add(key_id)
                operator_public_keys.add(resolved["raw"])
                verified_operator += 1
            else:
                if key_id in operator_key_ids or resolved["raw"] in operator_public_keys:
                    errors.append("ENTRY_SIGNERS_NOT_INDEPENDENT")
                reviewer_keys_by_entry.append((key_id, resolved["raw"]))
                verified_evidence_reviewer += 1

        operator_sig = record.get("operator")
        reviewer_sig = record.get("reviewer")
        if isinstance(operator_sig, dict) and isinstance(reviewer_sig, dict):
            if operator_sig.get("keyId") == reviewer_sig.get("keyId"):
                errors.append("ENTRY_SIGNERS_NOT_INDEPENDENT")

    for extra_id in set(entry_map) - valid_packet_ids:
        errors.append("ATTESTATION_REFERENCES_UNKNOWN_EVIDENCE")
    for reviewer_key_id, reviewer_public_key in reviewer_keys_by_entry:
        if reviewer_key_id in operator_key_ids or reviewer_public_key in operator_public_keys:
            errors.append("ENTRY_SIGNERS_NOT_INDEPENDENT")

    final_map: dict[str, dict[str, Any]] = {}
    for record in final_records:
        if not isinstance(record, dict) or set(record) != FINAL_SIGNATURE_FIELDS:
            errors.append("FINAL_REVIEW_SIGNATURE_FIELDS_INVALID")
            continue
        role = record.get("reviewerRole")
        if not isinstance(role, str) or role not in FINAL_REVIEW_ROLES:
            errors.append("FINAL_REVIEW_SIGNATURE_ROLE_INVALID")
            continue
        if role in final_map:
            errors.append("DUPLICATE_FINAL_REVIEW_SIGNATURE")
            continue
        if not _signature_record_valid({
            "keyId": record.get("keyId"),
            "signedAtUtc": record.get("signedAtUtc"),
            "signatureBase64": record.get("signatureBase64"),
        }):
            errors.append("FINAL_REVIEW_SIGNATURE_INVALID")
        final_map[role] = record

    review = packet.get("review") if isinstance(packet.get("review"), dict) else {}
    decision = review.get("decision")
    raw_final_roles = review.get("reviewerRoles")
    if (
        decision in {"APPROVED", "REJECTED"}
        and isinstance(raw_final_roles, list)
        and all(isinstance(role, str) for role in raw_final_roles)
    ):
        expected_final_roles = set(raw_final_roles)
    else:
        expected_final_roles = set()
    if decision == "NOT_REVIEWED" and final_map:
        errors.append("FINAL_REVIEW_SIGNATURES_UNEXPECTED")
    for role in set(final_map) - expected_final_roles:
        errors.append("FINAL_REVIEW_ROLE_NOT_IN_PACKET")
    missing_final_signatures = len(expected_final_roles - set(final_map))

    final_key_ids: set[str] = set()
    final_public_keys: set[bytes] = set()
    for role in sorted(expected_final_roles & set(final_map)):
        record = final_map[role]
        signed_at = record.get("signedAtUtc", "")
        reviewed_at = review.get("reviewedAtUtc")
        try:
            payload = _final_review_payload(packet, digest, role, signed_at)
        except (KeyError, TypeError):
            errors.append("FINAL_REVIEW_PAYLOAD_INVALID")
            continue
        signature_object = {
            "keyId": record.get("keyId"),
            "signedAtUtc": signed_at,
            "signatureBase64": record.get("signatureBase64"),
        }
        resolved, error = _verify_signature(signature_object, role, payload, key_index, reviewed_at)
        if error:
            errors.append(error)
            continue
        assert resolved is not None
        if record["keyId"] in final_key_ids or resolved["raw"] in final_public_keys:
            errors.append("FINAL_REVIEWERS_NOT_INDEPENDENT")
        if record["keyId"] in operator_key_ids or resolved["raw"] in operator_public_keys:
            errors.append("FINAL_REVIEWER_WAS_EVIDENCE_OPERATOR")
        final_key_ids.add(record["keyId"])
        final_public_keys.add(resolved["raw"])
        verified_final_reviewers += 1

    if len(final_public_keys) != len(final_key_ids):
        errors.append("FINAL_REVIEWERS_NOT_INDEPENDENT")

    return {
        "errors": sorted(set(errors)),
        "missingEntrySignatures": missing_entry_signatures,
        "missingFinalSignatures": missing_final_signatures,
        "verifiedOperator": verified_operator,
        "verifiedEvidenceReviewer": verified_evidence_reviewer,
        "verifiedFinalReviewers": verified_final_reviewers,
        "trustedKeyCount": len(key_index),
        "trustAnchorVerified": trust_anchor_verified,
        "trustAnchorError": trust_anchor_error,
    }


def _empty_envelope(packet: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema": ATTESTATION_SCHEMA,
        "packetId": packet.get("packetId"),
        "packetSha256": packet_digest(packet),
        "canonicalization": CANONICALIZATION,
        "entryAttestations": [],
        "finalReviewSignatures": [],
    }


def _load_or_initialize_envelope(path: Path, packet: dict[str, Any]) -> dict[str, Any]:
    expected = packet_digest(packet)
    if not path.exists():
        return _empty_envelope(packet)
    current, errors = _validate_envelope_shape(_load_json(path))
    if errors or current is None:
        raise AttestationError(errors[0] if errors else "ATTESTATION_ENVELOPE_INVALID")
    if current["packetId"] is None and current["packetSha256"] is None and not current["entryAttestations"] and not current["finalReviewSignatures"]:
        current["packetId"] = packet.get("packetId")
        current["packetSha256"] = expected
        current["canonicalization"] = CANONICALIZATION
    elif current["packetId"] != packet.get("packetId") or current["packetSha256"] != expected:
        raise AttestationError("ATTESTATION_PACKET_BINDING_MISMATCH_RECREATE_ENVELOPE")
    return current


def _load_signing_key(
    private_key_path: Path,
    key_id: str,
    role: str,
    signed_at: str,
    keyring: object,
    password_env: str | None,
) -> tuple[Ed25519PrivateKey, dict[str, Any]]:
    index, errors = _keyring_index(keyring)
    if errors:
        raise AttestationError(errors[0])
    record, error = _resolve_key(index, key_id, role, signed_at)
    if error or record is None:
        raise AttestationError(error or "TRUSTED_KEY_NOT_FOUND")
    password_text = os.environ.get(password_env, "") if password_env else ""
    password = password_text.encode("utf-8") if password_text else None
    try:
        private = serialization.load_pem_private_key(private_key_path.read_bytes(), password=password)
    except (OSError, ValueError, TypeError) as exc:
        raise AttestationError("PRIVATE_KEY_LOAD_FAILED") from exc
    if not isinstance(private, Ed25519PrivateKey):
        raise AttestationError("PRIVATE_KEY_TYPE_INVALID")
    raw_public = private.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )
    if raw_public != record["raw"]:
        raise AttestationError("PRIVATE_KEY_DOES_NOT_MATCH_TRUSTED_KEY")
    return private, record


def sign_entry(
    packet: dict[str, Any], envelope: dict[str, Any], keyring: object,
    evidence_id: str, signer_type: str, key_id: str,
    private_key: Ed25519PrivateKey, signed_at: str,
) -> dict[str, Any]:
    if signer_type not in {"operator", "reviewer"}:
        raise AttestationError("SIGNER_TYPE_INVALID")
    entries = [item for item in packet.get("entries", []) if isinstance(item, dict) and item.get("evidenceId") == evidence_id]
    if len(entries) != 1:
        raise AttestationError("EVIDENCE_ID_NOT_UNIQUE_IN_PACKET")
    entry = entries[0]
    role_field = "operatorRole" if signer_type == "operator" else "reviewerRole"
    role = entry.get(role_field)
    if not isinstance(role, str):
        raise AttestationError("PACKET_ENTRY_ROLE_INVALID")
    index, errors = _keyring_index(keyring)
    if errors:
        raise AttestationError(errors[0])
    trusted, error = _resolve_key(index, key_id, role, signed_at)
    if error or trusted is None:
        raise AttestationError(error or "TRUSTED_KEY_NOT_FOUND")
    if private_key.public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw
    ) != trusted["raw"]:
        raise AttestationError("PRIVATE_KEY_DOES_NOT_MATCH_TRUSTED_KEY")

    digest = packet_digest(packet)
    if envelope.get("packetId") != packet.get("packetId") or envelope.get("packetSha256") != digest:
        raise AttestationError("ATTESTATION_PACKET_BINDING_MISMATCH_RECREATE_ENVELOPE")
    records = envelope["entryAttestations"]
    if any(not isinstance(item, dict) or set(item) != ENTRY_ATTESTATION_FIELDS for item in records):
        raise AttestationError("ENTRY_ATTESTATION_FIELDS_INVALID")
    record = next((item for item in records if item.get("evidenceId") == evidence_id), None)
    if record is None:
        record = {"evidenceId": evidence_id, "operator": None, "reviewer": None}
        records.append(record)
    if record.get(signer_type) is not None:
        raise AttestationError("SIGNATURE_ALREADY_EXISTS")
    other = record.get("reviewer" if signer_type == "operator" else "operator")
    if isinstance(other, dict) and other.get("keyId") == key_id:
        raise AttestationError("ENTRY_SIGNERS_NOT_INDEPENDENT")
    payload = _entry_payload(packet, digest, entry, signer_type, role, signed_at)
    signature = private_key.sign(_canonical(payload))
    record[signer_type] = {
        "keyId": key_id,
        "signedAtUtc": signed_at,
        "signatureBase64": base64.b64encode(signature).decode("ascii"),
    }
    records.sort(key=lambda item: item["evidenceId"])
    return envelope


def sign_final_review(
    packet: dict[str, Any], envelope: dict[str, Any], keyring: object,
    key_id: str, reviewer_role: str,
    private_key: Ed25519PrivateKey, signed_at: str, evidence_root: Path,
) -> dict[str, Any]:
    review = packet.get("review")
    if not isinstance(review, dict) or review.get("decision") not in {"APPROVED", "REJECTED"}:
        raise AttestationError("PACKET_REVIEW_NOT_FINAL")
    from validate_encounter_checkpoint_stage43_evidence import evaluate_packet
    stage43_result = evaluate_packet(packet, evidence_root, ROOT)
    if stage43_result.get("outcome") != "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW":
        raise AttestationError("STAGE43_PACKET_NOT_ELIGIBLE_FOR_REVIEW_SIGNATURE")
    if reviewer_role not in review.get("reviewerRoles", []):
        raise AttestationError("FINAL_REVIEW_ROLE_NOT_IN_PACKET")
    index, errors = _keyring_index(keyring)
    if errors:
        raise AttestationError(errors[0])
    trusted, error = _resolve_key(index, key_id, reviewer_role, signed_at)
    if error or trusted is None:
        raise AttestationError(error or "TRUSTED_KEY_NOT_FOUND")
    if private_key.public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw
    ) != trusted["raw"]:
        raise AttestationError("PRIVATE_KEY_DOES_NOT_MATCH_TRUSTED_KEY")
    digest = packet_digest(packet)
    if envelope.get("packetId") != packet.get("packetId") or envelope.get("packetSha256") != digest:
        raise AttestationError("ATTESTATION_PACKET_BINDING_MISMATCH_RECREATE_ENVELOPE")

    for record in envelope["entryAttestations"]:
        operator = record.get("operator")
        if isinstance(operator, dict) and operator.get("keyId") == key_id:
            raise AttestationError("FINAL_REVIEWER_WAS_EVIDENCE_OPERATOR")
    if any(not isinstance(item, dict) or set(item) != FINAL_SIGNATURE_FIELDS for item in envelope["finalReviewSignatures"]):
        raise AttestationError("FINAL_REVIEW_SIGNATURE_FIELDS_INVALID")
    if any(item.get("reviewerRole") == reviewer_role for item in envelope["finalReviewSignatures"]):
        raise AttestationError("FINAL_REVIEW_SIGNATURE_ALREADY_EXISTS")
    if any(item.get("keyId") == key_id for item in envelope["finalReviewSignatures"]):
        raise AttestationError("FINAL_REVIEWERS_NOT_INDEPENDENT")

    payload = _final_review_payload(packet, digest, reviewer_role, signed_at)
    signature = private_key.sign(_canonical(payload))
    envelope["finalReviewSignatures"].append({
        "reviewerRole": reviewer_role,
        "keyId": key_id,
        "signedAtUtc": signed_at,
        "signatureBase64": base64.b64encode(signature).decode("ascii"),
    })
    envelope["finalReviewSignatures"].sort(key=lambda item: item["reviewerRole"])
    return envelope


def evaluate(
    packet: object, evidence_root: Path, envelope: object, keyring: object,
    trusted_keyring_sha256: str | None, keyring_raw: bytes | None,
    project_root: Path | None = None,
) -> dict[str, Any]:
    base = stage43.evaluate_packet(packet, evidence_root, project_root)
    attestation = verify_attestations(packet, envelope, keyring, trusted_keyring_sha256, keyring_raw)
    errors = attestation["errors"]
    if base["outcome"] == "INVALID_PACKET":
        outcome = "INVALID_PACKET"
    elif errors:
        outcome = "INVALID_ATTESTATION"
    elif base["outcome"] == "BLOCKED_MISSING_EVIDENCE":
        outcome = "BLOCKED_MISSING_EVIDENCE"
    elif base["outcome"] == "REJECTED_BY_REVIEW":
        outcome = "REJECTED_BY_REVIEW"
    elif base["outcome"] == "WAITING_FOR_INDEPENDENT_REVIEW":
        outcome = "WAITING_FOR_INDEPENDENT_REVIEW"
    elif attestation["trustAnchorError"] == "TRUST_ANCHOR_HASH_REQUIRED":
        outcome = "BLOCKED_TRUST_ANCHOR_UNPINNED"
    elif attestation["trustAnchorError"] is not None:
        outcome = "INVALID_ATTESTATION"
    elif attestation["trustedKeyCount"] == 0:
        outcome = "BLOCKED_MISSING_TRUSTED_KEYS"
    elif attestation["missingEntrySignatures"] or attestation["missingFinalSignatures"]:
        outcome = "BLOCKED_MISSING_ATTESTATIONS"
    elif not attestation["trustAnchorVerified"]:
        outcome = "BLOCKED_TRUST_ANCHOR_UNPINNED"
    else:
        outcome = "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW"

    verified = outcome == "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW"
    return {
        "schema": REPORT_SCHEMA,
        "stage": 44,
        "packetId": base.get("packetId"),
        "evaluatedAtUtc": utc_now(),
        "outcome": outcome,
        "stage43Outcome": base.get("outcome"),
        "attestationStatus": "VERIFIED" if verified else ("INVALID" if errors or outcome == "INVALID_ATTESTATION" else "BLOCKED"),
        "verifiedOperatorSignatures": attestation["verifiedOperator"],
        "verifiedEvidenceReviewerSignatures": attestation["verifiedEvidenceReviewer"],
        "verifiedFinalReviewSignatures": attestation["verifiedFinalReviewers"],
        "missingEntrySignatures": attestation["missingEntrySignatures"],
        "missingFinalReviewSignatures": attestation["missingFinalSignatures"],
        "trustedKeyCount": attestation["trustedKeyCount"],
        "trustAnchorVerified": attestation["trustAnchorVerified"],
        "errors": errors,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "promotionAllowed": False,
        "nextAction": (
            "A verified attestation packet is eligible only for a separate release-board decision; no deployment or readiness flag is changed."
            if verified else
            "Do not promote. Collect real Stage 43 evidence, register trusted public keys out of band, and complete all required signatures."
        ),
    }


def _load_private_key(path: Path, password_env: str | None) -> Ed25519PrivateKey:
    password_text = os.environ.get(password_env, "") if password_env else ""
    password = password_text.encode("utf-8") if password_text else None
    try:
        private = serialization.load_pem_private_key(path.read_bytes(), password=password)
    except (OSError, ValueError, TypeError) as exc:
        raise AttestationError("PRIVATE_KEY_LOAD_FAILED") from exc
    if not isinstance(private, Ed25519PrivateKey):
        raise AttestationError("PRIVATE_KEY_TYPE_INVALID")
    return private


def _load_packet(path: Path) -> dict[str, Any]:
    value = _load_json(path)
    if not isinstance(value, dict):
        raise AttestationError("PACKET_ROOT_MUST_BE_OBJECT")
    return value


def _load_keyring(path: Path) -> tuple[object, bytes]:
    raw = path.read_bytes()
    try:
        value = json.loads(raw.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise AttestationError("KEYRING_JSON_INVALID") from exc
    return value, raw


def cmd_keygen(args: argparse.Namespace) -> int:
    if args.private_key_out.resolve() == args.public_key_out.resolve():
        raise AttestationError("OUTPUT_PATHS_MUST_DIFFER")
    if args.private_key_out.exists() or args.public_key_out.exists():
        raise AttestationError("OUTPUT_FILE_ALREADY_EXISTS")
    roles = sorted(set(args.role))
    if not roles or any(role not in ALL_ROLES for role in roles):
        raise AttestationError("KEY_ROLES_INVALID")
    password = getpass("Private-key passphrase (minimum 12 characters): ")
    confirm = getpass("Repeat passphrase: ")
    if len(password) < 12 or password != confirm:
        raise AttestationError("PASSPHRASE_TOO_SHORT_OR_MISMATCH")
    private = Ed25519PrivateKey.generate()
    private_bytes = private.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8,
        serialization.BestAvailableEncryption(password.encode("utf-8")),
    )
    public_bytes = private.public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw
    )
    record = {
        "keyId": args.key_id,
        "publicKeyBase64": base64.b64encode(public_bytes).decode("ascii"),
        "roles": roles,
        "status": "ACTIVE",
        "validFromUtc": args.valid_from_utc or utc_now(),
        "validUntilUtc": args.valid_until_utc,
    }
    probe, errors = _keyring_index({"schema": KEYRING_SCHEMA, "keys": [record]})
    if errors:
        raise AttestationError(errors[0])
    args.private_key_out.parent.mkdir(parents=True, exist_ok=True)
    args.public_key_out.parent.mkdir(parents=True, exist_ok=True)
    with args.private_key_out.open("xb") as handle:
        handle.write(private_bytes)
    try:
        os.chmod(args.private_key_out, 0o600)
    except OSError:
        pass
    _write_json(args.public_key_out, record)
    print(json.dumps({"keyId": args.key_id, "roles": roles, "publicKeyFile": str(args.public_key_out)}, indent=2))
    return 0


def _add_signing_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--packet", type=Path, required=True)
    parser.add_argument("--attestation", type=Path, required=True)
    parser.add_argument("--keyring", type=Path, required=True)
    parser.add_argument("--key-id", required=True)
    parser.add_argument("--private-key", type=Path, required=True)
    parser.add_argument("--password-env", default="JUMANJI_EVIDENCE_PRIVATE_KEY_PASSWORD")
    parser.add_argument("--signed-at-utc", default=utc_now())


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    keygen = sub.add_parser("keygen", help="Generate an encrypted Ed25519 key outside the repository.")
    keygen.add_argument("--key-id", required=True)
    keygen.add_argument("--role", action="append", required=True, choices=sorted(ALL_ROLES))
    keygen.add_argument("--private-key-out", type=Path, required=True)
    keygen.add_argument("--public-key-out", type=Path, required=True)
    keygen.add_argument("--valid-from-utc")
    keygen.add_argument("--valid-until-utc")
    keygen.set_defaults(func=cmd_keygen)

    entry = sub.add_parser("sign-entry", help="Sign one evidence entry as its assigned operator or reviewer.")
    _add_signing_options(entry)
    entry.add_argument("--evidence-id", required=True)
    entry.add_argument("--signer-type", required=True, choices=("operator", "reviewer"))
    entry.set_defaults(func=None)

    final = sub.add_parser("sign-final-review", help="Sign the final packet review with an independent reviewer key.")
    _add_signing_options(final)
    final.add_argument("--reviewer-role", required=True, choices=sorted(FINAL_REVIEW_ROLES))
    final.add_argument("--evidence-root", type=Path, required=True)
    final.set_defaults(func=None)

    verify = sub.add_parser("verify", help="Verify the Stage 43 packet and all Stage 44 signatures.")
    verify.add_argument("--packet", type=Path, required=True)
    verify.add_argument("--attestation", type=Path, required=True)
    verify.add_argument("--keyring", type=Path, required=True)
    verify.add_argument("--evidence-root", type=Path, required=True)
    verify.add_argument("--trusted-keyring-sha256")
    verify.add_argument("--project-root", type=Path, default=ROOT)
    verify.add_argument("--report", type=Path)
    verify.add_argument("--require-eligible", action="store_true")
    verify.set_defaults(func=None)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        if args.command == "keygen":
            return cmd_keygen(args)
        if args.command in {"sign-entry", "sign-final-review"}:
            packet = _load_packet(args.packet)
            keyring, _ = _load_keyring(args.keyring)
            signed_at = args.signed_at_utc
            if _parse_utc(signed_at) is None:
                raise AttestationError("SIGNATURE_TIMESTAMP_INVALID")
            private = _load_private_key(args.private_key, args.password_env)
            index, errors = _keyring_index(keyring)
            if errors:
                raise AttestationError(errors[0])
            role = args.reviewer_role if args.command == "sign-final-review" else next(
                (item.get("operatorRole" if args.signer_type == "operator" else "reviewerRole")
                 for item in packet.get("entries", []) if isinstance(item, dict) and item.get("evidenceId") == args.evidence_id),
                None,
            )
            if not isinstance(role, str):
                raise AttestationError("PACKET_ENTRY_ROLE_INVALID")
            _, key_error = _resolve_key(index, args.key_id, role, signed_at)
            if key_error:
                raise AttestationError(key_error)
            envelope = _load_or_initialize_envelope(args.attestation, packet)
            if args.command == "sign-entry":
                sign_entry(packet, envelope, keyring, args.evidence_id, args.signer_type, args.key_id, private, signed_at)
            else:
                sign_final_review(packet, envelope, keyring, args.key_id, args.reviewer_role, private, signed_at, args.evidence_root)
            _write_json(args.attestation, envelope)
            print("Signature added. The private key was not written to the attestation file.")
            return 0

        packet = _load_packet(args.packet)
        envelope = _load_json(args.attestation)
        keyring, raw = _load_keyring(args.keyring)
        report = evaluate(
            packet, args.evidence_root, envelope, keyring,
            args.trusted_keyring_sha256, raw, args.project_root,
        )
        encoded = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
        if args.report:
            _write_json(args.report, report)
        print(encoded, end="")
        if report["outcome"] in {"INVALID_PACKET", "INVALID_ATTESTATION"}:
            return 1
        if args.require_eligible and report["outcome"] != "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW":
            return 2
        return 0
    except (AttestationError, OSError, ValueError, TypeError) as exc:
        code = str(exc) if str(exc) else "ATTESTATION_OPERATION_FAILED"
        print(f"Stage 44 evidence attestation: FAIL ({code})", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
