#!/usr/bin/env python3
"""Build a bounded, evidence-labelled capacity estimate from a Stage 39 report."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROFILE_PATH = ROOT / "Data/Encounter/EncounterRecoverySLOProfiles.csv"
MAX_UTILIZATION_FACTOR = 0.90
EXPECTED_BENCHMARK = "JUMANJI Encounter Checkpoint Recovery"


class CapacityPlanningError(ValueError):
    pass


def _finite_positive(value: object, label: str) -> float:
    if type(value) not in (int, float) or not math.isfinite(float(value)) or float(value) <= 0:
        raise CapacityPlanningError(f"{label} must be a finite positive number")
    return float(value)


def _load_profile(path: Path, profile_name: str) -> dict[str, str]:
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            rows = [row for row in reader if (row.get("Profile") or "").strip() == profile_name]
    except (OSError, UnicodeError, csv.Error) as error:
        raise CapacityPlanningError(f"Cannot read SLO profile matrix: {error}") from None
    if len(rows) != 1:
        raise CapacityPlanningError(f"Expected exactly one SLO profile named {profile_name}")
    return rows[0]


def _read_report(path: Path) -> tuple[dict[str, Any], str]:
    try:
        raw = path.read_bytes()
        report = json.loads(raw.decode("utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise CapacityPlanningError(f"Cannot read benchmark report: {error}") from None
    if not isinstance(report, dict):
        raise CapacityPlanningError("Benchmark report root must be a JSON object")
    return report, hashlib.sha256(raw).hexdigest()


def _load_inventory(path: Path | None) -> tuple[dict[str, Any] | None, str]:
    if path is None:
        return None, "NOT_PROVIDED"
    try:
        raw = path.read_bytes()
        inventory = json.loads(raw.decode("utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise CapacityPlanningError(f"Cannot read host inventory: {error}") from None
    if not isinstance(inventory, dict) or type(inventory.get("schemaVersion")) is not int or inventory.get("schemaVersion") != 1:
        raise CapacityPlanningError("Host inventory must be a schemaVersion 1 JSON object")
    required = (
        "hostId", "hostRole", "measuredAtUtc", "osName", "osVersion", "cpuModel",
        "physicalCoreCount", "logicalProcessorCount", "memoryGiB", "storageModel",
        "storageType", "filesystem", "pythonVersion", "sqliteVersion",
        "benchmarkCommitSha", "benchmarkReportSha256",
    )
    missing = [key for key in required if inventory.get(key) in (None, "")]
    if missing:
        raise CapacityPlanningError("Host inventory is incomplete: " + ", ".join(missing))
    placeholder_values = ("REPLACE_", "TODO", "UNKNOWN")
    if any(
        isinstance(inventory[key], str)
        and inventory[key].strip().upper().startswith(placeholder_values)
        for key in required
    ):
        raise CapacityPlanningError("Host inventory still contains placeholder values")
    sensitive_names = ("hostname", "ipaddress", "profileid", "token", "password", "privatekey", "secret")
    if any(any(sensitive in str(key).casefold() for sensitive in sensitive_names) for key in inventory):
        raise CapacityPlanningError("Host inventory contains a forbidden identity or secret field")
    try:
        measured = datetime.fromisoformat(str(inventory["measuredAtUtc"]).replace("Z", "+00:00"))
        if measured.tzinfo is None:
            raise ValueError
    except ValueError:
        raise CapacityPlanningError("Host inventory measuredAtUtc must be timezone-aware ISO-8601") from None
    if not re.fullmatch(r"[A-Za-z0-9_-]{3,64}", str(inventory["hostId"])):
        raise CapacityPlanningError("Host inventory hostId must be a non-identifying 3-64 character label")
    if not re.fullmatch(r"[0-9a-fA-F]{7,40}", str(inventory["benchmarkCommitSha"])):
        raise CapacityPlanningError("Host inventory benchmarkCommitSha must be a hexadecimal commit SHA")
    if not re.fullmatch(r"[0-9a-fA-F]{64}", str(inventory["benchmarkReportSha256"])):
        raise CapacityPlanningError("Host inventory benchmarkReportSha256 must be a 64-character SHA-256")
    for key in ("physicalCoreCount", "logicalProcessorCount"):
        if type(inventory[key]) is not int or inventory[key] <= 0:
            raise CapacityPlanningError(f"Host inventory {key} must be a positive integer")
    if type(inventory["memoryGiB"]) not in (int, float) or not math.isfinite(float(inventory["memoryGiB"])) or inventory["memoryGiB"] <= 0:
        raise CapacityPlanningError("Host inventory memoryGiB must be positive")
    return inventory, hashlib.sha256(raw).hexdigest()


def build_capacity_plan(
    report: dict[str, Any],
    profile: dict[str, str],
    report_sha256: str,
    inventory: dict[str, Any] | None = None,
    inventory_sha256: str | None = None,
) -> dict[str, Any]:
    if report.get("benchmark") != EXPECTED_BENCHMARK or report.get("stage") != 39:
        raise CapacityPlanningError("Only a Stage 39 checkpoint recovery benchmark report is accepted")
    if report.get("failed_http_requests") != 0 or report.get("worker_setup_failures") != 0:
        raise CapacityPlanningError("Benchmark contains failures; it is not eligible as capacity evidence")
    duration = _finite_positive(report.get("measurement_window_seconds"), "measurement_window_seconds")
    workers = report.get("workers")
    if type(workers) is not int or not 1 <= workers <= 64:
        raise CapacityPlanningError("workers must be an integer from 1 to 64")
    pairs = report.get("completed_write_read_pairs")
    if type(pairs) is not int or pairs < 1:
        raise CapacityPlanningError("completed_write_read_pairs must be a positive integer")
    environment = report.get("environment")
    if not isinstance(environment, dict):
        raise CapacityPlanningError("Benchmark environment metadata is missing")
    required_environment = ("python", "platform", "processor", "cpu_count", "sqlite_version", "database_journal_mode")
    if any(environment.get(key) in (None, "") for key in required_environment):
        raise CapacityPlanningError("Benchmark host fingerprint is incomplete")
    if str(environment["database_journal_mode"]).upper() != str(profile.get("RequiredJournalMode", "WAL")).upper():
        raise CapacityPlanningError("Benchmark journal mode does not match the profile")
    if inventory:
        if inventory.get("hostRole") != "single-host-checkpoint-prototype":
            raise CapacityPlanningError("Stage 39 SQLite loopback evidence must be tied to a single-host prototype inventory")
        if inventory.get("benchmarkReportSha256", "").casefold() != report_sha256.casefold():
            raise CapacityPlanningError("Host inventory benchmarkReportSha256 does not match the supplied report")
        if inventory.get("pythonVersion") != environment.get("python"):
            raise CapacityPlanningError("Host inventory Python version does not match the benchmark report")
        if inventory.get("sqliteVersion") != environment.get("sqlite_version"):
            raise CapacityPlanningError("Host inventory SQLite version does not match the benchmark report")

    try:
        minimum_duration = int(profile["MinimumDurationSeconds"])
        minimum_workers = int(profile["MinimumWorkers"])
        minimum_pairs = int(profile["MinimumCompletedPairs"])
        minimum_samples = int(profile["MinimumLatencySamplesPerOperation"])
        cadence = int(profile["CheckpointCadenceSeconds"])
        utilization = float(profile["UtilizationFactor"])
    except (KeyError, TypeError, ValueError):
        raise CapacityPlanningError("SLO profile contains invalid numeric planning requirements") from None
    if not 0 < utilization <= MAX_UTILIZATION_FACTOR:
        raise CapacityPlanningError(f"UtilizationFactor must not exceed {MAX_UTILIZATION_FACTOR}")
    if duration < minimum_duration:
        raise CapacityPlanningError(f"Benchmark duration {duration:g}s is below profile minimum {minimum_duration}s")
    if workers < minimum_workers:
        raise CapacityPlanningError(f"Benchmark worker count {workers} is below profile minimum {minimum_workers}")
    if pairs < minimum_pairs:
        raise CapacityPlanningError(f"Completed pair count {pairs} is below profile minimum {minimum_pairs}")

    operations = report.get("operations")
    if not isinstance(operations, dict):
        raise CapacityPlanningError("Per-operation latency metrics are missing")
    measured: dict[str, dict[str, float | int]] = {}
    for operation_name in ("checkpoint_write", "checkpoint_read"):
        operation = operations.get(operation_name)
        if not isinstance(operation, dict):
            raise CapacityPlanningError(f"Missing operation metrics: {operation_name}")
        if operation.get("failures") != 0 or operation.get("successes", 0) < 1:
            raise CapacityPlanningError(f"{operation_name} contains errors or has no successes")
        sample_count = operation.get("latency_sample_count")
        if type(sample_count) is not int or sample_count < minimum_samples:
            raise CapacityPlanningError(
                f"{operation_name} has {sample_count} latency samples; profile requires {minimum_samples}"
            )
        measured[operation_name] = {
            "p50_ms": _finite_positive(operation.get("p50_ms"), f"{operation_name}.p50_ms"),
            "p95_ms": _finite_positive(operation.get("p95_ms"), f"{operation_name}.p95_ms"),
            "p99_ms": _finite_positive(operation.get("p99_ms"), f"{operation_name}.p99_ms"),
            "latency_sample_count": sample_count,
        }
        if measured[operation_name]["p95_ms"] > measured[operation_name]["p99_ms"]:
            raise CapacityPlanningError(f"{operation_name} p95 latency exceeds p99 latency")

    throughput = report.get("throughput")
    if not isinstance(throughput, dict):
        raise CapacityPlanningError("Benchmark throughput metrics are missing")
    pair_rate = _finite_positive(
        throughput.get("successful_checkpoint_pairs_per_second"),
        "successful_checkpoint_pairs_per_second",
    )

    # This is deliberately a planning envelope, not a measured multi-host capacity.
    # It never extrapolates beyond the concurrency actually tested in Stage 39.
    write_p99 = float(measured["checkpoint_write"]["p99_ms"])
    read_p99 = float(measured["checkpoint_read"]["p99_ms"])
    component_p99_sum = write_p99 + read_p99
    cadence_ms = cadence * 1000.0
    throughput_limit = pair_rate * cadence
    latency_limit = cadence_ms / component_p99_sum
    tested_worker_limit = float(workers)
    raw_limit = min(throughput_limit, latency_limit, tested_worker_limit)
    safe_encounters = max(0, math.floor(raw_limit * utilization))

    status = str(profile.get("Status", "UNBASELINED")).upper()
    if status not in {"UNBASELINED", "PROPOSED", "APPROVED"}:
        raise CapacityPlanningError(f"SLO profile status {status} is not eligible for planning")
    slo_assessment = "NOT_ASSESSED"
    observed_against_targets: dict[str, Any] = {}
    if status == "APPROVED":
        budget_mapping = {
            "TargetWriteP95Ms": ("checkpoint_write", "p95_ms"),
            "TargetWriteP99Ms": ("checkpoint_write", "p99_ms"),
            "TargetReadP95Ms": ("checkpoint_read", "p95_ms"),
            "TargetReadP99Ms": ("checkpoint_read", "p99_ms"),
            "TargetPairsPerSecond": ("throughput", "successful_checkpoint_pairs_per_second"),
        }
        passed = True
        for target_field, (source_name, metric_name) in budget_mapping.items():
            try:
                target = float(profile[target_field])
            except (KeyError, TypeError, ValueError):
                raise CapacityPlanningError(f"Approved SLO profile is missing {target_field}") from None
            actual = (
                pair_rate if source_name == "throughput"
                else float(measured[source_name][metric_name])
            )
            field_passed = actual >= target if source_name == "throughput" else actual <= target
            observed_against_targets[target_field] = {"target": target, "observed": actual, "passed": field_passed}
            passed = passed and field_passed
        slo_assessment = "PASS" if passed else "FAIL"

    plan = {
        "planner": "JUMANJI Encounter Recovery Capacity Planner",
        "plannerVersion": 1,
        "createdAtUtc": datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z"),
        "profile": profile.get("Profile", ""),
        "profileStatus": status,
        "sloAssessment": slo_assessment,
        "sloTargetsAreApproved": status == "APPROVED",
        "evidenceClass": "STAGE39_SINGLE_HOST_SQLITE_WAL_LOOPBACK",
        "scope": "single-host planning estimate only; not distributed, multi-region or production SLO evidence",
        "benchmark": {
            "stage": 39,
            "measurementWindowSeconds": duration,
            "testedWorkers": workers,
            "completedWriteReadPairs": pairs,
            "pairRatePerSecond": pair_rate,
            "operations": measured,
            "environment": {key: environment[key] for key in required_environment},
            "databaseJournalMode": environment["database_journal_mode"],
            "failedHttpRequests": 0,
            "workerSetupFailures": 0,
            "reportSha256": report_sha256,
        },
        "planningAssumptions": {
            "checkpointCadenceSeconds": cadence,
            "utilizationFactor": utilization,
            "noExtrapolationBeyondTestedWorkers": True,
            "operationP99SumMs": round(component_p99_sum, 3),
            "operationP99SumIsPlanningProxyNotMeasuredPairPercentile": True,
        },
        "capacityEnvelope": {
            "throughputLimitedEncounters": round(throughput_limit, 3),
            "latencyLimitedEncounters": round(latency_limit, 3),
            "testedWorkerLimit": workers,
            "projectedSafeConcurrentEncounters": safe_encounters,
            "formula": "max(0, floor(min(pairRate * checkpointCadence, cadenceMs / (writeP99 + readP99), testedWorkers) * utilizationFactor))",
            "interpretation": "Conservative planning indicator bounded by observed worker concurrency; requires repeated named-host measurements before sizing a deployment.",
        },
        "hostInventoryStatus": "COMPLETE" if inventory else "NOT_PROVIDED",
        "hostInventorySha256": inventory_sha256,
        "hostInventory": inventory,
        "observedAgainstApprovedTargets": observed_against_targets,
        "approvalGate": {
            "approved": status == "APPROVED" and bool(inventory),
            "reason": (
                "Approved SLO profile and complete host inventory supplied."
                if status == "APPROVED" and inventory
                else "No approved SLO or complete host inventory; this output is planning evidence only."
            ),
        },
    }
    return plan


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True, help="Stage 39 benchmark JSON report.")
    parser.add_argument("--profile", required=True, help="Name from EncounterRecoverySLOProfiles.csv.")
    parser.add_argument("--profiles", type=Path, default=DEFAULT_PROFILE_PATH)
    parser.add_argument("--host-inventory", type=Path)
    parser.add_argument("--output", type=Path, help="Optional JSON output path.")
    args = parser.parse_args()

    try:
        profile = _load_profile(args.profiles, args.profile)
        report, report_sha256 = _read_report(args.report)
        inventory, inventory_sha256 = _load_inventory(args.host_inventory)
        plan = build_capacity_plan(report, profile, report_sha256, inventory, inventory_sha256)
    except (OSError, CapacityPlanningError, ValueError) as error:
        print(f"Encounter recovery capacity planning: REJECTED ({error})", file=sys.stderr)
        return 1

    encoded = json.dumps(plan, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    print(
        f"Capacity plan generated: {plan['capacityEnvelope']['projectedSafeConcurrentEncounters']} "
        f"safe planning slots; SLO assessment={plan['sloAssessment']}.",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
