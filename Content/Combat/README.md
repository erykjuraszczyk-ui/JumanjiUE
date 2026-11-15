# Combat Asset Setup — JUMANJI 2.111.1

## GE_Damage_Base
Create an Instant Gameplay Effect with one Health modifier:
- Modifier operation: Additive.
- Magnitude: SetByCaller.
- Data tag: Data.Damage.
The melee ability supplies a negative value (for example -16.25), which subtracts Health. Do not subtract Defense again inside this Gameplay Effect.

## Great Spirit primary attack
Create Blueprint children GA_GS_I_Primary through GA_GS_IX_Primary from UJumanjiMeleeAttackAbility. Configure AbilitySlot=PrimaryAttack, InputTag=Input.Ability.Primary, DamageEffectClass=GE_Damage_Base, TraceDistance, TraceRadius and BaseDamage. Add a cooldown Gameplay Effect if the attack cadence requires one. Assign each class to PrimaryAttackAbilityClass in DT_GreatSpiritRoster.

## Enemy melee ability
Create GA_Enemy_Melee from UJumanjiMeleeAttackAbility with InputTag=Input.Enemy.Attack and GE_Damage_Base. Add a cooldown, an attack wind-up montage and a clear Gameplay Cue. Assign it to BasicAttackAbilityClass in BP_JumanjiEnemy.

## Enemy initialization effect
Create an Instant effect that sets MaxHealth, Health, AttackPower and Defense. Assign it to InitializationEffectClass in BP_JumanjiEnemy. Example prototype values: Health/MaxHealth 100, AttackPower 8, Defense 2.

## Boss initialization effect
Create GE_BossGuardian_Init. Example prototype values: Health/MaxHealth 600, AttackPower 18, Defense 12. Assign it to BP_BoardGuardian (child of AJumanjiBossCharacter). These values require balance testing.

## Blueprint events
- BP_OnDamaged: play a hit reaction, material flash or sound.
- BP_OnDefeated: play the defeat animation and disable reward interaction until the server confirms the reward.
- BP_OnBossPhaseChanged: change attack pattern, animation state, VFX and arena events for phases 2 and 3.

## AI map setup
Place a NavMeshBoundsVolume around the encounter space, rebuild navigation and verify the enemy can path to the player. The prototype AI uses Sight perception and MoveToActor; it does not use a Behavior Tree yet.