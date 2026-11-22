# Ability HUD — Settings & QA (Stage 23)

## Native assets

- `UJumanjiGameUserSettings` — device-local subclass of `UGameUserSettings`.
- `UJumanjiAbilityHUDSettingsWidget` — parent class for `WBP_AbilityHUDSettings`.
- `FJumanjiAbilityHUDPreferences` — HUD scale, touch target scale, high contrast and reduced motion.

Set `GameUserSettingsClassName=/Script/JumanjiUE.JumanjiGameUserSettings` in `[/Script/Engine.Engine]`.

## Runtime

The local PlayerController applies saved preferences after creating `WBP_AbilityHUD`. The settings screen uses `GetSavedPreferences`, `ApplyAndSavePreferences` and `ResetAndSavePreferences`.

Persist only local presentation preferences here. Do not write them into the server-side player profile or PlayerState.

## Widget Blueprint

Create `/Game/UI/AbilityHUD/WBP_AbilityHUDSettings` with parent `UJumanjiAbilityHUDSettingsWidget`. Use controls for HUDScale, TouchTargetScale, HighContrast and ReduceMotion. Save on Apply, not on each slider movement.

See `Docs/STAGE_23_ABILITY_HUD_SETTINGS_PERSISTENCE_DEVICE_QA.md`.

## Device QA

- Matrix: `Data/UI/AbilityHUD_DeviceQAMatrix.csv`.
- Validator: `Scripts/validate_ability_hud_device_qa.py`.
- Tests: `Scripts/Tests/test_validate_ability_hud_device_qa.py`.
- Workflow: `.github/workflows/ability-hud-device-qa.yml`.

The validator checks matrix data only. Rows remain `NOT_RUN` until the corresponding physical-device test is performed.
