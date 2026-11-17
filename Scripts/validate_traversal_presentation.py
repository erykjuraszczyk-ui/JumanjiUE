#!/usr/bin/env python3
"""Validate the source CSV for the JUMANJI traversal presentation DataTable."""

from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path

EXPECTED_TYPES = {"ContextJump", "Climb", "Zipline", "Lift"}
REQUIRED_COLUMNS = {
    "Name",
    "TraversalType",
    "DisplayName",
    "PromptText",
    "InProgressText",
    "CompletedText",
    "InterruptedText",
    "PromptIcon",
    "StartMontage",
    "LoopMontage",
    "EndMontage",
    "StartSound",
    "EndSound",
    "StartEffect",
    "EndEffect",
    "bAllowCameraLook",
    "bShowProgress",
    "PromptHoldSeconds",
}
TEXT_COLUMNS = (
    "DisplayName",
    "PromptText",
    "InProgressText",
    "CompletedText",
    "InterruptedText",
)
ASSET_COLUMNS = (
    "PromptIcon",
    "StartMontage",
    "LoopMontage",
    "EndMontage",
    "StartSound",
    "EndSound",
    "StartEffect",
    "EndEffect",
)


def validate_manifest(path: Path) -> list[str]:
    errors: list[str] = []
    if not path.is_file():
        return [f"missing manifest: {path}"]

    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            headers = set(reader.fieldnames or [])
            missing_columns = sorted(REQUIRED_COLUMNS - headers)
            if missing_columns:
                return [f"missing CSV columns: {', '.join(missing_columns)}"]

            rows = list(reader)
    except (OSError, csv.Error) as exc:
        return [f"cannot read manifest: {exc}"]

    if len(rows) != 4:
        errors.append(f"expected exactly 4 starter rows, found {len(rows)}")

    seen_ids: set[str] = set()
    seen_types: set[str] = set()

    for line_number, row in enumerate(rows, start=2):
        row_id = (row.get("Name") or "").strip()
        traversal_type = (row.get("TraversalType") or "").strip()

        if not row_id:
            errors.append(f"line {line_number}: Name is empty")
        elif row_id in seen_ids:
            errors.append(f"line {line_number}: duplicate row name {row_id}")
        seen_ids.add(row_id)

        if traversal_type not in EXPECTED_TYPES:
            errors.append(f"line {line_number}: unsupported TraversalType {traversal_type!r}")
        elif traversal_type in seen_types:
            errors.append(f"line {line_number}: duplicate starter type {traversal_type}")
        seen_types.add(traversal_type)

        for column in TEXT_COLUMNS:
            if not (row.get(column) or "").strip():
                errors.append(f"line {line_number}: {column} is empty")

        for column in ("bAllowCameraLook", "bShowProgress"):
            value = (row.get(column) or "").strip().lower()
            if value not in {"true", "false"}:
                errors.append(f"line {line_number}: {column} must be True or False")

        try:
            hold_seconds = float((row.get("PromptHoldSeconds") or "").strip())
            if not math.isfinite(hold_seconds) or not 0.2 <= hold_seconds <= 3.0:
                errors.append(f"line {line_number}: PromptHoldSeconds must be in [0.2, 3.0]")
        except ValueError:
            errors.append(f"line {line_number}: PromptHoldSeconds is not a number")

        for column in ASSET_COLUMNS:
            asset_path = (row.get(column) or "").strip()
            if asset_path and not asset_path.startswith(("/Game/", "/Engine/")):
                errors.append(
                    f"line {line_number}: {column} must be a soft Unreal asset path or blank"
                )

    missing_types = sorted(EXPECTED_TYPES - seen_types)
    if missing_types:
        errors.append(f"missing traversal types: {', '.join(missing_types)}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("Data/Traversal/TraversalPresentationManifest.csv"),
    )
    args = parser.parse_args()

    errors = validate_manifest(args.manifest)
    if errors:
        print("Traversal presentation manifest: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Traversal presentation manifest: PASS (4 unique traversal types)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
