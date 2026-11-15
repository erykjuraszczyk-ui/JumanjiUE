# JUMANJI 2.111.1 — Stage 05: Great Spirit Ability Core

## Goal
Establish the GAS runtime foundation for the nine Great Spirits: replicated attributes, a PlayerState-owned Ability System Component, selected-spirit ability grants, and input-tag activation for five ability slots.

This is reusable gameplay infrastructure. It does not claim that all 45 character-specific abilities have already been authored, balanced or tested.

## Architecture decision
The Ability System Component (ASC) and Attribute Set live on AJumanjiPlayerState. AJumanjiCharacter is the current Avatar Actor. This keeps ability state associated with the player session if a pawn is replaced. The character initializes the ASC actor info on server possession and client PlayerState replication.

## Added classes

### UJumanjiAttributeSet
Replicated attributes:
- Health / MaxHealth;
- Energy / MaxEnergy;
- Resonance / MaxResonance;
- AttackPower;
- Defense.

Initial development values: Health 100, MaxHealth 100, Energy 100, MaxEnergy 100, Resonance 0, MaxResonance 100, AttackPower 10 and Defense 5. These are prototype defaults, not final balance values.

Current Health, Energy and Resonance are clamped to their corresponding maximum attributes. Runtime changes should be applied through Gameplay Effects, not by setting attributes directly from UI code.

### UJumanjiAbilitySystemComponent
Project wrapper over UAbilitySystemComponent. It provides:
- TryActivateAbilitiesByInputTag(InputTag);
- GrantCharacterAbilityLoadout(CharacterDefinition, OutGrantedHandles);
- ClearGrantedAbilities(GrantedHandles).

Granting and clearing are server-only. A loaded class must derive from UJumanjiGameplayAbility and declare the expected ability slot. The grant function checks the slot before granting primary, active/signature, passive, ultimate and dodge classes. Passive abilities may set bActivateOnGranted when designed to remain active until removed.

### UJumanjiGameplayAbility
Common base for character abilities. It exposes InputTag, AbilitySlot and bActivateOnGranted. Derived Blueprint classes configure cooldown and cost Gameplay Effects, activation tags, animation tasks, target selection, Gameplay Effects and Gameplay Cues.

### AJumanjiPlayerState
Implements IAbilitySystemInterface and owns UJumanjiAbilitySystemComponent, UJumanjiAttributeSet and replicated SelectedCharacterId.

### AJumanjiCharacter
- initializes ASC actor info on server possession and client PlayerState replication;
- listens for selected-character changes;
- clears old character-specific grant handles on the server;
- grants the newly selected spirit's configured loadout;
- routes primary, active, ultimate and dodge input tags to the ASC;
- exposes ActivateAbilityByInputTag for future touch UI buttons.

## Five ability slots

| Slot | Input tag | Purpose | Typical resource |
|---|---|---|---|
| PrimaryAttack | Input.Ability.Primary | repeatable core action | none or very low cost |
| Active | Input.Ability.Active | signature action | Energy |
| Passive | no input required | persistent/event-driven trait | none |
| Ultimate | Input.Ability.Ultimate | major team or area action | Resonance |
| Dodge | Input.Ability.Dodge | evasive movement | cooldown or Energy |

The roster contains display names and soft class references for these slots. Class references remain empty in the built-in fallback catalog until ability assets are authored. FJumanjiCharacterDefinition now derives from FTableRowBase, so the full roster and its ability class references can be authored in a DataTable.

## Input mappings
Keyboard: Primary = Left Mouse Button; Active = E; Ultimate = R; Dodge = Left Shift.
Gamepad: Primary = Right Shoulder; Active = Face Button Left; Ultimate = Face Button Top; Dodge = Face Button Right.
Mobile UI should call AJumanjiCharacter::ActivateAbilityByInputTag or UJumanjiAbilitySystemComponent::TryActivateAbilitiesByInputTag. Touch buttons must not write attributes directly.

## Data-driven roster and ability assignment
The roster subsystem supports an optional ConfiguredRosterDataTable setting. It first builds the built-in nine-spirit fallback, then attempts to load the configured DataTable. The override is accepted only if its row struct is FJumanjiCharacterDefinition, contains exactly nine rows, has unique CharacterId values and SpiritNumber values 1–9, and has non-empty display names. If validation fails, the built-in roster remains active.

After creating DT_GreatSpiritRoster in Unreal Editor, set this in DefaultGame.ini:

[/Script/JumanjiUE.JumanjiCharacterRosterSubsystem]
ConfiguredRosterDataTable=/Game/Data/DT_GreatSpiritRoster.DT_GreatSpiritRoster

Each DataTable row can assign portrait, mesh and the five soft ability class fields. Use Roman-numeral row keys such as GREAT_SPIRIT_I, while CharacterId remains the stable runtime identity. This lets designers assign Blueprint ability classes without editing the C++ roster builder.

## Per-character loadout design
All nine proposed ability kits are documented in Docs/Design/GreatSpiritAbilityDesign.md. They are concepts until ability classes, Gameplay Effects, animations, cues and targeting logic are implemented.

## Resource and effect rules
- Health cannot exceed MaxHealth.
- Energy cannot exceed MaxEnergy.
- Resonance cannot exceed MaxResonance.
- Ultimate abilities should consume Resonance through a Gameplay Effect cost or an authoritative cost rule.
- Active abilities should use cost and cooldown Gameplay Effects configured on their ability classes.
- Passive effects should be removed when that character loadout is cleared.
- UI reads replicated attributes and cooldown state; it does not set them.

## Gameplay Tags
Config/DefaultGameplayTags.ini defines Input.Ability.Primary, Input.Ability.Active, Input.Ability.Ultimate and Input.Ability.Dodge. It also contains initial State.Combat and Element tags. Keep spellings stable because input mappings and ability class defaults refer to exact names.

## Asset authoring plan
Create Blueprint classes derived from UJumanjiGameplayAbility. Naming pattern for each spirit I–IX: GA_GS_[ROMAN]_Primary, GA_GS_[ROMAN]_Active, GA_GS_[ROMAN]_Passive, GA_GS_[ROMAN]_Ultimate and GA_GS_[ROMAN]_Dodge.

For each ability asset:
1. Set the matching AbilitySlot.
2. Set InputTag for primary, active, ultimate and dodge. Passive abilities normally have no input tag.
3. Assign cost and cooldown Gameplay Effects.
4. Set activation-required, activation-blocked and activation-owned tags.
5. Add animation, targeting, Gameplay Effects and Gameplay Cues.
6. Test client prediction and server validation.
7. Assign the class to the corresponding soft class field in the Great Spirit definition.

Use Gameplay Effects for attribute changes and cooldowns. Do not modify Health, Energy or Resonance directly from widgets or client-only code.

## Acceptance checklist
- [ ] Project compiles in the target Unreal Engine 5.8 installation.
- [ ] PlayerState implements IAbilitySystemInterface.
- [ ] ASC and AttributeSet are created on PlayerState.
- [ ] ASC is replicated in Mixed mode.
- [ ] Character initializes actor info on server possession.
- [ ] Character initializes actor info on client PlayerState replication.
- [ ] Attributes replicate and remain clamped.
- [ ] Input tags exist and match input mapping names.
- [ ] A configured DataTable overrides the fallback roster only when all nine rows pass validation.
- [ ] A test UJumanjiGameplayAbility class can be granted by the server.
- [ ] A matching input tag activates the corresponding granted ability.
- [ ] A class with the wrong ability slot is rejected.
- [ ] Switching Great Spirits clears only previously granted character ability handles.
- [ ] A passive configured to activate on grant starts and is removed with its loadout.
- [ ] A client cannot grant abilities or authoritatively alter attributes.
- [ ] Multiplayer test verifies ability activation and attribute replication.

## Not included
- 45 completed character-specific ability assets;
- concrete Gameplay Effect assets for cost, cooldown, damage, healing or shields;
- animation montages, targeting, server-side hit validation and Gameplay Cues;
- complete elemental reaction implementation;
- final touch HUD cooldown rings and resource bars;
- mobile performance tests and packaged APK/AAB.

These are separate content-authoring and gameplay-validation tasks.