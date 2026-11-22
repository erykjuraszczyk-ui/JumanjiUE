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

## Stage 22 — Mobile HUD Polish & Accessibility

The native HUD exposes `ApplyMobileAccessibilitySettings(HUDScale, TouchTargetScale, HighContrast, ReduceMotion)`.

- HUD scale is clamped to 0.75–1.35.
- Individual touch-slot scale is clamped to 0.85–1.5 and scales the button hit target with its visuals.
- High Contrast switches slot text to white/yellow semantic colors; keep an opaque/dark backing panel in the Widget Blueprint to preserve contrast.
- Reduce Motion disables the activation pulse animation. Gameplay feedback and GAS state are unchanged.

In `WBP_AbilityHUD`, wrap the complete HUD content in a `SafeZone` widget. Use anchors/constraints rather than fixed screen pixels. Build landscape and portrait arrangements with a Widget Switcher or responsive Canvas/SizeBox branches. Keep the passive card noninteractive.

Recommended authoring baseline: at least 48 logical UMG units for ordinary touch targets, larger for Primary. Validate actual physical touch size after the Android DPI curve is applied. Do not interpret UMG logical units as raw device pixels.

Persist the four user choices from the settings screen through the game's user-settings/SaveGame layer; this widget API applies values but does not write a profile by itself. See `Docs/STAGE_22_MOBILE_HUD_POLISH_ACCESSIBILITY.md`.


## Stage 23 — Settings Persistence

The engine's configured Game User Settings class is `UJumanjiGameUserSettings`. It stores the four local accessibility preferences in `GameUserSettings.ini`; these values are not part of the server-side player profile.

Create `/Game/UI/AbilityHUD/Settings/WBP_AbilityHUDSettings` with parent `UJumanjiAbilityHUDSettingsWidget`. The native base provides:
- `GetSavedPreferences`
- `ApplyAndSavePreferences`
- `ResetAndSavePreferences`
- `OnPreferencesSaved`

Apply and Reset save locally and ask the owning `AJumanjiPlayerController` to reapply the values to the live HUD. The blueprint should update slider/toggle display from `OnPreferencesSaved` and avoid writing on every slider movement.
