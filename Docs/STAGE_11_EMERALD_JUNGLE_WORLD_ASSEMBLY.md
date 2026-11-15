# JUMANJI 2.111.1 — Stage 11: Emerald Jungle World Assembly

## Goal

Establish the runtime and authoring foundation for the Emerald Jungle overworld. The world atlas contains a central safe hub and nine elemental regions, per-player exploration progress, server-authoritative region discovery, discoverable landmarks and a Blueprint-facing world map widget base.

This stage supplies C++ systems and an editor assembly contract. It does not claim that the binary map, meshes, foliage, landscape, materials or widget assets have been authored.

## Added runtime systems

### UJumanjiWorldAtlasSubsystem

World-local registry with a built-in fallback catalog:
- 10 regions: one central hub and nine Great Spirit regions;
- 10 landmarks: the Living Board plus one landmark in each region;
- stable RegionId and LandmarkId values;
- display names, descriptions, element tags, related trial objective IDs and recommended Great Spirit IDs;
- normalized 0..1 map coordinates for map-pin placement;
- optional MapIcon and LandmarkMesh soft references.

An optional pair of DataTables can replace the built-in catalog:
- DT_Jumanji_WorldRegions, row type FJumanjiWorldRegionDefinition;
- DT_Jumanji_WorldLandmarks, row type FJumanjiWorldLandmarkDefinition.

Set both entries in DefaultGame.ini:

[/Script/JumanjiUE.JumanjiWorldAtlasSubsystem]
RegionDataTable=/Game/Data/DT_Jumanji_WorldRegions.DT_Jumanji_WorldRegions
LandmarkDataTable=/Game/Data/DT_Jumanji_WorldLandmarks.DT_Jumanji_WorldLandmarks

The override is all-or-nothing. Both tables must load and use the exact row structs. Region rows must number 10–32, have unique non-empty IDs and names, and contain normalized map coordinates. Landmark rows must number 1–128, have unique non-empty IDs and names, and reference an existing region. If either table is missing or invalid, the built-in atlas remains active.

### Per-player exploration snapshot

AJumanjiPlayerState now replicates FJumanjiWorldExplorationState:
- DiscoveredRegionIds;
- VisitedLandmarkIds;
- monotonically increasing Revision.

DiscoverWorldRegion and VisitWorldLandmark are server-authoritative and reject unknown IDs through the atlas subsystem. Repeated discovery is idempotent. Visiting a known landmark also reveals its containing region.

Exploration is per player, not shared across the whole party. The state persists for the PlayerState's current session; profile/SaveGame persistence is not part of this stage.

### AJumanjiWorldRegionVolume

Place and scale one volume at the entrance/boundary of each region. Configure RegionId to one of the registered atlas IDs. When the server detects a player pawn entering the volume, that player's PlayerState records the region as discovered.

In the editor, verify the brush has collision enabled, overlaps Pawns and generates overlap events. Do not use a blocking collision preset for a discovery-only boundary.

### AJumanjiWorldLandmark

Place a discoverable point-of-interest actor. Configure:
- LandmarkId;
- RegionId;
- DiscoveryRadius;
- optional LandmarkVisual static mesh in a Blueprint child.

The server verifies the LandmarkId/RegionId pair against the atlas before recording a visit. The discovery sphere overlaps Pawn objects only. Mesh collision is disabled so the landmark does not block traversal.

### UJumanjiWorldAtlasWidget and PlayerController API

Create WBP_WorldAtlas as a Widget Blueprint derived from UJumanjiWorldAtlasWidget. Set WorldAtlasWidgetClass on BP_JumanjiPlayerController.

Blueprint getters:
- GetExplorationState;
- IsRegionDiscovered;
- IsLandmarkVisited;
- GetDiscoveredRegionCount;
- GetVisitedLandmarkCount;
- GetWorldRegions;
- GetWorldLandmarks.

Implement OnWorldAtlasChanged to refresh map pins, locked/discovered styling, region detail panel and discovery counters.

Open and close the panel from a menu or mobile touch button using:
- AJumanjiPlayerController::OpenWorldAtlas;
- AJumanjiPlayerController::CloseWorldAtlas.

The widget is optional. If WorldAtlasWidgetClass is not assigned, gameplay continues without a map panel.

## Atlas catalog

### Central hub

| RegionId | Display name | LandmarkId |
|---|---|---|
| REGION_HEART_GROVE | Serce Dżungli | LANDMARK_LIVING_BOARD |

### Nine Great Spirit regions

| Spirit | RegionId | Display name | LandmarkId | Related objective |
|---|---|---|---|---|
| I — Root | REGION_ROOT_GROVE | Gaj Korzeni | LANDMARK_ROOT_ALTAR | TRIAL_ROOT_RUNES |
| II — Tide | REGION_TIDE_FALLS | Wodospady Przypływu | LANDMARK_TIDE_CHANNEL | TRIAL_TIDE_CHANNELS |
| III — Stone | REGION_STONE_SANCTUM | Kamienne Sanktuarium | LANDMARK_STONE_PILLARS | TRIAL_STONE_BALANCE |
| IV — Storm | REGION_STORM_PEAK | Szczyt Burz | LANDMARK_STORM_BEACON | TRIAL_STORM_WINDS |
| V — Ember | REGION_EMBER_CAVERN | Jaskinia Żaru | LANDMARK_EMBER_LANTERN | TRIAL_EMBER_SEQUENCE |
| VI — Dawn | REGION_DAWN_MIRRORS | Galeria Świtu | LANDMARK_DAWN_MIRROR | TRIAL_DAWN_MIRRORS |
| VII — Shadow | REGION_SHADOW_MARSH | Mokradła Cienia | LANDMARK_SHADOW_ARCH | TRIAL_SHADOW_PATTERN |
| VIII — Life | REGION_LIFE_GARDEN | Ogród Życia | LANDMARK_LIFE_FOUNTAIN | TRIAL_LIFE_GARDEN |
| IX — Frost | REGION_FROST_GATE | Lodowa Brama | LANDMARK_FROST_GATE | TRIAL_FROST_GATES |

The built-in map coordinates are normalized design placeholders for UI layout, not surveyed world-space coordinates. Reposition them after the landscape and level layout have been authored.

## World Partition map assembly

Create the actual map in Unreal Editor:

1. Create a new **Open World** map and save it as Content/Maps/L_EmeraldJungle_World.umap.
2. Keep the map as one persistent World Partition level. Avoid starting a new large-world map with the legacy World Composition workflow.
3. Open World Settings and verify World Partition and streaming are enabled for the intended target.
4. Enable One File Per Actor so level artists can work on separate actors without repeatedly editing one monolithic map file.
5. Create an Editor Data Layer named DL_EJ_Core for shared traversal, lighting, sky, global landscape materials and common set dressing.
6. Create Editor Data Layers for the hub and the nine regions:
   - DL_EJ_HeartGrove
   - DL_EJ_RootGrove
   - DL_EJ_TideFalls
   - DL_EJ_StoneSanctum
   - DL_EJ_StormPeak
   - DL_EJ_EmberCavern
   - DL_EJ_DawnMirrors
   - DL_EJ_ShadowMarsh
   - DL_EJ_LifeGarden
   - DL_EJ_FrostGate
7. Use Runtime Data Layers only for content that genuinely needs to change visibility or load state during play, such as a late-game alternate version of a landmark. Do not create one Runtime Data Layer per decorative prop.
8. Place the nine AJumanjiWorldRegionVolume instances and configure their RegionIds.
9. Place ten AJumanjiWorldLandmark actors and set each ID pair from the atlas table.
10. Place AJumanjiAdventureChapterDirector and the nine Stage 10 trial actors in the matching regions. Keep the chapter director and globally required managers spatially loaded as appropriate.
11. Add environmental landmarks, traversal paths, waterfalls, ruins, bridges, canopy routes and visual navigation cues.
12. Save the map and associated assets through Git LFS. Do not store binary .umap or large texture/mesh assets as plain text Git objects.

### Suggested spatial layout

Use Serce Dżungli as the central orientation hub. Arrange the nine regions around it as a loop so a player can choose multiple routes instead of following a mandatory straight line. Keep the Root Grove near the initial route to introduce discovery and puzzle interaction, then allow branching to the other elemental regions.

These are layout recommendations, not existing map geometry. Distances, height ranges, foliage density and region boundaries must be measured and profiled in the actual map.

### Data Layers and Level Instances

Use Editor Data Layers to keep region work organized. Use Runtime Data Layers selectively for actual runtime progression. Build reusable temple interiors or repeated environmental structures as Level Instances where that improves authoring, while keeping the number of separately streamed levels modest.

World Partition is the preferred large-world streaming system for this UE5 project. Data Layers are part of the World Partition workflow; Runtime Data Layers can be changed at runtime, but excessive simultaneous loading can reduce performance. See the official Epic references linked below.

## Android performance checklist

- Build and profile a compact greybox before importing all foliage and decorative assets.
- Use LODs and instancing for repeated foliage, rocks and props.
- Keep collision simple; use dedicated simplified collision for traversal and interaction.
- Use texture sizes appropriate to mobile viewing distance.
- Avoid placing many overlapping translucent effects in the same view.
- Profile streaming, memory, draw calls and frame time on the actual target device.
- Do not assume desktop Nanite, shadow or post-process settings are safe for the Android target without testing.

## Acceptance checklist

- [ ] Project compiles in the target Unreal Engine 5.8 installation.
- [ ] The built-in atlas returns exactly 10 regions and 10 landmarks.
- [ ] Invalid DataTable override falls back to the built-in atlas without partial replacement.
- [ ] Region and landmark IDs are unique and map landmarks to known regions.
- [ ] A server-side region-volume overlap discovers the region for the correct PlayerState.
- [ ] A client cannot directly change discovery arrays.
- [ ] Visiting a landmark discovers its parent region and records the landmark.
- [ ] Duplicate region/landmark discovery does not increment revision twice.
- [ ] Exploration snapshot replicates to the owning client.
- [ ] A second player does not automatically inherit another player's discoveries.
- [ ] WBP_WorldAtlas reads region definitions and player exploration state.
- [ ] OpenWorldAtlas and CloseWorldAtlas behave correctly on keyboard, gamepad and touch UI.
- [ ] The Open World map is saved with World Partition enabled.
- [ ] Region Data Layers and landmark actors are assigned and reviewed in the World Partition/World Outliner.
- [ ] A two-client PIE session confirms independent exploration progress.

## Not included

- Actual L_EmeraldJungle_World.umap file or other binary Unreal assets.
- Landscape, foliage, water, sky, materials, meshes, sound, Niagara or final lighting.
- Widget Blueprint .uasset for WBP_WorldAtlas.
- Authored DataTable assets, portraits, map icons or landmark meshes.
- Runtime Data Layer switching logic for quest phase changes.
- Persistent exploration save data, backend profile synchronization or account unlocks.
- Android APK/AAB, device profiling results or Unreal compile logs.

## Official Unreal Engine references

- World Partition in Unreal Engine 5.8: https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine
- World Partition Data Layers: https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---data-layers-in-unreal-engine
- Level Instancing: https://dev.epicgames.com/documentation/en-us/unreal-engine/level-instancing-in-unreal-engine
- Level Streaming: https://dev.epicgames.com/documentation/en-us/unreal-engine/level-streaming-in-unreal-engine

## Next stage

Stage 12 — Emerald Jungle Traversal & Navigation: connect the region layout with traversal routes, navigation meshes, POI guidance and mobile-friendly world map navigation.
