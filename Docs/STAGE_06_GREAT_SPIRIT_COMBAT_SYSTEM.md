# JUMANJI 2.111.1 — Stage 06: Great Spirit Combat System

## Goal
Create the first shared combat runtime: server-authoritative melee hit detection, GAS damage effects, damage feedback events, a GAS-enabled enemy base, perception-driven prototype AI and a three-phase boss foundation.

This is a combat vertical-slice foundation. It is not a complete enemy roster, finished boss encounter, final animations or a build-verified game.

## Included runtime classes

### UJumanjiMeleeAttackAbility
- derives from UJumanjiGameplayAbility;
- uses ServerOnly execution so the server owns the trace and damage result;
- calls CommitAbility before resolving the attack;
- performs a forward sphere sweep on ECC_Pawn;
- ignores the source actor and duplicate targets;
- applies a simple PvE side filter: player pawns can damage enemies, enemy pawns can damage player pawns; same-side hits are ignored;
- resolves the target ASC through IAbilitySystemInterface;
- reads source AttackPower and target Defense;
- calculates damage as `max(1, BaseDamage + AttackPower * AttackPowerCoefficient - Defense * DefenseCoefficient)`;
- creates the configured Gameplay Effect spec;
- sets the negative magnitude on the SetByCaller tag Data.Damage;
- applies the spec to the target ASC;
- sends Event.Combat.Hit with source, target, hit context and final damage magnitude.

A native fallback class, UJumanjiDefaultDamageGameplayEffect, is now assigned by default. It applies the negative Data.Damage SetByCaller data-name value to Health. A designer-authored Gameplay Effect can override it in a derived ability Blueprint.

### Player target integration
AJumanjiCharacter now implements IAbilitySystemInterface and returns the ASC owned by its AJumanjiPlayerState. This allows incoming attacks to resolve a player pawn as a valid GAS target without moving ASC ownership from PlayerState.

### AJumanjiEnemyCharacter
- implements IAbilitySystemInterface;
- owns a replicated UJumanjiAbilitySystemComponent and UJumanjiAttributeSet;
- initializes the ASC actor info with the enemy as owner and avatar;
- optionally applies an InitializationEffectClass on the server;
- optionally grants a BasicAttackAbilityClass on the server;
- exposes AttackRange and AttackInputTagName to the AI controller;
- listens to replicated Health changes;
- calls BP_OnDamaged when Health decreases;
- disables movement/collision and calls BP_OnDefeated at zero Health;
- replicates the defeated state and removes the actor after a short delay.

### AJumanjiEnemyAIController
- uses AI Perception Sight;
- detects pawn targets, excluding other JUMANJI enemy actors;
- moves toward the target while outside AttackRange;
- stops and focuses the target within range;
- periodically attempts to activate the configured attack ability using Input.Enemy.Attack.

The AI needs a baked Navigation Mesh in the test level. It is a lightweight prototype controller, not a full behavior tree or tactical AI.

### AJumanjiBossCharacter
- extends AJumanjiEnemyCharacter;
- tracks replicated BossPhase values 1–3;
- enters phase 2 when Health reaches the configured PhaseTwoHealthThreshold (default 66%);
- enters phase 3 when Health reaches PhaseThreeHealthThreshold (default 33%);
- maintains replicated State.Boss.Phase.Two or State.Boss.Phase.Three loose tags on its ASC;
- triggers BP_OnBossPhaseChanged so a Blueprint can change attack patterns, animation, VFX or arena behavior.

The C++ class exposes phase events; the actual attack pattern changes must be authored in a boss Blueprint or behavior tree.

## Required Unreal Editor assets

### 1. Damage Gameplay Effect
A native fallback UJumanjiDefaultDamageGameplayEffect is included, configured as:
- Duration Policy: Instant.
- Modifier Attribute: Health.
- Modifier Operation: Additive.
- Modifier Magnitude: SetByCaller by data name.
- SetByCaller Data Name: Data.Damage.
- The C++ ability supplies a negative value, so the modifier subtracts Health.

A designer may create GE_Damage_Base in Content/Combat/Effects and assign it to a derived ability to replace the native fallback. Do not add a second defense subtraction to the Gameplay Effect; defense is already factored into FinalDamage.

### 2. Great Spirit primary attack class
Create a Blueprint child of UJumanjiMeleeAttackAbility for each Spirit primary attack, for example GA_GS_I_Primary through GA_GS_IX_Primary.
For each class:
- set AbilitySlot to PrimaryAttack;
- set InputTag to Input.Ability.Primary;
- optionally assign a designer-authored DamageEffectClass to override the native UJumanjiDefaultDamageGameplayEffect fallback;
- tune BaseDamage, TraceDistance and TraceRadius;
- set a suitable Cooldown Gameplay Effect if the attack must be rate-limited;
- add animation montage and Gameplay Cues when their assets exist.

Assign the class to PrimaryAttackAbilityClass in the corresponding DT_GreatSpiritRoster row.

### 3. Enemy attack ability
A native UJumanjiDefaultEnemyMeleeAbility is assigned automatically when BasicAttackAbilityClass is unset. It uses Input.Enemy.Attack and the native default damage effect. A designer can replace it with GA_Enemy_Melee, a child of UJumanjiMeleeAttackAbility:
- set InputTag to Input.Enemy.Attack;
- optionally assign a custom damage Gameplay Effect;
- configure damage and cooldown;
- add a readable animation and wind-up cue.

A native UJumanjiEnemyInitializationGameplayEffect supplies prototype enemy attributes when InitializationEffectClass is unset. BP_JumanjiEnemy can override either native default.
- assign skeletal mesh, animation blueprint and hit/death events.

### 4. Boss initialization
A native UJumanjiBossInitializationGameplayEffect supplies prototype boss attributes (MaxHealth/Health 600, AttackPower 18 and Defense 12). A designer may override it with GE_BossGuardian_Init as an Instant Gameplay Effect.

Create BP_BoardGuardian as a child of AJumanjiBossCharacter:
- optionally assign a custom initialization effect;
- assign GA_Enemy_Melee or a boss-specific phase-aware attack class;
- implement BP_OnDamaged for hit response;
- implement BP_OnDefeated for the defeat sequence;
- implement BP_OnBossPhaseChanged for phase transition visuals and behavior;
- configure the mesh, animation blueprint, collision, health bar and loot drop.

### 5. Navigation
Add a NavMeshBoundsVolume around the playable combat area, build navigation, and verify that the AI controller can reach the player without leaving the encounter space.

## Gameplay Tags added
- Input.Enemy.Attack — enemy AI attack request.
- Data.Damage — SetByCaller data name (FName), not a Gameplay Tag.
- Event.Combat.Hit — event broadcast to the target ASC after a validated hit.
- State.Boss.Phase.Two — boss phase two.
- State.Boss.Phase.Three — boss phase three.

## Combat flow
1. Player presses Primary Attack or enemy AI requests Input.Enemy.Attack.
2. GAS resolves the ability spec and applies its configured cooldown/cost.
3. Server runs a forward sphere sweep.
4. The server gathers unique actors implementing IAbilitySystemInterface.
5. The server computes damage from the source AttackPower and target Defense.
6. The configured Damage Gameplay Effect applies the negative SetByCaller magnitude to Health.
7. The target ASC receives Event.Combat.Hit for hit-reaction abilities or effects.
8. Enemy Health reaches zero: the enemy disables movement/collision and replicates defeated state.
9. Boss Health crosses a phase threshold: server updates BossPhase and phase Gameplay Tags; clients receive the replicated phase.

## Prototype balance values

| Parameter | Initial value |
|---|---:|
| Melee trace distance | 220 cm |
| Melee sweep radius | 90 cm |
| Base melee damage | 10 |
| AttackPower coefficient | 1.0 |
| Defense coefficient | 0.75 |
| Enemy sight radius | 1400 cm |
| Enemy lose-sight radius | 1700 cm |
| Enemy attack range | 220 cm |
| Enemy AI attack request interval | 1.0 s |
| Boss phase 2 threshold | 66% Health |
| Boss phase 3 threshold | 33% Health |

These are prototype values only. Test against character abilities, attack cadence and actual mobile frame rate before final balancing.

## Known scope limits
- The current melee attack is a forward sphere sweep, not a full combo or weapon-hitbox system.
- The first PvE side filter distinguishes AJumanjiEnemyCharacter from non-enemy pawns. A general faction/team matrix, neutral factions and PvP rules are not implemented.
- Player defeat/respawn flow is not complete.
- The enemy AI uses sight, pursuit and periodic attack requests, not a behavior tree or squad tactics.
- Boss phase events are provided, but phase-specific attack assets and encounter scripting must be authored.
- Custom Gameplay Effect assets, unique ability Blueprint children, animation montages, Niagara effects, sounds, maps and Navigation Mesh are editor-authored assets. Native fallback damage, enemy attributes, boss attributes and basic attacks are included in C++.
- No APK/AAB or Unreal Editor build is produced in this source-only repository operation.

## Acceptance checklist
- [ ] Project compiles in the target Unreal Engine 5.8 installation.
- [ ] Player pawn exposes its PlayerState ASC through IAbilitySystemInterface.
- [ ] Enemy pawn exposes its own ASC through IAbilitySystemInterface.
- [ ] A configured melee ability activates on the server only.
- [ ] If no custom primary ability is configured, the native default primary ability is granted.
- [ ] If no custom enemy ability is configured, the native default enemy ability is granted.
- [ ] Default enemy and boss initialization Gameplay Effects populate their prototype attributes.
- [ ] A forward trace detects an opposing PvE target once per activation.
- [ ] Same-side player/player and enemy/enemy hit results are ignored.
- [ ] GE_Damage_Base subtracts Health using the negative Data.Damage magnitude.
- [ ] Defense affects the calculated damage exactly once.
- [ ] Event.Combat.Hit reaches the target ASC.
- [ ] Enemy Health reaching zero replicates defeated state and disables collision/movement.
- [ ] Enemy AI acquires a player, navigates to range and requests its tagged ability.
- [ ] A Navigation Mesh is present and pathfinding works.
- [ ] Boss phase changes at the configured Health thresholds and replicates to clients.
- [ ] Phase tags and BP_OnBossPhaseChanged are applied once per transition.
- [ ] Multiplayer test confirms the client cannot supply an authoritative hit result.
- [ ] All effects, montages and VFX meet the target Android performance profile.