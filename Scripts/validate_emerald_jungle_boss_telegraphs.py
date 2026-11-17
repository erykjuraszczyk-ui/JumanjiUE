#!/usr/bin/env python3
"""Validate the Emerald Jungle boss telegraph source manifest.

This checks authoring data only. It does not compile Unreal C++, validate
Blueprint attack graphs, import assets, or test network replication.
"""
from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from pathlib import Path

REQUIRED_COLUMNS = {
    "AttackId",
    "DisplayName",
    "Phase",
    "Pattern",
    "WarningSeconds",
    "ActiveSeconds",
    "RadiusCm",
    "LengthCm",
    "WidthCm",
    "Notes",
}
ALLOWED_PATTERNS = {"Radial", "Line", "Cone", "Focused"}
EXPECTED_PHASES = {1, 2, 3}
ATTACK_ID_PATTERN = re.compile(r"^[A-Z][A-Z0-9_]{2,63}$")
NUMERIC_BOUNDS = {
    "WarningSeconds": (0.25, 8.0),
    "ActiveSeconds": (0.1, 6.0),
    "RadiusCm": (25.0, 2500.0),
    "LengthCm": (0.0, 4000.0),
    "WidthCm": (0.0, 1500.0),
}


def _number(row: dict[str, str], field: str, errors: list[str], line: int) -> float | None:
    raw = row.get(field, "").strip()
    try:
        value = float(raw)
    except (TypeError, ValueError):
        errors.append(f"line {line}: {field} must be a number")
        return None
    if not math.isfinite(value):
        errors.append(f"line {line}: {field} must be finite")
        return None
    low, high = NUMERIC_BOUNDS[field]
    if not low <= value <= high:
        errors.append(f"line {line}: {field} must be within {low}..{high}")
        return None
    return value


def validate_project(project_root: Path) -> list[str]:
    errors: list[str] = []
    path = project_root / "Data" / "Encounter" / "EmeraldJungleBossTelegraphs.csv"
    if not path.is_file():
        return [f"Missing source file: {path}"]

    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            columns = set(reader.fieldnames or [])
            rows = list(reader)
    except (OSError, UnicodeError, csv.Error) as exc:
        return [f"Cannot read {path.name}: {exc}"]

    missing = sorted(REQUIRED_COLUMNS - columns)
    if missing:
        return [f"Missing required columns: {', '.join(missing)}"]
    if not rows:
        return ["Boss telegraph manifest must contain at least one attack row"]

    seen_ids: set[str] = set()
    seen_phases: set[int] = set()

    for line, row in enumerate(rows, start=2):
        attack_id = row.get("AttackId", "").strip()
        display_name = row.get("DisplayName", "").strip()
        notes = row.get("Notes", "").strip()

        if not ATTACK_ID_PATTERN.fullmatch(attack_id):
            errors.append(f"line {line}: AttackId must be 3..64 uppercase letters/digits/underscores")
        if attack_id in seen_ids:
            errors.append(f"line {line}: duplicate AttackId {attack_id!r}")
        seen_ids.add(attack_id)

        if not display_name:
            errors.append(f"line {line}: DisplayName is required")
        if not notes:
            errors.append(f"line {line}: Notes are required")

        try:
            phase = int(row.get("Phase", "").strip())
        except (TypeError, ValueError):
            errors.append(f"line {line}: Phase must be an integer from 1 to 3")
            phase = None
        if phase is not None:
            if phase not in EXPECTED_PHASES:
                errors.append(f"line {line}: Phase must be one of 1, 2 or 3")
            else:
                seen_phases.add(phase)

        pattern = row.get("Pattern", "").strip()
        if pattern not in ALLOWED_PATTERNS:
            errors.append(
                f"line {line}: Pattern must be one of {', '.join(sorted(ALLOWED_PATTERNS))}"
            )

        for field in NUMERIC_BOUNDS:
            _number(row, field, errors, line)

    missing_phases = sorted(EXPECTED_PHASES - seen_phases)
    if missing_phases:
        errors.append(
            "Manifest must contain at least one attack for each boss phase; "
            f"missing phase(s): {', '.join(map(str, missing_phases))}"
        )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())

    if errors:
        print(f"Emerald Jungle boss telegraph validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Emerald Jungle boss telegraph validation: PASS")
    print("Unique attack IDs, all three phases, patterns and timing/shape bounds are valid.")
    print("Note: CSV PASS does not validate Blueprint execution, Niagara assets or multiplayer runtime.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
