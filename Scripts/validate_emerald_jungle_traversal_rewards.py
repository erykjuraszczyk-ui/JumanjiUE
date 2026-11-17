#!/usr/bin/env python3
"""Validate one-time Emerald Jungle traversal reward source data.

This checks CSV consistency against the Stage 25 traversal placement manifest.
It does not execute Unreal C++, validate a DataTable .uasset, or test runtime rewards.
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

REWARD_COLUMNS = {
    "TraversalId", "CurrencyId", "CurrencyAmount", "ItemId", "ItemAmount", "Notes"
}
ALLOWED_CURRENCIES = {"CURRENCY_EXPLORER_COINS"}
ALLOWED_ITEMS = {
    "MAT_ROOT_RESIN", "MAT_TIDE_PEARL", "MAT_STONE_CORE", "MAT_STORM_ESSENCE",
    "MAT_EMBER_ASH", "MAT_DAWN_SHARD", "MAT_SHADOW_SILK", "MAT_LIFE_BLOOM",
    "MAT_FROST_CRYSTAL", "MAT_SPIRIT_MEMORY",
}
MAX_CURRENCY_REWARD = 5000
MAX_ITEM_REWARD = 9999


def _read_rows(path: Path, required: set[str]) -> tuple[list[dict[str, str]], list[str]]:
    if not path.is_file():
        return [], [f"Missing source file: {path}"]
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            columns = set(reader.fieldnames or [])
            missing = sorted(required - columns)
            if missing:
                return [], [f"{path.name} missing columns: {', '.join(missing)}"]
            return list(reader), []
    except (OSError, UnicodeError, csv.Error) as exc:
        return [], [f"Cannot read {path.name}: {exc}"]


def _parse_int(value: str, field: str) -> int:
    try:
        return int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{field} must be an integer") from exc


def validate_project(project_root: Path) -> list[str]:
    errors: list[str] = []
    data_dir = project_root / "Data" / "Traversal"
    links, link_errors = _read_rows(
        data_dir / "EmeraldJungle_TraversalPlacement.csv", {"TraversalId"}
    )
    rewards, reward_errors = _read_rows(
        data_dir / "EmeraldJungle_TraversalRewards.csv", REWARD_COLUMNS
    )
    errors.extend(link_errors + reward_errors)
    if link_errors or reward_errors:
        return errors

    link_ids = [row.get("TraversalId", "").strip() for row in links]
    if any(not value for value in link_ids):
        errors.append("Traversal placement contains an empty TraversalId")
    if len(link_ids) != len(set(link_ids)):
        errors.append("Traversal placement contains duplicate TraversalId values")

    expected = set(link_ids)
    seen: set[str] = set()
    for line_number, row in enumerate(rewards, start=2):
        traversal_id = row.get("TraversalId", "").strip()
        if not traversal_id:
            errors.append(f"Reward line {line_number}: empty TraversalId")
            continue
        if traversal_id in seen:
            errors.append(f"Duplicate traversal reward: {traversal_id}")
            continue
        seen.add(traversal_id)

        if traversal_id not in expected:
            errors.append(f"{traversal_id}: no matching traversal placement row")

        currency_id = row.get("CurrencyId", "").strip()
        if currency_id not in ALLOWED_CURRENCIES:
            errors.append(f"{traversal_id}: unsupported CurrencyId {currency_id!r}")

        try:
            currency_amount = _parse_int(row.get("CurrencyAmount", ""), "CurrencyAmount")
            item_amount = _parse_int(row.get("ItemAmount", ""), "ItemAmount")
            if not 1 <= currency_amount <= MAX_CURRENCY_REWARD:
                errors.append(
                    f"{traversal_id}: CurrencyAmount must be within 1..{MAX_CURRENCY_REWARD}"
                )
            if not 0 <= item_amount <= MAX_ITEM_REWARD:
                errors.append(
                    f"{traversal_id}: ItemAmount must be within 0..{MAX_ITEM_REWARD}"
                )
        except ValueError as exc:
            errors.append(f"{traversal_id}: {exc}")
            continue

        item_id = row.get("ItemId", "").strip()
        if item_amount == 0 and item_id:
            errors.append(f"{traversal_id}: ItemId must be empty when ItemAmount is zero")
        elif item_amount > 0 and item_id not in ALLOWED_ITEMS:
            errors.append(f"{traversal_id}: unsupported ItemId {item_id!r}")

        if not row.get("Notes", "").strip():
            errors.append(f"{traversal_id}: Notes are required")

    missing = expected - seen
    for traversal_id in sorted(missing):
        errors.append(f"{traversal_id}: missing traversal reward row")

    extra = seen - expected
    if extra:
        errors.append(f"Unexpected traversal reward IDs: {sorted(extra)}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())

    if errors:
        print(f"Traversal reward validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Traversal reward validation: PASS")
    print("One unique, bounded reward row exists for each of the 12 traversal links.")
    print("Note: CSV PASS does not validate the Unreal DataTable asset or runtime profile transaction.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
