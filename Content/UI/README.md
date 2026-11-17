# Great Spirit Selection UI

The C++ base widget is UJumanjiGreatSpiritSelectionWidget. Create WBP_GreatSpiritSelection as a Widget Blueprint child of this class after compiling the project.

Create a reusable WBP_GreatSpiritCard for each of the nine roster entries. Use the roster data for names, roles, elements and color accents. If a portrait is not assigned, show a numbered placeholder rather than reusing another character's art.

See Docs/STAGE_04_GREAT_SPIRIT_SELECT_UI.md for the layout, Blueprint graph wiring, preview-scene setup and acceptance checklist.

The actual .uasset widgets, portrait textures and preview render-target assets are not included yet.

## Adventure HUD

Create WBP_AdventureHUD as a Widget Blueprint child of UJumanjiAdventureHUDWidget. Configure this class as AdventureHUDWidgetClass on BP_JumanjiPlayerController. The local PlayerController creates it at BeginPlay.

Implement the OnAdventurePresentationChanged event to redraw the HUD from:
- GetChapterState;
- GetAdventureObjectives;
- GetCompletedObjectiveCount;
- GetTotalObjectiveCount;
- GetObjectiveProgress.

The widget base subscribes to replicated GameState chapter/objective changes and unsubscribes in NativeDestruct. It is a presentation layer only; it must not write puzzle or objective progress.


## Stage 18 — Ability HUD & Input Feedback

Create WBP_AbilityHUD as a Widget Blueprint child of UJumanjiAbilityHUDWidget. Set it in BP_JumanjiPlayerController as AbilityHUDWidgetClass. The local PlayerController adds it to the viewport at BeginPlay.

### Slot contract

The native widget returns five slot snapshots in stable order: PrimaryAttack, Active, Passive, Ultimate and Dodge. Use GetAbilitySlotStates to build the slot cards. Passive is informational and must not receive a manual activation button.

The four manual buttons call ActivateAbilitySlot with the matching EJumanjiAbilitySlot value. The widget forwards the ability's InputTag through the existing AJumanjiCharacter::ActivateAbilityByInputTag entry point, so touch controls share the same GAS activation path as hardware input.

### Resources and cooldowns

Use GetResourceState for Energy/Resonance values, maximums and percentages. Use CooldownRemaining, CooldownDuration and CooldownPercent for the radial overlay. These values are read from the PlayerState ASC and the granted Gameplay Ability cooldown state.

Set HUDDisplayName, HUDDescription, HUDIcon, HUDResourceType and HUDResourceCost in each ability Blueprint. Resource type/cost are UI metadata only and must mirror the matching Gameplay Effect configuration. Never subtract resources from the widget.

### Feedback and local target focus

Implement OnAbilityInputFeedback to show a brief result for activation, insufficient resource, cooldown or unavailable state.

SetTargetFocus accepts a local UI focus candidate only if the actor has an Ability System Component and is within MaxTargetFocusDistance. Use OnAbilityTargetFocusChanged for a target label/marker. This local focus marker is not gameplay target data and cannot bypass server-side ability validation.

See Docs/STAGE_18_ABILITY_HUD_INPUT_FEEDBACK.md for full widget wiring and mobile acceptance checks.


## Stage 19 — Ability Presentation Asset Pack

Stage 19 defines the presentation asset contract for all 45 Great Spirit ability slots. See:
- `Docs/STAGE_19_ABILITY_PRESENTATION_ASSET_PACK.md` for icon, material, audio, Gameplay Cue, VFX and mobile authoring specifications;
- `Content/UI/AbilityPresentation/AbilityPresentationManifest.csv` for the 45 stable AbilityId rows, target asset paths, slot cooldown references and proposed Great Spirit palettes;
- `Content/UI/AbilityPresentation/README.md` for the import and assignment workflow.

The CSV is a production manifest, not a claim that the referenced PNG/.uasset/Sound Wave/Niagara assets already exist. Create/import those binary assets in Unreal Editor. Stage 18 remains the source of live cooldown/resource state; UI must never mutate GAS attributes or apply effects.


## Stage 20 — Ability Presentation Runtime

After compiling the project:

- Import `Content/UI/AbilityPresentation/AbilityPresentationDataTable.csv` as DataTable `DT_AbilityPresentation` with row type `FJumanjiAbilityPresentationRow`.
- Configure `ConfiguredPresentationDataTable` on `UJumanjiAbilityPresentationSubsystem` in `DefaultGame.ini`.
- Set `HUDAbilityId` on each `GA_GS_[ROMAN]_[SLOT]` Blueprint class to the matching stable catalog ID.
- Read the added soft icon, palette, cooldown material, sound and Gameplay Cue metadata from `FJumanjiAbilityHUDSlotState`.
- Keep cooldown and cost presentation bound to the live GAS state from Stage 18. The manifest cooldown is reference-only.

See `Docs/STAGE_20_ABILITY_PRESENTATION_RUNTIME.md` and `Content/UI/AbilityPresentation/README.md` for the complete workflow.

## Stage 28 — Emerald Jungle Encounter HUD

Create `WBP_EmeraldJungleEncounterHUD` as a Widget Blueprint child of `UJumanjiWorldEncounterHUDWidget` in `/Game/UI/Encounter`. Assign it to `WorldEncounterHUDWidgetClass` on `BP_JumanjiPlayerController`. The local controller creates the widget at BeginPlay.

Implement these events:
- `OnEncounterHUDPresentationChanged`: render phase status, Great Spirit trial count, current/total enemy wave, active enemy count, boss Health percentage and boss phase.
- `OnEncounterPhaseCue`: play a short visual/audio cue when the replicated phase changes. Ignore Inactive if no idle announcement is wanted.
- `OnBossTelegraphPresentationChanged`: show or clear the warning using Pattern, Origin, Direction, Radius, Length and Width. Use a readable shape and a visible countdown.
 
Use `GetTelegraphTimeRemaining` or `GetTelegraphRemainingPercent` for the countdown. These values are derived from the synchronized GameState server time, not the phone's local wall clock.

The widget is a read-only presentation surface. Do not modify WorldEncounterState, boss health, enemy count, reward state or telegraph state from UMG. The real boss action is invoked by the server-side `BP_OnBossTelegraphExecutionRequested` event on the boss Blueprint; the visual warning itself does not apply damage.

Set up the boss Blueprint:
1. Call `BeginBossTelegraph` from server-owned boss logic with a stable AttackId from `Data/Encounter/EmeraldJungleBossTelegraphs.csv`.
2. Use `BP_OnBossTelegraphChanged` for the warning/clear VFX.
3. Use `BP_OnBossTelegraphExecutionRequested` to run the actual server-authoritative GAS/ability behavior.
4. Cancel on interruption or phase-specific transition.

See `Docs/STAGE_28_EMERALD_JUNGLE_ENCOUNTER_PRESENTATION_QA.md` for the full contract, validation and multiplayer/mobile QA checklist. The CSV is an authoring manifest, not an imported DataTable asset; the UMG Blueprint and all final icon/material/audio/Niagara assets still need to be created in Unreal Editor.
