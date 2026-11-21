#!/usr/bin/env python3
"""Validate Stage 40 checkpoint capacity/SLO profiles without inventing targets."""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILE_PATH = Path("Data/Encounter/EncounterRecoverySLOProfiles.csv")
INVENTORY_PATH = Path("Data/Encounter/EncounterRecoveryHostInventoryTemplate.json")

COLUMNS = (
    "Profile", "TargetEnvironment", "CheckpointCadenceSeconds", "UtilizationFactor",
    "MinimumDurationSeconds", "MinimumWorkers", "MinimumCompletedPairs",
    "MinimumLatencySamplesPerOperation", "RequiredJournalMode",
    "TargetWriteP95Ms", "TargetWriteP99Ms", "TargetReadP95Ms", "TargetReadP99Ms",
    "TargetPairsPerSecond", "Status", "EvidenceId", "Approver", "ReviewedAtUtc", "Notes",
)
REQUIRED_PROFILES = {
    "CI-SMOKE": (60, 0.50, 3, 2, 1, 1),
    "LOCAL-BASELINE": (60, 0.60, 60, 4, 100, 100),
    "LOCAL-SOAK30": (60, 0.60, 1800, 8, 1000, 500),
    "LOCAL-SOAK60": (60, 0.60, 3600, 8, 3000, 1000),
}
TARGET_FIELDS = (
    "TargetWriteP95Ms", "TargetWriteP99Ms", "TargetReadP95Ms",
    "TargetReadP99Ms", "TargetPairsPerSecond",
)
SOURCE_REQUIREMENTS = {
    "Scripts/plan_encounter_recovery_capacity.py": (
        "successful_checkpoint_pairs_per_second", "checkpoint_write", "checkpoint_read",
        "UNBASELINED", "projected_safe_concurrent_encounters", "report_sha256",
        "hostInventoryStatus", "MAX_UTILIZATION_FACTOR",
    ),
    "Scripts/Tests/test_encounter_recovery_capacity_planning.py": (
        "test_clean_report_produces_bounded_capacity_estimate",
        "test_failed_benchmark_is_not_eligible", "test_capacity_never_exceeds_tested_worker_envelope",
        "test_unbaselined_profile_never_claims_slo_pass",
    ),
    "Data/Encounter/EncounterRecoverySLOProfiles.csv": (
        "CI-SMOKE", "LOCAL-BASELINE", "LOCAL-SOAK30", "LOCAL-SOAK60", "UNBASELINED",
    ),
    "Data/Encounter/EncounterRecoveryHostInventoryTemplate.json": (
        "cpuModel", "physicalCoreCount", "memoryGiB", "storageModel",
        "benchmarkCommitSha", "benchmarkReportSha256",
    ),
    "Docs/STAGE_40_RECOVERY_CAPACITY_PLANNING_SLO_BASELINES.md": (
        "nie są zatwierdzone", "UNBASELINED", "p95", "p99",
        "nie wolno ekstrapolować", "Stage 39",
    ),
    ".github/workflows/encounter-recovery-capacity-slo.yml": (
        "plan_encounter_recovery_capacity.py", "test_encounter_recovery_capacity_planning.py",
        "test_validate_encounter_recovery_capacity_slo.py",
        "benchmark_encounter_checkpoint_recovery.py",
    ),
    "README.md": (
        "Stage 40 — Recovery Capacity Planning & SLO Baselines",
        "Docs/STAGE_40_RECOVERY_CAPACITY_PLANNING_SLO_BASELINES.md",
        "Data/Encounter/EncounterRecoverySLOProfiles.csv",
        "Scripts/plan_encounter_recovery_capacity.py",
    ),
}


def _number(value: str, field: str, line: int, *, allow_blank: bool = False) -> float | None:
    raw = (value or "").strip()
    if not raw and allow_blank:
        return None
    try:
        parsed = float(raw)
    except (TypeError, ValueError):
        raise ValueError(f"row {line}: {field} must be numeric") from None
    if not math.isfinite(parsed):
        raise ValueError(f"row {line}: {field} must be finite")
    return parsed


def validate_profiles(path: Path) -> list[str]:
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            missing = [column for column in COLUMNS if column not in (reader.fieldnames or [])]
            if missing:
                return [f"profile matrix missing columns: {', '.join(missing)}"]
            rows = list(reader)
    except (OSError, UnicodeError, csv.Error) as error:
        return [f"profile matrix cannot be read: {error}"]

    errors: list[str] = []
    by_name: dict[str, dict[str, str]] = {}
    for line, row in enumerate(rows, start=2):
        name = (row.get("Profile") or "").strip()
        if not name:
            errors.append(f"row {line}: Profile is empty")
            continue
        if name in by_name:
            errors.append(f"row {line}: duplicate Profile {name}")
            continue
        by_name[name] = row
        for field in ("TargetEnvironment", "RequiredJournalMode", "Notes"):
            if not (row.get(field) or "").strip():
                errors.append(f"{name}: {field} is required")
        if (row.get("RequiredJournalMode") or "").strip().upper() != "WAL":
            errors.append(f"{name}: RequiredJournalMode must be WAL")

        integer_fields = (
            "CheckpointCadenceSeconds", "MinimumDurationSeconds", "MinimumWorkers",
            "MinimumCompletedPairs", "MinimumLatencySamplesPerOperation",
        )
        parsed_ints: dict[str, int] = {}
        for field in integer_fields:
            try:
                raw = (row.get(field) or "").strip()
                parsed = int(raw)
                if str(parsed) != raw:
                    raise ValueError
                parsed_ints[field] = parsed
            except (TypeError, ValueError):
                errors.append(f"{name}: {field} must be an integer")
        if len(parsed_ints) == len(integer_fields):
            if not 1 <= parsed_ints["CheckpointCadenceSeconds"] <= 86400:
                errors.append(f"{name}: CheckpointCadenceSeconds must be 1..86400")
            if not 1 <= parsed_ints["MinimumDurationSeconds"] <= 3600:
                errors.append(f"{name}: MinimumDurationSeconds must be 1..3600")
            if not 1 <= parsed_ints["MinimumWorkers"] <= 64:
                errors.append(f"{name}: MinimumWorkers must be 1..64")
            if parsed_ints["MinimumCompletedPairs"] < 1:
                errors.append(f"{name}: MinimumCompletedPairs must be positive")
            if parsed_ints["MinimumLatencySamplesPerOperation"] < 1:
                errors.append(f"{name}: minimum latency sample count must be positive")

        try:
            utilization = _number(row.get("UtilizationFactor", ""), "UtilizationFactor", line)
            if utilization is not None and not 0.10 <= utilization <= 0.90:
                errors.append(f"{name}: UtilizationFactor must be 0.10..0.90")
        except ValueError as error:
            errors.append(str(error))

        status = (row.get("Status") or "").strip().upper()
        if status not in {"UNBASELINED", "PROPOSED", "APPROVED", "RETIRED"}:
            errors.append(f"{name}: Status must be UNBASELINED, PROPOSED, APPROVED or RETIRED")

        targets: dict[str, float | None] = {}
        for field in TARGET_FIELDS:
            try:
                targets[field] = _number(row.get(field, ""), field, line, allow_blank=True)
            except ValueError as error:
                errors.append(str(error))
        metadata = [row.get("EvidenceId", "").strip(), row.get("Approver", "").strip(), row.get("ReviewedAtUtc", "").strip()]
        if status == "UNBASELINED":
            if any(value is not None for value in targets.values()):
                errors.append(f"{name}: UNBASELINED profile must not contain invented SLO targets")
            if any(metadata):
                errors.append(f"{name}: UNBASELINED profile must not claim approval evidence")
        elif status in {"PROPOSED", "APPROVED"}:
            if any(value is None or value <= 0 for value in targets.values()):
                errors.append(f"{name}: {status} profile requires all positive SLO targets")
            if not all(metadata):
                errors.append(f"{name}: {status} profile requires EvidenceId, Approver and ReviewedAtUtc")
            else:
                try:
                    parsed_date = datetime.fromisoformat(metadata[2].replace("Z", "+00:00"))
                    if parsed_date.tzinfo is None:
                        errors.append(f"{name}: ReviewedAtUtc must include a timezone")
                except ValueError:
                    errors.append(f"{name}: ReviewedAtUtc must be ISO-8601")
        if targets.get("TargetWriteP95Ms") and targets.get("TargetWriteP99Ms") and targets["TargetWriteP95Ms"] > targets["TargetWriteP99Ms"]:
            errors.append(f"{name}: write p95 budget must not exceed write p99 budget")
        if targets.get("TargetReadP95Ms") and targets.get("TargetReadP99Ms") and targets["TargetReadP95Ms"] > targets["TargetReadP99Ms"]:
            errors.append(f"{name}: read p95 budget must not exceed read p99 budget")

    for name, expected in REQUIRED_PROFILES.items():
        row = by_name.get(name)
        if row is None:
            errors.append(f"profile matrix missing required profile {name}")
            continue
        actual: list[float] = []
        for field in ("CheckpointCadenceSeconds", "UtilizationFactor", "MinimumDurationSeconds",
                      "MinimumWorkers", "MinimumCompletedPairs", "MinimumLatencySamplesPerOperation"):
            try:
                actual.append(float(row[field]))
            except (KeyError, TypeError, ValueError):
                actual.append(float("nan"))
        if not all(math.isclose(a, float(b), rel_tol=0.0, abs_tol=1e-9) for a, b in zip(actual, expected)):
            errors.append(f"{name}: profile values differ from the reviewed Stage 40 defaults")
    return errors


def validate_project(root: Path) -> list[str]:
    errors = validate_profiles(root / PROFILE_PATH)
    for relative, markers in SOURCE_REQUIREMENTS.items():
        try:
            content = (root / relative).read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            errors.append(f"{relative}: cannot read: {error}")
            continue
        for marker in markers:
            if marker not in content:
                errors.append(f"{relative}: missing Stage 40 requirement {marker!r}")

    template_path = root / INVENTORY_PATH
    try:
        template = json.loads(template_path.read_text(encoding="utf-8"))
        required_inventory_fields = {
            "schemaVersion", "hostId", "hostRole", "measuredAtUtc", "osName",
            "osVersion", "cpuModel", "physicalCoreCount", "logicalProcessorCount",
            "memoryGiB", "storageModel", "storageType", "filesystem",
            "pythonVersion", "sqliteVersion", "benchmarkCommitSha", "benchmarkReportSha256",
        }
        if not isinstance(template, dict) or not required_inventory_fields.issubset(template):
            errors.append("host inventory template is missing required fields")
        elif template.get("schemaVersion") != 1:
            errors.append("host inventory template schemaVersion must be 1")
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        errors.append(f"host inventory template cannot be read: {error}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())
    if errors:
        print(f"Encounter recovery capacity/SLO validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Encounter recovery capacity/SLO validation: PASS")
    print(f"Validated {len(REQUIRED_PROFILES)} unapproved planning profiles and the host inventory template.")
    print("No p95/p99/throughput SLO has been approved or fabricated.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
