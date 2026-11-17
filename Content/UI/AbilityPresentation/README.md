# Ability Presentation Assets — Stage 19 and Stage 20

This folder contains the presentation source manifest and the import-ready DataTable source for the 45 Great Spirit ability slots.

## Files

- `AbilityPresentationManifest.csv` — production source manifest with stable AbilityId values, Polish display names, palettes, icon motifs, planned asset paths and reference cooldown values.
- `AbilityPresentationDataTable.csv` — generated Unreal DataTable import file. It prepends a `Name` row-key column and preserves `AbilityId` as a normal data field.
- `Docs/STAGE_19_ABILITY_PRESENTATION_ASSET_PACK.md` — icon, material, audio, Gameplay Cue, VFX and mobile authoring specification.
- `Docs/STAGE_20_ABILITY_PRESENTATION_RUNTIME.md` — runtime DataTable reader, class ID binding and HUD integration.

## Import the DataTable

1. Compile the project in Unreal Engine 5.8 so `FJumanjiAbilityPresentationRow` is available.
2. In Content Browser, create or open `/Game/UI/AbilityPresentation`.
3. Import `Content/UI/AbilityPresentation/AbilityPresentationDataTable.csv`.
4. Select `FJumanjiAbilityPresentationRow` as the row struct.
5. Use the first `Name` column as the DataTable row key. Keep `AbilityId` as its own imported field.
6. Save as `DT_AbilityPresentation`.
7. Add this section to `Config/DefaultGame.ini`:

```ini
[/Script/JumanjiUE.JumanjiAbilityPresentationSubsystem]
ConfiguredPresentationDataTable=/Game/UI/AbilityPresentation/DT_AbilityPresentation.DT_AbilityPresentation
```

8. Assign the matching `HUDAbilityId` in every `GA_GS_[ROMAN]_[SLOT]` Blueprint default.

## Runtime contract

- `UJumanjiAbilityPresentationSubsystem` returns the matching display name, soft icon/material/audio references, palette, Gameplay Cue tag and reference cooldown.
- Every returned row is validated against the Stage 16 ability catalog by `AbilityId`, `CharacterId` and `SlotId`.
- A non-empty `HUDAbilityId` on a Gameplay Ability class must match its catalog row.
- The five-slot HUD receives the presentation data in `FJumanjiAbilityHUDSlotState`.
- Use asynchronous asset loading in `WBP_AbilityHUD`. Do not synchronously load 45 icons when the HUD opens.
- Missing rows or missing binary assets fall back to class metadata/placeholder presentation.

## Authority and asset status

The CSV cooldown value is a reference only. Runtime cooldown and resource values still come from GAS. The subsystem does not activate Gameplay Abilities, play audio automatically or execute Gameplay Cues.

The CSV files do not create the referenced binary PNG, Material, Sound Wave, Gameplay Cue, Niagara or Widget Blueprint assets. Those assets must be authored/imported in Unreal Editor and tested on an Android device.
