#!/usr/bin/env python3
"""Validate Emerald Jungle traversal link and lift-platform source manifests.

This validates CSV/JSON authoring data only. It does not inspect Unreal assets,
compile C++, build World Partition navigation, or prove runtime collision.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from typing import Any

LINK_TYPES = {"ContextJump", "Climb", "Zipline", "Lift"}
EXPECTED_TYPE_COUNTS = {"ContextJump": 4, "Climb": 3, "Zipline": 3, "Lift": 2}
LINK_COLUMNS = {
    "TraversalId", "TraversalType", "DisplayName", "RegionId", "EditorDataLayer",
    "EntryX_cm", "EntryY_cm", "EntryZ_cm", "ExitX_cm", "ExitY_cm", "ExitZ_cm",
    "Duration_s", "ArcHeight_cm", "Bidirectional", "LiftPlatformId",
    "LinkActorClass", "Notes",
}
LIFT_COLUMNS = {
    "LiftPlatformId", "RegionId", "EditorDataLayer",
    "LowerX_cm", "LowerY_cm", "LowerZ_cm",
    "UpperX_cm", "UpperY_cm", "UpperZ_cm",
    "TravelDuration_s", "MaxPassengers", "BoardingWindowAlpha",
    "PlatformActorClass", "Notes",
}


def _read_csv(path: Path, required: set[str]) -> tuple[list[dict[str, str]], list[str]]:
    errors: list[str] = []
    if not path.is_file():
        return [], [f"Missing source file: {path}"]
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            columns = set(reader.fieldnames or [])
            missing = sorted(required - columns)
            if missing:
                errors.append(f"{path.name} missing columns: {', '.join(missing)}")
                return [], errors
            return list(reader), errors
    except (OSError, UnicodeError, csv.Error) as exc:
        return [], [f"Cannot read {path.name}: {exc}"]


def _number(row: dict[str, str], name: str) -> float:
    try:
        value = float(row.get(name, ""))
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{name} is not numeric") from exc
    if not math.isfinite(value):
        raise ValueError(f"{name} must be finite")
    return value


def _bool(value: str, name: str) -> bool:
    normalized = value.strip().lower()
    if normalized in {"true", "1", "yes"}:
        return True
    if normalized in {"false", "0", "no"}:
        return False
    raise ValueError(f"{name} must be True/False")


def _point(row: dict[str, str], prefix: str) -> tuple[float, float, float]:
    return (
        _number(row, f"{prefix}X_cm"),
        _number(row, f"{prefix}Y_cm"),
        _number(row, f"{prefix}Z_cm"),
    )


def _distance(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def validate_project(project_root: Path) -> list[str]:
    errors: list[str] = []
    traversal_dir = project_root / "Data" / "Traversal"
    world_dir = project_root / "Data" / "World"

    links, link_errors = _read_csv(
        traversal_dir / "EmeraldJungle_TraversalPlacement.csv", LINK_COLUMNS
    )
    lifts, lift_errors = _read_csv(
        traversal_dir / "EmeraldJungle_LiftPlatformManifest.csv", LIFT_COLUMNS
    )
    regions, region_errors = _read_csv(
        world_dir / "DT_Jumanji_WorldRegions.csv", {"Name", "RegionId"}
    )
    region_volumes, volume_errors = _read_csv(
        world_dir / "EmeraldJungle_GreyboxPlacement.csv",
        {"ActorId", "ActorType", "EditorDataLayer"},
    )
    errors.extend(link_errors + lift_errors + region_errors + volume_errors)

    manifest_path = traversal_dir / "EmeraldJungle_TraversalManifest.json"
    try:
        manifest: dict[str, Any] = json.loads(manifest_path.read_text(encoding="utf-8"))
        if not isinstance(manifest, dict):
            errors.append("Traversal manifest must be a JSON object")
            manifest = {}
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        errors.append(f"Cannot read traversal manifest JSON: {exc}")
        manifest = {}

    known_regions = {
        row.get("RegionId", "").strip()
        for row in regions
        if row.get("RegionId", "").strip()
    }
    region_layers = {
        row.get("ActorId", "").strip(): row.get("EditorDataLayer", "").strip()
        for row in region_volumes
        if row.get("ActorType") == "RegionVolume"
    }

    if len(links) != 12:
        errors.append(f"Expected 12 traversal links, found {len(links)}")
    if len(lifts) != 2:
        errors.append(f"Expected 2 lift platforms, found {len(lifts)}")

    link_by_id: dict[str, dict[str, str]] = {}
    type_counts = {kind: 0 for kind in LINK_TYPES}
    lift_references: set[str] = set()

    expected_actor_class = {
        "ContextJump": "BP_TraversalLink_ContextJump",
        "Climb": "BP_TraversalLink_Climb",
        "Zipline": "BP_TraversalLink_Zipline",
        "Lift": "BP_TraversalLink_Lift",
    }

    for line_number, row in enumerate(links, start=2):
        traversal_id = row.get("TraversalId", "").strip()
        kind = row.get("TraversalType", "").strip()
        if not traversal_id:
            errors.append(f"Traversal line {line_number}: empty TraversalId")
            continue
        if traversal_id in link_by_id:
            errors.append(f"Duplicate TraversalId: {traversal_id}")
            continue
        link_by_id[traversal_id] = row

        if kind not in LINK_TYPES:
            errors.append(f"{traversal_id}: unsupported TraversalType {kind!r}")
            continue
        type_counts[kind] += 1

        region_id = row.get("RegionId", "").strip()
        if region_id not in known_regions:
            errors.append(f"{traversal_id}: unknown RegionId {region_id!r}")
        expected_layer = region_layers.get(region_id)
        if not expected_layer or row.get("EditorDataLayer", "").strip() != expected_layer:
            errors.append(
                f"{traversal_id}: EditorDataLayer does not match region {region_id!r}"
            )

        if not row.get("DisplayName", "").strip():
            errors.append(f"{traversal_id}: DisplayName is required")
        if row.get("LinkActorClass", "").strip() != expected_actor_class[kind]:
            errors.append(
                f"{traversal_id}: expected LinkActorClass {expected_actor_class[kind]!r}"
            )

        try:
            entry = _point(row, "Entry")
            exit_point = _point(row, "Exit")
            if _distance(entry, exit_point) < 50.0:
                errors.append(f"{traversal_id}: entry and exit anchors must be at least 50 cm apart")
            duration = _number(row, "Duration_s")
            arc = _number(row, "ArcHeight_cm")
            if not 0.2 <= duration <= 20.0:
                errors.append(f"{traversal_id}: Duration_s must be within 0.2..20")
            if not 0.0 <= arc <= 1500.0:
                errors.append(f"{traversal_id}: ArcHeight_cm must be within 0..1500")
            _bool(row.get("Bidirectional", ""), "Bidirectional")
        except ValueError as exc:
            errors.append(f"{traversal_id}: {exc}")

        platform_id = row.get("LiftPlatformId", "").strip()
        if kind == "Lift":
            if not platform_id:
                errors.append(f"{traversal_id}: LiftPlatformId is required for Lift links")
            else:
                lift_references.add(platform_id)
        elif platform_id:
            errors.append(f"{traversal_id}: non-Lift link must not reference a LiftPlatformId")

    for kind, expected_count in EXPECTED_TYPE_COUNTS.items():
        if type_counts.get(kind) != expected_count:
            errors.append(
                f"Expected {expected_count} {kind} link(s), found {type_counts.get(kind, 0)}"
            )

    lift_by_id: dict[str, dict[str, str]] = {}
    for line_number, row in enumerate(lifts, start=2):
        platform_id = row.get("LiftPlatformId", "").strip()
        if not platform_id:
            errors.append(f"Lift platform line {line_number}: empty LiftPlatformId")
            continue
        if platform_id in lift_by_id:
            errors.append(f"Duplicate LiftPlatformId: {platform_id}")
            continue
        lift_by_id[platform_id] = row

        region_id = row.get("RegionId", "").strip()
        if region_id not in known_regions:
            errors.append(f"{platform_id}: unknown RegionId {region_id!r}")
        expected_layer = region_layers.get(region_id)
        if not expected_layer or row.get("EditorDataLayer", "").strip() != expected_layer:
            errors.append(f"{platform_id}: EditorDataLayer does not match its region")
        if row.get("PlatformActorClass", "").strip() != "BP_JumanjiLiftPlatform":
            errors.append(f"{platform_id}: PlatformActorClass must be BP_JumanjiLiftPlatform")

        try:
            lower = _point(row, "Lower")
            upper = _point(row, "Upper")
            if _distance(lower, upper) < 100.0:
                errors.append(f"{platform_id}: lift stops must be at least 100 cm apart")
            duration = _number(row, "TravelDuration_s")
            passengers = int(row.get("MaxPassengers", ""))
            boarding = _number(row, "BoardingWindowAlpha")
            if not 0.5 <= duration <= 60.0:
                errors.append(f"{platform_id}: TravelDuration_s must be within 0.5..60")
            if not 1 <= passengers <= 8:
                errors.append(f"{platform_id}: MaxPassengers must be within 1..8")
            if not 0.0 <= boarding <= 0.25:
                errors.append(f"{platform_id}: BoardingWindowAlpha must be within 0..0.25")
        except (ValueError, TypeError) as exc:
            errors.append(f"{platform_id}: invalid lift data: {exc}")

    if set(lift_by_id) != lift_references:
        errors.append(
            f"LiftPlatform references mismatch: link references={sorted(lift_references)}, "
            f"platform rows={sorted(lift_by_id)}"
        )

    for traversal_id, link in link_by_id.items():
        if link.get("TraversalType") != "Lift":
            continue
        platform_id = link.get("LiftPlatformId", "").strip()
        platform = lift_by_id.get(platform_id)
        if not platform:
            continue
        if platform.get("RegionId") != link.get("RegionId"):
            errors.append(f"{traversal_id}: link and platform must be in the same region")
        try:
            entry = _point(link, "Entry")
            exit_point = _point(link, "Exit")
            lower = _point(platform, "Lower")
            upper = _point(platform, "Upper")
            if _distance(entry, lower) > 1.0 or _distance(exit_point, upper) > 1.0:
                errors.append(
                    f"{traversal_id}: link entry/exit anchors must match platform lower/upper stops"
                )
            if abs(_number(link, "Duration_s") - _number(platform, "TravelDuration_s")) > 0.01:
                errors.append(f"{traversal_id}: link duration must match platform travel duration")
        except ValueError as exc:
            errors.append(f"{traversal_id}: lift/link anchor validation: {exc}")

    expected_counts = manifest.get("expectedCounts", {})
    if expected_counts.get("traversalLinks") != 12 or expected_counts.get("liftPlatforms") != 2:
        errors.append("Manifest expectedCounts must declare 12 traversal links and 2 lift platforms")
    if expected_counts.get("traversalTypes") != EXPECTED_TYPE_COUNTS:
        errors.append("Manifest traversalTypes do not match the starter link type distribution")
    if manifest.get("status") != "AUTHORING_PLACEHOLDERS_NOT_SURVEYED_WORLD_DATA":
        errors.append("Manifest must explicitly mark all world positions as greybox placeholders")

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
        print(f"Traversal world validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Traversal world validation: PASS")
    print("12 links, 2 lift platforms, region/Data Layer mapping, anchor and duration checks passed.")
    print("Note: source-data PASS does not validate .umap/.uasset, NavMesh or runtime multiplayer.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
