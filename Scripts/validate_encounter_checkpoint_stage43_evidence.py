#!/usr/bin/env python3
"""Fail-closed intake and integrity gate for Stage 42 operator evidence.

The gate verifies sanitized evidence artifacts and independent review. It does
not perform staging/DR operations, authenticate the humans named by roles,
approve production, approve HA, or change Stage 40 SLO governance.
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
PACKET_SCHEMA = "JUMANJI.ProductionReadinessEvidencePacket.v1"
GATE_SCHEMA = "JUMANJI.ProductionReadinessGateReport.v1"
MAX_ARTIFACT_BYTES = 5 * 1024 * 1024
SYNTHETIC_ID = re.compile(r"^DRILL_STAGE42_[A-Z0-9_]{8,64}$")
SHA256 = re.compile(r"^[a-f0-9]{64}$")
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{2,95}$")
FORBIDDEN_KEY = re.compile(
    r"(token|password|secret|private.?key|profile.?id|participant.?key|"
    r"lease.?token|authorization|certificate.?private|connection.?string|"
    r"database.?url|dsn|cookie|session.?cookie|api.?key)",
    re.IGNORECASE,
)

REQUIRED_KINDS = (
    "staging_smoke",
    "postgres_primary_failover",
    "postgres_pitr_restore",
    "post_restore_fencing_dry_run",
    "post_restore_fencing_apply",
    "signing_key_overlap_rotation",
    "ue58_dedicated_server_recovery",
    "ue58_pie_multiplayer",
    "android_device_package",
    "stage40_slo_approval",
)

SOURCE_CLASS_BY_KIND = {
    "staging_smoke": "operator-staging",
    "postgres_primary_failover": "dr-rehearsal",
    "postgres_pitr_restore": "dr-rehearsal",
    "post_restore_fencing_dry_run": "dr-rehearsal",
    "post_restore_fencing_apply": "dr-rehearsal",
    "signing_key_overlap_rotation": "dr-rehearsal",
    "ue58_dedicated_server_recovery": "ue-dedicated-server",
    "ue58_pie_multiplayer": "ue-pie-multiplayer",
    "android_device_package": "android-device",
    "stage40_slo_approval": "stage40-slo-approval",
}

DATABASE_ENVIRONMENT_KINDS = {
    "staging_smoke",
    "postgres_primary_failover",
    "postgres_pitr_restore",
    "post_restore_fencing_dry_run",
    "post_restore_fencing_apply",
    "signing_key_overlap_rotation",
}

SUMMARY_SCHEMAS = {
    "postgres_primary_failover": "JUMANJI.PostgresPrimaryFailoverEvidence.v1",
    "postgres_pitr_restore": "JUMANJI.PostgresPitrRestoreEvidence.v1",
    "signing_key_overlap_rotation": "JUMANJI.SigningKeyOverlapEvidence.v1",
    "ue58_dedicated_server_recovery": "JUMANJI.UE58DedicatedServerRecoveryEvidence.v1",
    "ue58_pie_multiplayer": "JUMANJI.UE58PIMultiplayerRecoveryEvidence.v1",
    "android_device_package": "JUMANJI.AndroidDeviceRecoveryEvidence.v1",
    "stage40_slo_approval": "JUMANJI.Stage40SLOApprovalEvidence.v1",
}

CLAIM_KEYS = {
    "postgres_primary_failover": {
        "failoverCompleted", "newPrimaryReady", "oldPrimaryWriteRejected",
        "signedCheckpointReadbackPassed", "checkpointSequencePreserved",
        "authorityEpochAdvanced", "rtoSeconds", "rpoSeconds",
        "syntheticEncounterId", "checkpointSequence",
    },
    "postgres_pitr_restore": {
        "backupRestored", "isolatedTarget", "allWritersStopped",
        "ingressBlocked", "signedCheckpointReadbackPassed",
        "checkpointSequencePreserved", "authorityEpochAdvanced",
        "rtoSeconds", "rpoSeconds", "syntheticEncounterId",
        "checkpointSequence",
    },
    "signing_key_overlap_rotation": {
        "oldKeyCanVerify", "newKeySignsNewCheckpoints",
        "oldAndNewKeyIdsDistinct", "overlapWindowVerified",
        "oldKeyId", "newKeyId", "syntheticEncounterId",
    },
    "ue58_dedicated_server_recovery": {
        "engineVersion", "dedicatedServerTarget", "serverBuildSucceeded",
        "restoreAttempted", "restorePassed", "participantBarrierPassed",
        "gameStateConsistent", "rewardNotDuplicated", "recoveryDurationMs",
        "syntheticEncounterId", "checkpointSequence",
    },
    "ue58_pie_multiplayer": {
        "engineVersion", "clientCount", "serverRestartTested",
        "reconnectPassed", "replicatedGameStateConsistent",
        "rewardNotDuplicated", "noClientRestoreAuthority",
    },
    "android_device_package": {
        "packageFormat", "packageSha256", "installedOnPhysicalDevice",
        "restoreSmokePassed", "deviceModel", "androidApiLevel",
        "packageBuildId",
    },
    "stage40_slo_approval": {
        "profileName", "profileStatus", "profileSha256",
        "benchmarkReportSha256", "hostInventorySha256",
        "capacityPlanSha256", "baselineRuns", "soakMinutes",
        "targetsApproved", "allSamplesWithinApprovedBudgets",
        "approverRoles",
    },
}

TRUE_CLAIMS = {
    "postgres_primary_failover": (
        "failoverCompleted", "newPrimaryReady", "oldPrimaryWriteRejected",
        "signedCheckpointReadbackPassed", "checkpointSequencePreserved",
        "authorityEpochAdvanced",
    ),
    "postgres_pitr_restore": (
        "backupRestored", "isolatedTarget", "allWritersStopped",
        "ingressBlocked", "signedCheckpointReadbackPassed",
        "checkpointSequencePreserved", "authorityEpochAdvanced",
    ),
    "signing_key_overlap_rotation": (
        "oldKeyCanVerify", "newKeySignsNewCheckpoints",
        "oldAndNewKeyIdsDistinct", "overlapWindowVerified",
    ),
    "ue58_dedicated_server_recovery": (
        "dedicatedServerTarget", "serverBuildSucceeded", "restoreAttempted",
        "restorePassed", "participantBarrierPassed", "gameStateConsistent",
        "rewardNotDuplicated",
    ),
    "ue58_pie_multiplayer": (
        "serverRestartTested", "reconnectPassed",
        "replicatedGameStateConsistent", "rewardNotDuplicated",
        "noClientRestoreAuthority",
    ),
    "android_device_package": (
        "installedOnPhysicalDevice", "restoreSmokePassed",
    ),
    "stage40_slo_approval": (
        "targetsApproved", "allSamplesWithinApprovedBudgets",
    ),
}

ROLE_VALUES = {
    "SERVICE_OPERATOR", "DATABASE_OPERATOR", "SECURITY_OPERATOR",
    "UNREAL_QA_OPERATOR", "ANDROID_QA_OPERATOR", "SLO_REVIEWER",
}
REVIEWER_ROLE_VALUES = {
    "INDEPENDENT_REVIEWER", "SECURITY_REVIEWER", "DATABASE_REVIEWER",
    "PLATFORM_REVIEWER", "QA_REVIEWER",
}
FINAL_REVIEW_ROLES = {
    "SERVICE_OWNER", "SECURITY_REVIEWER", "DATABASE_REVIEWER",
    "PLATFORM_REVIEWER", "QA_REVIEWER",
}
CRITICAL_EVIDENCE_KINDS = {
    "postgres_primary_failover", "postgres_pitr_restore",
    "post_restore_fencing_apply", "signing_key_overlap_rotation",
}

PACKET_KEYS = {
    "schema", "stage", "packetId", "createdAtUtc", "environment",
    "submissionStatus", "entries", "review",
}
ENVIRONMENT_KEYS = {"id", "class", "storageProvider"}
ENTRY_KEYS = {
    "evidenceId", "kind", "sourceClass", "environmentId",
    "capturedAtUtc", "operatorRole", "reviewerRole", "artifact",
}
ARTIFACT_KEYS = {"path", "sha256", "mediaType"}
REVIEW_KEYS = {"decision", "reviewerRoles", "reviewedAtUtc"}


def _aware_utc(value: object) -> bool:
    if not isinstance(value, str):
        return False
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return False
    return parsed.tzinfo is not None and parsed.utcoffset() is not None and parsed.utcoffset().total_seconds() == 0


def _forbidden_keys(value: object, location: str = "$") -> list[str]:
    found: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            if FORBIDDEN_KEY.search(str(key)):
                found.append(f"{location}: forbidden sensitive field name")
            found.extend(_forbidden_keys(child, f"{location}.{key}"))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            found.extend(_forbidden_keys(child, f"{location}[{index}]"))
    return found


def _safe_artifact_path(evidence_root: Path, relative: object) -> tuple[Path | None, str | None]:
    if not isinstance(relative, str) or not relative or "\\" in relative or "://" in relative:
        return None, "ARTIFACT_PATH_INVALID"
    parsed = PurePosixPath(relative)
    if parsed.is_absolute() or any(part in {"", ".", ".."} for part in parsed.parts):
        return None, "ARTIFACT_PATH_TRAVERSAL_REJECTED"
    root = evidence_root.resolve()
    candidate = root.joinpath(*parsed.parts)
    try:
        if any(parent.is_symlink() for parent in (candidate, *candidate.parents) if parent != root.parent):
            return None, "ARTIFACT_SYMLINK_REJECTED"
        resolved = candidate.resolve(strict=True)
    except OSError:
        return None, "ARTIFACT_FILE_MISSING"
    if not resolved.is_relative_to(root):
        return None, "ARTIFACT_PATH_OUTSIDE_ROOT"
    if not resolved.is_file():
        return None, "ARTIFACT_NOT_A_FILE"
    if resolved.stat().st_size > MAX_ARTIFACT_BYTES:
        return None, "ARTIFACT_SIZE_LIMIT_EXCEEDED"
    return resolved, None


def _load_artifact(evidence_root: Path, entry: dict[str, Any]) -> tuple[dict[str, Any] | None, str | None]:
    artifact = entry.get("artifact")
    if not isinstance(artifact, dict) or set(artifact) != ARTIFACT_KEYS:
        return None, "ARTIFACT_DESCRIPTOR_INVALID"
    if artifact.get("mediaType") != "application/json":
        return None, "ARTIFACT_MEDIA_TYPE_UNSUPPORTED"
    if not isinstance(artifact.get("sha256"), str) or not SHA256.fullmatch(artifact["sha256"]):
        return None, "ARTIFACT_SHA256_INVALID"

    path, path_error = _safe_artifact_path(evidence_root, artifact.get("path"))
    if path_error:
        return None, path_error
    assert path is not None

    try:
        raw = path.read_bytes()
    except OSError:
        return None, "ARTIFACT_READ_FAILED"
    actual_digest = hashlib.sha256(raw).hexdigest()
    if actual_digest != artifact["sha256"]:
        return None, "ARTIFACT_SHA256_MISMATCH"
    try:
        document = json.loads(raw.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError):
        return None, "ARTIFACT_JSON_INVALID"
    if not isinstance(document, dict):
        return None, "ARTIFACT_ROOT_NOT_OBJECT"
    if _forbidden_keys(document):
        return None, "ARTIFACT_SENSITIVE_FIELDS_REJECTED"
    return document, None


def _check_synthetic_id(value: object) -> bool:
    return isinstance(value, str) and SYNTHETIC_ID.fullmatch(value) is not None


def _check_non_negative_number(value: object) -> bool:
    return type(value) in {int, float} and value >= 0


def _validate_staging_report(report: dict[str, Any], entry: dict[str, Any]) -> list[str]:
    from validate_encounter_checkpoint_staging_report import validate_report

    errors = validate_report(report)
    if errors:
        return ["STAGING_REPORT_SCHEMA_OR_PRIVACY_REJECTED"]
    environment = report.get("environment", {})
    if report.get("result") != "PASS":
        errors.append("STAGING_REPORT_NOT_PASS")
    if environment.get("class") != "staging":
        errors.append("EPHEMERAL_CI_IS_NOT_OPERATOR_STAGING")
    if environment.get("id") != entry.get("environmentId"):
        errors.append("STAGING_REPORT_ENVIRONMENT_MISMATCH")
    if environment.get("storageProvider") != "postgresql" or environment.get("transport") != "https-mtls":
        errors.append("STAGING_REPORT_TRANSPORT_OR_STORAGE_MISMATCH")
    if not _check_synthetic_id(report.get("evidence", {}).get("encounterId")):
        errors.append("STAGING_REPORT_SYNTHETIC_ENCOUNTER_REQUIRED")
    return errors


def _validate_fencing_report(
    report: dict[str, Any], entry: dict[str, Any], expected_mode: str
) -> list[str]:
    required = {
        "schema", "drillId", "mode", "schemaVersion", "rowCount",
        "maxAuthorityEpochBefore", "epochOverflowRows", "leaseOwnerRowsBefore",
        "rowsFenced", "leaseOwnerRowsInvalidated", "maxAuthorityEpochAfter",
        "checkpointRowsPreserved", "completedAtUtc", "environmentClass",
        "environmentId", "result",
    }
    if set(report) != required:
        return ["FENCING_REPORT_FIELDS_INVALID"]
    if report.get("schema") != "JUMANJI.CheckpointRestoreFencingReport.v1":
        return ["FENCING_REPORT_SCHEMA_INVALID"]
    if report.get("result") != "PASS" or report.get("mode") != expected_mode:
        return ["FENCING_REPORT_MODE_OR_RESULT_INVALID"]
    if report.get("environmentClass") not in {"staging", "dr-rehearsal"}:
        return ["FENCING_REPORT_ENVIRONMENT_INVALID"]
    if report.get("environmentId") != entry.get("environmentId"):
        return ["FENCING_REPORT_ENVIRONMENT_MISMATCH"]
    if not _aware_utc(report.get("completedAtUtc")):
        return ["FENCING_REPORT_TIMESTAMP_INVALID"]

    integer_fields = (
        "schemaVersion", "rowCount", "maxAuthorityEpochBefore", "epochOverflowRows",
        "leaseOwnerRowsBefore", "rowsFenced", "leaseOwnerRowsInvalidated",
        "maxAuthorityEpochAfter", "checkpointRowsPreserved",
    )
    if any(type(report.get(field)) is not int or report[field] < 0 for field in integer_fields):
        return ["FENCING_REPORT_INTEGER_FIELDS_INVALID"]
    if report["epochOverflowRows"] != 0 or report["checkpointRowsPreserved"] != report["rowCount"]:
        return ["FENCING_REPORT_DATA_INTEGRITY_INVALID"]
    if expected_mode == "dry-run":
        if (
            report["rowsFenced"] != 0
            or report["leaseOwnerRowsInvalidated"] != 0
            or report["maxAuthorityEpochAfter"] != report["maxAuthorityEpochBefore"]
        ):
            return ["FENCING_DRY_RUN_MUST_NOT_MUTATE"]
    else:
        if (
            report["rowCount"] < 1
            or report["rowsFenced"] != report["rowCount"]
            or report["leaseOwnerRowsInvalidated"] != report["leaseOwnerRowsBefore"]
            or report["maxAuthorityEpochAfter"] <= report["maxAuthorityEpochBefore"]
        ):
            return ["FENCING_APPLY_DID_NOT_FENCE_ALL_ROWS"]
    try:
        UUID(str(report.get("drillId", "")))
    except ValueError:
        return ["FENCING_REPORT_DRILL_ID_INVALID"]
    return []


def _validate_summary_report(
    kind: str, report: dict[str, Any], entry: dict[str, Any]
) -> list[str]:
    expected_schema = SUMMARY_SCHEMAS[kind]
    required_top = {"schema", "result", "environmentId", "capturedAtUtc", "claims"}
    if set(report) != required_top:
        return ["SUMMARY_REPORT_FIELDS_INVALID"]
    if report.get("schema") != expected_schema or report.get("result") != "PASS":
        return ["SUMMARY_REPORT_SCHEMA_OR_RESULT_INVALID"]
    if report.get("environmentId") != entry.get("environmentId"):
        return ["SUMMARY_REPORT_ENVIRONMENT_MISMATCH"]
    if not _aware_utc(report.get("capturedAtUtc")):
        return ["SUMMARY_REPORT_TIMESTAMP_INVALID"]
    claims = report.get("claims")
    if not isinstance(claims, dict) or set(claims) != CLAIM_KEYS[kind]:
        return ["SUMMARY_REPORT_CLAIMS_INVALID"]

    for key in TRUE_CLAIMS.get(kind, ()):
        if claims.get(key) is not True:
            return [f"REQUIRED_CLAIM_FALSE:{key}"]

    if kind in {"postgres_primary_failover", "postgres_pitr_restore"}:
        for key in ("rtoSeconds", "rpoSeconds"):
            if not _check_non_negative_number(claims.get(key)):
                return [f"INVALID_RECOVERY_METRIC:{key}"]
        if not _check_synthetic_id(claims.get("syntheticEncounterId")):
            return ["RECOVERY_EVIDENCE_REQUIRES_SYNTHETIC_ENCOUNTER"]
        if type(claims.get("checkpointSequence")) is not int or claims["checkpointSequence"] < 1:
            return ["RECOVERY_EVIDENCE_CHECKPOINT_SEQUENCE_INVALID"]

    elif kind == "signing_key_overlap_rotation":
        old_key, new_key = claims.get("oldKeyId"), claims.get("newKeyId")
        if not all(isinstance(item, str) and SAFE_ID.fullmatch(item) for item in (old_key, new_key)):
            return ["SIGNING_KEY_IDS_INVALID"]
        if old_key == new_key:
            return ["SIGNING_KEY_IDS_MUST_DIFFER"]
        if not _check_synthetic_id(claims.get("syntheticEncounterId")):
            return ["KEY_ROTATION_SYNTHETIC_ENCOUNTER_REQUIRED"]

    elif kind == "ue58_dedicated_server_recovery":
        if claims.get("engineVersion") != "5.8":
            return ["UE_ENGINE_VERSION_MISMATCH"]
        if not _check_synthetic_id(claims.get("syntheticEncounterId")):
            return ["UE_RECOVERY_SYNTHETIC_ENCOUNTER_REQUIRED"]
        if type(claims.get("checkpointSequence")) is not int or claims["checkpointSequence"] < 1:
            return ["UE_RECOVERY_CHECKPOINT_SEQUENCE_INVALID"]
        if not _check_non_negative_number(claims.get("recoveryDurationMs")):
            return ["UE_RECOVERY_DURATION_INVALID"]

    elif kind == "ue58_pie_multiplayer":
        if claims.get("engineVersion") != "5.8":
            return ["UE_ENGINE_VERSION_MISMATCH"]
        if type(claims.get("clientCount")) is not int or claims["clientCount"] < 2:
            return ["PIE_REQUIRES_AT_LEAST_TWO_CLIENTS"]

    elif kind == "android_device_package":
        if claims.get("packageFormat") not in {"APK", "AAB"}:
            return ["ANDROID_PACKAGE_FORMAT_INVALID"]
        if not isinstance(claims.get("packageSha256"), str) or not SHA256.fullmatch(claims["packageSha256"]):
            return ["ANDROID_PACKAGE_SHA256_INVALID"]
        if not isinstance(claims.get("deviceModel"), str) or not claims["deviceModel"].strip():
            return ["ANDROID_DEVICE_MODEL_REQUIRED"]
        if type(claims.get("androidApiLevel")) is not int or claims["androidApiLevel"] < 1:
            return ["ANDROID_API_LEVEL_INVALID"]
        if not isinstance(claims.get("packageBuildId"), str) or not SAFE_ID.fullmatch(claims["packageBuildId"]):
            return ["ANDROID_BUILD_ID_INVALID"]

    elif kind == "stage40_slo_approval":
        for key in ("profileSha256", "benchmarkReportSha256", "hostInventorySha256", "capacityPlanSha256"):
            if not isinstance(claims.get(key), str) or not SHA256.fullmatch(claims[key]):
                return [f"SLO_ARTIFACT_HASH_INVALID:{key}"]
        if claims.get("profileStatus") != "APPROVED":
            return ["SLO_PROFILE_NOT_APPROVED"]
        if type(claims.get("baselineRuns")) is not int or claims["baselineRuns"] < 3:
            return ["SLO_REQUIRES_THREE_BASELINE_RUNS"]
        if type(claims.get("soakMinutes")) is not int or claims["soakMinutes"] < 30:
            return ["SLO_REQUIRES_AT_LEAST_THIRTY_MINUTE_SOAK"]
        approvers = claims.get("approverRoles")
        if (
            not isinstance(approvers, list)
            or len(approvers) < 2
            or len(set(approvers)) != len(approvers)
            or not set(approvers).issubset(FINAL_REVIEW_ROLES)
        ):
            return ["SLO_REQUIRES_TWO_DISTINCT_APPROVER_ROLES"]

    return []


def _validate_evidence_entry(
    entry: dict[str, Any], evidence_root: Path, packet_environment_id: str
) -> list[str]:
    errors: list[str] = []
    if set(entry) != ENTRY_KEYS:
        return ["EVIDENCE_ENTRY_FIELDS_INVALID"]
    kind = entry.get("kind")
    if kind not in REQUIRED_KINDS:
        return ["EVIDENCE_KIND_UNSUPPORTED"]
    if entry.get("sourceClass") != SOURCE_CLASS_BY_KIND[kind]:
        errors.append("EVIDENCE_SOURCE_CLASS_MISMATCH")
    if not isinstance(entry.get("environmentId"), str) or not SAFE_ID.fullmatch(entry["environmentId"]):
        errors.append("EVIDENCE_ENVIRONMENT_ID_INVALID")
    if kind in DATABASE_ENVIRONMENT_KINDS and entry.get("environmentId") != packet_environment_id:
        errors.append("DATABASE_EVIDENCE_ENVIRONMENT_MISMATCH")
    if not _aware_utc(entry.get("capturedAtUtc")):
        errors.append("EVIDENCE_TIMESTAMP_INVALID")
    try:
        UUID(str(entry.get("evidenceId", "")))
    except (ValueError, TypeError, AttributeError):
        errors.append("EVIDENCE_ID_INVALID")
    if not isinstance(entry.get("operatorRole"), str) or entry.get("operatorRole") not in ROLE_VALUES:
        errors.append("EVIDENCE_OPERATOR_ROLE_INVALID")
    if not isinstance(entry.get("reviewerRole"), str) or entry.get("reviewerRole") not in REVIEWER_ROLE_VALUES:
        errors.append("EVIDENCE_REVIEWER_ROLE_INVALID")
    if entry.get("kind") in CRITICAL_EVIDENCE_KINDS and entry.get("reviewerRole") not in {
        "INDEPENDENT_REVIEWER", "SECURITY_REVIEWER", "DATABASE_REVIEWER"
    }:
        errors.append("CRITICAL_EVIDENCE_REQUIRES_INDEPENDENT_REVIEW")
    if entry.get("operatorRole") == entry.get("reviewerRole"):
        errors.append("OPERATOR_AND_REVIEWER_MUST_BE_DISTINCT_ROLES")
    if errors:
        return errors

    report, artifact_error = _load_artifact(evidence_root, entry)
    if artifact_error:
        return [artifact_error]
    assert report is not None
    kind = entry["kind"]

    if kind == "staging_smoke":
        return _validate_staging_report(report, entry)
    if kind == "post_restore_fencing_dry_run":
        return _validate_fencing_report(report, entry, "dry-run")
    if kind == "post_restore_fencing_apply":
        return _validate_fencing_report(report, entry, "apply")
    return _validate_summary_report(kind, report, entry)


def validate_project_boundaries(project_root: Path) -> list[str]:
    errors: list[str] = []
    stage41_path = project_root / "Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json"
    stage42_path = project_root / "Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json"
    for path in (stage41_path, stage42_path):
        if not path.is_file():
            errors.append(f"READINESS_BOUNDARY_FILE_MISSING:{path.name}")
    if errors:
        return errors

    try:
        stage41 = json.loads(stage41_path.read_text(encoding="utf-8"))
        stage42 = json.loads(stage42_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return ["READINESS_BOUNDARY_MANIFEST_INVALID"]

    if (
        stage41.get("productionReady") is not False
        or stage41.get("highAvailabilityApproved") is not False
        or stage41.get("sloStatus") != "UNBASELINED"
    ):
        errors.append("STAGE41_READINESS_MUST_REMAIN_FAIL_CLOSED")

    stage42_evidence = stage42.get("evidence", {})
    required_false = (
        "realStagingDeploymentVerified",
        "postgresPrimaryFailoverDrillCompleted",
        "postgresPitrRestoreDrillCompleted",
        "postRestoreLeaseFencingDrillCompleted",
        "signingKeyOverlapRotationDrillCompleted",
        "liveUnrealDedicatedServerRecoveryVerified",
        "unrealEngine58BuildVerified",
        "androidDevicePackageVerified",
    )
    if (
        stage42.get("productionReady") is not False
        or stage42.get("highAvailabilityApproved") is not False
        or stage42.get("sloStatus") != "UNBASELINED"
        or any(stage42_evidence.get(field) is not False for field in required_false)
    ):
        errors.append("STAGE42_EXTERNAL_EVIDENCE_MUST_REMAIN_FALSE_UNTIL_FORMALLY_RECORDED")
    return errors


def evaluate_packet(
    packet: object,
    evidence_root: Path,
    project_root: Path | None = None,
) -> dict[str, Any]:
    errors: list[str] = []
    packet_id = ""
    missing = list(REQUIRED_KINDS)
    accepted: list[str] = []
    rejected: list[dict[str, str]] = []

    if not isinstance(packet, dict):
        errors.append("PACKET_ROOT_MUST_BE_OBJECT")
    else:
        if set(packet) != PACKET_KEYS:
            errors.append("PACKET_FIELDS_INVALID")
        if packet.get("schema") != PACKET_SCHEMA or packet.get("stage") != 43:
            errors.append("PACKET_SCHEMA_OR_STAGE_INVALID")
        try:
            packet_id = str(UUID(str(packet.get("packetId", ""))))
        except (ValueError, TypeError, AttributeError):
            errors.append("PACKET_ID_INVALID")
        if not _aware_utc(packet.get("createdAtUtc")):
            errors.append("PACKET_TIMESTAMP_INVALID")

        environment = packet.get("environment")
        if not isinstance(environment, dict) or set(environment) != ENVIRONMENT_KEYS:
            errors.append("PACKET_ENVIRONMENT_FIELDS_INVALID")
            packet_environment_id = ""
        else:
            packet_environment_id = environment.get("id", "")
            if not isinstance(packet_environment_id, str) or not SAFE_ID.fullmatch(packet_environment_id):
                errors.append("PACKET_ENVIRONMENT_ID_INVALID")
            if environment.get("class") != "staging" or environment.get("storageProvider") != "postgresql":
                errors.append("PACKET_ENVIRONMENT_MUST_BE_OPERATOR_MANAGED_POSTGRES_STAGING")

        entries = packet.get("entries")
        if not isinstance(entries, list):
            errors.append("PACKET_ENTRIES_MUST_BE_ARRAY")
            entries = []
        seen_ids: set[str] = set()
        by_kind: dict[str, dict[str, Any]] = {}
        for entry in entries:
            if not isinstance(entry, dict):
                errors.append("EVIDENCE_ENTRY_MUST_BE_OBJECT")
                continue
            try:
                evidence_id = str(UUID(str(entry.get("evidenceId", ""))))
            except (ValueError, TypeError, AttributeError):
                evidence_id = "INVALID_EVIDENCE_ID"
            kind = entry.get("kind")
            if kind in by_kind:
                rejected.append({"evidenceId": evidence_id, "code": "DUPLICATE_EVIDENCE_KIND"})
                errors.append(f"DUPLICATE_EVIDENCE_KIND:{kind}")
                continue
            if evidence_id in seen_ids:
                errors.append("DUPLICATE_EVIDENCE_ID")
                rejected.append({"evidenceId": evidence_id, "code": "DUPLICATE_EVIDENCE_ID"})
                continue
            seen_ids.add(evidence_id)
            if kind not in REQUIRED_KINDS:
                rejected.append({"evidenceId": evidence_id, "code": "EVIDENCE_KIND_UNSUPPORTED"})
                errors.append("EVIDENCE_KIND_UNSUPPORTED")
                continue
            entry_errors = _validate_evidence_entry(entry, evidence_root, packet_environment_id)
            if entry_errors:
                rejected.append({"evidenceId": evidence_id, "code": entry_errors[0]})
                errors.extend(f"{evidence_id}:{code}" for code in entry_errors)
                continue
            by_kind[kind] = entry
            accepted.append(kind)

        missing = [kind for kind in REQUIRED_KINDS if kind not in by_kind]
        review = packet.get("review")
        if not isinstance(review, dict) or set(review) != REVIEW_KEYS:
            errors.append("PACKET_REVIEW_FIELDS_INVALID")
            review_decision = "NOT_REVIEWED"
        else:
            review_decision = review.get("decision")
            reviewer_roles = review.get("reviewerRoles")
            if (
                not isinstance(reviewer_roles, list)
                or any(not isinstance(role, str) or role not in FINAL_REVIEW_ROLES for role in reviewer_roles)
            ):
                errors.append("FINAL_REVIEWER_ROLES_INVALID")
                reviewer_roles = []
            if len(set(reviewer_roles)) != len(reviewer_roles):
                errors.append("FINAL_REVIEWER_ROLES_DUPLICATED")
            if review_decision == "APPROVED":
                if len(reviewer_roles) < 2 or not (set(reviewer_roles) & {"SECURITY_REVIEWER", "DATABASE_REVIEWER"}):
                    errors.append("APPROVAL_REQUIRES_TWO_REVIEWERS_INCLUDING_SECURITY_OR_DATABASE")
                if not _aware_utc(review.get("reviewedAtUtc")):
                    errors.append("APPROVAL_TIMESTAMP_REQUIRED")
            elif review_decision in {"NOT_REVIEWED", "REJECTED"}:
                if review.get("reviewedAtUtc") is not None and not _aware_utc(review.get("reviewedAtUtc")):
                    errors.append("REVIEW_TIMESTAMP_INVALID")
            else:
                errors.append("REVIEW_DECISION_INVALID")

        submission = packet.get("submissionStatus")
        if missing and submission != "AWAITING_OPERATOR_EVIDENCE":
            errors.append("SUBMISSION_STATUS_MUST_REFLECT_MISSING_EVIDENCE")
        elif not missing and review_decision == "NOT_REVIEWED" and submission != "SUBMITTED_FOR_REVIEW":
            errors.append("COMPLETE_PACKET_MUST_BE_SUBMITTED_FOR_REVIEW")
        elif review_decision in {"APPROVED", "REJECTED"} and submission != "REVIEWED":
            errors.append("REVIEWED_PACKET_STATUS_MISMATCH")

    if project_root is not None:
        errors.extend(validate_project_boundaries(project_root))

    if errors:
        outcome = "INVALID_PACKET"
    elif missing:
        outcome = "BLOCKED_MISSING_EVIDENCE"
    elif packet["review"]["decision"] == "REJECTED":
        outcome = "REJECTED_BY_REVIEW"
    elif packet["review"]["decision"] != "APPROVED":
        outcome = "WAITING_FOR_INDEPENDENT_REVIEW"
    else:
        outcome = "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW"

    return {
        "schema": GATE_SCHEMA,
        "stage": 43,
        "packetId": packet_id or None,
        "evaluatedAtUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "outcome": outcome,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "acceptedEvidenceKinds": [kind for kind in REQUIRED_KINDS if kind in accepted],
        "missingEvidenceKinds": missing,
        "rejectedEvidence": rejected,
        "errors": errors,
        "promotionAllowed": False,
        "nextAction": (
            "Do not promote. Gather the missing operator evidence, validate hashes, and obtain an independent two-role review."
            if outcome != "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW"
            else "Evidence is eligible for a separate final release-board decision. This gate does not change production/HA/SLO manifests."
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--packet",
        type=Path,
        default=ROOT / "Data/Encounter/ProductionReadinessEvidencePacket.template.json",
    )
    parser.add_argument(
        "--evidence-root",
        type=Path,
        required=True,
        help="Directory containing operator-supplied sanitized JSON evidence artifacts.",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=ROOT,
        help="Repository root used to verify existing Stage 41/42 fail-closed manifests.",
    )
    parser.add_argument("--report", type=Path, help="Optional sanitized gate-result JSON output.")
    parser.add_argument(
        "--require-eligible",
        action="store_true",
        help="Exit 2 unless every evidence kind and the independent approval are present.",
    )
    args = parser.parse_args()

    try:
        packet = json.loads(args.packet.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        print("Stage 43 evidence gate: INVALID_PACKET (PACKET_READ_FAILED)", file=sys.stderr)
        return 1

    report = evaluate_packet(packet, args.evidence_root, args.project_root)
    encoded = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    if report["outcome"] == "INVALID_PACKET":
        return 1
    if args.require_eligible and report["outcome"] != "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW":
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
