#!/usr/bin/env python3
"""Offline Stage 49 post-incident service re-entry evidence gate.

This tool verifies evidence and bindings only. It never changes traffic,
restarts services, deploys software, mutates databases or clears release holds.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any
from uuid import UUID

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "JUMANJI.ProductionServiceReentryGate.v1"
INCIDENT_SCHEMA = "JUMANJI.ProductionDeploymentIncidentRecord.v1"
ATTESTATION_SCHEMA = "JUMANJI.ProductionDeploymentIncidentAttestation.v1"
REPORT_SCHEMA = "JUMANJI.ProductionDeploymentIncidentEvidenceReport.v1"
REQUIRED_CHECKS = (
    "service_health", "database_integrity", "lease_fencing",
    "signing_key_verification", "queue_backlog", "telemetry_alerts",
    "gameplay_synthetic", "client_reconnect", "security_access",
    "oncall_handover", "stability_soak",
)
MAX_FILE_BYTES = 25 * 1024 * 1024
MAX_TOTAL_BYTES = 100 * 1024 * 1024
SHA256 = re.compile(r"^[a-f0-9]{64}$")
SAFE_PATH = re.compile(r"^[A-Za-z0-9._/-]{1,240}$")
SAFE_REF = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._:-]{7,95}$")
ALLOWED_MEDIA = {
    "application/json", "text/plain", "text/csv",
    "application/octet-stream", "image/png", "image/jpeg",
}
SECRET_TEXT = (
    re.compile(r"(?i)\bBearer\s+[A-Za-z0-9._~+/-]+=*"),
    re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    re.compile(r"\bAKIA[0-9A-Z]{16}\b"),
)
SECRET_FIELD = re.compile(
    r"(?:password|passwd|secret|access.?token|refresh.?token|authorization|"
    r"private.?key|api.?key|lease.?token|credential)", re.IGNORECASE
)


class GateError(ValueError):
    """Fail-closed Stage 49 evidence rejection."""


def _sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def _read_json(path: Path, label: str, max_bytes: int = 8 * 1024 * 1024) -> tuple[object, bytes]:
    if path.is_symlink() or not path.is_file():
        raise GateError(f"{label}_FILE_INVALID")
    raw = path.read_bytes()
    if not raw or len(raw) > max_bytes:
        raise GateError(f"{label}_SIZE_INVALID")
    try:
        value = json.loads(raw.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise GateError(f"{label}_JSON_INVALID") from exc
    if not isinstance(value, dict):
        raise GateError(f"{label}_ROOT_INVALID")
    return value, raw


def _utc(value: object, label: str) -> datetime:
    if not isinstance(value, str):
        raise GateError(f"{label}_TIMESTAMP_INVALID")
    try:
        dt = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise GateError(f"{label}_TIMESTAMP_INVALID") from exc
    if dt.tzinfo is None or dt.utcoffset() is None or dt.utcoffset().total_seconds() != 0:
        raise GateError(f"{label}_MUST_BE_UTC")
    return dt.astimezone(timezone.utc)


def _safe_file(root: Path, relative: object) -> Path:
    if not isinstance(relative, str) or not SAFE_PATH.fullmatch(relative) or "\\" in relative:
        raise GateError("EVIDENCE_PATH_INVALID")
    parts = relative.split("/")
    if any(part in {"", ".", ".."} for part in parts) or PurePosixPath(relative).is_absolute():
        raise GateError("EVIDENCE_PATH_TRAVERSAL")
    if root.is_symlink():
        raise GateError("EVIDENCE_ROOT_SYMLINK")
    try:
        root_real = root.resolve(strict=True)
    except OSError as exc:
        raise GateError("EVIDENCE_ROOT_MISSING") from exc
    if not root_real.is_dir():
        raise GateError("EVIDENCE_ROOT_INVALID")
    current = root_real
    for part in parts:
        current = current / part
        if current.is_symlink():
            raise GateError("EVIDENCE_SYMLINK_REJECTED")
    try:
        target = current.resolve(strict=True)
        target.relative_to(root_real)
    except (OSError, ValueError) as exc:
        raise GateError("EVIDENCE_FILE_MISSING_OR_OUTSIDE_ROOT") from exc
    if not target.is_file():
        raise GateError("EVIDENCE_NOT_REGULAR_FILE")
    return target


def _verify_attachment(root: Path, evidence: object, seen_paths: set[str], total: list[int]) -> str:
    if not isinstance(evidence, dict) or set(evidence) != {"path", "sha256", "sizeBytes", "mediaType"}:
        raise GateError("EVIDENCE_DESCRIPTOR_INVALID")
    path_text = evidence.get("path")
    if path_text in seen_paths:
        raise GateError("EVIDENCE_PATH_DUPLICATE")
    seen_paths.add(path_text)
    digest = evidence.get("sha256")
    if not isinstance(digest, str) or not SHA256.fullmatch(digest):
        raise GateError("EVIDENCE_SHA256_INVALID")
    size = evidence.get("sizeBytes")
    if type(size) is not int or not 1 <= size <= MAX_FILE_BYTES:
        raise GateError("EVIDENCE_SIZE_INVALID")
    media = evidence.get("mediaType")
    if media not in ALLOWED_MEDIA:
        raise GateError("EVIDENCE_MEDIA_TYPE_INVALID")
    path = _safe_file(root, path_text)
    if path.stat().st_size != size:
        raise GateError("EVIDENCE_SIZE_MISMATCH")
    raw = path.read_bytes()
    if _sha(raw) != digest:
        raise GateError("EVIDENCE_HASH_MISMATCH")
    total[0] += len(raw)
    if total[0] > MAX_TOTAL_BYTES:
        raise GateError("EVIDENCE_TOTAL_SIZE_LIMIT")
    if media in {"application/json", "text/plain", "text/csv"}:
        try:
            text = raw.decode("utf-8")
        except UnicodeError as exc:
            raise GateError("EVIDENCE_TEXT_ENCODING_INVALID") from exc
        if any(pattern.search(text) for pattern in SECRET_TEXT):
            raise GateError("EVIDENCE_SECRET_PATTERN_REJECTED")
        if media == "application/json":
            try:
                payload = json.loads(text)
            except json.JSONDecodeError as exc:
                raise GateError("EVIDENCE_JSON_INVALID") from exc
            def walk(value: object) -> None:
                if isinstance(value, dict):
                    for key, child in value.items():
                        if SECRET_FIELD.search(str(key)):
                            raise GateError("EVIDENCE_SECRET_FIELD_REJECTED")
                        walk(child)
                elif isinstance(value, list):
                    for child in value:
                        walk(child)
            walk(payload)
    return digest


def validate_gate(
    gate: object,
    report: object,
    report_raw: bytes,
    incident: object,
    incident_raw: bytes,
    attestation: object,
    attestation_raw: bytes,
    evidence_root: Path,
) -> dict[str, Any]:
    if not all(isinstance(item, dict) for item in (gate, report, incident, attestation)):
        raise GateError("INPUT_ROOT_INVALID")
    assert isinstance(gate, dict) and isinstance(report, dict)
    assert isinstance(incident, dict) and isinstance(attestation, dict)

    if gate.get("schema") != SCHEMA or gate.get("stage") != 49:
        raise GateError("GATE_SCHEMA_INVALID")
    if gate.get("status") != "SUBMITTED_FOR_HUMAN_REVIEW":
        raise GateError("GATE_NOT_SUBMITTED")
    expected_gate_fields = {
        "schema", "stage", "gateId", "createdAtUtc", "environment", "status",
        "incidentBinding", "checks", "approvals", "releaseControls",
    }
    if set(gate) != expected_gate_fields:
        raise GateError("GATE_FIELDS_INVALID")
    try:
        UUID(str(gate.get("gateId")))
    except (ValueError, TypeError, AttributeError) as exc:
        raise GateError("GATE_ID_INVALID") from exc
    created = _utc(gate.get("createdAtUtc"), "GATE")
    if created > datetime.now(timezone.utc):
        raise GateError("GATE_TIMESTAMP_IN_FUTURE")
    env = gate.get("environment")
    if not isinstance(env, dict) or set(env) != {"class", "id"}:
        raise GateError("GATE_ENVIRONMENT_INVALID")
    if env.get("class") != "production" or not isinstance(env.get("id"), str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{2,95}", env["id"]):
        raise GateError("GATE_ENVIRONMENT_INVALID")

    binding = gate.get("incidentBinding")
    if not isinstance(binding, dict) or set(binding) != {
        "incidentId", "incidentRecordSha256", "incidentAttestationSha256", "incidentReportSha256"
    }:
        raise GateError("INCIDENT_BINDING_INVALID")
    if _sha(incident_raw) != binding.get("incidentRecordSha256"):
        raise GateError("INCIDENT_RECORD_HASH_MISMATCH")
    if _sha(attestation_raw) != binding.get("incidentAttestationSha256"):
        raise GateError("INCIDENT_ATTESTATION_HASH_MISMATCH")
    if _sha(report_raw) != binding.get("incidentReportSha256"):
        raise GateError("INCIDENT_REPORT_HASH_MISMATCH")
    incident_id = incident.get("incidentId")
    if incident.get("schema") != INCIDENT_SCHEMA or incident.get("stage") != 48:
        raise GateError("INCIDENT_RECORD_SCHEMA_INVALID")
    if (
        incident.get("status") != "RESOLVED_FOR_REVIEW"
        or incident.get("serviceState") != "RESTORED"
        or incident.get("dataIntegrityState") != "VERIFIED"
        or incident.get("releaseDisposition") != "HOLD"
    ):
        raise GateError("INCIDENT_RECORD_NOT_READY_FOR_REENTRY_REVIEW")
    if incident_id != binding.get("incidentId") or report.get("incidentId") != incident_id:
        raise GateError("INCIDENT_ID_BINDING_MISMATCH")
    if _sha(incident_raw) != report.get("incidentSha256"):
        raise GateError("INCIDENT_REPORT_RECORD_BINDING_MISMATCH")
    if report.get("schema") != REPORT_SCHEMA or report.get("outcome") != "INCIDENT_EVIDENCE_VERIFIED":
        raise GateError("STAGE48_REPORT_NOT_VERIFIED")
    if report.get("incidentClosedForAudit") is not True:
        raise GateError("STAGE48_AUDIT_NOT_CLOSED")
    if report.get("releaseDisposition") != "HOLD" or report.get("promotionAllowed") is not False:
        raise GateError("STAGE48_RELEASE_BOUNDARY_INVALID")
    if report.get("productionReady") is not False or report.get("highAvailabilityApproved") is not False or report.get("sloStatus") != "UNBASELINED":
        raise GateError("STAGE48_READINESS_BOUNDARY_INVALID")

    if attestation.get("schema") != ATTESTATION_SCHEMA or attestation.get("incidentId") != incident_id:
        raise GateError("INCIDENT_ATTESTATION_BINDING_INVALID")
    if attestation.get("incidentSha256") != _sha(incident_raw) or attestation.get("canonicalization") != "RFC8785":
        raise GateError("INCIDENT_ATTESTATION_HASH_INVALID")
    signatures = attestation.get("signatures")
    if not isinstance(signatures, list) or len(signatures) != 2:
        raise GateError("INCIDENT_ATTESTATION_QUORUM_INVALID")
    roles = [item.get("role") for item in signatures if isinstance(item, dict)]
    key_ids = [item.get("keyId") for item in signatures if isinstance(item, dict)]
    if set(roles) != {"SERVICE_OPERATOR", "INDEPENDENT_REVIEWER"} or len(roles) != 2 or len(set(key_ids)) != 2:
        raise GateError("INCIDENT_ATTESTATION_ROLES_INVALID")

    controls = gate.get("releaseControls")
    if controls != {
        "releaseDisposition": "HOLD",
        "serviceReentryState": "HUMAN_REVIEW_REQUIRED",
        "trafficMutationAllowed": False,
        "promotionAllowed": False,
    }:
        raise GateError("RELEASE_CONTROLS_MUST_REMAIN_CLOSED")

    checks = gate.get("checks")
    if not isinstance(checks, list) or len(checks) != len(REQUIRED_CHECKS):
        raise GateError("REENTRY_CHECK_COUNT_INVALID")
    check_ids: list[str] = []
    seen_paths: set[str] = set()
    total = [0]
    verified: list[dict[str, str]] = []
    for check in checks:
        if not isinstance(check, dict) or set(check) != {"id", "status", "observedAtUtc", "summary", "evidence"}:
            raise GateError("REENTRY_CHECK_INVALID")
        check_id = check.get("id")
        if check_id not in REQUIRED_CHECKS or check_id in check_ids:
            raise GateError("REENTRY_CHECK_ID_INVALID_OR_DUPLICATE")
        check_ids.append(check_id)
        if check.get("status") != "PASS":
            raise GateError(f"REENTRY_CHECK_NOT_PASS:{check_id}")
        observed = _utc(check.get("observedAtUtc"), f"CHECK_{check_id}")
        if observed < created or observed > datetime.now(timezone.utc):
            raise GateError(f"REENTRY_CHECK_TIMESTAMP_OUT_OF_RANGE:{check_id}")
        summary = check.get("summary")
        if not isinstance(summary, str) or not 8 <= len(summary) <= 512:
            raise GateError(f"REENTRY_CHECK_SUMMARY_INVALID:{check_id}")
        digest = _verify_attachment(evidence_root, check.get("evidence"), seen_paths, total)
        verified.append({"id": check_id, "sha256": digest})
    if set(check_ids) != set(REQUIRED_CHECKS):
        raise GateError("REENTRY_CHECK_MATRIX_INCOMPLETE")

    approvals = gate.get("approvals")
    if not isinstance(approvals, list) or len(approvals) != 2:
        raise GateError("REENTRY_APPROVAL_QUORUM_INVALID")
    approval_roles: set[str] = set()
    reviewer_refs: set[str] = set()
    approval_digests: list[str] = []
    for approval in approvals:
        if not isinstance(approval, dict) or set(approval) != {
            "role", "reviewerRef", "decision", "approvedAtUtc", "evidence"
        }:
            raise GateError("REENTRY_APPROVAL_INVALID")
        role = approval.get("role")
        ref = approval.get("reviewerRef")
        if role not in {"SERVICE_OWNER", "INDEPENDENT_OPERATIONS_REVIEWER"} or role in approval_roles:
            raise GateError("REENTRY_APPROVAL_ROLE_INVALID")
        if not isinstance(ref, str) or not SAFE_REF.fullmatch(ref) or ref in reviewer_refs:
            raise GateError("REENTRY_REVIEWER_NOT_INDEPENDENT")
        if approval.get("decision") != "APPROVED":
            raise GateError("REENTRY_APPROVAL_NOT_APPROVED")
        approved = _utc(approval.get("approvedAtUtc"), f"APPROVAL_{role}")
        if approved < created or approved > datetime.now(timezone.utc):
            raise GateError("REENTRY_APPROVAL_TIMESTAMP_OUT_OF_RANGE")
        digest = _verify_attachment(evidence_root, approval.get("evidence"), seen_paths, total)
        approval_roles.add(role)
        reviewer_refs.add(ref)
        approval_digests.append(digest)
    if approval_roles != {"SERVICE_OWNER", "INDEPENDENT_OPERATIONS_REVIEWER"}:
        raise GateError("REENTRY_APPROVAL_QUORUM_INCOMPLETE")

    return {
        "schema": "JUMANJI.ProductionServiceReentryEvidenceReport.v1",
        "stage": 49,
        "outcome": "REENTRY_EVIDENCE_VERIFIED_FOR_HUMAN_REVIEW",
        "gateId": gate["gateId"],
        "incidentId": incident_id,
        "environmentId": env["id"],
        "verifiedCheckCount": len(verified),
        "verifiedApprovalCount": len(approval_digests),
        "verifiedEvidenceCount": len(verified) + len(approval_digests),
        "evidenceSha256": [item["sha256"] for item in verified] + approval_digests,
        "serviceReentryDecision": "HUMAN_REVIEW_REQUIRED",
        "releaseDisposition": "HOLD",
        "trafficMutationAllowed": False,
        "promotionAllowed": False,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "sideEffectsPerformedByVerifier": False,
    }


def validate_repository(root: Path = ROOT) -> list[str]:
    required = (
        "Data/Encounter/ProductionServiceReentryGate.schema.json",
        "Data/Encounter/ProductionServiceReentryGate.template.json",
        "Scripts/validate_encounter_checkpoint_stage49_reentry.py",
        "Scripts/Tests/test_validate_encounter_checkpoint_stage49_reentry.py",
        "Docs/STAGE_49_POST_INCIDENT_SERVICE_REENTRY_OPERATIONAL_CONTINUITY_GATE.md",
        ".github/workflows/encounter-checkpoint-stage49-reentry.yml",
        "Scripts/validate_encounter_checkpoint_stage48_incident.py",
    )
    errors = [f"REQUIRED_FILE_MISSING:{name}" for name in required if not (root / name).is_file()]
    if errors:
        return errors
    try:
        schema = json.loads((root / required[0]).read_text(encoding="utf-8"))
        template = json.loads((root / required[1]).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return ["STAGE49_SCHEMA_OR_TEMPLATE_INVALID"]
    if schema.get("properties", {}).get("stage", {}).get("const") != 49 or schema.get("additionalProperties") is not False:
        errors.append("STAGE49_SCHEMA_NOT_STRICT")
    if template.get("status") != "BLOCKED_MISSING_EVIDENCE" or template.get("checks") != [] or template.get("approvals") != []:
        errors.append("STAGE49_TEMPLATE_MUST_REMAIN_BLOCKED")
    if template.get("releaseControls") != {
        "releaseDisposition": "HOLD", "serviceReentryState": "HUMAN_REVIEW_REQUIRED",
        "trafficMutationAllowed": False, "promotionAllowed": False,
    }:
        errors.append("STAGE49_TEMPLATE_RELEASE_BOUNDARY_INVALID")
    workflow = (root / required[5]).read_text(encoding="utf-8")
    allowed_actions = {"actions/checkout@v4", "actions/setup-python@v5"}
    declared_actions = re.findall(r"(?m)^\\s+uses:\\s*([^\\s]+)", workflow)
    if not declared_actions or any(action not in allowed_actions for action in declared_actions):
        errors.append("STAGE49_WORKFLOW_ACTION_ALLOWLIST_INVALID")
    if "environment:" in workflow or re.search(r"(?m)^\\s*deployment:", workflow):
        errors.append("STAGE49_WORKFLOW_MUST_NOT_BIND_DEPLOYMENT_ENVIRONMENT")
    if "permissions:\\n  contents: read" not in workflow:
        errors.append("STAGE49_WORKFLOW_PERMISSIONS_MUST_BE_READ_ONLY")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-repository", action="store_true")
    parser.add_argument("--gate")
    parser.add_argument("--incident-report")
    parser.add_argument("--incident-record")
    parser.add_argument("--incident-attestation")
    parser.add_argument("--evidence-root")
    parser.add_argument("--report")
    args = parser.parse_args()
    if args.check_repository:
        errors = validate_repository()
        if errors:
            print("Stage 49 repository contract: FAIL")
            for error in errors:
                print("ERROR: " + error, file=sys.stderr)
            return 1
        print("Stage 49 repository contract: PASS")
        return 0
    required = (args.gate, args.incident_report, args.incident_record, args.incident_attestation, args.evidence_root, args.report)
    if any(value is None for value in required):
        parser.error("validation requires --gate, --incident-report, --incident-record, --incident-attestation, --evidence-root and --report")
    try:
        gate, _ = _read_json(Path(args.gate), "GATE")
        incident_report, report_raw = _read_json(Path(args.incident_report), "INCIDENT_REPORT")
        incident_record, incident_raw = _read_json(Path(args.incident_record), "INCIDENT_RECORD")
        attestation, attestation_raw = _read_json(Path(args.incident_attestation), "INCIDENT_ATTESTATION")
        result = validate_gate(
            gate, incident_report, report_raw, incident_record, incident_raw,
            attestation, attestation_raw, Path(args.evidence_root),
        )
        output = Path(args.report)
        if output.is_symlink():
            raise GateError("OUTPUT_SYMLINK_REJECTED")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (GateError, OSError) as exc:
        print(f"Stage 49 service re-entry evidence rejected: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
