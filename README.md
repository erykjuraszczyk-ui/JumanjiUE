# JUMANJI 2.111.1 — Unreal Engine 5.8

Open-world third-person adventure RPG prototype with a shared-world direction and the original Living Board event system.

## Project stages
- Stage 01 — UE5 Foundation: native C++ project, third-person character, GameMode/GameState and server-authoritative Living Board state.
- Stage 02 — Great Spirit Roster: nine playable-character definitions, server-validated selection and visual art brief.
- Stage 03 — Great Spirit Presentation Core: binds replicated PlayerState selection to the pawn and applies the matching skeletal mesh when an asset is assigned.
- Stage 04 — Great Spirit Select UI: C++ UMG widget base, roster card data flow, preview/confirm/cancel behavior and controller open/close methods.
- Stage 05 — Great Spirit Ability Core: PlayerState-owned GAS, replicated attributes, input-tag activation, five-slot loadouts and optional validated DataTable roster overrides.
- Stage 06 — Great Spirit Combat System: server-authoritative melee hit detection, GAS damage effects, GAS-enabled enemies, perception-driven AI and three-phase boss state.
- Stage 07 — Living Board Adventure Core: validated server-owned objective sets, replicated puzzle progress, and adventure completion gated by all objectives.
- Stage 08 — Emerald Jungle Trial Actors: server-routed puzzle interactions, revision-checked submissions, replicated trial state and the first Root Rune sequence.
- Stage 09 — Emerald Jungle Chapter Flow & Adventure UI: server-side chapter orchestration, replicated chapter phase, director-to-trial validation and a Blueprint-facing HUD base.
- Stage 10 — Great Spirit Trial Content Pack: eight additional native environmental trials, nine matching objective contracts and Blueprint presentation asset hooks.
- Stage 11 — Emerald Jungle World Assembly: ten-region world atlas, server-authoritative discovery volumes and landmarks, per-player replicated exploration progress and a Blueprint map HUD base.
- Stage 12 — Emerald Jungle Traversal & Navigation: local route selection for discovered regions/landmarks, NavMesh path queries, world-space destination anchors and mobile navigation UI integration hooks.
- Stage 13 — Player Profile & Exploration Persistence: versioned profile snapshot, server-bound profile identity, async local SaveGame and persistence of Great Spirit choice, exploration and completed chapters.
- Stage 14 — Profile Progression & Inventory Persistence: schema v2 with v1 migration, account and Great Spirit levels, currencies, upgrade materials, logical ability-slot persistence and idempotent all-or-nothing profile transactions.
- Stage 15 — Progression Gameplay Integration & Reward Tables: Great Spirit level-derived GAS base attributes, idempotent chapter completion rewards, server-validated material/currency upgrade recipes and optional reward/recipe DataTable overrides.
- Stage 16 — Saved Ability Loadout Runtime: allowlisted AbilityId-to-GAS class catalog, profile slot restoration, server-validated equipment RPC and ASC loadout replacement without duplicate grants.
- Stage 17 — Great Spirit Ability Content Pack: server-authoritative configured ability execution, target/effect modes, montage and Gameplay Cue hooks, plus a 45-slot production matrix.
- Stage 18 — Ability HUD & Input Feedback: mobile slot controls, GAS cooldown/resource indicators, activation feedback and local target-focus presentation.
- Stage 19 — Ability Presentation Asset Pack: 45-slot icon manifest, nine Great Spirit palettes, radial cooldown material specification, UI audio feedback and Gameplay Cue/VFX authoring contract.
- Stage 20 — Ability Presentation Runtime Integration: validated manifest DataTable reader, stable AbilityId binding, soft-reference presentation data and HUD integration without changing GAS authority.
- Stage 21 — Emerald Jungle World Authoring Pack: import-ready region/landmark DataTable CSVs, 29-row greybox placement ledger and World Partition assembly manifest.

## The nine playable Great Spirits
1. Great Spirit I — Duch Korzeni (Root / Guardian)
2. Great Spirit II — Duch Przypływu (Tide / Support)
3. Great Spirit III — Duch Kamienia (Stone / Vanguard)
4. Great Spirit IV — Duch Burzy (Storm / Controller)
5. Great Spirit V — Duch Żaru (Ember / Striker)
6. Great Spirit VI — Duch Świtu (Dawn / Ranger)
7. Great Spirit VII — Duch Cienia (Shadow / Duelist)
8. Great Spirit VIII — Duch Życia (Life / Healer)
9. Great Spirit IX — Duch Mrozu (Frost / Sentinel)

The names, roles, elements and ability concepts are original gameplay proposals based on the supplied visual reference. The image establishes the nine portrait identities and visual direction; it does not specify gameplay statistics or abilities.

## Requirements
- Unreal Engine 5.8;
- C++ compiler/toolchain supported by the installed UE version;
- Android SDK/NDK/JDK configured for UE 5.8 when packaging Android builds.

## Open
1. Clone the repository and check out the desired stage branch.
2. Open JumanjiUE.uproject with Unreal Engine 5.8.
3. Generate project files and compile the project when prompted.
4. Create Content/Maps/L_Jumanji_Prototype.umap and set it as the default map.
5. Press Play to test the initial third-person character.

## Stage documentation
- Docs/STAGE_01_FOUNDATION.md — project setup and first prototype checklist.
- Docs/STAGE_02_GREAT_SPIRIT_ROSTER.md — nine-character roster, selection flow and acceptance criteria.
- Docs/STAGE_03_GREAT_SPIRIT_PRESENTATION.md — runtime mesh presentation and UMG integration contract.
- Docs/STAGE_04_GREAT_SPIRIT_SELECT_UI.md — selection layout, Blueprint wiring and acceptance criteria.
- Docs/STAGE_05_GREAT_SPIRIT_ABILITY_CORE.md — GAS architecture, slots, input tags and acceptance checklist.
- Docs/Design/GreatSpiritAbilityDesign.md — proposed five-slot ability kits for each spirit.
- Content/Abilities/README.md — Blueprint ability and Gameplay Effect asset authoring guide, including the Stage 16 AbilityId catalog and saved-loadout integration.
- Docs/STAGE_16_SAVED_ABILITY_LOADOUT_RUNTIME.md — allowlisted ability resolver, profile restoration, server equipment RPC, ASC grant lifecycle and acceptance checklist.
- Docs/STAGE_17_GREAT_SPIRIT_ABILITY_CONTENT_PACK.md — configured ability runtime, costs/cooldowns, Blueprint/Gameplay Effect authoring and acceptance checklist.
- Docs/STAGE_18_ABILITY_HUD_INPUT_FEEDBACK.md — mobile ability HUD, slot input, Energy/Resonance indicators, cooldown feedback and local target focus.
- Docs/STAGE_19_ABILITY_PRESENTATION_ASSET_PACK.md — 45 ability icon definitions, palettes, cooldown material, feedback audio/VFX contract and Unreal Editor acceptance checklist.
- Docs/STAGE_20_ABILITY_PRESENTATION_RUNTIME.md — manifest DataTable import, AbilityId class binding, HUD presentation metadata and acceptance checklist.
- Content/UI/AbilityPresentation/README.md — asset import, naming and per-ability assignment workflow.
- Content/UI/AbilityPresentation/AbilityPresentationManifest.csv — 45 stable ability presentation records.
- Content/UI/AbilityPresentation/AbilityPresentationDataTable.csv — DataTable import copy with Name row keys and 45 stable IDs.
- Docs/Design/GreatSpiritAbilityProductionMatrix.md — production specification for all 45 Great Spirit ability slots.
- Docs/STAGE_06_GREAT_SPIRIT_COMBAT_SYSTEM.md — combat runtime, asset setup, flow and acceptance tests.
- Docs/Design/FirstCombatEncounter.md — first three-phase boss encounter design.
- Content/Combat/README.md — damage effect, enemy, boss and AI setup.
- Docs/STAGE_07_LIVING_BOARD_ADVENTURE_CORE.md — objective replication, server validation, nine elemental trial concepts and acceptance checklist.
- Content/Adventure/README.md — adventure asset layout and Blueprint integration contract.
- Docs/STAGE_08_EMERALD_JUNGLE_TRIAL_ACTORS.md — native trial runtime, Root Rune Sequence, multiplayer interaction and acceptance checklist.
- Docs/STAGE_09_EMERALD_JUNGLE_CHAPTER_FLOW.md — chapter director, replicated chapter state, HUD integration and nine-trial authoring contract.
- Docs/STAGE_10_GREAT_SPIRIT_TRIAL_CONTENT_PACK.md — nine trial definitions, puzzle sequences, presentation hooks and Blueprint authoring workflow.
- Docs/STAGE_11_EMERALD_JUNGLE_WORLD_ASSEMBLY.md — World Partition map layout, ten-region atlas, Data Layers, discovery/landmark setup and acceptance checklist.
- Docs/STAGE_21_EMERALD_JUNGLE_WORLD_AUTHORING_PACK.md — DataTable import, greybox coordinates, actor placement and validation workflow.
- Content/World/README.md — world atlas, navigation and profile-persistence setup.
- Content/Adventure/Trials/README.md — Blueprint trial and UMG authoring guide.
- Content/UI/README.md — Widget Blueprint asset setup.
- Docs/Art/GreatSpiritCharacterArtBrief.md — individual visual targets and character art requirements.
- Content/Characters/README.md — Unreal character asset naming and import plan.
- Docs/STAGE_12_EMERALD_JUNGLE_TRAVERSAL_NAVIGATION.md — NavMesh, World Partition navigation, route guidance, POI targeting and mobile map integration.
- Docs/STAGE_13_PLAYER_PROFILE_EXPLORATION_PERSISTENCE.md — initial profile schema v1, trusted identity binding, exploration/chapter persistence and acceptance tests.
- Docs/STAGE_14_PROFILE_PROGRESSION_INVENTORY.md — schema v2 migration, account/character levels, currency, inventory, logical loadouts, transaction validation and acceptance tests.
- Docs/STAGE_15_PROGRESSION_GAMEPLAY_REWARDS.md — GAS attribute progression, chapter reward transactions, Great Spirit upgrade recipes, DataTable paths and acceptance tests.
- Content/Progression/README.md — reward and upgrade DataTable authoring guide.
- Content/Inventory/README.md — catalog IDs, material stacks and reward transaction integration.
- [Docs/UE5_BUILD_PIPELINE.md](Docs/UE5_BUILD_PIPELINE.md) — GitHub Actions workflow, Windows self-hosted runner and Android toolchain setup.

## Current implementation
- native Unreal Engine 5.8 C++ project descriptor and targets;
- third-person character with camera and starter movement input;
- GameMode and replicated GameState;
- server-authoritative Living Board event state;
- nine Great Spirit definitions in UJumanjiCharacterRosterSubsystem;
- server-validated player selection replicated through AJumanjiPlayerState;
- pawn presentation binding that resolves the selected definition and applies its skeletal mesh when a soft asset reference is configured;
- UJumanjiGreatSpiritSelectionWidget base with roster population, preview/confirm/cancel flow and PlayerState confirmation updates;
- PlayerController methods to open and close the configured selection Widget Blueprint;
- PlayerState-owned replicated GAS ASC and AttributeSet;
- Great Spirit loadout grants validated by slot and input tag;
- primary, active, ultimate and dodge input routing;
- optional nine-row DataTable override for designer-assigned ability and character assets;
- server-only melee sweep ability with Defense-aware damage calculation and Data.Damage SetByCaller effect;
- hit Gameplay Event dispatch to target ASC;
- GAS-enabled enemy base with damage/defeat callbacks;
- perception-based enemy AI pursuit and attack ability requests;
- three-phase boss health thresholds and replicated phase tags;
- replicated Living Board adventure objective snapshots with a monotonically increasing revision;
- server-side validation for objective definitions, progress increments and completion gates;
- adventure lifecycle API in UJumanjiBoardWorldSubsystem, including StartBoardAdventure, AdvanceBoardAdventureObjective and CompleteBoardAdventure;
- abstract AJumanjiTrialActor with replicated state, step index and revision;
- eight additional Great Spirit trial classes with three server-validated puzzle steps each;
- Blueprint-readable soft-reference hooks for trial meshes, activation/step/completion audio and Niagara completion effects;
- AJumanjiRootRuneTrial with a three-step Seed → Sprout → Tree example;
- PlayerController-owned server RPC for trial requests, with server-side range, option and stale-revision validation;
- server-side AJumanjiAdventureChapterDirector for chapter startup, trial actor preflight and completion orchestration;
- replicated FJumanjiAdventureChapterState with Active/Completed phases and revision;
- UJumanjiAdventureHUDWidget Blueprint base with objective/chapter event subscriptions and progress getters;
- local PlayerController creation of the configured adventure HUD widget;
- UJumanjiWorldAtlasSubsystem with a built-in 10-region/10-landmark catalog and validated DataTable overrides;
- source CSVs for 10 atlas region rows and 10 landmark rows, plus a 29-actor Emerald Jungle greybox placement ledger (Stage 21);
- server-authoritative region volume and landmark discovery;
- per-player replicated exploration snapshot on AJumanjiPlayerState;
- UJumanjiWorldAtlasWidget base and PlayerController OpenWorldAtlas/CloseWorldAtlas API;
- UJumanjiWorldNavigationComponent on the local PlayerController, with discovery-validated region/landmark targets and local NavMesh route data.
- UJumanjiPlayerProfileSubsystem with versioned asynchronous SaveGame profile slots for the current single-server prototype.
- profile restore on PostLogin using server-bound platform identity where available.
- versioned Player Profile schema v2 with automatic in-memory migration from Stage 13 schema v1;
- asynchronous persistence of selected Great Spirit, exploration, completed chapters, account progression and character progression;
- source-controlled currency catalog and ten upgrade-material item definitions;
- server-authoritative inventory/currency transaction validation with duplicate TransactionId rejection;
- replicated account level/XP, inventory stacks, wallet balances, nine Great Spirit progression entries and logical ability loadout slots;
- profile-level transaction ledger persisted with the balances it protects;
- level-derived MaxHealth, MaxEnergy, MaxResonance, AttackPower and Defense base values applied to the selected Great Spirit's GAS AttributeSet;
- health, energy and resonance percentage preserved when the selected spirit's level changes its maximum values;
- UJumanjiProgressionRewardSubsystem with validated chapter reward definitions and nine spirit-specific upgrade recipes;
- optional DT_ChapterRewards and DT_SpiritUpgradeRecipes DataTable overrides with native starter catalog fallback;
- idempotent chapter completion reward transactions and retry of configured missing rewards after profile restoration;
- server-routed Great Spirit upgrade RPC using all-or-nothing currency/material/XP profile transactions;
- UJumanjiAbilityCatalogSubsystem with 45 stable Great Spirit/slot identifiers and optional validated DT_GreatSpiritAbilityCatalog override;
- server-side validation of saved AbilityId values against CharacterId, SlotId, UJumanjiGameplayAbility class, AbilitySlot and InputTag;
- restore-time filtering of unknown or incompatible saved ability slots;
- server RPC to equip catalog-approved abilities into profile schema v2;
- runtime resolution of saved ability IDs into approved soft Gameplay Ability classes;
- UJumanjiConfiguredEffectAbility base with server-only Self/ForwardSweep/RadialPulse/Dash execution, PvE target filtering, SetByCaller target/self Gameplay Effects, AttackPower scaling and Gameplay Cue/Animation Montage hooks;
- PlayerState-owned ASC replacement of the Great Spirit loadout, tracking and clearing only the previous granted ability handles, and avoiding duplicate grants when the same loadout is reapplied;
- UJumanjiAbilityHUDWidget with five-slot snapshots, GAS cooldown timing, Energy/Resonance values, activation feedback and local target-focus indicator;
- HUD presentation metadata on UJumanjiGameplayAbility for display name, description, icon, stable HUDAbilityId and matching resource-cost display;
- UJumanjiAbilityPresentationSubsystem with validated ability presentation DataTable lookup and soft-reference conversion;
- ability HUD slot snapshots enriched with per-ability palette, icon, cooldown material, audio references and Gameplay Cue tag.
- PlayerController creation of the configured ability HUD for the local player.

## Not yet included
This is a source-code foundation, not a finished game or installable Android build. The repository still needs:
- saved Unreal maps, including Content/Maps/L_EmeraldJungle_World.umap;
- imported DT_Jumanji_WorldRegions and DT_Jumanji_WorldLandmarks DataTable .uasset files (CSV source files are now present);
- final portrait textures and 3D character models;
- rigs, animation blueprints and facial animation;
- final touch-first Widget Blueprint assets, icons, UI materials and Enhanced Input mapping assets;
- actual UMG Widget Blueprint .uasset files and card layout;
- the configured Emerald Jungle chapter director instance and nine placed trial actors;
- imported DT_AbilityPresentation DataTable and configured subsystem path;
- final Ability Blueprint HUDAbilityId assignments and binary ability icons/material/audio assets;
- 45 character-specific ability Blueprint assets and configured Gameplay Effects;
- configured enemy/boss meshes, animations and initialization effects;
- final boss attack patterns and encounter reward service;
- Blueprint children, visuals and playable implementations for all nine Great Spirit trials;
- the Root Rune trial Blueprint, rune meshes, UI widget, map and presentation assets;
- trial-specific reward rows and automatic reward orchestration for all nine trial actors;
- persistent in-progress objective state;
- a production profile backend, account authentication, cross-shard synchronization and profile conflict handling;
- authored NavMesh data, world-space DataTable anchors and traversal link actors/animations;
- asynchronous asset preloading and device profiling;
- production account authentication, secure economy backend and cross-shard transaction processing;
- dedicated-server deployment;
- a packaged and device-tested APK/AAB.

The supplied reference image is used as an art direction source. The binary image itself and Unreal binary assets are not added by the text-file GitHub workflow; add them through Git LFS with provenance/license notes.

## Build verification
The source has not yet been compiled in an Unreal Engine 5.8 environment during these repository changes. The GitHub Actions workflow and `Scripts/Build-UE5.ps1` are included. The workflow requires a configured Windows self-hosted runner with a local UE 5.8 installation before it can execute. Complete the stage acceptance checklists locally before treating the project as build-verified. A successful C++ build alone does not prove that the game is playable or produce an installable Android package.
