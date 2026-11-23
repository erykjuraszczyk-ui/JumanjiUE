#!/usr/bin/env python3
"""Packet-bound, fail-closed Release Board authorization for JUMANJI Stage 45.

This gate authorizes a specific release candidate for a later controlled
deployment. It never deploys, marks production ready, approves HA, or promotes
SLO baselines.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import sys
import uuid
from datetime import datetime, timedelta, timezone
from pathlib import Path, PurePosixPath
from typing import Any

import rfc8785
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import encounter_checkpoint_evidence_attestation as stage44  # noqa: E402

CANDIDATE_SCHEMA = "JUMANJI.ProductionReleaseCandidate.v1"
DECISION_SCHEMA = "JUMANJI.ProductionReleaseBoardDecision.v1"
REPORT_SCHEMA = "JUMANJI.ProductionReleaseBoardReport.v1"
CANONICALIZATION = "RFC8785"
SHA256 = re.compile(r"^[a-f0-9]{64}$")
COMMIT_SHA = re.compile(r"^[a-f0-9]{40}$")
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{2,95}$")
BOARD_ROLES = ("SERVICE_OWNER", "SECURITY_REVIEWER", "PLATFORM_REVIEWER")
CANDIDATE_FIELDS = {
    "schema", "candidateId", "buildId", "packetId", "packetSha256",
    "environmentId", "sourceCommitSha", "engineVersion", "createdAtUtc", "targets",
}
TARGET_FIELDS = {"target", "path", "sha256", "sizeBytes", "mediaType"}
DECISION_FIELDS = {
    "schema", "stage", "decisionId", "candidateId", "candidateSha256",
    "packetId", "packetSha256", "attestationSha256", "keyringSha256",
    "targetEnvironment", "decision", "createdAtUtc", "boardSignatures",
}
BOARD_SIGNATURE_FIELDS = {"boardRole", "keyId", "signedAtUtc", "signatureBase64"}
MAX_ARTIFACT_BYTES = 8 * 1024 * 1024 * 1024


class ReleaseBoardError(ValueError):
    """A fail-closed Stage 45 input or operation error."""


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


def _json_no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ReleaseBoardError("JSON_DUPLICATE_KEY")
        value[key] = item
    return value


def _load_json_bytes(raw: bytes, label: str) -> object:
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=_json_no_duplicates)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ReleaseBoardError(f"{label}_JSON_INVALID") from exc


def _load_path(path: Path, label: str) -> tuple[object, bytes]:
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise ReleaseBoardError(f"{label}_READ_FAILED") from exc
    return _load_json_bytes(raw, label), raw


def _sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            block = handle.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def _write_json(path: Path, value: object, *, create_only: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink():
        raise ReleaseBoardError("OUTPUT_SYMLINK_REJECTED")
    encoded = (json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")
    if create_only:
        try:
            with path.open("xb") as handle:
                handle.write(encoded)
            try:
                os.chmod(path, 0o600)
            except OSError:
                pass
        except FileExistsError as exc:
            raise ReleaseBoardError("OUTPUT_ALREADY_EXISTS") from exc
        return
    temporary = path.with_name(path.name + ".stage45.tmp")
    if temporary.exists() or temporary.is_symlink():
        raise ReleaseBoardError("OUTPUT_TEMPORARY_PATH_EXISTS")
    try:
        with temporary.open("xb") as handle:
            handle.write(encoded)
        os.replace(temporary, path)
    except OSError as exc:
        try:
            temporary.unlink(missing_ok=True)
        except OSError:
            pass
        raise ReleaseBoardError("OUTPUT_WRITE_FAILED") from exc


def _safe_artifact_path(root: Path, relative: object) -> Path:
    if not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative:
        raise ReleaseBoardError("CANDIDATE_ARTIFACT_PATH_INVALID")
    posix = PurePosixPath(relative)
    if posix.is_absolute() or any(part in {"", ".", ".."} for part in posix.parts):
        raise ReleaseBoardError("CANDIDATE_ARTIFACT_PATH_TRAVERSAL")
    if not re.fullmatch(r"[A-Za-z0-9._/-]{1,240}", relative):
        raise ReleaseBoardError("CANDIDATE_ARTIFACT_PATH_INVALID")
    root_path = root.resolve(strict=True)
    if root.is_symlink() or not root_path.is_dir():
        raise ReleaseBoardError("CANDIDATE_ARTIFACT_ROOT_INVALID")
    current = root_path
    for part in posix.parts:
        current = current / part
        if current.is_symlink():
            raise ReleaseBoardError("CANDIDATE_ARTIFACT_SYMLINK_REJECTED")
    try:
        resolved = current.resolve(strict=True)
        resolved.relative_to(root_path)
    except (OSError, ValueError) as exc:
        raise ReleaseBoardError("CANDIDATE_ARTIFACT_OUTSIDE_ROOT_OR_MISSING") from exc
    if not resolved.is_file():
        raise ReleaseBoardError("CANDIDATE_ARTIFACT_NOT_FILE")
    return resolved


def _summary_for_kind(packet: dict[str, Any], evidence_root: Path, kind: str) -> dict[str, Any]:
    matches = [item for item in packet.get("entries", []) if isinstance(item, dict) and item.get("kind") == kind]
    if len(matches) != 1:
        raise ReleaseBoardError(f"REQUIRED_STAGE43_EVIDENCE_MISSING_OR_DUPLICATE:{kind}")
    relative = matches[0].get("artifact", {}).get("path")
    path = _safe_artifact_path(evidence_root, relative)
    try:
        report = _load_json_bytes(path.read_bytes(), "STAGE43_SUMMARY")
    except OSError as exc:
        raise ReleaseBoardError("STAGE43_SUMMARY_READ_FAILED") from exc
    if not isinstance(report, dict) or not isinstance(report.get("claims"), dict):
        raise ReleaseBoardError("STAGE43_SUMMARY_SHAPE_INVALID")
    return report


def validate_candidate(
    packet: object,
    candidate: object,
    candidate_raw: bytes,
    evidence_root: Path,
    artifact_root: Path,
) -> list[str]:
    errors: list[str] = []
    if not isinstance(packet, dict):
        return ["PACKET_ROOT_INVALID"]
    if not isinstance(candidate, dict) or set(candidate) != CANDIDATE_FIELDS:
        return ["CANDIDATE_FIELDS_INVALID"]
    if candidate.get("schema") != CANDIDATE_SCHEMA:
        errors.append("CANDIDATE_SCHEMA_INVALID")
    for field in ("candidateId", "buildId", "environmentId"):
        if not isinstance(candidate.get(field), str) or not SAFE_ID.fullmatch(candidate[field]):
            errors.append(f"CANDIDATE_{field.upper()}_INVALID")
    if candidate.get("packetId") != packet.get("packetId"):
        errors.append("CANDIDATE_PACKET_ID_MISMATCH")
    try:
        packet_sha = stage44.packet_digest(packet)
    except Exception:
        packet_sha = ""
    if candidate.get("packetSha256") != packet_sha:
        errors.append("CANDIDATE_PACKET_DIGEST_MISMATCH")
    if candidate.get("environmentId") != packet.get("environment", {}).get("id"):
        errors.append("CANDIDATE_ENVIRONMENT_MISMATCH")
    if not isinstance(candidate.get("sourceCommitSha"), str) or not COMMIT_SHA.fullmatch(candidate["sourceCommitSha"]):
        errors.append("CANDIDATE_SOURCE_COMMIT_INVALID")
    if candidate.get("engineVersion") != "5.8":
        errors.append("CANDIDATE_ENGINE_VERSION_MISMATCH")
    created_at = _parse_utc(candidate.get("createdAtUtc"))
    if created_at is None or created_at > datetime.now(timezone.utc) + timedelta(minutes=5):
        errors.append("CANDIDATE_TIMESTAMP_INVALID")
    targets = candidate.get("targets")
    if not isinstance(targets, list) or not 2 <= len(targets) <= 8:
        errors.append("CANDIDATE_TARGET_COUNT_INVALID")
        targets = []
    seen: set[str] = set()
    verified: dict[str, dict[str, Any]] = {}
    for target in targets:
        if not isinstance(target, dict) or set(target) != TARGET_FIELDS:
            errors.append("CANDIDATE_TARGET_FIELDS_INVALID")
            continue
        name = target.get("target")
        if not isinstance(name, str) or name not in {"ue58-dedicated-server", "android-package", "windows-client"}:
            errors.append("CANDIDATE_TARGET_UNKNOWN")
            continue
        if name in seen:
            errors.append("CANDIDATE_TARGET_DUPLICATE")
            continue
        seen.add(name)
        if not isinstance(target.get("sha256"), str) or not SHA256.fullmatch(target["sha256"]):
            errors.append(f"CANDIDATE_TARGET_SHA256_INVALID:{name}")
            continue
        size = target.get("sizeBytes")
        if type(size) is not int or size < 1 or size > MAX_ARTIFACT_BYTES:
            errors.append(f"CANDIDATE_TARGET_SIZE_INVALID:{name}")
            continue
        try:
            artifact = _safe_artifact_path(artifact_root, target.get("path"))
            actual_size = artifact.stat().st_size
            if actual_size != size:
                errors.append(f"CANDIDATE_ARTIFACT_SIZE_MISMATCH:{name}")
                continue
            if _sha256_file(artifact) != target["sha256"]:
                errors.append(f"CANDIDATE_ARTIFACT_HASH_MISMATCH:{name}")
                continue
        except (OSError, ReleaseBoardError) as exc:
            errors.append(str(exc) if isinstance(exc, ReleaseBoardError) else f"CANDIDATE_ARTIFACT_READ_FAILED:{name}")
            continue
        allowed_media = {
            "ue58-dedicated-server": {"application/octet-stream", "application/zip", "application/x-tar"},
            "android-package": {"application/vnd.android.package-archive", "application/zip"},
            "windows-client": {"application/octet-stream", "application/zip"},
        }
        media_type = target.get("mediaType")
        if not isinstance(media_type, str) or media_type not in allowed_media[name]:
            errors.append(f"CANDIDATE_MEDIA_TYPE_INVALID:{name}")
            continue
        verified[name] = target
    if not {"ue58-dedicated-server", "android-package"}.issubset(seen):
        errors.append("CANDIDATE_REQUIRED_TARGETS_MISSING")

    if "android-package" in verified:
        try:
            summary = _summary_for_kind(packet, evidence_root, "android_device_package")
            claims = summary["claims"]
            expected_media = (
                "application/vnd.android.package-archive"
                if claims.get("packageFormat") == "APK" else
                "application/zip" if claims.get("packageFormat") == "AAB" else None
            )
            android = verified["android-package"]
            if claims.get("packageSha256") != android.get("sha256"):
                errors.append("CANDIDATE_ANDROID_PACKAGE_NOT_BOUND_TO_STAGE43_EVIDENCE")
            if claims.get("packageBuildId") != candidate.get("buildId"):
                errors.append("CANDIDATE_ANDROID_BUILD_ID_MISMATCH")
            if android.get("mediaType") != expected_media:
                errors.append("CANDIDATE_ANDROID_MEDIA_TYPE_MISMATCH")
        except ReleaseBoardError as exc:
            errors.append(str(exc))

    if "ue58-dedicated-server" in verified:
        try:
            summary = _summary_for_kind(packet, evidence_root, "ue58_dedicated_server_recovery")
            if summary["claims"].get("engineVersion") != "5.8":
                errors.append("CANDIDATE_SERVER_EVIDENCE_ENGINE_MISMATCH")
            if summary["claims"].get("serverBuildSucceeded") is not True:
                errors.append("CANDIDATE_SERVER_BUILD_EVIDENCE_MISSING")
        except ReleaseBoardError as exc:
            errors.append(str(exc))
    return errors


def _upstream_report(
    packet: object, evidence_root: Path, attestation: object,
    keyring: object, trusted_keyring_sha256: str | None,
    keyring_raw: bytes, project_root: Path,
) -> dict[str, Any]:
    return stage44.evaluate(
        packet, evidence_root, attestation, keyring,
        trusted_keyring_sha256, keyring_raw, project_root,
    )


def _upstream_signer_ids(envelope: object) -> set[str]:
    result: set[str] = set()
    if not isinstance(envelope, dict):
        return result
    for item in envelope.get("entryAttestations", []):
        if not isinstance(item, dict):
            continue
        for signer_type in ("operator", "reviewer"):
            signature = item.get(signer_type)
            if isinstance(signature, dict) and isinstance(signature.get("keyId"), str):
                result.add(signature["keyId"])
    for signature in envelope.get("finalReviewSignatures", []):
        if isinstance(signature, dict) and isinstance(signature.get("keyId"), str):
            result.add(signature["keyId"])
    return result


def _board_payload(decision: dict[str, Any], role: str, signed_at: str) -> dict[str, Any]:
    return {
        "schema": "JUMANJI.ProductionReleaseBoardDecisionPayload.v1",
        "canonicalization": CANONICALIZATION,
        "decisionId": decision["decisionId"],
        "candidateId": decision["candidateId"],
        "candidateSha256": decision["candidateSha256"],
        "packetId": decision["packetId"],
        "packetSha256": decision["packetSha256"],
        "attestationSha256": decision["attestationSha256"],
        "keyringSha256": decision["keyringSha256"],
        "targetEnvironment": decision["targetEnvironment"],
        "decision": decision["decision"],
        "createdAtUtc": decision["createdAtUtc"],
        "boardRole": role,
        "signedAtUtc": signed_at,
    }


def prepare_decision(
    packet: dict[str, Any],
    candidate: dict[str, Any],
    candidate_raw: bytes,
    attestation: dict[str, Any],
    attestation_raw: bytes,
    keyring: object,
    keyring_raw: bytes,
    trusted_keyring_sha256: str | None,
    evidence_root: Path,
    artifact_root: Path,
    decision_value: str = "APPROVE",
    created_at_utc: str | None = None,
    project_root: Path | None = None,
) -> dict[str, Any]:
    upstream = _upstream_report(
        packet, evidence_root, attestation, keyring,
        trusted_keyring_sha256, keyring_raw, project_root or ROOT,
    )
    if upstream.get("outcome") != "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW":
        raise ReleaseBoardError("UPSTREAM_ATTESTATION_NOT_ELIGIBLE")
    errors = validate_candidate(packet, candidate, candidate_raw, evidence_root, artifact_root)
    if errors:
        raise ReleaseBoardError(errors[0])
    if decision_value not in {"APPROVE", "REJECT"}:
        raise ReleaseBoardError("RELEASE_BOARD_DECISION_INVALID")
    stamp = created_at_utc or utc_now()
    parsed = _parse_utc(stamp)
    if parsed is None or parsed > datetime.now(timezone.utc) + timedelta(minutes=5):
        raise ReleaseBoardError("RELEASE_BOARD_TIMESTAMP_INVALID")
    return {
        "schema": DECISION_SCHEMA,
        "stage": 45,
        "decisionId": str(uuid.uuid4()),
        "candidateId": candidate["candidateId"],
        "candidateSha256": _sha256(candidate_raw),
        "packetId": packet["packetId"],
        "packetSha256": stage44.packet_digest(packet),
        "attestationSha256": _sha256(attestation_raw),
        "keyringSha256": _sha256(keyring_raw),
        "targetEnvironment": "production",
        "decision": decision_value,
        "createdAtUtc": stamp,
        "boardSignatures": [],
    }


def sign_decision(
    decision: dict[str, Any],
    packet: dict[str, Any],
    candidate: dict[str, Any],
    candidate_raw: bytes,
    attestation: dict[str, Any],
    attestation_raw: bytes,
    envelope: dict[str, Any],
    keyring: object,
    keyring_raw: bytes,
    trusted_keyring_sha256: str | None,
    evidence_root: Path,
    artifact_root: Path,
    key_id: str,
    board_role: str,
    private_key: Ed25519PrivateKey,
    signed_at_utc: str | None = None,
    project_root: Path | None = None,
) -> dict[str, Any]:
    current = evaluate(
        packet, evidence_root, attestation, attestation_raw, envelope,
        keyring, keyring_raw, trusted_keyring_sha256, candidate,
        candidate_raw, artifact_root, decision, project_root or ROOT,
    )
    if current["outcome"] not in {"WAITING_FOR_RELEASE_BOARD_SIGNATURES"}:
        raise ReleaseBoardError("RELEASE_BOARD_DECISION_NOT_SIGNABLE")
    if board_role not in BOARD_ROLES:
        raise ReleaseBoardError("RELEASE_BOARD_ROLE_INVALID")
    index, key_errors = stage44._keyring_index(keyring)
    if key_errors:
        raise ReleaseBoardError(key_errors[0])
    signed_at = signed_at_utc or utc_now()
    timestamp = _parse_utc(signed_at)
    if timestamp is None or timestamp > datetime.now(timezone.utc) + timedelta(minutes=5):
        raise ReleaseBoardError("RELEASE_BOARD_SIGNATURE_TIMESTAMP_INVALID")
    packet_reviewed = _parse_utc(packet.get("review", {}).get("reviewedAtUtc"))
    candidate_created = _parse_utc(candidate.get("createdAtUtc"))
    decision_created = _parse_utc(decision.get("createdAtUtc"))
    if any(item is None for item in (packet_reviewed, candidate_created, decision_created)):
        raise ReleaseBoardError("RELEASE_BOARD_SIGNATURE_TIME_BOUNDARY_INVALID")
    if timestamp < max(packet_reviewed, candidate_created, decision_created):
        raise ReleaseBoardError("RELEASE_BOARD_SIGNATURE_PREDATES_INPUTS")
    trusted, error = stage44._resolve_key(index, key_id, board_role, signed_at)
    if error or trusted is None:
        raise ReleaseBoardError(error or "TRUSTED_KEY_NOT_FOUND")
    if key_id in _upstream_signer_ids(envelope):
        raise ReleaseBoardError("BOARD_KEY_REUSED_UPSTREAM")
    if any(item.get("boardRole") == board_role for item in decision.get("boardSignatures", [])):
        raise ReleaseBoardError("RELEASE_BOARD_ROLE_ALREADY_SIGNED")
    if any(item.get("keyId") == key_id for item in decision.get("boardSignatures", [])):
        raise ReleaseBoardError("RELEASE_BOARD_KEY_ALREADY_USED")
    public_bytes = private_key.public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw
    )
    if public_bytes != trusted["raw"]:
        raise ReleaseBoardError("PRIVATE_KEY_DOES_NOT_MATCH_TRUSTED_BOARD_KEY")
    signature = private_key.sign(rfc8785.dumps(_board_payload(decision, board_role, signed_at)))
    updated = dict(decision)
    updated["boardSignatures"] = list(decision.get("boardSignatures", [])) + [{
        "boardRole": board_role,
        "keyId": key_id,
        "signedAtUtc": signed_at,
        "signatureBase64": base64.b64encode(signature).decode("ascii"),
    }]
    updated["boardSignatures"].sort(key=lambda item: item["boardRole"])
    return updated


def _validate_decision_shape(decision: object) -> list[str]:
    if not isinstance(decision, dict) or set(decision) != DECISION_FIELDS:
        return ["RELEASE_BOARD_DECISION_FIELDS_INVALID"]
    errors: list[str] = []
    if decision.get("schema") != DECISION_SCHEMA or decision.get("stage") != 45:
        errors.append("RELEASE_BOARD_DECISION_SCHEMA_INVALID")
    try:
        uuid.UUID(str(decision.get("decisionId")))
    except (ValueError, TypeError, AttributeError):
        errors.append("RELEASE_BOARD_DECISION_ID_INVALID")
    for field in ("candidateId", "packetId"):
        if not isinstance(decision.get(field), str):
            errors.append(f"RELEASE_BOARD_{field.upper()}_INVALID")
    for field in ("candidateSha256", "packetSha256", "attestationSha256", "keyringSha256"):
        if not isinstance(decision.get(field), str) or not SHA256.fullmatch(decision[field]):
            errors.append(f"RELEASE_BOARD_{field.upper()}_INVALID")
    if decision.get("targetEnvironment") != "production":
        errors.append("RELEASE_BOARD_TARGET_ENVIRONMENT_INVALID")
    if not isinstance(decision.get("decision"), str) or decision.get("decision") not in {"APPROVE", "REJECT"}:
        errors.append("RELEASE_BOARD_DECISION_VALUE_INVALID")
    created = _parse_utc(decision.get("createdAtUtc"))
    if created is None or created > datetime.now(timezone.utc) + timedelta(minutes=5):
        errors.append("RELEASE_BOARD_CREATED_AT_INVALID")
    signatures = decision.get("boardSignatures")
    if not isinstance(signatures, list) or len(signatures) > len(BOARD_ROLES):
        errors.append("RELEASE_BOARD_SIGNATURES_INVALID")
    return errors


def evaluate(
    packet: object,
    evidence_root: Path,
    attestation: object,
    attestation_raw: bytes,
    envelope: object,
    keyring: object,
    keyring_raw: bytes,
    trusted_keyring_sha256: str | None,
    candidate: object,
    candidate_raw: bytes,
    artifact_root: Path,
    decision: object,
    project_root: Path | None = None,
) -> dict[str, Any]:
    upstream = _upstream_report(
        packet, evidence_root, attestation, keyring,
        trusted_keyring_sha256, keyring_raw, project_root or ROOT,
    )
    base = {
        "schema": REPORT_SCHEMA,
        "stage": 45,
        "packetId": packet.get("packetId") if isinstance(packet, dict) else None,
        "candidateId": candidate.get("candidateId") if isinstance(candidate, dict) else None,
        "evaluatedAtUtc": utc_now(),
        "upstreamOutcome": upstream.get("outcome"),
        "outcome": "BLOCKED_UPSTREAM_NOT_ELIGIBLE",
        "deploymentAuthorizationGranted": False,
        "deploymentExecuted": False,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "promotionAllowed": False,
        "verifiedBoardRoles": [],
        "missingBoardRoles": list(BOARD_ROLES),
        "errors": [],
        "nextAction": "Do not deploy. Complete real Stage 43 evidence and Stage 44 signatures before Release Board review.",
    }
    if upstream.get("outcome") != "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW":
        return base

    candidate_errors = validate_candidate(packet, candidate, candidate_raw, evidence_root, artifact_root)
    if candidate_errors:
        base["outcome"] = "INVALID_RELEASE_CANDIDATE"
        base["errors"] = candidate_errors
        base["nextAction"] = "Rebuild the release candidate manifest and verify every artifact against the Stage 43 packet."
        return base

    decision_errors = _validate_decision_shape(decision)
    if decision_errors:
        base["outcome"] = "INVALID_RELEASE_BOARD_DECISION"
        base["errors"] = decision_errors
        return base
    assert isinstance(packet, dict) and isinstance(candidate, dict) and isinstance(decision, dict)
    expected = {
        "candidateId": candidate["candidateId"],
        "candidateSha256": _sha256(candidate_raw),
        "packetId": packet["packetId"],
        "packetSha256": stage44.packet_digest(packet),
        "attestationSha256": _sha256(attestation_raw),
        "keyringSha256": _sha256(keyring_raw),
    }
    for field, value in expected.items():
        if decision.get(field) != value:
            base["errors"].append(f"RELEASE_BOARD_BINDING_MISMATCH:{field}")
    if base["errors"]:
        base["outcome"] = "INVALID_RELEASE_BOARD_DECISION"
        return base

    index, key_errors = stage44._keyring_index(keyring)
    if key_errors:
        base["outcome"] = "INVALID_RELEASE_BOARD_DECISION"
        base["errors"] = key_errors
        return base
    used_upstream = _upstream_signer_ids(envelope)
    verified_roles: list[str] = []
    used_board_keys: set[str] = set()
    signature_errors: list[str] = []
    review_time = _parse_utc(packet.get("review", {}).get("reviewedAtUtc"))
    candidate_time = _parse_utc(candidate.get("createdAtUtc"))
    decision_time = _parse_utc(decision.get("createdAtUtc"))
    for record in decision["boardSignatures"]:
        if not isinstance(record, dict) or set(record) != BOARD_SIGNATURE_FIELDS:
            signature_errors.append("RELEASE_BOARD_SIGNATURE_RECORD_INVALID")
            continue
        role, signer_id, signed_text = record.get("boardRole"), record.get("keyId"), record.get("signedAtUtc")
        if role not in BOARD_ROLES:
            signature_errors.append("RELEASE_BOARD_ROLE_INVALID")
            continue
        if role in verified_roles:
            signature_errors.append("RELEASE_BOARD_ROLE_DUPLICATE")
            continue
        if not isinstance(signer_id, str):
            signature_errors.append("RELEASE_BOARD_SIGNER_KEY_INVALID")
            continue
        if signer_id in used_board_keys:
            signature_errors.append("RELEASE_BOARD_SIGNER_KEY_DUPLICATE")
            continue
        if signer_id in used_upstream:
            signature_errors.append("BOARD_KEY_REUSED_UPSTREAM")
            continue
        timestamp = _parse_utc(signed_text)
        if timestamp is None or timestamp > datetime.now(timezone.utc) + timedelta(minutes=5):
            signature_errors.append("RELEASE_BOARD_SIGNATURE_TIMESTAMP_INVALID")
            continue
        if any(item is None for item in (review_time, candidate_time, decision_time)) or timestamp < max(review_time, candidate_time, decision_time):
            signature_errors.append("RELEASE_BOARD_SIGNATURE_PREDATES_INPUTS")
            continue
        trusted, error = stage44._resolve_key(index, signer_id, role, signed_text)
        if error or trusted is None:
            signature_errors.append(error or "TRUSTED_KEY_NOT_FOUND")
            continue
        encoded = record.get("signatureBase64")
        try:
            signature = base64.b64decode(encoded, validate=True)
            if len(signature) != 64:
                raise ValueError("invalid signature length")
        except (ValueError, TypeError):
            signature_errors.append("RELEASE_BOARD_SIGNATURE_ENCODING_INVALID")
            continue
        try:
            trusted["public"].verify(
                signature,
                rfc8785.dumps(_board_payload(decision, role, signed_text)),
            )
        except InvalidSignature:
            signature_errors.append("RELEASE_BOARD_SIGNATURE_VERIFICATION_FAILED")
            continue
        used_board_keys.add(signer_id)
        verified_roles.append(role)

    base["verifiedBoardRoles"] = sorted(verified_roles)
    base["missingBoardRoles"] = sorted(set(BOARD_ROLES) - set(verified_roles))
    if signature_errors:
        base["outcome"] = "INVALID_RELEASE_BOARD_DECISION"
        base["errors"] = signature_errors
        return base
    if base["missingBoardRoles"]:
        base["outcome"] = "WAITING_FOR_RELEASE_BOARD_SIGNATURES"
        base["nextAction"] = "Obtain signatures from three distinct Release Board keys: SERVICE_OWNER, SECURITY_REVIEWER and PLATFORM_REVIEWER."
        return base
    if decision["decision"] == "REJECT":
        base["outcome"] = "REJECTED_BY_RELEASE_BOARD"
        base["nextAction"] = "Do not deploy. Resolve the board findings and submit a newly bound candidate and decision."
        return base

    base["outcome"] = "APPROVED_FOR_DEPLOYMENT"
    base["deploymentAuthorizationGranted"] = True
    base["nextAction"] = "A controlled deployment is authorized for this exact candidate only. Execute it in a separate deployment procedure and collect post-deployment evidence."
    return base


def _load_private_key(path: Path, password_env: str) -> Ed25519PrivateKey:
    return stage44._load_private_key(path, password_env)


def _add_common_inputs(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--packet", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, required=True)
    parser.add_argument("--attestation", type=Path, required=True)
    parser.add_argument("--keyring", type=Path, required=True)
    parser.add_argument("--trusted-keyring-sha256")
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--artifact-root", type=Path, required=True)
    parser.add_argument("--project-root", type=Path, default=ROOT)


def _load_context(args: argparse.Namespace) -> tuple:
    packet, _ = _load_path(args.packet, "PACKET")
    attestation, attestation_raw = _load_path(args.attestation, "ATTESTATION")
    keyring, keyring_raw = _load_path(args.keyring, "KEYRING")
    candidate, candidate_raw = _load_path(args.candidate, "CANDIDATE")
    if not isinstance(packet, dict) or not isinstance(attestation, dict) or not isinstance(candidate, dict):
        raise ReleaseBoardError("UPSTREAM_JSON_ROOT_INVALID")
    if not isinstance(keyring, dict):
        raise ReleaseBoardError("KEYRING_ROOT_INVALID")
    return packet, attestation, attestation_raw, keyring, keyring_raw, candidate, candidate_raw


def cmd_prepare(args: argparse.Namespace) -> int:
    packet, attestation, attestation_raw, keyring, keyring_raw, candidate, candidate_raw = _load_context(args)
    decision = prepare_decision(
        packet, candidate, candidate_raw, attestation, attestation_raw,
        keyring, keyring_raw, args.trusted_keyring_sha256, args.evidence_root,
        args.artifact_root, args.decision_value, project_root=args.project_root,
    )
    _write_json(args.decision_file, decision, create_only=True)
    print(json.dumps({"outcome": "WAITING_FOR_RELEASE_BOARD_SIGNATURES", "decisionId": decision["decisionId"]}, indent=2))
    return 0


def cmd_sign(args: argparse.Namespace) -> int:
    packet, attestation, attestation_raw, keyring, keyring_raw, candidate, candidate_raw = _load_context(args)
    decision_value, _ = _load_path(args.decision_file, "RELEASE_BOARD_DECISION")
    if not isinstance(decision_value, dict):
        raise ReleaseBoardError("RELEASE_BOARD_DECISION_ROOT_INVALID")
    private_key = _load_private_key(args.private_key, args.password_env)
    updated = sign_decision(
        decision_value, packet, candidate, candidate_raw,
        attestation, attestation_raw, attestation, keyring, keyring_raw,
        args.trusted_keyring_sha256, args.evidence_root, args.artifact_root,
        args.key_id, args.board_role, private_key, args.signed_at_utc,
        args.project_root,
    )
    _write_json(args.decision_file, updated)
    print(json.dumps({"outcome": "SIGNATURE_ADDED", "boardRole": args.board_role, "keyId": args.key_id}, indent=2))
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    packet, attestation, attestation_raw, keyring, keyring_raw, candidate, candidate_raw = _load_context(args)
    decision, _ = _load_path(args.decision_file, "RELEASE_BOARD_DECISION")
    report = evaluate(
        packet, args.evidence_root, attestation, attestation_raw, attestation,
        keyring, keyring_raw, args.trusted_keyring_sha256, candidate,
        candidate_raw, args.artifact_root, decision, args.project_root,
    )
    if args.report:
        _write_json(args.report, report)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if args.require_approved and report["outcome"] != "APPROVED_FOR_DEPLOYMENT":
        return 2
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    prepare = sub.add_parser("prepare", help="Create an unsigned decision bound to a verified candidate.")
    _add_common_inputs(prepare)
    prepare.add_argument("--decision-file", type=Path, required=True)
    prepare.add_argument("--decision-value", choices=("APPROVE", "REJECT"), default="APPROVE")
    prepare.set_defaults(func=cmd_prepare)

    sign = sub.add_parser("sign", help="Add one independent Release Board signature.")
    _add_common_inputs(sign)
    sign.add_argument("--decision-file", type=Path, required=True)
    sign.add_argument("--key-id", required=True)
    sign.add_argument("--board-role", required=True, choices=BOARD_ROLES)
    sign.add_argument("--private-key", type=Path, required=True)
    sign.add_argument("--password-env", default="JUMANJI_EVIDENCE_PRIVATE_KEY_PASSWORD")
    sign.add_argument("--signed-at-utc", default=utc_now())
    sign.set_defaults(func=cmd_sign)

    verify = sub.add_parser("verify", help="Verify the exact release candidate and all Release Board signatures.")
    _add_common_inputs(verify)
    verify.add_argument("--decision-file", type=Path, required=True)
    verify.add_argument("--report", type=Path)
    verify.add_argument("--require-approved", action="store_true")
    verify.set_defaults(func=cmd_verify)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        return args.func(args)
    except (ReleaseBoardError, stage44.AttestationError, OSError, ValueError) as exc:
        print(json.dumps({"outcome": "BLOCKED_OR_INVALID", "error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
