# Emerald Jungle World

Stages 11–12 provide the native world-atlas, exploration and local route-guidance foundation.

## Stage 11 — World atlas and exploration

- UJumanjiWorldAtlasSubsystem — built-in atlas plus optional DataTable overrides.
- AJumanjiWorldRegionVolume — server-authoritative region discovery boundary.
- AJumanjiWorldLandmark — discoverable point of interest.
- AJumanjiPlayerState — replicated per-player exploration snapshot.
- UJumanjiWorldAtlasWidget — Blueprint-facing map and discovery panel base.
- AJumanjiPlayerController — OpenWorldAtlas and CloseWorldAtlas.

## Stage 12 — Traversal and navigation

- UJumanjiWorldNavigationComponent — per-local-player destination selection and NavMesh path queries.
- FJumanjiNavigationRoute — target metadata, path points, path length, direct distance and partial-path state.
- Region and landmark targets are validated against the player's discovered-region state.
- Navigation route data is local presentation state; it is not replicated.
- Region/landmark definitions can store world-space navigation anchors for actors that are not currently loaded by World Partition.

## DataTables

Optional DataTable assets:
- DT_Jumanji_WorldRegions — FJumanjiWorldRegionDefinition.
- DT_Jumanji_WorldLandmarks — FJumanjiWorldLandmarkDefinition.

Both tables must be configured together in DefaultGame.ini. If either is missing or invalid, the built-in 10-region/10-landmark catalog is used.

For navigation, set:
- Region rows: bHasNavigationAnchor=true and NavigationAnchor to the region's world-space destination.
- Landmark rows: bHasWorldLocation=true and WorldLocation to the point-of-interest destination.

World coordinates are Unreal centimeters. MapCoordinate remains normalized 0..1 UI space.

## Map

Create Content/Maps/L_EmeraldJungle_World.umap from the Unreal Editor Open World template, enable World Partition streaming, add NavMeshBoundsVolume over playable traversal areas, and follow:
- Docs/STAGE_11_EMERALD_JUNGLE_WORLD_ASSEMBLY.md
- Docs/STAGE_12_EMERALD_JUNGLE_TRAVERSAL_NAVIGATION.md

The map, navigation build data, DataTables, meshes, landscape, materials and Widget Blueprints are editor/binary assets. Add them with Git LFS. The text-source changes do not create these assets automatically.
