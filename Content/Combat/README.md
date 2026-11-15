# Combat Asset Setup — JUMANJI 2.111.1

## Damage Gameplay Effect
The native UJumanjiDefaultDamageGameplayEffect is assigned automatically. It is Instant, applies an Additive modifier to Health, and reads the negative SetByCaller data name Data.Damage.

A designer may create GE_Damage_Base as a replacement:
- Duration Policy: Instant.
- Modifier operation: Additive.
- Magnitude: SetByCaller.
- Data name: Data.Damage.
The melee ability supplies a negative value (for example -16.25), which subtracts Health. Do not subtract Defense again inside the effect.

## Great Spirit primary attack
If a primary attack class is not assigned in DT_GreatSpiritRoster, the server grants UJumanjiDefaultPlayerMeleeAbility as a temporary fallback to all nine spirits. For unique attacks, create Blueprint children GA_GS_I_Primary through GA_GS_IX_Primary from UJumanjiMeleeAttackAbility. Configure AbilitySlot=PrimaryAttack, InputTag=Input.Ability.Primary, optional DamageEffectClass override, TraceDistance, TraceRadius and BaseDamage. Assign each class to PrimaryAttackAbilityClass in DT_GreatSpiritRoster.

## Enemy melee ability
UJumanjiDefaultEnemyMeleeAbility is assigned automatically when BasicAttackAbilityClass is unset. For a custom enemy move, create GA_Enemy_Melee from UJumanjiMeleeAttackAbility with InputTag=Input.Enemy.Attack, an optional custom damage effect, cooldown, attack wind-up and Gameplay Cue. Assign it to BasicAttackAbilityClass in BP_JumanjiEnemy.

## Enemy initialization effect
UJumanjiEnemyInitializationGameplayEffect is assigned by default. It sets Health/MaxHealth 100, AttackPower 8 and Defense 2. Assign a custom Instant Gameplay Effect to InitializationEffectClass when a specific enemy needs different values.

## Boss initialization effect
UJumanjiBossInitializationGameplayEffect is assigned by default. It sets Health/MaxHealth 600, AttackPower 18 and Defense 12. A BP_BoardGuardian child may override InitializationEffectClass with GE_BossGuardian_Init. These values require balance testing.

## Blueprint events
- BP_OnDamaged: play a hit reaction, material flash or sound.
- BP_OnDefeated: play the defeat animation and disable reward interaction until the server confirms the reward.
- BP_OnBossPhaseChanged: change attack pattern, animation state, VFX and arena events for phases 2 and 3.

## AI map setup
Place a NavMeshBoundsVolume around the encounter space, rebuild navigation and verify the enemy can path to the player. The prototype AI uses Sight perception and MoveToActor; it does not use a Behavior Tree yet.