#!/usr/bin/env python3
"""Validate Stage 42 sanitized staging drill evidence against its report schema."""
from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import datetime
from pathlib import Path
from uuid import UUID

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_PATH = ROOT / "Data/Encounter/EncounterCheckpointStagingDrillReport.schema.json"
EXPECTED_CHECKS = (
    "https_health",
    "postgres_readiness",
    "mtls_client_certificate_required",
    "bearer_authentication_enforced",
    "lease_acquire",
    "lease_exclusion",
    "lease_renewal",
    "preexisting_checkpoint_probe",
    "checkpoint_write",
    "checkpoint_cas_conflict",
    "signed_readback",
    "lease_release",
    "lease_reacquire_epoch",
    "stale_lease_fenced",
    "durable_readback_after_reacquire",
    "final_lease_release",
)
FORBIDDEN_KEY_PATTERN = re.compile(
    r"(token|password|secret|private.?key|profile.?id|participant.?key|"
    r"lease.?token|authorization|certificate.?private)",
    re.IGNORECASE,
)


def _is_utc_date_time(value: object) -> bool:
    if not isinstance(value, str):
        return False
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return False
    return parsed.tzinfo is not None


def _find_forbidden_keys(value: object, path: str = "$") -> list[str]:
    errors: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            if FORBIDDEN_KEY_PATTERN.search(str(key)):
                errors.append(f"{path}.{key}: forbidden sensitive field name")
            errors.extend(_find_forbidden_keys(child, f"{path}.{key}"))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(_find_forbidden_keys(child, f"{path}[{index}]"))
    return errors


def validate_report(report: object) -> list[str]:
    errors: list[str] = []
    if not isinstance(report, dict):
        return ["report root must be an object"]

    required = {
        "schema", "stage", "runId", "startedAtUtc", "completedAtUtc",
        "result", "environment", "checks", "evidence", "failureCode",
    }
    if set(report) != required:
        errors.append("report has missing or unsupported top-level fields")
    if report.get("schema") != "JUMANJI.CheckpointStagingDrillReport.v1":
        errors.append("report schema must be v1")
    if report.get("stage") != 42:
        errors.append("stage must be 42")
    try:
        UUID(str(report.get("runId", "")))
    except (ValueError, TypeError, AttributeError):
        errors.append("runId must be a UUID")
    for field in ("startedAtUtc", "completedAtUtc"):
        if not _is_utc_date_time(report.get(field)):
            errors.append(f"{field} must be timezone-aware ISO-8601")
    if report.get("result") not in {"PASS", "FAIL"}:
        errors.append("result must be PASS or FAIL")

    environment = report.get("environment")
    if not isinstance(environment, dict) or set(environment) != {
        "class", "id", "storageProvider", "serviceHost", "transport"
    }:
        errors.append("environment has missing or unsupported fields")
    else:
        if environment["class"] not in {"staging", "ephemeral-ci"}:
            errors.append("environment class must be staging or ephemeral-ci")
        if not isinstance(environment["id"], str) or not environment["id"].strip():
            errors.append("environment id is required")
        if environment["storageProvider"] != "postgresql":
            errors.append("storageProvider must be postgresql")
        if environment["transport"] != "https-mtls":
            errors.append("transport must be https-mtls")
        host = environment["serviceHost"]
        if not isinstance(host, str) or not host or "://" in host or "/" in host:
            errors.append("serviceHost must be a hostname only")

    checks = report.get("checks")
    if not isinstance(checks, list):
        errors.append("checks must be an array")
        checks = []
    by_id: dict[str, dict] = {}
    for index, check in enumerate(checks):
        if not isinstance(check, dict):
            errors.append(f"checks[{index}] must be an object")
            continue
        if not {"id", "status", "durationMs", "detailCode"}.issubset(check):
            errors.append(f"checks[{index}] is missing required fields")
            continue
        if set(check) - {"id", "status", "durationMs", "detailCode", "evidence"}:
            errors.append(f"checks[{index}] has unsupported fields")
        check_id = check.get("id")
        if not isinstance(check_id, str) or not re.fullmatch(r"[a-z][a-z0-9_]{2,63}", check_id):
            errors.append(f"checks[{index}].id is invalid")
            continue
        if check_id in by_id:
            errors.append(f"duplicate check id {check_id}")
        by_id[check_id] = check
        if check.get("status") not in {"PASS", "FAIL"}:
            errors.append(f"{check_id}: invalid status")
        duration = check.get("durationMs")
        if type(duration) not in {int, float} or duration < 0:
            errors.append(f"{check_id}: durationMs must be non-negative")
        if not isinstance(check.get("detailCode"), str) or not check["detailCode"]:
            errors.append(f"{check_id}: detailCode is required")

    missing = [check_id for check_id in EXPECTED_CHECKS if check_id not in by_id]
    if report.get("result") == "PASS":
        if missing:
            errors.append("PASS report is missing checks: " + ", ".join(missing))
        failed = [check_id for check_id, check in by_id.items() if check.get("status") != "PASS"]
        if failed:
            errors.append("PASS report contains failed checks")
        if report.get("failureCode") is not None:
            errors.append("PASS report must have null failureCode")
    else:
        if not isinstance(report.get("failureCode"), str) or not report["failureCode"]:
            errors.append("FAIL report requires failureCode")
        if not any(check.get("status") == "FAIL" for check in by_id.values()) and not report.get("failureCode"):
            errors.append("FAIL report must include a failed check or configuration failure code")

    evidence = report.get("evidence")
    if not isinstance(evidence, dict):
        errors.append("evidence must be an object")
    else:
        if "encounterId" in evidence and (
            not isinstance(evidence["encounterId"], str)
            or not re.fullmatch(r"DRILL_STAGE42_[A-Z0-9_]{8,64}", evidence["encounterId"])
        ):
            errors.append("evidence encounterId must be a synthetic DRILL_STAGE42 id")
        for field in ("payloadSha256", "envelopeSha256"):
            if field in evidence and (
                not isinstance(evidence[field], str)
                or not re.fullmatch(r"[a-f0-9]{64}", evidence[field])
            ):
                errors.append(f"evidence {field} must be SHA-256 hex")
        for field in ("firstAuthorityEpoch", "reacquiredAuthorityEpoch", "priorSequence", "writtenSequence"):
            if field in evidence and (type(evidence[field]) is not int or evidence[field] < 0):
                errors.append(f"evidence {field} must be a non-negative integer")

    errors.extend(_find_forbidden_keys(report))
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        report = json.loads(args.report.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        print(f"Stage 42 report validation: FAIL ({type(error).__name__})", file=sys.stderr)
        return 1
    errors = validate_report(report)
    if errors:
        print(f"Stage 42 report validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Stage 42 report validation: PASS")
    print(f"Validated {len(report['checks'])} checks; result={report['result']}; no sensitive payload fields.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
