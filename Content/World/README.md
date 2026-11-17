# Emerald Jungle World — Authoring Data & Validation

Stages 11–12 provide the runtime world atlas, per-player discovery, landmark actors and local NavMesh route guidance. Stage 21 supplies importable CSV source and the 29-row greybox placement ledger. Stage 22 validates those sources and defines the Unreal Editor/Android test process.

## Stage 21 source files
- `Data/World/DT_Jumanji_WorldRegions.csv` — import with row type `FJumanjiWorldRegionDefinition`.
- `Data/World/DT_Jumanji_WorldLandmarks.csv` — import with row type `FJumanjiWorldLandmarkDefinition`.
- `Data/World/EmeraldJungle_GreyboxPlacement.csv` — 29-actor placement ledger.
- `Data/World/EmeraldJungle_WorldManifest.json` — coordinate convention and Data Layer list.

## Stage 22 validation
Run from the repository root:

```bash
python Scripts/validate_emerald_jungle_world.py --project-root . --report-dir Saved/WorldValidation
python -m unittest discover -s Scripts/Tests -p "test_*.py" -v
```

The validator checks source data consistency only. It does not open Unreal binary assets, build a NavMesh or measure Android performance.

The GitHub Actions workflow `.github/workflows/emerald-jungle-world-validation.yml` runs the checks on relevant pull requests and main updates and publishes JSON/Markdown validation reports.

## Unreal Editor and mobile tests
Follow `Docs/STAGE_22_EMERALD_JUNGLE_GREYBOX_VALIDATION.md` for DataTable import, region/landmark/trial PIE checks, World Partition NavMesh build, and Android performance capture.

Use `Data/World/MobileValidationRunTemplate.csv` to record physical-device measurements. Fill the device model, Android version, renderer and Device Profile for every run. Do not treat greybox coordinates or source-validator PASS as proof of map, NavMesh or device readiness.


## Stage 22 validation

Run from the repository root:

```bash
python Scripts/validate_emerald_jungle_world.py --project-root . --report-dir Saved/WorldValidation
python -m unittest discover -s Scripts/Tests -p "test_*.py" -v
```

GitHub Actions runs the source-data checks for relevant pull requests and updates to main. Reports are uploaded as a workflow artifact.

The validator checks only CSV/JSON consistency; it does not inspect Unreal binary assets, build Navigation Data Chunks, validate runtime collision, or measure Android performance. Use `Docs/STAGE_22_EMERALD_JUNGLE_GREYBOX_VALIDATION.md` for the editor and device test plan.
