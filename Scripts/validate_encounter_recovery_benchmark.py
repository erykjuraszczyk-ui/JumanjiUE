#!/usr/bin/env python3
"""Validate Stage 39 recovery load/soak benchmark profiles and test wiring."""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MATRIX_PATH = Path("Data/Encounter/EncounterRecoveryBenchmarkProfiles.csv")
COLUMNS = (
    "Profile",
    "DurationSeconds",
    "Workers",
    "LeaseTtlSeconds",
    "WarmupIterations",
    "UseCase",
    "Acceptance",
)
REQUIRED_PROFILES = {
    "Smoke": (3, 2, 120, 1),
    "Baseline": (60, 4, 120, 3),
    "Load": (180, 12, 120, 3),
    "Soak30": (1800, 8, 120, 3),
    "Soak60": (3600, 8, 120, 3),
}
SOURCE_REQUIREMENTS = {
    "Scripts/benchmark_encounter_checkpoint_recovery.py": (
        "EncounterCheckpointBackend",
        "make_handler",
        "Ed25519PrivateKey.generate()",
        "127.0.0.1",
        "checkpoint_write",
        "checkpoint_read",
        "lease_renew",
        "p50_ms",
        "p95_ms",
        "p99_ms",
        "samples_ms",
        "sample_cap",
        "TemporaryDirectory",
    ),
    "Scripts/Tests/test_encounter_recovery_benchmark.py": (
        "test_percentile_calculation_uses_nearest_rank",
        "test_bounded_latency_reservoir_tracks_all_requests",
        "test_real_http_sqlite_checkpoint_smoke",
        "test_invalid_benchmark_limits_are_rejected",
        "leaseToken",
        "private_key",
    ),
    "Docs/STAGE_39_RECOVERY_LOAD_SOAK_PERFORMANCE_BENCHMARKS.md": (
        "Baseline",
        "Load",
        "Soak30",
        "Soak60",
        "p50",
        "p95",
        "p99",
        "nie jest wynikiem multi-region",
        "Nie wpisujemy fikcyjnych wartości",
    ),
    ".github/workflows/encounter-recovery-benchmarks.yml": (
        "benchmark_encounter_checkpoint_recovery.py",
        "test_encounter_recovery_benchmark.py",
        "test_encounter_checkpoint_service.py",
        "test_encounter_checkpoint_chaos.py",
        "validate_encounter_checkpoint_contract.py",
        "validate_encounter_recovery_chaos.py",
    ),
    "README.md": (
        "Stage 39 — Recovery Load, Soak & Performance Benchmarks",
        "Docs/STAGE_39_RECOVERY_LOAD_SOAK_PERFORMANCE_BENCHMARKS.md",
        "Data/Encounter/EncounterRecoveryBenchmarkProfiles.csv",
        "Scripts/benchmark_encounter_checkpoint_recovery.py",
    ),
}


def validate_matrix(path: Path) -> list[str]:
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            missing = [column for column in COLUMNS if column not in (reader.fieldnames or [])]
            if missing:
                return [f"benchmark matrix missing columns: {', '.join(missing)}"]
            rows = list(reader)
    except (OSError, UnicodeError, csv.Error) as error:
        return [f"benchmark matrix cannot be read: {error}"]

    errors: list[str] = []
    by_profile: dict[str, dict[str, str]] = {}
    for line_number, row in enumerate(rows, start=2):
        name = row.get("Profile", "").strip()
        if not name:
            errors.append(f"row {line_number}: Profile is empty")
            continue
        if name in by_profile:
            errors.append(f"row {line_number}: duplicate Profile {name}")
            continue
        by_profile[name] = row
        for field in ("UseCase", "Acceptance"):
            if not row.get(field, "").strip():
                errors.append(f"{name}: {field} is required")
        try:
            duration = int(row["DurationSeconds"])
            workers = int(row["Workers"])
            lease_ttl = int(row["LeaseTtlSeconds"])
            warmup = int(row["WarmupIterations"])
        except (KeyError, ValueError):
            errors.append(f"{name}: duration/workers/lease TTL/warmup must be integers")
            continue
        if not 1 <= duration <= 3600:
            errors.append(f"{name}: DurationSeconds must be 1..3600")
        if not 1 <= workers <= 64:
            errors.append(f"{name}: Workers must be 1..64")
        if not 15 <= lease_ttl <= 120:
            errors.append(f"{name}: LeaseTtlSeconds must be 15..120")
        if not 0 <= warmup <= 10:
            errors.append(f"{name}: WarmupIterations must be 0..10")

    for name, expected in REQUIRED_PROFILES.items():
        row = by_profile.get(name)
        if row is None:
            errors.append(f"benchmark matrix missing required profile {name}")
            continue
        actual = tuple(
            int(row[field])
            for field in ("DurationSeconds", "Workers", "LeaseTtlSeconds", "WarmupIterations")
        )
        if actual != expected:
            errors.append(f"{name}: expected {expected}, found {actual}")
    return errors


def validate_sources(root: Path) -> list[str]:
    errors: list[str] = []
    for relative, markers in SOURCE_REQUIREMENTS.items():
        try:
            content = (root / relative).read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            errors.append(f"{relative}: cannot read: {error}")
            continue
        for marker in markers:
            if marker not in content:
                errors.append(f"{relative}: missing Stage 39 requirement {marker!r}")
    return errors


def validate_project(root: Path) -> list[str]:
    return validate_sources(root) + validate_matrix(root / MATRIX_PATH)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())
    if errors:
        print(f"Encounter recovery benchmark validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Encounter recovery benchmark validation: PASS")
    print(f"Validated {len(REQUIRED_PROFILES)} benchmark profiles and {len(SOURCE_REQUIREMENTS)} source/workflow contracts.")
    print("This checks benchmark wiring; actual load results must come from a measured run.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
