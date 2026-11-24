#!/usr/bin/env python3
"""Prepare and verify a Stage 46 deployment plan without executing deployment.

The plan is bound to a freshly re-verified Stage 45 Release Board decision and
an independently hashed last-known-good production rollback baseline.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import encounter_checkpoint_release_board as stage45  # noqa: E402

PLAN_SCHEMA = "JUMANJI.ProductionDeploymentPlan.v1"
BASELINE_SCHEMA = "JUMANJI.ProductionRollbackBaseline.v1"
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{2,95}$")
SHA256 = re.compile(r"^[a-f0-9]{64}$")
COMMIT_SHA = re.compile(r"^[a-f0-9]{40}$")
MAX_ARTIFACT_BYTES = 8 * 1024 * 1024 * 1024
REQUIRED_TARGETS = {"ue58-dedicated-server", "android-package"}
TARGET_MEDIA = {
    "ue58-dedicated-server": {"application/octet-stream", "application/zip", "application/x-tar"},
    "android-package": {"application/vnd.android.package-archive", "application/zip"},
    "windows-client": {"application/octet-stream", "application/zip"},
}
PHASES = [
    {
        "phase": "PREFLIGHT",
        "status": "NOT_STARTED",
        "requiredEvidence": [
            "approved_change_window",
            "production_backup_and_restore_point",
            "operator_and_observer_identity",
            "pre_change_service_health",
            "rollback_baseline_retrieval_check",
        ],
    },
    {
        "phase": "CANARY",
        "status": "NOT_STARTED",
        "requiredEvidence": [
            "canary_target_identity",
            "canary_service_health",
            "checkpoint_write_read_and_signature_smoke",
            "operator_canary_observation",
        ],
    },
    {
        "phase": "PROGRESSIVE_ROLLOUT",
        "status": "NOT_STARTED",
        "requiredEvidence": [
            "operator_approval_for_each_batch",
            "per_batch_service_and_database_health",
            "lease_fencing_and_recovery_smoke",
            "client_compatibility_observation",
        ],
    },
    {
        "phase": "POST_DEPLOYMENT_VERIFICATION",
        "status": "NOT_STARTED",
        "requiredEvidence": [
            "dedicated_server_startup_and_health",
            "encounter_join_and_checkpoint_resume_smoke",
            "android_install_launch_and_network_smoke",
            "post_change_backup_and_restore_point",
            "operator_and_independent_observer_acceptance",
        ],
    },
]


class DeploymentPlanError(ValueError):
    """A fail-closed Stage 46 input, binding or filesystem error."""


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
            raise DeploymentPlanError("JSON_DUPLICATE_KEY")
        value[key] = item
    return value


def _load_json_bytes(raw: bytes, label: str) -> object:
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=_json_no_duplicates)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise DeploymentPlanError(f"{label}_JSON_INVALID") from exc


def _load_path(path: Path, label: str) -> tuple[object, bytes]:
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise DeploymentPlanError(f"{label}_READ_FAILED") from exc
    return _load_json_bytes(raw, label), raw


def _sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _safe_artifact_path(root: Path, relative: object, label: str) -> Path:
    if not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative:
        raise DeploymentPlanError(f"{label}_PATH_INVALID")
    posix = PurePosixPath(relative)
    if posix.is_absolute() or any(part in {"", ".", ".."} for part in posix.parts):
        raise DeploymentPlanError(f"{label}_PATH_TRAVERSAL")
    if not re.fullmatch(r"[A-Za-z0-9._/-]{1,240}", relative):
        raise DeploymentPlanError(f"{label}_PATH_INVALID")
    try:
        root_path = root.resolve(strict=True)
    except OSError as exc:
        raise DeploymentPlanError(f"{label}_ROOT_MISSING") from exc
    if root.is_symlink() or not root_path.is_dir():
        raise DeploymentPlanError(f"{label}_ROOT_INVALID")
    current = root_path
    for part in posix.parts:
        current = current / part
        if current.is_symlink():
            raise DeploymentPlanError(f"{label}_SYMLINK_REJECTED")
    try:
        resolved = current.resolve(strict=True)
        resolved.relative_to(root_path)
    except (OSError, ValueError) as exc:
        raise DeploymentPlanError(f"{label}_FILE_MISSING_OR_OUTSIDE_ROOT") from exc
    if not resolved.is_file():
        raise DeploymentPlanError(f"{label}_NOT_A_FILE")
    return resolved


def validate_rollback_baseline(
    baseline: object,
    baseline_raw: bytes,
    rollback_artifact_root: Path,
    candidate: dict[str, Any],
) -> dict[str, Any]:
    required_fields = {
        "schema", "baselineId", "environmentId", "capturedAtUtc", "sourceCommitSha", "targets"
    }
    if not isinstance(baseline, dict) or set(baseline) != required_fields:
        raise DeploymentPlanError("ROLLBACK_BASELINE_FIELDS_INVALID")
    if baseline.get("schema") != BASELINE_SCHEMA:
        raise DeploymentPlanError("ROLLBACK_BASELINE_SCHEMA_INVALID")
    if not isinstance(baseline.get("baselineId"), str) or not SAFE_ID.fullmatch(baseline["baselineId"]):
        raise DeploymentPlanError("ROLLBACK_BASELINE_ID_INVALID")
    if baseline.get("environmentId") != "production":
        raise DeploymentPlanError("ROLLBACK_BASELINE_ENVIRONMENT_INVALID")
    captured_at = _parse_utc(baseline.get("capturedAtUtc"))
    candidate_at = _parse_utc(candidate.get("createdAtUtc"))
    if captured_at is None or captured_at > datetime.now(timezone.utc):
        raise DeploymentPlanError("ROLLBACK_BASELINE_TIMESTAMP_INVALID")
    if candidate_at is None or captured_at > candidate_at:
        raise DeploymentPlanError("ROLLBACK_BASELINE_MUST_PRECEDE_CANDIDATE")
    if not isinstance(baseline.get("sourceCommitSha"), str) or not COMMIT_SHA.fullmatch(baseline["sourceCommitSha"]):
        raise DeploymentPlanError("ROLLBACK_BASELINE_SOURCE_COMMIT_INVALID")

    records = baseline.get("targets")
    if not isinstance(records, list) or not 2 <= len(records) <= 3:
        raise DeploymentPlanError("ROLLBACK_BASELINE_TARGET_COUNT_INVALID")
    seen: set[str] = set()
    verified: list[dict[str, Any]] = []
    for record in records:
        if not isinstance(record, dict) or set(record) != {"target", "path", "sha256", "sizeBytes", "mediaType"}:
            raise DeploymentPlanError("ROLLBACK_BASELINE_TARGET_FIELDS_INVALID")
        target = record.get("target")
        if not isinstance(target, str) or target not in TARGET_MEDIA:
            raise DeploymentPlanError("ROLLBACK_BASELINE_TARGET_UNKNOWN")
        if target in seen:
            raise DeploymentPlanError("ROLLBACK_BASELINE_TARGET_DUPLICATE")
        seen.add(target)
        digest = record.get("sha256")
        size = record.get("sizeBytes")
        media = record.get("mediaType")
        if not isinstance(digest, str) or not SHA256.fullmatch(digest):
            raise DeploymentPlanError(f"ROLLBACK_BASELINE_SHA256_INVALID:{target}")
        if type(size) is not int or size < 1 or size > MAX_ARTIFACT_BYTES:
            raise DeploymentPlanError(f"ROLLBACK_BASELINE_SIZE_INVALID:{target}")
        if not isinstance(media, str) or media not in TARGET_MEDIA[target]:
            raise DeploymentPlanError(f"ROLLBACK_BASELINE_MEDIA_TYPE_INVALID:{target}")
        artifact = _safe_artifact_path(rollback_artifact_root, record.get("path"), "ROLLBACK_BASELINE")
        if artifact.stat().st_size != size:
            raise DeploymentPlanError(f"ROLLBACK_BASELINE_SIZE_MISMATCH:{target}")
        if _sha256_file(artifact) != digest:
            raise DeploymentPlanError(f"ROLLBACK_BASELINE_HASH_MISMATCH:{target}")
        verified.append({
            "target": target,
            "path": record["path"],
            "sha256": digest,
            "sizeBytes": size,
            "mediaType": media,
        })

    candidate_targets = {
        item.get("target") for item in candidate.get("targets", []) if isinstance(item, dict)
    }
    if not REQUIRED_TARGETS.issubset(candidate_targets):
        raise DeploymentPlanError("CANDIDATE_REQUIRED_TARGETS_MISSING")
    if seen != candidate_targets:
        raise DeploymentPlanError("ROLLBACK_BASELINE_TARGET_SET_MISMATCH")
    return {
        "baselineId": baseline["baselineId"],
        "baselineSha256": _sha256(baseline_raw),
        "sourceCommitSha": baseline["sourceCommitSha"],
        "capturedAtUtc": baseline["capturedAtUtc"],
        "targets": sorted(verified, key=lambda item: item["target"]),
    }


def prepare_plan(
    packet: object,
    evidence_root: Path,
    attestation: object,
    attestation_raw: bytes,
    keyring: object,
    keyring_raw: bytes,
    trusted_keyring_sha256: str | None,
    candidate: object,
    candidate_raw: bytes,
    artifact_root: Path,
    decision: object,
    decision_raw: bytes,
    baseline: object,
    baseline_raw: bytes,
    rollback_artifact_root: Path,
    project_root: Path | None = None,
    plan_id: str | None = None,
    created_at_utc: str | None = None,
) -> dict[str, Any]:
    """Build an operator plan only after re-verifying Stage 45 and rollback bytes."""
    if not isinstance(candidate, dict) or not isinstance(decision, dict):
        raise DeploymentPlanError("RELEASE_INPUT_ROOT_INVALID")
    decoded_decision = _load_json_bytes(decision_raw, "RELEASE_BOARD_DECISION")
    if decoded_decision != decision:
        raise DeploymentPlanError("RELEASE_BOARD_DECISION_BYTES_MISMATCH")

    release_report = stage45.evaluate(
        packet,
        evidence_root,
        attestation,
        attestation_raw,
        attestation,
        keyring,
        keyring_raw,
        trusted_keyring_sha256,
        candidate,
        candidate_raw,
        artifact_root,
        decision,
        project_root or ROOT,
    )
    if release_report.get("outcome") != "APPROVED_FOR_DEPLOYMENT":
        raise DeploymentPlanError(f"STAGE45_NOT_APPROVED:{release_report.get('outcome')}")
    if release_report.get("deploymentAuthorizationGranted") is not True:
        raise DeploymentPlanError("STAGE45_AUTHORIZATION_MISSING")
    if release_report.get("deploymentExecuted") is not False:
        raise DeploymentPlanError("STAGE45_DEPLOYMENT_BOUNDARY_INVALID")
    if sorted(release_report.get("verifiedBoardRoles", [])) != sorted(stage45.BOARD_ROLES):
        raise DeploymentPlanError("STAGE45_BOARD_QUORUM_INVALID")
    if decision.get("decision") != "APPROVE":
        raise DeploymentPlanError("RELEASE_BOARD_NOT_APPROVED")

    rollback = validate_rollback_baseline(
        baseline, baseline_raw, rollback_artifact_root, candidate
    )
    stamp = created_at_utc or utc_now()
    parsed_stamp = _parse_utc(stamp)
    if parsed_stamp is None or parsed_stamp > datetime.now(timezone.utc):
        raise DeploymentPlanError("DEPLOYMENT_PLAN_TIMESTAMP_INVALID")
    identifier = plan_id or str(uuid.uuid4())
    try:
        uuid.UUID(identifier)
    except (ValueError, TypeError, AttributeError) as exc:
        raise DeploymentPlanError("DEPLOYMENT_PLAN_ID_INVALID") from exc

    assert isinstance(candidate, dict) and isinstance(decision, dict)
    candidate_targets = []
    for item in sorted(candidate["targets"], key=lambda entry: entry["target"]):
        candidate_targets.append({
            "target": item["target"],
            "path": item["path"],
            "sha256": item["sha256"],
            "sizeBytes": item["sizeBytes"],
            "mediaType": item["mediaType"],
        })
    return {
        "schema": PLAN_SCHEMA,
        "stage": 46,
        "planId": identifier,
        "createdAtUtc": stamp,
        "environmentId": candidate["environmentId"],
        "targetEnvironment": "production",
        "executionMode": "MANUAL_OUT_OF_BAND",
        "state": "READY_FOR_OPERATOR_REVIEW",
        "candidate": {
            "candidateId": candidate["candidateId"],
            "buildId": candidate["buildId"],
            "sourceCommitSha": candidate["sourceCommitSha"],
            "engineVersion": candidate["engineVersion"],
            "candidateSha256": _sha256(candidate_raw),
            "targets": candidate_targets,
        },
        "authorization": {
            "stage45Outcome": "APPROVED_FOR_DEPLOYMENT",
            "decisionId": decision["decisionId"],
            "decisionSha256": _sha256(decision_raw),
            "candidateSha256": _sha256(candidate_raw),
            "verifiedBoardRoles": sorted(release_report["verifiedBoardRoles"]),
        },
        "rollbackBaseline": rollback,
        "rollout": {
            "mode": "OPERATOR_GATED_SEQUENCE",
            "stopOnFailedGate": True,
            "automaticRetries": False,
            "phases": PHASES,
        },
        "rollbackPolicy": {
            "required": True,
            "action": "MANUAL_REDEPLOY_LAST_KNOWN_GOOD",
            "automaticRollback": False,
            "abortOnFailedHealthGate": True,
        },
        "sideEffectsPerformed": False,
        "deploymentExecuted": False,
        "postDeploymentVerified": False,
        "rollbackExecuted": False,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "promotionAllowed": False,
    }


def verify_plan(plan: object, *prepare_args: Any, **prepare_kwargs: Any) -> dict[str, Any]:
    """Recompute all bindings and require byte-independent structural equality."""
    if not isinstance(plan, dict):
        raise DeploymentPlanError("DEPLOYMENT_PLAN_ROOT_INVALID")
    if plan.get("schema") != PLAN_SCHEMA or plan.get("stage") != 46:
        raise DeploymentPlanError("DEPLOYMENT_PLAN_SCHEMA_INVALID")
    expected = prepare_plan(
        *prepare_args,
        plan_id=plan.get("planId"),
        created_at_utc=plan.get("createdAtUtc"),
        **prepare_kwargs,
    )
    if plan != expected:
        raise DeploymentPlanError("DEPLOYMENT_PLAN_BINDING_MISMATCH")
    return {
        "outcome": "PLAN_VERIFIED_NO_SIDE_EFFECTS",
        "planId": plan["planId"],
        "candidateId": plan["candidate"]["candidateId"],
        "deploymentExecuted": False,
        "postDeploymentVerified": False,
        "rollbackExecuted": False,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "promotionAllowed": False,
    }


def _write_json_create_only(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink():
        raise DeploymentPlanError("OUTPUT_SYMLINK_REJECTED")
    encoded = (json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")
    try:
        with path.open("xb") as handle:
            handle.write(encoded)
        try:
            os.chmod(path, 0o600)
        except OSError:
            pass
    except FileExistsError as exc:
        raise DeploymentPlanError("OUTPUT_ALREADY_EXISTS") from exc
    except OSError as exc:
        raise DeploymentPlanError("OUTPUT_WRITE_FAILED") from exc


def _add_common_inputs(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--packet", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, required=True)
    parser.add_argument("--attestation", type=Path, required=True)
    parser.add_argument("--keyring", type=Path, required=True)
    parser.add_argument("--trusted-keyring-sha256", required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--artifact-root", type=Path, required=True)
    parser.add_argument("--decision-file", type=Path, required=True)
    parser.add_argument("--rollback-baseline", type=Path, required=True)
    parser.add_argument("--rollback-artifact-root", type=Path, required=True)
    parser.add_argument("--project-root", type=Path, default=ROOT)


def _load_inputs(args: argparse.Namespace) -> tuple:
    packet, _ = _load_path(args.packet, "PACKET")
    attestation, attestation_raw = _load_path(args.attestation, "ATTESTATION")
    keyring, keyring_raw = _load_path(args.keyring, "KEYRING")
    candidate, candidate_raw = _load_path(args.candidate, "CANDIDATE")
    decision, decision_raw = _load_path(args.decision_file, "RELEASE_BOARD_DECISION")
    baseline, baseline_raw = _load_path(args.rollback_baseline, "ROLLBACK_BASELINE")
    if not all(isinstance(item, dict) for item in (packet, attestation, keyring, candidate, decision, baseline)):
        raise DeploymentPlanError("INPUT_JSON_ROOT_INVALID")
    if not isinstance(args.trusted_keyring_sha256, str) or not SHA256.fullmatch(args.trusted_keyring_sha256):
        raise DeploymentPlanError("TRUSTED_KEYRING_SHA256_INVALID")
    return (
        packet, attestation, attestation_raw, keyring, keyring_raw,
        candidate, candidate_raw, decision, decision_raw, baseline, baseline_raw,
    )


def _prepare_from_args(
    args: argparse.Namespace,
    plan_id: str | None = None,
    created_at_utc: str | None = None,
) -> dict[str, Any]:
    (
        packet, attestation, attestation_raw, keyring, keyring_raw,
        candidate, candidate_raw, decision, decision_raw, baseline, baseline_raw,
    ) = _load_inputs(args)
    return prepare_plan(
        packet, args.evidence_root, attestation, attestation_raw,
        keyring, keyring_raw, args.trusted_keyring_sha256,
        candidate, candidate_raw, args.artifact_root,
        decision, decision_raw, baseline, baseline_raw,
        args.rollback_artifact_root, args.project_root,
        plan_id=plan_id, created_at_utc=created_at_utc,
    )


def cmd_prepare(args: argparse.Namespace) -> int:
    plan = _prepare_from_args(args)
    _write_json_create_only(args.plan_file, plan)
    print(json.dumps({
        "outcome": "READY_FOR_OPERATOR_REVIEW",
        "planId": plan["planId"],
        "deploymentExecuted": False,
        "sideEffectsPerformed": False,
    }, indent=2))
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    plan_value, _ = _load_path(args.plan_file, "DEPLOYMENT_PLAN")
    if not isinstance(plan_value, dict):
        raise DeploymentPlanError("DEPLOYMENT_PLAN_ROOT_INVALID")
    values = _load_inputs(args)
    verification = verify_plan(
        plan_value,
        values[0], args.evidence_root, values[1], values[2],
        values[3], values[4], args.trusted_keyring_sha256,
        values[5], values[6], args.artifact_root,
        values[7], values[8], values[9], values[10],
        args.rollback_artifact_root, args.project_root,
    )
    print(json.dumps(verification, indent=2))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    prepare = sub.add_parser("prepare", help="Create a non-executing plan for a signed release candidate.")
    _add_common_inputs(prepare)
    prepare.add_argument("--plan-file", type=Path, required=True)
    prepare.set_defaults(func=cmd_prepare)

    verify = sub.add_parser("verify", help="Recheck all release, rollback and deployment-plan bindings.")
    _add_common_inputs(verify)
    verify.add_argument("--plan-file", type=Path, required=True)
    verify.set_defaults(func=cmd_verify)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        return args.func(args)
    except (
        DeploymentPlanError,
        stage45.ReleaseBoardError,
        OSError,
        ValueError,
        KeyError,
        TypeError,
    ) as exc:
        print(json.dumps({"outcome": "BLOCKED_OR_INVALID", "error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
