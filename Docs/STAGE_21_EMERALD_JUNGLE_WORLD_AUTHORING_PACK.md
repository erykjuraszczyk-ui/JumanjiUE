# JUMANJI 2.111.1 — Stage 21: Emerald Jungle World Authoring Pack

## Goal

Convert the existing Stage 11 World Atlas C++ catalog into importable DataTable source files and a reproducible greybox placement plan. Stage 11 already supplies runtime atlas/discovery code; this stage supplies the source data needed to author the actual DataTable assets and place the first map actors.

This is an authoring-data stage. The CSV files are source material, not Unreal .uasset DataTables. The JSON/CSV world coordinates are starting greybox values, not surveyed or NavMesh-validated positions. The binary map, landscape, meshes, foliage and widget assets must still be authored in Unreal Editor.

## Files added

- `Data/World/DT_Jumanji_WorldRegions.csv` — 10 world region records matching FJumanjiWorldRegionDefinition.
- `Data/World/DT_Jumanji_WorldLandmarks.csv` — 10 point-of-interest records matching FJumanjiWorldLandmarkDefinition.
- `Data/World/EmeraldJungle_GreyboxPlacement.csv` — 29 actor placement rows: 10 region volumes, 10 landmarks and 9 trial actors.
- `Data/World/EmeraldJungle_WorldManifest.json` — coordinate convention, map bounds, World Partition/Data Layer names and placement source references.

## DataTable schema matching

Both C++ row structs inherit from FTableRowBase. CSV first column is `Name`; the remaining column headings exactly match reflected UPROPERTY member names.

### DT_Jumanji_WorldRegions

Row type: `FJumanjiWorldRegionDefinition`.

Columns: Name, RegionId, DisplayName, Description, ElementTag, RelatedObjectiveId, RecommendedGreatSpiritId, SortOrder, MapCoordinate, bSafeHub, bHasNavigationAnchor, NavigationAnchor, MapIcon.

### DT_Jumanji_WorldLandmarks

Row type: `FJumanjiWorldLandmarkDefinition`.

Columns: Name, LandmarkId, RegionId, DisplayName, Description, MapIcon, LandmarkMesh, bHasWorldLocation, WorldLocation.

Empty MapIcon and LandmarkMesh cells are intentional. Assign real assets in the DataTable editor after importing meshes and icons.

The first CSV column is the row key named `Name`, as required by the Unreal DataTable CSV importer. Column names must remain in sync with the C++ row struct. See the official Epic Data Driven Gameplay documentation and UDataTable API references.

## Import in Unreal Editor

1. Open the JUMANJI project in Unreal Engine 5.8 after the native C++ source has compiled.
2. In the Content Browser, create the folder `Content/Data/World`.
3. Import `Data/World/DT_Jumanji_WorldRegions.csv` from the repository using Import.
4. Choose DataTable as the import type and select `FJumanjiWorldRegionDefinition` as Row Structure.
5. Save the resulting asset as `Content/Data/World/DT_Jumanji_WorldRegions.uasset`.
6. Import `Data/World/DT_Jumanji_WorldLandmarks.csv` as a DataTable using `FJumanjiWorldLandmarkDefinition`.
7. Save the resulting asset as `Content/Data/World/DT_Jumanji_WorldLandmarks.uasset`.
8. Open both assets and check the row counts: exactly 10 regions and 10 landmarks. Check that row names match the RegionId/LandmarkId fields.
9. Check each landmark RegionId resolves to one of the region rows. Check MapCoordinate stays within 0..1.
10. Add the atlas configuration to `Config/DefaultGame.ini`:

```ini
[/Script/JumanjiUE.JumanjiWorldAtlasSubsystem]
RegionDataTable=/Game/Data/World/DT_Jumanji_WorldRegions.DT_Jumanji_WorldRegions
LandmarkDataTable=/Game/Data/World/DT_Jumanji_WorldLandmarks.DT_Jumanji_WorldLandmarks
```

11. Restart the editor or restart PIE so UJumanjiWorldAtlasSubsystem initializes with both configured tables.
12. Check the Output Log. A missing table, wrong row struct, invalid coordinate, duplicate ID or unresolved region reference causes the all-or-nothing built-in atlas fallback to remain active.

Do not set only one table path: the atlas override is intentionally all-or-nothing.

## Greybox coordinate convention

All world coordinates are Unreal centimeters.

- World origin: center of the initial map at X=0, Y=0.
- Suggested horizontal greybox extent: 400,000 cm × 400,000 cm (4 km × 4 km).
- Region DataTable MapCoordinate is normalized 0..1.
- Conversion: `WorldX = (MapCoordinate.X - 0.5) * 400000`; `WorldY = (MapCoordinate.Y - 0.5) * 400000`.
- NavigationAnchor/WorldLocation values are initial target centers. Project them onto the final NavMesh after the landscape and traversable routes exist.
- Z heights are staging placeholders to distinguish elevated peaks, lowlands and cave spaces. They are not terrain elevations.

Do not ship these coordinates as final navigation without testing them against the actual level.

## Greybox placement CSV

`EmeraldJungle_GreyboxPlacement.csv` is a placement ledger, not an importable DataTable. It has 29 rows:

- 10 RegionVolume rows: place AJumanjiWorldRegionVolume, set RegionId, center and extents. Discovery-only collision must overlap Pawn and must not block movement.
- 10 Landmark rows: place AJumanjiWorldLandmark, set LandmarkId/RegionId and DiscoveryRadius. Its visual mesh uses no collision.
- 9 Trial rows: place the matching trial actor in the appropriate region. Use the stable ObjectiveId from Stage 10.

Use each row's EditorDataLayer value to organize the actor in the matching Editor Data Layer. Create `DL_EJ_Core` for shared world content and the ten region-specific layers listed in the World Manifest.

Suggested region half-extents are greybox starting values. Adjust boundaries so they do not overlap neighboring regions unexpectedly and so the discovery trigger activates after the player enters the intended region.

## Map assembly order

1. Create `Content/Maps/L_EmeraldJungle_World.umap` from the Open World template.
2. Enable World Partition and One File Per Actor.
3. Establish the landscape bounds around the 4 km × 4 km greybox extent.
4. Create the Editor Data Layers from the World Manifest.
5. Block out Serce Dżungli as the central hub and establish visible routes to the nine elemental regions.
6. Add region volumes from the placement CSV.
7. Add landmark actors and assign their IDs.
8. Add nine trial actors and match their ObjectiveIds to the Stage 10/Stage 09 objective contracts.
9. Place AJumanjiAdventureChapterDirector and configure nine objectives with RequiredProgress=3. Reference all nine placed trial actors once.
10. Add NavMeshBoundsVolume to walkable space and build navigation after terrain/obstacle placement.
11. Test each anchor and discovery volume with a player pawn in PIE.
12. Save the `.umap` and binary assets through Git LFS; do not commit generated local SaveGame or DerivedDataCache content.

## Acceptance checklist

- [ ] Region CSV imports with FJumanjiWorldRegionDefinition.
- [ ] Landmark CSV imports with FJumanjiWorldLandmarkDefinition.
- [ ] Region table has 10 rows and landmark table has 10 rows.
- [ ] Row names equal their stable RegionId/LandmarkId values.
- [ ] All 10 landmark RegionId values resolve to an imported region.
- [ ] All normalized map coordinates are between 0 and 1.
- [ ] All NavigationAnchor and WorldLocation values are finite.
- [ ] Both table paths are set together in DefaultGame.ini.
- [ ] UJumanjiWorldAtlasSubsystem loads the configured tables rather than using fallback.
- [ ] Greybox placement ledger contains 29 rows: 10 region volumes, 10 landmarks and 9 trials.
- [ ] Each placed region volume discovers only its configured region.
- [ ] Each landmark discovers its own parent region and landmark ID.
- [ ] All nine trial actors are placed in the intended regions and match Stage 10 IDs.
- [ ] Navigation anchors project to a valid NavMesh path after map assembly.
- [ ] A two-client PIE test confirms that exploration remains per-player.
- [ ] The final map and authored Unreal assets are saved in the project and handled by Git LFS.

## Not included

- Unreal binary `.umap` or DataTable `.uasset` files.
- Landscape sculpt, foliage, water, sky, materials, meshes, lighting, audio or Niagara assets.
- Blueprint map widget and custom map icons.
- Final collision tuning, NavMesh build data, performance profile or mobile device test.
- Compiled Unreal Engine 5.8 build or Android APK/AAB.

## Next stage

Stage 22 — Emerald Jungle Greybox Validation & Mobile Performance: run the assembled map through region-boundary, streaming, landmark, navigation and Android performance validation before replacing greybox geometry with final art.