# JUMANJI 2.111.1 — Stage 10: Great Spirit Trial Content Pack

## Goal

Provide the full nine-trial Emerald Jungle content definition: retain the existing Root Rune prototype and add native C++ trial classes for Tide, Stone, Storm, Ember, Dawn, Shadow, Life and Frost. Each trial has a stable objective ID and three server-validated puzzle steps. Add reusable soft-reference presentation hooks so Blueprint children can assign meshes, audio and a completion Niagara effect without hard-coding assets into the C++ module.

This stage defines puzzle content and asset integration points. It does not claim that final 3D art, audio, Niagara systems, maps or Blueprint .uasset files have been authored.

## Nine Great Spirit trials

| Spirit | Native actor | Objective ID | Display name | Steps |
|---|---|---|---|---:|
| I — Root | AJumanjiRootRuneTrial | TRIAL_ROOT_RUNES | Próba Korzeni | 3 |
| II — Tide | AJumanjiTideChannelTrial | TRIAL_TIDE_CHANNELS | Próba Przypływu | 3 |
| III — Stone | AJumanjiStoneBalanceTrial | TRIAL_STONE_BALANCE | Próba Kamienia | 3 |
| IV — Storm | AJumanjiStormWindTrial | TRIAL_STORM_WINDS | Próba Burzy | 3 |
| V — Ember | AJumanjiEmberSequenceTrial | TRIAL_EMBER_SEQUENCE | Próba Żaru | 3 |
| VI — Dawn | AJumanjiDawnMirrorTrial | TRIAL_DAWN_MIRRORS | Próba Świtu | 3 |
| VII — Shadow | AJumanjiShadowPatternTrial | TRIAL_SHADOW_PATTERN | Próba Cienia | 3 |
| VIII — Life | AJumanjiLifeGardenTrial | TRIAL_LIFE_GARDEN | Próba Życia | 3 |
| IX — Frost | AJumanjiFrostGateTrial | TRIAL_FROST_GATES | Próba Mrozu | 3 |

The nine chapter objectives must use the exact IDs above and RequiredProgress=3. The chapter director's objective count must equal its TrialActors count, with each placed actor referenced once.

## Puzzle content

### I — Root: Próba Korzeni
Existing AJumanjiRootRuneTrial prototype.
1. Seed — RUNE_SEED
2. Sprout — RUNE_SPROUT
3. Tree — RUNE_TREE

### II — Tide: Próba Przypływu
Native class: AJumanjiTideChannelTrial.
1. Source — TIDE_SOURCE
2. Channel — TIDE_CHANNEL
3. Reservoir — TIDE_RESERVOIR

### III — Stone: Próba Kamienia
Native class: AJumanjiStoneBalanceTrial.
1. Base — STONE_BASE
2. Support — STONE_PILLAR
3. Crown — STONE_CROWN

### IV — Storm: Próba Burzy
Native class: AJumanjiStormWindTrial.
1. Breeze — STORM_BREEZE
2. Vortex — STORM_VORTEX
3. Clear sky — STORM_CLEAR_SKY

### V — Ember: Próba Żaru
Native class: AJumanjiEmberSequenceTrial.
1. Spark — EMBER_SPARK
2. Flame — EMBER_FLAME
3. Beacon — EMBER_LANTERN

### VI — Dawn: Próba Świtu
Native class: AJumanjiDawnMirrorTrial.
1. Prism — DAWN_PRISM
2. Mirror — DAWN_MIRROR
3. Gate — DAWN_GATE

### VII — Shadow: Próba Cienia
Native class: AJumanjiShadowPatternTrial.
1. Shade — SHADOW_SHADE
2. Eclipse — SHADOW_ECLIPSE
3. Return — SHADOW_RETURN

### VIII — Life: Próba Życia
Native class: AJumanjiLifeGardenTrial.
1. Seed — LIFE_SEED
2. Water — LIFE_WATER
3. Bloom — LIFE_BLOOM

### IX — Frost: Próba Mrozu
Native class: AJumanjiFrostGateTrial.
1. Water — FROST_WATER
2. Freeze — FROST_FREEZE
3. Crystal — FROST_CRYSTAL

Every step offers three unique option IDs. The correct option is checked only on the server by AJumanjiTrialActor; wrong answers do not advance objective progress. The actor replicates its trial state, current step index and revision, not the correct answer.

## Presentation asset hooks

AJumanjiTrialActor now exposes Blueprint-readable presentation fields:
- TrialDisplayName and TrialDescription;
- TrialDisplayMesh (TSoftObjectPtr<UStaticMesh>);
- ActivationSound (TSoftObjectPtr<USoundBase>);
- StepAcceptedSound (TSoftObjectPtr<USoundBase>);
- CompletionSound (TSoftObjectPtr<USoundBase>);
- CompletionEffect (TSoftObjectPtr<UNiagaraSystem>).

The native base stores soft references only. It does not automatically load, spawn or play these assets. In each Blueprint child, use asynchronous soft-reference loading when the interaction begins; play ActivationSound after the authoritative Available state is observed, StepAcceptedSound after an accepted replicated revision increase, CompletionSound and CompletionEffect once after Completed is observed. Do not play success feedback merely because a client button was pressed or an RPC was queued.

## Unreal Editor authoring workflow

1. Compile the C++ project in Unreal Engine 5.8.
2. Create the Emerald Jungle test map and place one AJumanjiAdventureChapterDirector.
3. Create one Blueprint child for each native trial actor:
   - BP_Trial_Root
   - BP_Trial_Tide
   - BP_Trial_Stone
   - BP_Trial_Storm
   - BP_Trial_Ember
   - BP_Trial_Dawn
   - BP_Trial_Shadow
   - BP_Trial_Life
   - BP_Trial_Frost
4. Assign a unique mesh/material treatment, interaction marker, activation sound, step feedback sound, completion sound and optional Niagara completion effect to each child.
5. Place each trial in the level and ensure its collision/interaction location is reachable by the player.
6. Configure ChapterObjectives with the nine exact ObjectiveIds in the table and RequiredProgress=3 for each.
7. Add all nine placed actors to TrialActors exactly once. The actor ObjectiveId and objective ID must match.
8. Configure the chapter title and description, then start the chapter from server-authoritative level logic.
9. Create or update the trial interaction widget to read the prompt, option IDs, state and revision. Submit choices through the owning PlayerController.
10. Bind the chapter HUD to the replicated objective and chapter snapshots.

## Localization

The current native prompt and display text are prototype FText literals in Polish. Before production localization, move player-facing strings into String Tables and use stable localization keys. Do not translate or localize ObjectiveId, StepId or OptionId identifiers; they are protocol/content keys and must remain stable.

## Acceptance checklist

- [ ] Project compiles in Unreal Engine 5.8.
- [ ] All eight new native trial classes are discoverable as Blueprint parents.
- [ ] Each new trial exposes exactly three steps.
- [ ] Each step has a unique StepId, three unique options and one valid correct option.
- [ ] All nine objective IDs match the chapter design table.
- [ ] Each objective uses RequiredProgress=3.
- [ ] Chapter director accepts nine unique matching actor references.
- [ ] Chapter director rejects duplicate, missing or mismatched actor/objective pairs.
- [ ] Wrong options do not change objective progress.
- [ ] Correct options advance exactly one step and replicate to a second client.
- [ ] Stale revisions cannot advance a later step.
- [ ] Blueprint children can assign all soft-reference presentation fields.
- [ ] Audio/VFX feedback occurs only after observing the authoritative replicated state.
- [ ] A two-client PIE session completes all nine trials and completes the chapter only after all nine objectives reach 3/3.

## Not included

- Actual .uasset Blueprint child assets, map, meshes, materials, animation, audio, Niagara systems or final UI.
- Asynchronous asset-loading implementation and asset bundles.
- String Table localization files or translations beyond the Polish prototype text.
- Persistent quest journal, save-game, inventory rewards or account progression.
- Dedicated-server deployment, Unreal Engine compile logs, multiplayer PIE evidence or Android APK/AAB.

## Next stage

Stage 11 — Emerald Jungle World Assembly: assemble the map, nine trial spaces, traversal routes, landmarks, environmental storytelling and level streaming setup.