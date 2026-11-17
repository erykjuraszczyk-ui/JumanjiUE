#!/usr/bin/env python3
"""Validate Emerald Jungle world-encounter reward source data.

This validates the source CSV and the expected Stage 27 encounter contract.
It does not compile Unreal C++, import a DataTable asset, or exercise multiplayer runtime.
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

EXPECTED_ENCOUNTER_ID = "ENCOUNTER_EMERALD_JUNGLE_CITADEL"
EXPECTED_CHAPTER_ID = "EMERALD_JUNGLE_NINE_SEALS"
REQUIRED_COLUMNS = {
    "EncounterId",
    "RequiredChapterId",
    "AccountExperienceReward",
    "ActiveCharacterExperienceReward",
    "CurrencyId",
    "CurrencyAmount",
    "ItemId",
    "ItemAmount",
    "Notes",
}
ALLOWED_CURRENCIES = {
    "CURRENCY_EXPLORER_COINS",
    "CURRENCY_BOARD_SEALS",
    "CURRENCY_ECHO_CRYSTALS",
}
ALLOWED_ITEMS = {
    "MAT_ROOT_RESIN",
    "MAT_TIDE_PEARL",
    "MAT_STONE_CORE",
    "MAT_STORM_ESSENCE",
    "MAT_EMBER_ASH",
    "MAT_DAWN_SHARD",
    "MAT_SHADOW_SILK",
    "MAT_LIFE_BLOOM",
    "MAT_FROST_CRYSTAL",
    "MAT_SPIRIT_MEMORY",
}
MAX_EXPERIENCE = 1_000_000
MAX_CURRENCY = 2_000_000_000
MAX_ITEM_AMOUNT = 9_999


def _integer(row: dict[str, str], field: str, errors: list[str], line: int) -> int | None:
    try:
        return int(row.get(field, "").strip())
    except (TypeError, ValueError):
        errors.append(f"line {line}: {field} must be an integer")
        return None


def validate_project(project_root: Path) -> list[str]:
    errors: list[str] = []
    path = project_root / "Data" / "Progression" / "WorldEncounterRewards.csv"

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

    if len(rows) != 1:
        errors.append(f"Expected exactly one encounter reward row, found {len(rows)}")

    seen: set[str] = set()
    for line, row in enumerate(rows, start=2):
        encounter_id = row.get("EncounterId", "").strip()
        chapter_id = row.get("RequiredChapterId", "").strip()

        if encounter_id != EXPECTED_ENCOUNTER_ID:
            errors.append(
                f"line {line}: EncounterId must be {EXPECTED_ENCOUNTER_ID!r}"
            )
        if encounter_id in seen:
            errors.append(f"line {line}: duplicate EncounterId {encounter_id!r}")
        seen.add(encounter_id)

        if chapter_id != EXPECTED_CHAPTER_ID:
            errors.append(
                f"{encounter_id}: RequiredChapterId must be {EXPECTED_CHAPTER_ID!r}"
            )

        account_xp = _integer(row, "AccountExperienceReward", errors, line)
        spirit_xp = _integer(row, "ActiveCharacterExperienceReward", errors, line)
        currency_amount = _integer(row, "CurrencyAmount", errors, line)
        item_amount = _integer(row, "ItemAmount", errors, line)

        for field, value in (
            ("AccountExperienceReward", account_xp),
            ("ActiveCharacterExperienceReward", spirit_xp),
        ):
            if value is not None and not 0 <= value <= MAX_EXPERIENCE:
                errors.append(f"{encounter_id}: {field} must be within 0..{MAX_EXPERIENCE}")

        currency_id = row.get("CurrencyId", "").strip()
        if currency_id not in ALLOWED_CURRENCIES:
            errors.append(f"{encounter_id}: unsupported CurrencyId {currency_id!r}")
        if currency_amount is not None and not 0 <= currency_amount <= MAX_CURRENCY:
            errors.append(f"{encounter_id}: CurrencyAmount must be within 0..{MAX_CURRENCY}")
        if currency_amount == 0 and currency_id:
            errors.append(f"{encounter_id}: CurrencyId must be empty when CurrencyAmount is zero")

        item_id = row.get("ItemId", "").strip()
        if item_amount is not None and not 0 <= item_amount <= MAX_ITEM_AMOUNT:
            errors.append(f"{encounter_id}: ItemAmount must be within 0..{MAX_ITEM_AMOUNT}")
        if item_amount == 0 and item_id:
            errors.append(f"{encounter_id}: ItemId must be empty when ItemAmount is zero")
        elif item_amount is not None and item_amount > 0 and item_id not in ALLOWED_ITEMS:
            errors.append(f"{encounter_id}: unsupported ItemId {item_id!r}")

        rewards_total = any(
            value is not None and value > 0
            for value in (account_xp, spirit_xp, currency_amount, item_amount)
        )
        if not rewards_total:
            errors.append(f"{encounter_id}: reward row must grant at least one positive reward")

        if not row.get("Notes", "").strip():
            errors.append(f"{encounter_id}: Notes are required")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())

    if errors:
        print(f"Emerald Jungle encounter reward validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Emerald Jungle encounter reward validation: PASS")
    print("The configured Citadel encounter has one bounded reward row.")
    print("Note: CSV PASS does not validate a DataTable .uasset or runtime profile transaction.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
