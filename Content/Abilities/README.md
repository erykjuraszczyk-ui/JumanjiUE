# Great Spirit Ability Assets

The GAS runtime foundation is in C++. Create actual ability and Gameplay Effect assets in Unreal Editor after the project compiles.

## Ability classes
Create Blueprint classes derived from UJumanjiGameplayAbility. Naming pattern for each spirit I–IX:
- GA_GS_[ROMAN]_Primary
- GA_GS_[ROMAN]_Active
- GA_GS_[ROMAN]_Passive
- GA_GS_[ROMAN]_Ultimate
- GA_GS_[ROMAN]_Dodge

For each asset set the matching AbilitySlot. Set InputTag for primary, active, ultimate and dodge. Passive abilities normally leave InputTag unset and set bActivateOnGranted only when their implementation is designed to remain active.

## Configured ability runtime (Stage 17)

Create the standard combat Blueprint children from UJumanjiConfiguredEffectAbility for Primary, Active, Ultimate and Dodge. The shared base provides:
- TargetingMode: Self, ForwardSweep, RadialPulse or Dash;
- TargetTeam: SelfOnly, Friendly, Hostile or Any;
- server-authoritative target acquisition using ECC_Pawn;
- TargetEffectClass and a signed SetByCaller magnitude;
- optional SelfEffectClass for an additional owner effect;
- optional AttackPower scaling;
- optional Animation Montage and Gameplay Cue;
- BP_OnConfiguredTargetApplied for additional per-target Blueprint logic.

Recommended SetByCaller tag for new effects: Data.Ability.Magnitude. The effect asset and Blueprint field must use the exact same tag. Existing damage effects may keep their own tag, such as Data.Damage.

The configured base is a shared execution layer, not 36 finished combat designs. Multi-step combos, target marks, cleansing, damage absorption, world cover, faction rules and event-driven passives require dedicated Blueprint logic or additional native systems. Passive abilities should not be implemented as a one-frame sweep; manage any persistent Gameplay Effect so it is removed when the passive is unequipped.

For the current production names, per-character targets and asset list, use Docs/Design/GreatSpiritAbilityProductionMatrix.md and Docs/STAGE_17_GREAT_SPIRIT_ABILITY_CONTENT_PACK.md.

## Saved AbilityId catalog (Stage 16)

The logical ability IDs stored in profile schema v2 are resolved at runtime by UJumanjiAbilityCatalogSubsystem. Do not store Blueprint class paths in the player profile and do not accept client-supplied class references.

Create a DataTable:
- folder: /Game/Abilities/Data;
- name: DT_GreatSpiritAbilityCatalog;
- row structure: FJumanjiAbilityCatalogEntry;
- optional config in DefaultGame.ini:

```ini
[/Script/JumanjiUE.JumanjiAbilityCatalogSubsystem]
ConfiguredAbilityDataTable=/Game/Abilities/Data/DT_GreatSpiritAbilityCatalog.DT_GreatSpiritAbilityCatalog
```

Each row requires AbilityId, CharacterId, SlotId, AbilitySlot, InputTag, DisplayName, AbilityClass and bEnabled. The class must derive from UJumanjiGameplayAbility. AbilitySlot and InputTag must match the slot contract. Invalid rows are ignored.

Stable slot IDs:
- SLOT_PRIMARY → PrimaryAttack / Input.Ability.Primary
- SLOT_ACTIVE → Active / Input.Ability.Active
- SLOT_PASSIVE → Passive / no input tag
- SLOT_ULTIMATE → Ultimate / Input.Ability.Ultimate
- SLOT_DODGE → Dodge / Input.Ability.Dodge

Stable default IDs use the pattern ABILITY_GS_[ROMAN]_[SLOT], for example ABILITY_GS_I_PRIMARY and ABILITY_GS_IX_DODGE. Additional variants may use their own unique AbilityId while retaining the same CharacterId and SlotId.

The built-in fallback catalog provides a shared native primary melee ability for all nine spirits. Active, Passive, Ultimate and Dodge remain unavailable until their real Blueprint classes are assigned. A roster entry or catalog row with no real class is not treated as a playable ability.

## Runtime equipment

The selection/loadout UI reads valid options with GetAbilityChoices(CharacterId, SlotId), then requests a change through AJumanjiPlayerController::RequestEquipAbilitySlot. The server repeats catalog validation and stores only the stable ID through AJumanjiPlayerState. The pawn reacts to the replicated progression update and the ASC grants only allowlisted classes.

Do not call GiveAbility from a Widget Blueprint. Do not directly set ASC attributes from UI. The server-owned ASC tracks the handles it granted and removes only the old character loadout when replacing it.

## Gameplay Effects
Create project effects in Content/Abilities/Effects:
- GE_Cost_Energy_Active;
- GE_Cost_Resonance_Ultimate;
- GE_Cooldown_Active;
- GE_Cooldown_Ultimate;
- GE_Cooldown_Dodge;
- GE_Heal_Base;
- GE_Shield_Base;
- GE_Damage_Base for eligible PvE abilities;
- GE_Energy_Regen;
- GE_Resonance_Gain.

Configure attribute modifiers and durations in the editor. Use server-authoritative Gameplay Effects and suitable prediction policies. Do not change attributes directly from UI code.

## Animation and effects
Ability assets may use Animation Montages, Ability Tasks, Gameplay Events, Gameplay Cues, Niagara systems, audio and server-validated targeting. These binary content assets are authored in Unreal Editor.

## Mobile considerations
- Use short, readable ability effects.
- Provide a clear cooldown state in the HUD.
- Avoid large full-screen flashes.
- Use the same Gameplay Tags on touch buttons and keyboard/gamepad input.
- Profile every effect on the target Android devices.

## Scope reminder

Stage 16 provides the allowlisted runtime resolver and saved-slot activation path; it does not create the 45 Blueprint ability classes or their Gameplay Effects, animation assets, Niagara systems or final HUD icons. The source build and Android device validation remain required.