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
- Stage 21 — Ability Presentation Blueprint Integration: reusable native slot widget, five BindWidgetOptional slots, asynchronous icon/material/audio loading, radial cooldown parameters and per-slot feedback routing.
- Stage 21 — Emerald Jungle World Authoring Pack: import-ready region/landmark DataTable CSVs, 29-row greybox placement ledger and World Partition assembly manifest.
- Stage 22 — Emerald Jungle Greybox Validation & Mobile Performance: source-data validator, unit/integration tests, GitHub Actions quality gate and Android profiling run template.
- Stage 23 — Emerald Jungle Traversal Interactions & Off-Mesh Links: Smart Link traversal actors, four contextual movement types, server-authoritative traversal component and client request validation.
- Stage 24 — Traversal Animation & Interaction Presentation: validated traversal presentation DataTable, local mobile prompt widget, replicated completion/interruption feedback and optional server-sampled Zipline spline paths.
- Stage 25 — Traversal World Integration & Multiplayer Validation: local overlap-driven link focus, nearest-candidate prompt selection, server-driven replicated lift platforms with shared passenger attachment, 12-link/2-platform world placement manifests and automated source validation.
- Stage 26 — World Traversal QA, Lift Recovery & Traversal Rewards: replicated self-contained Zipline paths for World Partition unload resilience, collision-safe blocked-lift recovery, idempotent first-completion rewards for all 12 traversal links and reward-manifest CI validation.
- Stage 27 — Emerald Jungle World Encounter Integration: server-authoritative progression gate, nine-trial chapter orchestration, configurable enemy waves, boss encounter, replicated phase state and idempotent per-profile completion rewards.
- Stage 28 — Emerald Jungle Encounter Presentation & Multiplayer QA: replicated boss reference, server-timed attack telegraphs, encounter HUD base, three-phase telegraph manifest, source validator and multiplayer/mobile QA workflow.
- Stage 29 — Encounter Replay & Recovery: server-only retry of a matching Failed encounter, Living Board trial/objective reset, cleanup of all encounter-spawned actors and preservation of persistent profile/economy receipts.
- Stage 30 — Encounter Session Persistence & Reconnect Recovery: in-memory same-server participant registry, a bounded reconnect grace window, profile-restored rejoin validation and transient PlayerState reconnect state.
- Stage 31 — Encounter Reconnect UX & Session QA: replicated success/expired/ended feedback, a non-persistent local HUD banner contract, bounded terminal notices and a 13-scenario network QA matrix.

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
- Docs/STAGE_30_ENCOUNTER_SESSION_RECONNECT_RECOVERY.md — same-server disconnect/reconnect lifecycle, grace window, transient PlayerState notice and profile separation.
- Docs/STAGE_31_ENCOUNTER_RECONNECT_UX_SESSION_QA.md — success/expired/ended feedback, UMG banner wiring, terminal notice retention and multiplayer/Android QA matrix.
- Data/Encounter/EncounterReconnectSessionQAMatrix.csv — 13 reconnect outcome and network-interruption scenarios.
- Scripts/validate_emerald_jungle_encounter_reconnect.py — Stage 30 server reconnect source validator.
- Scripts/validate_emerald_jungle_encounter_reconnect_ux.py — Stage 31 reconnect UX contract and QA matrix validator.
- [Docs/UE5_BUILD_PIPELINE.md](Docs/UE5_BUILD_PIPELINE.md) — GitHub Actions workflow, Windows self-hosted runner and Android toolchain setup.

## Current implementation
The source project contains native Unreal Engine 5.8 gameplay systems and Blueprint-facing widget bases. It is not yet a finished game or a packaged Android build. Binary maps, imported DataTables, final meshes/animations/VFX/audio and actual Widget Blueprint assets must still be authored in Unreal Editor and committed through Git LFS.

## Build verification
Unreal Engine 5.8 compilation and device packaging must be run on an environment with the matching UE installation and Android toolchain. Source validators and GitHub Actions Python tests do not prove C++ compilation, PIE multiplayer correctness or mobile performance.
