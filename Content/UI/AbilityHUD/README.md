# Ability HUD Blueprint Assets — Stage 21

This directory is the Unreal Editor destination for the Stage 21 Widget Blueprint assets. The text repository contains the native widget bases and their binding contract; it does not contain generated binary .uasset files.

## Required Widget Blueprints

Create:

- `/Game/UI/AbilityHUD/WBP_AbilityHUD` — child of `UJumanjiAbilityHUDWidget`.
- `/Game/UI/AbilityHUD/Slots/WBP_AbilitySlot` — child of `UJumanjiAbilityHUDSlotWidget`.

Assign `WBP_AbilityHUD` to `AbilityHUDWidgetClass` on `BP_JumanjiPlayerController`.

## WBP_AbilityHUD child names

Add five instances of WBP_AbilitySlot and set their variable names exactly:

- `PrimarySlotWidget`
- `ActiveSlotWidget`
- `PassiveSlotWidget`
- `UltimateSlotWidget`
- `DodgeSlotWidget`

These names bind to the parent's `BindWidgetOptional` properties.

## WBP_AbilitySlot child names

The following names bind to optional native properties:

- `SlotButton` — Button
- `AbilityIcon` — Image
- `CooldownOverlay` — Image
- `CooldownFallbackBar` — Progress Bar
- `AbilityNameText` — Text Block
- `CooldownText` — Text Block
- `ResourceCostText` — Text Block
- `SlotStateText` — Text Block
- `ActivationPulseAnimation` — Widget Animation (optional)

Set the default brush on AbilityIcon to a neutral placeholder. The C++ slot widget preserves this brush while the configured icon is streamed.

## Runtime behavior

- The parent HUD updates each child from the stable slot enum.
- SlotButton broadcasts a local slot press; the parent routes it through the existing GAS input-tag activation path.
- Passive is always non-interactive.
- Icon and cooldown material are loaded asynchronously.
- The cooldown overlay reads the current GAS cooldown fraction. If no material is available, CooldownFallbackBar is used.
- Audio is played only from explicit per-slot feedback events.
- No widget writes GAS attributes or applies Gameplay Effects.

See `Docs/STAGE_21_ABILITY_PRESENTATION_BLUEPRINT_INTEGRATION.md` for the complete hierarchy, configuration sequence, mobile layout guidance and acceptance checklist.
