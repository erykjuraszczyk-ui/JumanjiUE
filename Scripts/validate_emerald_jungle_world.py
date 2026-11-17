#!/usr/bin/env python3
"""Validate the JUMANJI Emerald Jungle greybox authoring data.

Uses only the Python standard library. This checks source CSV/JSON consistency;
it does not open Unreal assets, build NavMesh, compile C++, or measure a device.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

EXPECTED_REGIONS = {
    "REGION_HEART_GROVE": ("", "", "DL_EJ_HeartGrove"),
    "REGION_ROOT_GROVE": ("Element.Root", "TRIAL_ROOT_RUNES", "DL_EJ_RootGrove"),
    "REGION_TIDE_FALLS": ("Element.Tide", "TRIAL_TIDE_CHANNELS", "DL_EJ_TideFalls"),
    "REGION_STONE_SANCTUM": ("Element.Stone", "TRIAL_STONE_BALANCE", "DL_EJ_StoneSanctum"),
    "REGION_STORM_PEAK": ("Element.Storm", "TRIAL_STORM_WINDS", "DL_EJ_StormPeak"),
    "REGION_EMBER_CAVERN": ("Element.Ember", "TRIAL_EMBER_SEQUENCE", "DL_EJ_EmberCavern"),
    "REGION_DAWN_MIRRORS": ("Element.Dawn", "TRIAL_DAWN_MIRRORS", "DL_EJ_DawnMirrors"),
    "REGION_SHADOW_MARSH": ("Element.Shadow", "TRIAL_SHADOW_PATTERN", "DL_EJ_ShadowMarsh"),
    "REGION_LIFE_GARDEN": ("Element.Life", "TRIAL_LIFE_GARDEN", "DL_EJ_LifeGarden"),
    "REGION_FROST_GATE": ("Element.Frost", "TRIAL_FROST_GATES", "DL_EJ_FrostGate"),
}
EXPECTED_LANDMARKS = {
    "LANDMARK_LIVING_BOARD": "REGION_HEART_GROVE",
    "LANDMARK_ROOT_ALTAR": "REGION_ROOT_GROVE",
    "LANDMARK_TIDE_CHANNEL": "REGION_TIDE_FALLS",
    "LANDMARK_STONE_PILLARS": "REGION_STONE_SANCTUM",
    "LANDMARK_STORM_BEACON": "REGION_STORM_PEAK",
    "LANDMARK_EMBER_LANTERN": "REGION_EMBER_CAVERN",
    "LANDMARK_DAWN_MIRROR": "REGION_DAWN_MIRRORS",
    "LANDMARK_SHADOW_ARCH": "REGION_SHADOW_MARSH",
    "LANDMARK_LIFE_FOUNTAIN": "REGION_LIFE_GARDEN",
    "LANDMARK_FROST_GATE": "REGION_FROST_GATE",
}
EXPECTED_TRIALS = {
    "TRIAL_ROOT_RUNES": "REGION_ROOT_GROVE",
    "TRIAL_TIDE_CHANNELS": "REGION_TIDE_FALLS",
    "TRIAL_STONE_BALANCE": "REGION_STONE_SANCTUM",
    "TRIAL_STORM_WINDS": "REGION_STORM_PEAK",
    "TRIAL_EMBER_SEQUENCE": "REGION_EMBER_CAVERN",
    "TRIAL_DAWN_MIRRORS": "REGION_DAWN_MIRRORS",
    "TRIAL_SHADOW_PATTERN": "REGION_SHADOW_MARSH",
    "TRIAL_LIFE_GARDEN": "REGION_LIFE_GARDEN",
    "TRIAL_FROST_GATES": "REGION_FROST_GATE",
}
REGION_COLUMNS = [
    "Name", "RegionId", "DisplayName", "Description", "ElementTag",
    "RelatedObjectiveId", "RecommendedGreatSpiritId", "SortOrder",
    "MapCoordinate", "bSafeHub", "bHasNavigationAnchor",
    "NavigationAnchor", "MapIcon",
]
LANDMARK_COLUMNS = [
    "Name", "LandmarkId", "RegionId", "DisplayName", "Description",
    "MapIcon", "LandmarkMesh", "bHasWorldLocation", "WorldLocation",
]
PLACEMENT_COLUMNS = [
    "ActorId", "ActorType", "DisplayName", "X_cm", "Y_cm", "Z_cm",
    "ExtentX_cm", "ExtentY_cm", "ExtentZ_cm", "EditorDataLayer", "Notes",
]
VECTOR_PATTERN = re.compile(
    r"([XYZ])\\s*=\\s*([+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?)"
)


@dataclass
class ValidationReport:
    checks: list[dict[str, Any]] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    def check(self, name: str, passed: bool, detail: str = "") -> None:
        self.checks.append({"name": name, "passed": bool(passed), "detail": detail})
        if not passed:
            self.errors.append(f"{name}: {detail or 'failed'}")

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    @property
    def passed(self) -> bool:
        return not self.errors

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema": "JUMANJI.EmeraldJungleValidation.v1",
            "generatedAtUtc": datetime.now(timezone.utc).isoformat(),
            "passed": self.passed,
            "checkCount": len(self.checks),
            "passedChecks": sum(1 for item in self.checks if item["passed"]),
            "failedChecks": sum(1 for item in self.checks if not item["passed"]),
            "errors": self.errors,
            "warnings": self.warnings,
            "checks": self.checks,
        }


def parse_bool(value: str) -> bool:
    normalized = value.strip().lower()
    if normalized in {"true", "1", "yes"}:
        return True
    if normalized in {"false", "0", "no"}:
        return False
    raise ValueError(f"not a boolean: {value!r}")


def parse_number(value: str, field_name: str) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{field_name} is not numeric: {value!r}") from exc
    if not math.isfinite(number):
        raise ValueError(f"{field_name} must be finite, got {value!r}")
    return number


def parse_vector(value: str, required_axes: tuple[str, ...]) -> dict[str, float]:
    matches = {axis: float(number) for axis, number in VECTOR_PATTERN.findall(value or "")}
    if set(matches) != set(required_axes):
        raise ValueError(
            f"expected vector axes {required_axes} in {value!r}, found {tuple(matches)}"
        )
    if not all(math.isfinite(number) for number in matches.values()):
        raise ValueError(f"vector contains non-finite value: {value!r}")
    return matches


def load_csv(path: Path, required_columns: list[str], report: ValidationReport, label: str) -> list[dict[str, str]]:
    if not path.is_file():
        report.check(f"{label} exists", False, str(path))
        return []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            columns = reader.fieldnames or []
            missing = [column for column in required_columns if column not in columns]
            report.check(
                f"{label} schema",
                not missing,
                f"missing columns: {', '.join(missing)}" if missing else f"{len(columns)} columns",
            )
            if missing:
                return []
            return list(reader)
    except (OSError, UnicodeError, csv.Error) as exc:
        report.check(f"{label} readable", False, str(exc))
        return []


def placement_vector(row: dict[str, str]) -> dict[str, float]:
    return {
        "X": parse_number(row.get("X_cm", ""), "X_cm"),
        "Y": parse_number(row.get("Y_cm", ""), "Y_cm"),
        "Z": parse_number(row.get("Z_cm", ""), "Z_cm"),
    }


def is_inside(point: dict[str, float], center: dict[str, float], extent: dict[str, float], tolerance: float = 1.0) -> bool:
    return all(
        abs(point[axis] - center[axis]) <= extent[axis] + tolerance
        for axis in ("X", "Y", "Z")
    )


def validate_project(project_root: Path) -> ValidationReport:
    report = ValidationReport()
    data_dir = project_root / "Data" / "World"
    regions = load_csv(data_dir / "DT_Jumanji_WorldRegions.csv", REGION_COLUMNS, report, "Region DataTable CSV")
    landmarks = load_csv(data_dir / "DT_Jumanji_WorldLandmarks.csv", LANDMARK_COLUMNS, report, "Landmark DataTable CSV")
    placements = load_csv(data_dir / "EmeraldJungle_GreyboxPlacement.csv", PLACEMENT_COLUMNS, report, "Greybox placement ledger")

    manifest_path = data_dir / "EmeraldJungle_WorldManifest.json"
    manifest: dict[str, Any] = {}
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        report.check("World manifest JSON", isinstance(manifest, dict), "valid JSON object")
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        report.check("World manifest JSON", False, str(exc))

    report.check("Region row count", len(regions) == 10, f"expected 10, found {len(regions)}")
    report.check("Landmark row count", len(landmarks) == 10, f"expected 10, found {len(landmarks)}")
    report.check("Placement row count", len(placements) == 29, f"expected 29, found {len(placements)}")

    region_by_id: dict[str, dict[str, str]] = {}
    landmark_by_id: dict[str, dict[str, str]] = {}
    placement_by_id: dict[str, dict[str, str]] = {}

    for row in regions:
        region_id = row.get("RegionId", "").strip()
        row_name = row.get("Name", "").strip()
        if not region_id:
            report.errors.append("Region row has an empty RegionId")
            continue
        if region_id in region_by_id:
            report.errors.append(f"Duplicate RegionId: {region_id}")
            continue
        region_by_id[region_id] = row
        report.check(
            f"Region row key {region_id}",
            row_name == region_id,
            f"Name={row_name!r}, RegionId={region_id!r}",
        )
        expected = EXPECTED_REGIONS.get(region_id)
        report.check(f"Known region {region_id}", expected is not None, "region ID must match the Stage 11 atlas")
        if expected:
            element, objective, _layer = expected
            report.check(f"Element tag {region_id}", row.get("ElementTag", "").strip() == element,
                         f"expected {element!r}, found {row.get('ElementTag', '').strip()!r}")
            report.check(f"Objective mapping {region_id}", row.get("RelatedObjectiveId", "").strip() == objective,
                         f"expected {objective!r}, found {row.get('RelatedObjectiveId', '').strip()!r}")
        if not row.get("DisplayName", "").strip():
            report.errors.append(f"Region {region_id} has no DisplayName")
        try:
            coord = parse_vector(row.get("MapCoordinate", ""), ("X", "Y"))
            report.check(f"Map coordinate range {region_id}",
                         all(0.0 <= coord[axis] <= 1.0 for axis in ("X", "Y")),
                         f"MapCoordinate={coord}")
            anchor = parse_vector(row.get("NavigationAnchor", ""), ("X", "Y", "Z"))
            has_anchor = parse_bool(row.get("bHasNavigationAnchor", ""))
            report.check(f"Navigation anchor enabled {region_id}", has_anchor,
                         "Stage 22 requires an explicit greybox anchor")
            if manifest:
                extent = manifest.get("coordinateConvention", {}).get("horizontalExtentCm", {})
                half_x = float(extent.get("x", 0)) / 2.0
                half_y = float(extent.get("y", 0)) / 2.0
                expected_x = (coord["X"] - 0.5) * float(extent.get("x", 0))
                expected_y = (coord["Y"] - 0.5) * float(extent.get("y", 0))
                report.check(
                    f"Map/world coordinate agreement {region_id}",
                    abs(anchor["X"] - expected_x) <= 1.0 and abs(anchor["Y"] - expected_y) <= 1.0,
                    f"anchor XY=({anchor['X']}, {anchor['Y']}), derived XY=({expected_x}, {expected_y})",
                )
                report.check(
                    f"Anchor within world bounds {region_id}",
                    abs(anchor["X"]) <= half_x and abs(anchor["Y"]) <= half_y,
                    f"anchor XY=({anchor['X']}, {anchor['Y']}), half extent=({half_x}, {half_y})",
                )
        except (ValueError, TypeError) as exc:
            report.errors.append(f"Region {region_id} coordinate data: {exc}")

    report.check(
        "Expected region set",
        set(region_by_id) == set(EXPECTED_REGIONS),
        f"missing={sorted(set(EXPECTED_REGIONS) - set(region_by_id))}; unexpected={sorted(set(region_by_id) - set(EXPECTED_REGIONS))}",
    )

    for row in landmarks:
        landmark_id = row.get("LandmarkId", "").strip()
        row_name = row.get("Name", "").strip()
        region_id = row.get("RegionId", "").strip()
        if not landmark_id:
            report.errors.append("Landmark row has an empty LandmarkId")
            continue
        if landmark_id in landmark_by_id:
            report.errors.append(f"Duplicate LandmarkId: {landmark_id}")
            continue
        landmark_by_id[landmark_id] = row
        expected_region = EXPECTED_LANDMARKS.get(landmark_id)
        report.check(f"Known landmark {landmark_id}", expected_region is not None, "landmark ID must match the Stage 11 atlas")
        report.check(f"Landmark row key {landmark_id}", row_name == landmark_id, f"Name={row_name!r}")
        report.check(f"Landmark parent region {landmark_id}",
                     region_id in region_by_id and expected_region == region_id,
                     f"RegionId={region_id!r}, expected={expected_region!r}")
        if not row.get("DisplayName", "").strip():
            report.errors.append(f"Landmark {landmark_id} has no DisplayName")
        try:
            location = parse_vector(row.get("WorldLocation", ""), ("X", "Y", "Z"))
            report.check(f"Landmark world location enabled {landmark_id}",
                         parse_bool(row.get("bHasWorldLocation", "")),
                         "WorldLocation must be enabled for the greybox target")
            if manifest:
                extent = manifest.get("coordinateConvention", {}).get("horizontalExtentCm", {})
                half_x = float(extent.get("x", 0)) / 2.0
                half_y = float(extent.get("y", 0)) / 2.0
                report.check(
                    f"Landmark within world bounds {landmark_id}",
                    abs(location["X"]) <= half_x and abs(location["Y"]) <= half_y,
                    f"world location XY=({location['X']}, {location['Y']})",
                )
        except (ValueError, TypeError) as exc:
            report.errors.append(f"Landmark {landmark_id} location data: {exc}")

    report.check(
        "Expected landmark set",
        set(landmark_by_id) == set(EXPECTED_LANDMARKS),
        f"missing={sorted(set(EXPECTED_LANDMARKS) - set(landmark_by_id))}; unexpected={sorted(set(landmark_by_id) - set(EXPECTED_LANDMARKS))}",
    )

    for row in placements:
        actor_id = row.get("ActorId", "").strip()
        if not actor_id:
            report.errors.append("Placement row has an empty ActorId")
            continue
        if actor_id in placement_by_id:
            report.errors.append(f"Duplicate placement ActorId: {actor_id}")
            continue
        placement_by_id[actor_id] = row
        try:
            point = placement_vector(row)
            extent = {
                "X": parse_number(row.get("ExtentX_cm", ""), "ExtentX_cm"),
                "Y": parse_number(row.get("ExtentY_cm", ""), "ExtentY_cm"),
                "Z": parse_number(row.get("ExtentZ_cm", ""), "ExtentZ_cm"),
            }
            if any(value < 0.0 for value in extent.values()):
                report.errors.append(f"Placement {actor_id} has a negative extent")
            if row.get("ActorType") == "RegionVolume" and any(value <= 0.0 for value in extent.values()):
                report.errors.append(f"RegionVolume {actor_id} must have positive half-extents")
            report.check(f"Placement coordinates finite {actor_id}",
                         all(math.isfinite(value) for value in (*point.values(), *extent.values())),
                         f"position={point}, extent={extent}")
        except ValueError as exc:
            report.errors.append(f"Placement {actor_id}: {exc}")

    by_type = {
        actor_type: [row for row in placements if row.get("ActorType") == actor_type]
        for actor_type in ("RegionVolume", "Landmark", "Trial")
    }
    report.check("10 region-volume placements", len(by_type["RegionVolume"]) == 10,
                 f"found {len(by_type['RegionVolume'])}")
    report.check("10 landmark placements", len(by_type["Landmark"]) == 10,
                 f"found {len(by_type['Landmark'])}")
    report.check("9 trial placements", len(by_type["Trial"]) == 9,
                 f"found {len(by_type['Trial'])}")
    report.check("No unknown actor types",
                 len(placements) == sum(len(rows) for rows in by_type.values()),
                 "only RegionVolume, Landmark and Trial rows are allowed")

    region_volumes = {
        row.get("ActorId", "").strip(): row
        for row in by_type["RegionVolume"]
    }

    def layer_for_region(region_id: str) -> str | None:
        expected = EXPECTED_REGIONS.get(region_id)
        return expected[2] if expected else None

    for region_id, region in region_by_id.items():
        volume = region_volumes.get(region_id)
        report.check(f"Region volume exists {region_id}", volume is not None, "ActorId must equal RegionId")
        if not volume:
            continue
        try:
            center = placement_vector(volume)
            extent = {
                "X": parse_number(volume["ExtentX_cm"], "ExtentX_cm"),
                "Y": parse_number(volume["ExtentY_cm"], "ExtentY_cm"),
                "Z": parse_number(volume["ExtentZ_cm"], "ExtentZ_cm"),
            }
            anchor = parse_vector(region.get("NavigationAnchor", ""), ("X", "Y", "Z"))
            report.check(
                f"Region volume matches atlas anchor {region_id}",
                all(abs(center[axis] - anchor[axis]) <= 1.0 for axis in ("X", "Y", "Z")),
                f"volume={center}, atlas={anchor}",
            )
            expected_layer = layer_for_region(region_id)
            report.check(
                f"Region volume Data Layer {region_id}",
                volume.get("EditorDataLayer", "") == expected_layer,
                f"expected {expected_layer!r}, found {volume.get('EditorDataLayer')!r}",
            )
            for landmark_id, landmark in landmark_by_id.items():
                if landmark.get("RegionId") != region_id:
                    continue
                poi_row = placement_by_id.get(landmark_id)
                report.check(f"Landmark placement exists {landmark_id}", poi_row is not None, "ActorId must equal LandmarkId")
                if poi_row:
                    poi = placement_vector(poi_row)
                    report.check(f"Landmark inside its region {landmark_id}",
                                 is_inside(poi, center, extent),
                                 f"landmark={poi}, volume center={center}, half extent={extent}")
                    report.check(f"Landmark placement Data Layer {landmark_id}",
                                 poi_row.get("EditorDataLayer", "") == expected_layer,
                                 f"expected {expected_layer!r}")
                    world_location = parse_vector(landmark.get("WorldLocation", ""), ("X", "Y", "Z"))
                    report.check(f"Landmark actor matches DataTable {landmark_id}",
                                 all(abs(poi[axis] - world_location[axis]) <= 1.0 for axis in ("X", "Y", "Z")),
                                 f"placement={poi}, DataTable={world_location}")
            objective_id = region.get("RelatedObjectiveId", "").strip()
            if objective_id:
                trial_row = placement_by_id.get(objective_id)
                report.check(f"Trial placement exists {objective_id}", trial_row is not None, "ActorId must equal RelatedObjectiveId")
                if trial_row:
                    trial = placement_vector(trial_row)
                    report.check(f"Trial inside its region {objective_id}",
                                 is_inside(trial, center, extent),
                                 f"trial={trial}, volume center={center}, half extent={extent}")
                    report.check(f"Trial Data Layer {objective_id}",
                                 trial_row.get("EditorDataLayer", "") == expected_layer,
                                 f"expected {expected_layer!r}")
        except (ValueError, TypeError) as exc:
            report.errors.append(f"Region placement validation {region_id}: {exc}")

    for landmark_id in EXPECTED_LANDMARKS:
        report.check(f"Landmark placement exists {landmark_id}",
                     landmark_id in placement_by_id and placement_by_id[landmark_id].get("ActorType") == "Landmark",
                     "must have exactly one Landmark row in the placement ledger")

    for trial_id, region_id in EXPECTED_TRIALS.items():
        report.check(f"Trial placement exists {trial_id}",
                     trial_id in placement_by_id and placement_by_id[trial_id].get("ActorType") == "Trial",
                     "must have exactly one Trial row in the placement ledger")
        if trial_id in placement_by_id:
            expected_layer = layer_for_region(region_id)
            report.check(f"Trial region Data Layer {trial_id}",
                         placement_by_id[trial_id].get("EditorDataLayer") == expected_layer,
                         f"expected {expected_layer!r}")

    if manifest:
        report.check(
            "Manifest schema",
            manifest.get("schema") == "JUMANJI.EmeraldJungleGreybox.v1",
            f"found {manifest.get('schema')!r}",
        )
        status = manifest.get("status", "")
        report.check(
            "Coordinates explicitly marked as greybox placeholders",
            status == "AUTHORING_PLACEHOLDERS_NOT_SURVEYED_WORLD_DATA",
            f"found status {status!r}",
        )
        if status == "AUTHORING_PLACEHOLDERS_NOT_SURVEYED_WORLD_DATA":
            report.warn("Manifest coordinates are authoring placeholders. They are not proof of final terrain, actor fit or NavMesh reachability.")
        partition = manifest.get("worldPartition", {})
        expected_layers = {"DL_EJ_Core"} | {value[2] for value in EXPECTED_REGIONS.values()}
        found_layers = set(partition.get("editorDataLayers", []))
        report.check(
            "World Partition Editor Data Layers",
            expected_layers.issubset(found_layers),
            f"missing={sorted(expected_layers - found_layers)}",
        )
        report.check(
            "No decorative Runtime Data Layers",
            not partition.get("runtimeDataLayers", []),
            f"runtimeDataLayers={partition.get('runtimeDataLayers', [])}",
        )
        report.check(
            "Greybox ledger reference",
            manifest.get("actorPlacementCsv") == "EmeraldJungle_GreyboxPlacement.csv",
            f"found {manifest.get('actorPlacementCsv')!r}",
        )

    report.warn("This validator checks CSV/JSON source consistency only. It cannot inspect .umap/.uasset files, collision presets, World Partition runtime loading or generated NavMesh.")
    report.warn("Android performance remains unmeasured until a Development build is profiled on physical target hardware.")
    return report


def render_markdown(report: ValidationReport) -> str:
    data = report.to_dict()
    lines = [
        "# JUMANJI Emerald Jungle Greybox Validation",
        "",
        f"- Result: **{'PASS' if report.passed else 'FAIL'}**",
        f"- Checks: **{data['passedChecks']}/{data['checkCount']} passed**",
        f"- Errors: **{len(report.errors)}**",
        f"- Warnings: **{len(report.warnings)}**",
        "",
        "## Checks",
        "",
        "| Result | Check | Detail |",
        "|---|---|---|",
    ]
    for item in report.checks:
        result = "PASS" if item["passed"] else "FAIL"
        detail = item["detail"].replace("|", "\\|").replace("\n", " ")
        lines.append(f"| {result} | {item['name']} | {detail} |")
    if report.errors:
        lines.extend(["", "## Errors", ""])
        lines.extend(f"- {error}" for error in report.errors)
    if report.warnings:
        lines.extend(["", "## Warnings / test limitations", ""])
        lines.extend(f"- {warning}" for warning in report.warnings)
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--report-dir", type=Path, default=None,
                        help="Optional directory for JSON and Markdown reports.")
    args = parser.parse_args()

    root = args.project_root.resolve()
    report = validate_project(root)
    summary = report.to_dict()

    print(f"Emerald Jungle validation: {'PASS' if report.passed else 'FAIL'}")
    print(f"Checks: {summary['passedChecks']}/{summary['checkCount']} passed")
    for error in report.errors:
        print(f"ERROR: {error}", file=sys.stderr)
    for warning in report.warnings:
        print(f"WARNING: {warning}")

    if args.report_dir:
        args.report_dir.mkdir(parents=True, exist_ok=True)
        (args.report_dir / "EmeraldJungle-Validation.json").write_text(
            json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        (args.report_dir / "EmeraldJungle-Validation.md").write_text(
            render_markdown(report), encoding="utf-8"
        )
        print(f"Reports written to: {args.report_dir.resolve()}")

    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
