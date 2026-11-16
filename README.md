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
- Content/Abilities/README.md — Blueprint ability and Gameplay Effect asset authoring guide.
- Docs/STAGE_06_GREAT_SPIRIT_COMBAT_SYSTEM.md — combat runtime, asset setup, flow and acceptance tests.
- Docs/Design/FirstCombatEncounter.md — first three-phase boss encounter design.
- Content/Combat/README.md — damage effect, enemy, boss and AI setup.
- Docs/STAGE_07_LIVING_BOARD_ADVENTURE_CORE.md — objective replication, server validation, nine elemental trial concepts and acceptance checklist.
- Content/Adventure/README.md — adventure asset layout and Blueprint integration contract.
- Docs/STAGE_08_EMERALD_JUNGLE_TRIAL_ACTORS.md — native trial runtime, Root Rune Sequence, multiplayer interaction and acceptance checklist.
- Docs/STAGE_09_EMERALD_JUNGLE_CHAPTER_FLOW.md — chapter director, replicated chapter state, HUD integration and nine-trial authoring contract.
- Docs/STAGE_10_GREAT_SPIRIT_TRIAL_CONTENT_PACK.md — nine trial definitions, puzzle sequences, presentation hooks and Blueprint authoring workflow.
- Docs/STAGE_11_EMERALD_JUNGLE_WORLD_ASSEMBLY.md — World Partition map layout, ten-region atlas, Data Layers, discovery/landmark setup and acceptance checklist.
- Content/World/README.md — world atlas code and DataTable setup.
- Content/Adventure/Trials/README.md — Blueprint trial and UMG authoring guide.
- Content/UI/README.md — Widget Blueprint asset setup.
- Docs/Art/GreatSpiritCharacterArtBrief.md — individual visual targets and character art requirements.
- Content/Characters/README.md — Unreal character asset naming and import plan.
- Docs/STAGE_12_EMERALD_JUNGLE_TRAVERSAL_NAVIGATION.md — NavMesh, World Partition navigation, route guidance, POI targeting and mobile map integration.
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
- server-authoritative region volume and landmark discovery;
- per-player replicated exploration snapshot on AJumanjiPlayerState;
- UJumanjiWorldAtlasWidget base and PlayerController OpenWorldAtlas/CloseWorldAtlas API;
- UJumanjiWorldNavigationComponent on the local PlayerController, with discovery-validated region/landmark targets and local NavMesh route data.

## Not yet included
This is a source-code foundation, not a finished game or installable Android build. The repository still needs:
- saved Unreal maps, including Content/Maps/L_EmeraldJungle_World.umap;
- final portrait textures and 3D character models;
- rigs, animation blueprints and facial animation;
- a touch-first HUD and final Enhanced Input assets;
- actual UMG Widget Blueprint .uasset files and card layout;
- the configured Emerald Jungle chapter director instance and nine placed trial actors;
- 45 character-specific ability Blueprint assets and configured Gameplay Effects;
- configured enemy/boss meshes, animations and initialization effects;
- final boss attack patterns and encounter reward service;
- Blueprint children, visuals and playable implementations for all nine Great Spirit trials;
- the Root Rune trial Blueprint, rune meshes, UI widget, map and presentation assets;
- persistent exploration/quest save data, inventory rewards and backend persistence;
- authored NavMesh data, world-space DataTable anchors and traversal link actors/animations;
- asynchronous asset preloading and device profiling;
- account persistence, unlock progression and backend services;
- dedicated-server deployment;
- a packaged and device-tested APK/AAB.

The supplied reference image is used as an art direction source. The binary image itself and Unreal binary assets are not added by the text-file GitHub workflow; add them through Git LFS with provenance/license notes.

## Build verification
The source has not yet been compiled in an Unreal Engine 5.8 environment during these repository changes. The GitHub Actions workflow and `Scripts/Build-UE5.ps1` are included. The workflow requires a configured Windows self-hosted runner with a local UE 5.8 installation before it can execute. Complete the stage acceptance checklists locally before treating the project as build-verified. A successful C++ build alone does not prove that the game is playable or produce an installable Android package.
