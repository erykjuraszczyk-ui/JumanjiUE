#!/usr/bin/env python3
"""Validate the Stage 23 Ability HUD device-QA matrix (not device execution)."""

from __future__ import annotations

import csv
import math
import sys
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MATRIX = ROOT / "Data" / "UI" / "AbilityHUD_DeviceQAMatrix.csv"

REQUIRED_COLUMNS = {
    "TestId",
    "DeviceProfile",
    "Orientation",
    "Viewport",
    "HUDScale",
    "TouchTargetScale",
    "HighContrast",
    "ReduceMotion",
    "SafeAreaCase",
    "ExpectedResult",
    "ExecutionStatus",
}
VALID_ORIENTATIONS = {"Landscape", "Portrait"}
VALID_SAFE_AREA = {"NoCutout", "CameraCutout", "GestureInset"}
VALID_STATUSES = {"NOT_RUN", "PASS", "FAIL", "BLOCKED"}


def validate_matrix(path: Path = DEFAULT_MATRIX) -> list[str]:
    errors: list[str] = []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            columns = set(reader.fieldnames or [])
            missing = sorted(REQUIRED_COLUMNS - columns)
            if missing:
                return [f"Missing required columns: {', '.join(missing)}"]
            rows = list(reader)
    except OSError as exc:
        return [f"Cannot read QA matrix: {exc}"]

    if len(rows) < 10:
        errors.append(f"Expected at least 10 QA cases, found {len(rows)}")

    seen_ids: set[str] = set()
    for index, row in enumerate(rows, start=2):
        test_id = (row.get("TestId") or "").strip()
        if not test_id:
            errors.append(f"Row {index}: TestId is empty")
        elif test_id in seen_ids:
            errors.append(f"Row {index}: duplicate TestId '{test_id}'")
        seen_ids.add(test_id)

        orientation = (row.get("Orientation") or "").strip()
        if orientation not in VALID_ORIENTATIONS:
            errors.append(f"Row {index}: invalid Orientation '{orientation}'")

        safe_area = (row.get("SafeAreaCase") or "").strip()
        if safe_area not in VALID_SAFE_AREA:
            errors.append(f"Row {index}: invalid SafeAreaCase '{safe_area}'")

        status = (row.get("ExecutionStatus") or "").strip()
        if status not in VALID_STATUSES:
            errors.append(f"Row {index}: invalid ExecutionStatus '{status}'")

        for column, minimum, maximum in (
            ("HUDScale", 0.75, 1.35),
            ("TouchTargetScale", 0.85, 1.50),
        ):
            try:
                value = float(row.get(column, ""))
                if not math.isfinite(value) or not minimum <= value <= maximum:
                    errors.append(
                        f"Row {index}: {column} must be finite and within {minimum}..{maximum}"
                    )
            except (TypeError, ValueError):
                errors.append(f"Row {index}: {column} is not numeric")

        for column in ("HighContrast", "ReduceMotion"):
            if (row.get(column) or "").strip().lower() not in {"true", "false"}:
                errors.append(f"Row {index}: {column} must be true or false")

        if not (row.get("ExpectedResult") or "").strip():
            errors.append(f"Row {index}: ExpectedResult is empty")

    return errors


def main(argv: Iterable[str] | None = None) -> int:
    args = list(argv if argv is not None else sys.argv[1:])
    path = Path(args[0]) if args else DEFAULT_MATRIX
    errors = validate_matrix(path)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print(f"Ability HUD device-QA matrix valid: {path}")
    print("Note: validation checks matrix structure only; it does not execute device tests.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
