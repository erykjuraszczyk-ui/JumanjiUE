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
- Stage 22 — Mobile HUD Polish & Accessibility (Ability HUD track): local HUD/touch scaling, high-contrast text mode, reduced-motion option, SafeZone/portrait-landscape Blueprint contract and Android usability QA.
- Stage 23 — Ability HUD Settings Persistence & Device QA: device-local UGameUserSettings persistence, Blueprint settings screen base, 14-case Android QA matrix, CSV validator, regression tests and GitHub Actions workflow.
- Stage 21 — Emerald Jungle World Authoring Pack: import-ready region/landmark DataTable CSVs, 29-row greybox placement ledger and World Partition assembly manifest.
- Stage 22 — Emerald Jungle Greybox Validation & Mobile Performance: source-data validator, unit/integration tests, GitHub Actions quality gate and Android profiling run template.
- Stage 23 — Emerald Jungle Traversal Interactions & Off-Mesh Links: Smart Link traversal actors, four contextual movement types, server-authoritative traversal component and client request validation.
- Stage 24 — Traversal Animation & Interaction Presentation: validated traversal presentation DataTable, local mobile prompt widget, replicated completion/interruption feedback and optional server-sampled Zipline spline paths.
- Stage 25 — Traversal World Integration & Multiplayer Validation: local overlap-driven link focus, nearest-candidate prompt selection, server-driven replicated lift platforms with shared passenger attachment, 12-link/2-platform world placement manifests and automated source validation.
- Stage 26 — World Traversal QA, Lift Recovery & Traversal Rewards: replicated self-contained Zipline paths for World Partition unload resilience, collision-safe blocked-lift recovery, idempotent first-completion rewards for all 12 traversal links and reward-manifest CI validation.
- Stage 27 — Emerald Jungle World Encounter Integration: server-authoritative progression gate, nine-trial chapter orchestration, configurable enemy waves, boss encounter, replicated phase state and idempotent per-profile completion rewards.
- Stage 28 — Emerald Jungle Encounter Presentation & Multiplayer QA: replicated boss reference, server-timed attack telegraphs, encounter HUD base, three-phase telegraph manifest, source validator and multiplayer/mobile QA workflow.
- Stage 29 — Encounter Replay & Recovery: server-only retry of a matching Failed encounter, Living Board trial/objective reset, cleanup of all encounter-spawned actors and preservation of persistent profile/economy receipts.
- Stage 30 — Encounter Session Persistence & Reconnect Recovery: in-memory same-server participant registry, bounded reconnect grace window, profile-restored rejoin validation and transient PlayerState reconnect notice.
- Stage 31 — Encounter Reconnect UX & Session QA: replicated success/expired/ended feedback, a non-persistent local HUD banner contract, bounded terminal notices and a 13-scenario network QA matrix.
- Stage 32 — Encounter Session Checkpoint Contract & Server Recovery Design: strict versioned checkpoint envelope, signed logical payload contract, safe recovery barriers, anti-rollback/fencing requirements and recovery QA matrix; no backend or runtime resume is claimed.
- Stage 33 — Encounter Checkpoint Backend Adapter & Lease Service Prototype: trusted server-to-server checkpoint API, Ed25519/RFC 8785 signatures, SQLite CAS/lease fencing, server-only Unreal HTTP adapter and concurrency/security tests.
- Stage 34 — Safe-Point Snapshot Builder & Restore Staging: server snapshot creation at trial/wave/boss/reward boundaries, lease and fencing lifecycle, opaque participant IDs, signed checkpoint preflight and a private restore candidate without world mutation.
- Stage 35 — Encounter Restore Materialization & Atomic Publish: authenticated participant mapping, profile revision and reward-ledger reconciliation, trial/wave/boss reconstruction, restored session identity and a single server-side GameState commit before actor replication.
- Stage 36 — Encounter Startup Coordinator & Participant Barrier: automatic server recovery after profile restore, persistent-level director discovery, staged-candidate retry after each PostLogin and bounded 180-second wait for all checkpoint participants.
- Stage 37 — Recovery Observability & Operator Controls: secret-free process diagnostics, correlated outcome logs, bounded retry/abort server APIs, operator runbook, source validator and regression CI.
- Stage 38 — Recovery Chaos Testing & Failure Injection: lost-write-acknowledgement recovery, backend restart durability, SQLite rollback, sequence CAS races, signature/TTL tampering, transport failure and explicit Unreal PIE/manual fault scenarios.
- Stage 39 — Recovery Load, Soak & Performance Benchmarks: real HTTP/SQLite/Ed25519 service smoke benchmark, per-operation p50/p95/p99 latency, bounded sampling, concurrent workers, lease renewal, load and 30/60-minute soak profiles.
- Stage 40 — Recovery Capacity Planning & SLO Baselines: report-gated capacity planning, anonymous host fingerprint template, SLO approval governance and conservative concurrency projection; all latency/throughput targets remain UNBASELINED pending representative host measurements.
- Stage 41 — Production Checkpoint Store Adapter & Deployment Hardening: shared PostgreSQL provider with row locking and fencing, Ed25519 verification-key overlap rotation, TLS/mTLS listener, bounded request concurrency, non-root Docker/Compose, offline SQLite-to-PostgreSQL import and backup/restore runbook; no HA/SLO/production readiness is claimed.
- Stage 42 — Production Staging Validation & Recovery DR Drills: HTTPS/mTLS/PostgreSQL staging runner, sanitized signed-readback evidence, offline post-restore lease fencing, disposable PostgreSQL integration tests and ephemeral CI; real staging/PITR/failover/UE/Android evidence remains pending, no production or HA/SLO readiness is claimed.
- Stage 43 — Operator Evidence Intake & Production Readiness Gate: strict evidence packet schema, SHA-256/path/privacy validation, Stage 42 report and fencing verification, UE/Android/SLO evidence contracts and independent two-role review; empty template remains BLOCKED and never changes production/HA/SLO readiness.
- Stage 44 — Cryptographic Evidence Attestation: packet-bound Ed25519 signatures for operator, evidence reviewer and final release-board roles; RFC 8785 canonicalization, independently pinned public keyring hash, key revocation/role controls and fail-closed attestation validation; no production/HA/SLO promotion.
- Stage 45 — Release Board Authorization & Deployment Candidate Binding: SHA-256-bound UE 5.8 Dedicated Server and Android candidate artifacts, Stage 43/44 packet/keyring binding, three independent Release Board signatures and fail-closed authorization; no deployment or production/HA/SLO promotion.
- Stage 46 — Controlled Deployment Orchestration & Rollback Readiness: re-verifies Stage 45 signatures, requires hashed last-known-good server/Android assets, builds an operator-gated rollout plan and post-deployment evidence checklist; no automated deployment or production/HA/SLO promotion.

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
- Docs/STAGE_21_ABILITY_PRESENTATION_BLUEPRINT_INTEGRATION.md — WBP_AbilityHUD/WBP_AbilitySlot hierarchy, mobile layout, asynchronous assets and Blueprint acceptance checklist.
- Docs/STAGE_22_MOBILE_HUD_POLISH_ACCESSIBILITY.md — SafeZone, responsive landscape/portrait layouts, user-accessible scaling, contrast, reduced motion, optional haptics adapter and Android QA.
- Docs/STAGE_23_ABILITY_HUD_SETTINGS_PERSISTENCE_DEVICE_QA.md — local settings persistence, WBP_AbilityHUDSettings wiring, device QA matrix and automated validation limits.
- Content/UI/AbilityPresentation/README.md — asset import, naming and per-ability assignment workflow.
- Content/UI/AbilityHUD/README.md — WBP_AbilityHUD/WBP_AbilitySlot asset hierarchy and exact BindWidget names.
- Content/UI/AbilityHUD/Settings/README.md — WBP_AbilityHUDSettings parent class, controls and persistence contract.
- Data/UI/AbilityHUD_DeviceQAMatrix.csv — 14 manual mobile HUD validation cases (all initially NOT_RUN).
- Scripts/validate_ability_hud_device_qa.py and Scripts/Tests/test_validate_ability_hud_device_qa.py — QA matrix validator and regression tests.
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
- Docs/STAGE_22_EMERALD_JUNGLE_GREYBOX_VALIDATION.md — automated source validation, Unreal Editor/World Partition test plan and Android performance profiling.
- Docs/STAGE_23_EMERALD_JUNGLE_TRAVERSAL_INTERACTIONS.md — Smart Link setup, contextual traversal types, server validation and acceptance checklist.
- Docs/STAGE_24_TRAVERSAL_ANIMATION_INTERACTION_PRESENTATION.md — traversal presentation manifest, async montage/audio/VFX workflow, contextual mobile prompt, zipline spline setup and acceptance tests.
- Docs/STAGE_25_TRAVERSAL_WORLD_INTEGRATION_MULTIPLAYER.md — overlap focus registry, replicated lift platform, 12-link world manifest, editor placement, multiplayer and Android acceptance tests.
- Docs/STAGE_26_TRAVERSAL_QA_LIFT_RECOVERY_REWARDS.md — World Partition unload/load resilience, blocked-lift recovery, one-time traversal rewards, source-data validator and QA matrices.
- Docs/STAGE_27_EMERALD_JUNGLE_WORLD_ENCOUNTER_INTEGRATION.md — exploration/traversal prerequisites, nine-trial chapter, configurable enemy waves, boss lifecycle, replicated encounter state, one-time reward and Unreal Editor authoring/QA workflow.
- Docs/STAGE_28_EMERALD_JUNGLE_ENCOUNTER_PRESENTATION_QA.md — encounter HUD, replicated boss telemetry, attack telegraph lifecycle, Blueprint authoring, multiplayer QA and mobile profiling.
- Docs/STAGE_29_ENCOUNTER_REPLAY_RECOVERY.md — failed encounter retry, session-only chapter/trial reset, encounter-owned actor cleanup, profile/economy invariants and QA workflow.
- Docs/STAGE_30_ENCOUNTER_SESSION_RECONNECT_RECOVERY.md — same-server disconnect/reconnect lifecycle, grace window, transient PlayerState notice, profile separation and QA checklist.
- Docs/STAGE_31_ENCOUNTER_RECONNECT_UX_SESSION_QA.md — reconnect success/expired/ended outcomes, UMG banner wiring, bounded terminal notice retention and multiplayer/Android QA.
- Docs/STAGE_32_ENCOUNTER_SESSION_CHECKPOINT_CONTRACT.md — versioned checkpoint schema, safe-point rules, Ed25519 envelope, identity/privacy boundaries, anti-rollback lease design and recovery acceptance matrix.
- Docs/STAGE_33_ENCOUNTER_CHECKPOINT_BACKEND_LEASE_SERVICE.md — trusted backend, lease/CAS/fencing, Ed25519 signing, Unreal adapter, local runbook and limitations.
- Docs/STAGE_34_ENCOUNTER_SAFEPOINT_SNAPSHOT_RESTORE_STAGING.md — live-world safe-point snapshot builder, lease lifecycle, privacy boundaries, restore preflight and acceptance tests.
- Docs/STAGE_35_ENCOUNTER_RESTORE_MATERIALIZATION_ATOMIC_PUBLISH.md — participant reconciliation, profile revision and reward receipt checks, trial/wave/boss reconstruction, server commit ordering, startup integration and known recovery limitations.
- Docs/STAGE_36_ENCOUNTER_STARTUP_COORDINATOR_PARTICIPANT_BARRIER.md — automatic PostLogin recovery startup, persistent-level director selection, staged participant barrier, bounded timeout and host acceptance tests.
- Docs/STAGE_37_ENCOUNTER_RECOVERY_OBSERVABILITY_OPERATOR_CONTROLS.md — process-local diagnostics, stable outcome codes, correlated logs, native server-only operator retry/abort controls and recovery runbook.
- Docs/STAGE_38_RECOVERY_CHAOS_TESTING_FAILURE_INJECTION.md — fault-injection scope, backend integration tests, Unreal manual test run sheet, acceptance criteria and evidence limitations.
- Docs/STAGE_39_RECOVERY_LOAD_SOAK_PERFORMANCE_BENCHMARKS.md — benchmark scope, metrics, smoke/load/soak profiles, local commands, interpretation and single-host evidence boundaries.
- Docs/STAGE_40_RECOVERY_CAPACITY_PLANNING_SLO_BASELINES.md — evidence admission, capacity estimate formula, host inventory, SLO governance and local runbook.
- Docs/STAGE_41_PRODUCTION_CHECKPOINT_STORE_DEPLOYMENT_HARDENING.md — PostgreSQL provider, mTLS deployment, signing-key rotation, offline SQLite import, backup/restore and operational limitations.
- Docs/STAGE_42_PRODUCTION_STAGING_VALIDATION_RECOVERY_DR_DRILLS.md — staging smoke runner, sanitized evidence, post-PITR lease fencing, operator failover/restore steps and explicit production readiness gates.
- Docs/STAGE_43_OPERATOR_EVIDENCE_INTAKE_PRODUCTION_READINESS_GATE.md — required operator evidence packet, evidence provenance classes, report formats, hash checks, dual-role review and fail-closed release gate.
- Docs/STAGE_44_CRYPTOGRAPHIC_EVIDENCE_ATTESTATION.md — encrypted Ed25519 operator/reviewer signing, RFC 8785 packet binding, out-of-band trust-keyring pinning, key revocation, independent final review and operator runbook.
- Docs/STAGE_45_RELEASE_BOARD_DEPLOYMENT_AUTHORIZATION.md — candidate artifact binding, three-role Release Board quorum, Ed25519 signing, exact release authorization and deployment boundary.
- Docs/STAGE_46_CONTROLLED_DEPLOYMENT_ROLLBACK_READINESS.md — Stage 45 re-verification, rollback baseline, gated rollout sequence, operator runbook and deployment limitations.
- Data/Encounter/ProductionReleaseCandidate.schema.json and Data/Encounter/ProductionReleaseCandidate.template.json — strict release candidate manifest and empty template.
- Data/Encounter/ProductionRollbackBaseline.schema.json and Data/Encounter/ProductionRollbackBaseline.template.json — strict production last-known-good manifest and empty baseline template.
- Data/Encounter/ProductionDeploymentPlan.schema.json and Data/Encounter/ProductionDeploymentPlan.template.json — strict non-executing deployment plan and blocked template.
- Data/Encounter/ProductionReleaseBoardDecision.schema.json and Data/Encounter/ProductionReleaseBoardDecision.template.json — strict board decision envelope and unsigned template.
- Scripts/encounter_checkpoint_release_board.py — Stage 43/44 verification, candidate artifact hashing, independent board signatures and authorization report.
- Scripts/encounter_checkpoint_deployment.py — Stage 45 cryptographic re-verification, rollback artifact hashing, operator-gated plan generation and read-only plan verification.
- Scripts/validate_encounter_checkpoint_stage46_deployment.py and Scripts/Tests/test_validate_encounter_checkpoint_stage46_deployment.py — Stage 46 static governance validator and security/regression tests.
- .github/workflows/encounter-checkpoint-stage46-deployment.yml — CI for Stage 43–46; validates empty templates without deploying.
- Scripts/validate_encounter_checkpoint_stage45_release_board.py and Scripts/Tests/test_validate_encounter_checkpoint_stage45_release_board.py — fail-closed governance checks and synthetic cryptographic regression tests.
- .github/workflows/encounter-checkpoint-stage45-release-board.yml — CI for Stage 43/44/45 and blocked empty templates.
- Data/Encounter/ProductionEvidenceTrustKeyring.schema.json and Data/Encounter/ProductionEvidenceTrustKeyring.template.json — strict public-key trust schema and deliberately empty keyring template.
- Data/Encounter/ProductionEvidenceAttestation.schema.json and Data/Encounter/ProductionEvidenceAttestation.template.json — strict attestation envelope schema and empty signature template.
- Scripts/encounter_checkpoint_evidence_attestation.py — encrypted key generation, per-evidence operator/reviewer signatures, final review signatures and packet verification.
- Scripts/validate_encounter_checkpoint_stage44_attestation.py and Scripts/Tests/test_validate_encounter_checkpoint_stage44_attestation.py — fail-closed Stage 44 governance and Ed25519 security regression tests.
- .github/workflows/encounter-checkpoint-stage44-attestation.yml — CI for Stage 43 packet integrity, Stage 44 signature verification and blocked empty templates.
- Data/Encounter/ProductionReadinessEvidencePacket.schema.json and Data/Encounter/ProductionReadinessEvidencePacket.template.json — strict Stage43 schema and empty operator packet (awaiting evidence).
- Scripts/validate_encounter_checkpoint_stage43_evidence.py and Scripts/Tests/test_validate_encounter_checkpoint_stage43_evidence.py — artifact hash/path/privacy checks, Stage42 staging/fencing validation, platform evidence contracts and review gate tests.
- Scripts/validate_encounter_checkpoint_stage43.py and .github/workflows/encounter-checkpoint-stage43-evidence.yml — static governance validator and CI; CI validates an empty packet as blocked, never as production-ready.
- Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json — separate Stage42 evidence manifest; real staging, failover, PITR, UE and Android validations remain false until performed.
- Data/Encounter/EncounterCheckpointStagingDrillReport.schema.json — strict sanitized report schema for staging/ephemeral CI results.
- Scripts/run_encounter_checkpoint_staging_drill.py and Scripts/validate_encounter_checkpoint_staging_report.py — HTTPS/mTLS/PostgreSQL signed-checkpoint drill and report privacy validator.
- Scripts/fence_checkpoint_leases_after_restore.py — offline PostgreSQL restore fencing; dry-run by default, explicit staged-target confirmation required for apply.
- Scripts/validate_encounter_checkpoint_stage42.py and Scripts/Tests/test_encounter_checkpoint_stage42.py — Stage42 static, signature, payload and readiness governance validation.
- Scripts/Tests/test_checkpoint_postgres_restore_fencing.py — disposable-PostgreSQL tests for dry-run, restored lease invalidation, preserved signed checkpoints and epoch overflow.
- Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json — explicit provider, transport, secret, migration and production-readiness boundaries (SLO remains UNBASELINED).
- Data/Encounter/EncounterCheckpointVerificationKeyringTemplate.json — empty public verification-key overlap template; deployment keys are not committed.
- Backend/EncounterCheckpointService/Dockerfile and Backend/EncounterCheckpointService/compose.production.yml — non-root service image and private mTLS deployment template.
- Scripts/migrate_encounter_checkpoint_store.py — signed, offline SQLite-to-PostgreSQL import that carries checkpoint rows but never active lease tokens or owners.
- Scripts/validate_encounter_checkpoint_production.py and Scripts/Tests/test_validate_encounter_checkpoint_production.py — Stage 41 production-boundary validator and tests.
- Scripts/Tests/test_encounter_checkpoint_postgres.py — PostgreSQL integration tests for CAS, fencing, concurrent leases and SQLite import; requires a disposable PostgreSQL test database.
- Data/Encounter/EncounterRecoverySLOProfiles.csv and Data/Encounter/EncounterRecoveryHostInventoryTemplate.json — unapproved SLO planning profiles and host evidence template.
- Scripts/plan_encounter_recovery_capacity.py and Scripts/validate_encounter_recovery_capacity_slo.py — Stage 40 planning calculator and SLO governance validator.
- Data/Encounter/EncounterRecoveryChaosMatrix.csv — 14 backend, lease, participant, ledger and materialization failure scenarios.
- Data/Encounter/EncounterRecoveryBenchmarkProfiles.csv — reproducible CI smoke, baseline, load, 30-minute soak and 60-minute soak profiles.
- Scripts/validate_encounter_recovery_chaos.py and Scripts/Tests/test_validate_encounter_recovery_chaos.py — Stage 38 source/matrix contract validator and regression tests.
- Scripts/Tests/test_encounter_checkpoint_chaos.py — Stage 38 HTTP/SQLite failure injection for dropped acknowledgements, restart durability, atomic writes, CAS races, signature, expiry and transport failures.
- Scripts/benchmark_encounter_checkpoint_recovery.py — Stage 39 real-service HTTP/SQLite load and soak benchmark with bounded p50/p95/p99 latency sampling.
- Scripts/validate_encounter_recovery_benchmark.py and Scripts/Tests/test_validate_encounter_recovery_benchmark.py — Stage 39 benchmark profile and CI wiring validator/tests.
- Scripts/Tests/test_encounter_recovery_benchmark.py — Stage 39 live local HTTP/SQLite smoke, percentile and bounded-sample tests.
- Scripts/validate_encounter_startup_coordinator.py and Scripts/Tests/test_validate_encounter_startup_coordinator.py — Stage 36 static startup/recovery validator and regression tests.
- Scripts/validate_encounter_restore_materialization.py and Scripts/Tests/test_validate_encounter_restore_materialization.py — Stage 35 static source validator and regression tests.
- Backend/EncounterCheckpointService/README.md — local checkpoint backend setup and server environment.
- Scripts/validate_encounter_checkpoint_backend.py — Stage 33 backend integration contract validator.
- Data/Encounter/EncounterReconnectSessionQAMatrix.csv — 13 reconnect outcome and network interruption scenarios.
- Scripts/validate_emerald_jungle_encounter_reconnect.py — Stage 30 server reconnect source validator.
- Scripts/validate_emerald_jungle_encounter_reconnect_ux.py — Stage 31 reconnect UX contract and QA matrix validator.
- Data/Encounter/EmeraldJungleBossTelegraphs.csv — nine sample telegraph definitions across the three boss phases.
- Content/Traversal/README.md — traversal DataTable, prompt widget, montage and spline authoring guide.
- Data/Traversal/TraversalPresentationManifest.csv — four stable starter presentation records.
- Data/Traversal/EmeraldJungle_TraversalPlacement.csv — 12 starter contextual traversal links.
- Data/Traversal/EmeraldJungle_LiftPlatformManifest.csv — two starter replicated lift platforms.
- Data/Traversal/EmeraldJungle_TraversalManifest.json — link counts, lift settings and World Partition authoring rules.
- Data/Traversal/EmeraldJungle_TraversalRewards.csv — one-time reward for each of the 12 traversal links.
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
- UJumanjiWorldNavigationComponent on the local PlayerController, with discovery-validated region/landmark targets and local NavMesh route data;
- AJumanjiTraversalLink based on ANavLinkProxy, with authored entry/exit anchors and dynamically enabled Smart Links;
- UJumanjiTraversalComponent on AJumanjiCharacter, with replicated server-authoritative traversal state, swept movement and Blueprint presentation callbacks;
- PlayerController RequestTraversalLink RPC with server-side range, grounded-state and active-traversal validation.
- UJumanjiTraversalPresentationSubsystem with validated DataTable lookup, per-link overrides, type defaults and Polish fallback prompts.
- UJumanjiTraversalPromptWidget with a Blueprint view model, request timeout and server-confirmed active/completed/interrupted states.
- PlayerController creation and focus/clear API for the local traversal prompt;
- overlap-volume candidate registry on AJumanjiTraversalLink with nearest valid link selection in AJumanjiPlayerController;
- AJumanjiLiftPlatform with server-authoritative swept movement, replicated transform/state, passenger limit and shared rider attachment;
- server-only TryRecoverBlockedPlatform with no-passenger gating and collision checks at both authored stops before moving a blocked lift to the nearest safe stop;
- Lift traversal integration through UJumanjiTraversalComponent, with riders restored after the platform completes or is blocked;
- source manifests for 12 Emerald Jungle traversal links and two lift platforms, plus Python validator, unit tests and GitHub Actions workflow.
- Optional server-sampled Zipline spline path with collision sweep and bidirectional direction handling;
- 17-point FVector_NetQuantize10 Zipline path snapshot in the replicated traversal state so a ride can finish when its World Partition link actor unloads;
- one-time server-side exploration reward per stable TraversalId through UJumanjiProgressionRewardSubsystem and the existing PlayerState transaction ledger;
- 12 traversal reward definitions with optional DT_TraversalRewards override, source CSV, Python validator, unit tests and GitHub Actions quality gate;
- AJumanjiWorldEncounterDirector with server-side Frost Gate exploration/traversal receipt checks, existing nine-trial chapter integration, configurable bounded enemy waves and an AJumanjiBossCharacter finale;
- UJumanjiEncounterCheckpointRuntimeSubsystem with secret-free process diagnostics, correlated outcome logs and native server-only operator retry/abort controls (Stage 37);
- replicated FJumanjiWorldEncounterState with Trials/Combat/Boss/Reward/Completed/Failed phases and server-authoritative GameState lifecycle;
- PlayerController server RPC for starting encounters with a server-checked interaction distance;
- AJumanjiEnemyCharacter::OnEnemyDefeated event for server-side wave advancement and boss defeat sequencing;
- idempotent per-profile encounter completion reward through the existing progression transaction ledger, with optional DT_WorldEncounterRewards and source CSV validator/CI.
- CSV validator, unit tests and GitHub Actions quality gate for traversal presentation data.
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
- UJumanjiAbilityHUDSlotWidget reusable slot base with async icon/material/audio loading, cooldown MID parameters, placeholder fallback and slot touch event;
- five optional child-slot bindings in UJumanjiAbilityHUDWidget with per-slot feedback routing and delegate cleanup;
- local mobile HUD accessibility controls for overall scale, touch-target scale, high-contrast text and reduced-motion feedback (Stage 22 Ability HUD track);
- UJumanjiGameUserSettings for local HUD preference persistence in GameUserSettings.ini, applied by the local PlayerController; settings UI base widget and device QA matrix with validator/tests (Stage 23);
- ability HUD slot snapshots enriched with per-ability palette, icon, cooldown material, audio references and Gameplay Cue tag.
- PlayerController creation of the configured ability HUD for the local player.
- replicated encounter snapshot now carries the spawned AJumanjiBossCharacter reference for local presentation;
- AJumanjiBossCharacter server-timed Warning/Executing telegraph state with replicated pattern geometry, GAS state tags, cancellation on phase change/defeat and a Blueprint execution hook for the actual server-owned attack;
- UJumanjiWorldEncounterHUDWidget Blueprint base for encounter phase, trial/wave counters, boss Health/phase and synchronized telegraph countdown;
- local PlayerController creation of the configured Encounter HUD, plus nine-row boss telegraph CSV manifest, Python validator, unit tests and GitHub Actions workflow (Stage 28);
- server-authoritative encounter retry from Failed for the same EncounterId, chapter/trial reset and revision increments;
- tracking and cleanup of all spawned encounter actors, including defeated actors awaiting cosmetic lifespan expiry;
- static source validator, unit tests and GitHub Actions quality gate for encounter recovery (Stage 29);
- UJumanjiWorldEncounterSessionSubsystem for in-memory, profile-keyed encounter participant membership and a configurable reconnect grace window;
- server-side disconnect tracking in GameMode::Logout and profile-restored rejoin validation in GameMode::PostLogin;
- transient replicated PlayerState reconnect notice and Encounter HUD Blueprint callback;
- session registry cleanup on encounter completion, failure, director EndPlay and start rollback;
- static source validator, unit tests and GitHub Actions quality gate for same-server reconnect (Stage 30).

- UJumanjiEncounterCheckpointBackendSubsystem native-only async HTTP adapter; client net mode is rejected and server credentials/identity are read from process environment.
- Trusted Python checkpoint service with Ed25519/RFC 8785 signing, signature verification, bounded leases, monotonically increasing fencing epoch, SQLite development provider and PostgreSQL shared transactional provider.
- Stage 41 signing-key overlap verification keyring, TLS 1.2+/mTLS listener for non-loopback binds, bounded HTTP worker pool, separate readiness endpoint and file-based secret support.
- Stage 41 non-root read-only Docker image, private Compose deployment template and offline SQLite-to-PostgreSQL migration utility that never transfers lease tokens/owners.
- Stage42 synthetic staging runner exercises HTTPS/mTLS, PostgreSQL readiness, bearer authentication, lease exclusion/renewal, sequence CAS, Ed25519 readback, stale owner fencing and signed-envelope preservation after lease reacquisition.
- Stage42 post-restore fencing tool increments every encounter row fencing epoch and clears restored lease owners while preserving checkpoint sequence/envelope; dry-run is default and apply requires an isolated staging/DR target confirmation.
- Stage42 CI uses disposable PostgreSQL and generated test certificates; it does not claim managed failover, PITR, real staging deployment, UE build, Android packaging or approved production SLO.
- Stage43 evidence intake currently has no operator attachments: the required 10 evidence kinds remain pending until actual staging/DR/UE/Android/SLO artifacts are supplied and independently reviewed.
- Checkpoint backend unit tests for concurrency, lease fencing, stale sequence, signature tampering and server authentication; Stage 33 GitHub Actions workflow.
- AJumanjiGameMode PostLogin invokes the trusted checkpoint startup coordinator only after identity binding, profile restoration and same-server reconnect processing;
- persistent-level director discovery requires exactly one AJumanjiWorldEncounterDirector and rejects ambiguous/missing world configuration;
- staged checkpoint restores wait for every checkpoint participant to authenticate and restore their profile; later PostLogin callbacks retry commit without discarding the candidate;
- bounded 180-second participant barrier timeout clears the private candidate and releases the recovery lease without publishing partial GameState;
- Stage 36 source validator, regression tests and GitHub Actions workflow.
- Stage 38 chaos matrix validator, HTTP/SQLite failure-injection suite and a dedicated GitHub Actions workflow re-running the Stage 32–37 recovery gates.

## Not yet included
This is a source-code foundation, not a finished game or installable Android build. The repository still needs:
- saved Unreal maps, including Content/Maps/L_EmeraldJungle_World.umap;
- imported DT_Jumanji_WorldRegions and DT_Jumanji_WorldLandmarks DataTable .uasset files (CSV source files are now present);
- final portrait textures and 3D character models;
- rigs, animation blueprints and facial animation;
- final touch-first Widget Blueprint assets, icons, UI materials and Enhanced Input mapping assets;
- actual UMG Widget Blueprint .uasset files and card layout, including WBP_AbilityHUD and WBP_AbilitySlot;
- the configured Emerald Jungle chapter director instance and nine placed trial actors;
- imported DT_AbilityPresentation DataTable and configured subsystem path;
- final Ability Blueprint HUDAbilityId assignments and binary ability icons/material/audio assets;
- 45 character-specific ability Blueprint assets and configured Gameplay Effects;
- configured enemy/boss meshes, animations and initialization effects;
- actual boss attack Blueprint graphs, final boss telegraph VFX/audio and WBP_EmeraldJungleEncounterHUD .uasset; Stage 28 provides the native HUD and telegraph runtime contracts only;
- authored world encounter actor/map configuration and enemy wave spawn point placements;
- Blueprint children, visuals and playable implementations for all nine Great Spirit trials;
- the Root Rune trial Blueprint, rune meshes, UI widget, map and presentation assets;
- trial-specific reward rows and automatic reward orchestration for all nine trial actors;
- persistent in-progress objective state;
- a production profile backend, account authentication, cross-shard synchronization and profile conflict handling;
- authored NavMesh data, world-space DataTable anchors, placed traversal link actors and Blueprint children;
- imported DT_TraversalPresentation and DT_TraversalRewards DataTables, WBP_TraversalPrompt, authored animation montages, traversal icons/sounds/Niagara effects and spline mesh cable visuals;
- server-side operator/admin UI that invokes TryRecoverBlockedPlatform after manual obstruction checks;
- authored BP_EmeraldJungleWorldEncounter, enemy Blueprint classes, boss Blueprint, persistent-level spawn markers, nine-trial chapter director instance and DT_WorldEncounterRewards .uasset;
- placed traversal link actors and lift platforms in the saved Emerald Jungle .umap, with final landscape/NavMesh anchor alignment;
- asynchronous asset preloading and device profiling;
- production account authentication, secure economy backend and cross-shard transaction processing;
- dedicated-server deployment;
- production asynchronous profile-service integration: the current AJumanjiGameMode hook runs after the prototype's synchronous local profile restore; a future remote profile adapter must invoke the coordinator only after its authenticated profile load completes;
- full actor/AI transform, player position, active attack, telegraph and timer restoration; Stage 35 restarts at authored safe spawn anchors and stores only logical chapter/wave/boss phase/health data;
- production multi-region consensus, managed PostgreSQL HA/failover, database connection pooling, KMS/HSM, automated key/token rotation, an operator backup scheduler and completed PITR restore drills; Stage 41 supplies a PostgreSQL provider and runbook, not an operated HA service;
- real staging deployment evidence, managed PostgreSQL primary failover/PITR restore drills, signing-key overlap rehearsal, Unreal Engine 5.8 Dedicated Server/PIE recovery validation, Android device validation and approved p95/p99/throughput SLOs; Stage 40 profiles remain UNBASELINED; Stage42 CI is only ephemeral test evidence; Stage43 provides an intake gate but has not supplied these operator artifacts;
- a packaged and device-tested APK/AAB.

The supplied reference image is used as an art direction source. The binary image itself and Unreal binary assets are not added by the text-file GitHub workflow; add them through Git LFS with provenance/license notes.

## Build verification
The source has not yet been compiled in an Unreal Engine 5.8 environment during these repository changes. The GitHub Actions workflow and `Scripts/Build-UE5.ps1` are included. The workflow requires a configured Windows self-hosted runner with a local UE 5.8 installation before it can execute. Complete the stage acceptance checklists locally before treating the project as build-verified. A successful C++ build alone does not prove that the game is playable or produce an installable Android package.
