# Great Spirit character assets

Runtime metadata for Great Spirit I–IX is currently provided by UJumanjiCharacterRosterSubsystem.

Create and import these Unreal assets after the C++ project compiles:

- Portraits/T_GreatSpirit_I_Portrait through Portraits/T_GreatSpirit_IX_Portrait;
- Meshes/SK_GreatSpirit_I through Meshes/SK_GreatSpirit_IX;
- a shared skeleton and animation blueprint where compatible;
- material instances using each character's palette;
- per-character Niagara effects only when the related ability is implemented.

The roster definition has soft references ready for portrait textures, skeletal meshes and Gameplay Ability classes. Assign those references once the real .uasset files exist.

See Docs/STAGE_02_GREAT_SPIRIT_ROSTER.md and Docs/Art/GreatSpiritCharacterArtBrief.md.
