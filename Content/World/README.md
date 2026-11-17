# Emerald Jungle World — Authoring Data

Stages 11–12 provide the runtime world atlas, per-player discovery, landmark actors and local navigation. Stage 21 supplies source data for the actual Unreal DataTable assets and first map greybox.

## Stage 21 source files
- `Data/World/DT_Jumanji_WorldRegions.csv` — import with row type `FJumanjiWorldRegionDefinition`.
- `Data/World/DT_Jumanji_WorldLandmarks.csv` — import with row type `FJumanjiWorldLandmarkDefinition`.
- `Data/World/EmeraldJungle_GreyboxPlacement.csv` — actor placement ledger (not a DataTable).
- `Data/World/EmeraldJungle_WorldManifest.json` — world coordinate convention and Data Layer list.

Import the two DataTable CSVs as assets under `Content/Data/World`. Then configure both soft-object paths in `Config/DefaultGame.ini`:

```ini
[/Script/JumanjiUE.JumanjiWorldAtlasSubsystem]
RegionDataTable=/Game/Data/World/DT_Jumanji_WorldRegions.DT_Jumanji_WorldRegions
LandmarkDataTable=/Game/Data/World/DT_Jumanji_WorldLandmarks.DT_Jumanji_WorldLandmarks
```

Both tables must be configured together. If either is missing or invalid, the runtime atlas intentionally uses the built-in catalog.

All world coordinates are greybox placeholders in Unreal centimeters. Reposition/project them against the actual terrain and NavMesh before gameplay validation.

See `Docs/STAGE_11_EMERALD_JUNGLE_WORLD_ASSEMBLY.md` for the map assembly contract and `Docs/STAGE_21_EMERALD_JUNGLE_WORLD_AUTHORING_PACK.md` for CSV import, field mapping and validation.