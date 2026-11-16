# Ability Presentation Assets — Stage 19

This folder defines the content contract for the 45 Great Spirit ability slots.

## Files

- `AbilityPresentationManifest.csv` — one row per stable AbilityId, with the Polish ability name, slot, icon path, proposed palette, cooldown reference, common UI feedback sound paths and Gameplay Cue tag.
- `Docs/STAGE_19_ABILITY_PRESENTATION_ASSET_PACK.md` — full art direction and Unreal Editor implementation checklist.

## Import order

1. Create `/Game/UI/AbilityPresentation/Icons`.
2. Produce/import the 45 transparent 256×256 icon masters using the manifest's IconAssetPath values.
3. Create `/Game/UI/AbilityPresentation/Materials/M_UI_AbilityCooldown_Radial` with the parameters in Stage 19.
4. Create the radial material instance `MI_UI_AbilityCooldown_Radial`.
5. Import the four short UI feedback Sound Waves in `/Game/UI/AbilityPresentation/Audio`.
6. Create/assign Gameplay Cue assets only after their actual class/asset implementations exist.
7. Assign each icon and matching HUD metadata to its GA_GS_[ROMAN]_[SLOT] Blueprint child.
8. Test with WBP_AbilityHUD on a physical Android device.

## Important

Manifest paths are planned destinations, not proof of existing assets. The repository does not yet contain the binary PNG, Material, Sound Wave, Gameplay Cue, Niagara or Widget Blueprint assets. Stage 18's GAS values remain authoritative; this folder never defines runtime costs or overrides cooldowns.
