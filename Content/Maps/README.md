# Maps

## Prototype map

The initial prototype map may be saved as L_Jumanji_Prototype.umap.

## Emerald Jungle world map

Stage 11 target map: Content/Maps/L_EmeraldJungle_World.umap.

Create it in Unreal Editor using the Open World map template. Verify World Partition and runtime streaming are enabled. Keep one persistent world map and organize the central hub and nine elemental regions using Editor Data Layers. Use Runtime Data Layers only for true runtime state changes.

The map file is a binary Unreal asset. Save it in the project and commit it using Git LFS after the repository's LFS rules are configured.

See Docs/STAGE_11_EMERALD_JUNGLE_WORLD_ASSEMBLY.md for the region layout, ten atlas landmarks, region volumes, World Partition setup and acceptance checklist.
