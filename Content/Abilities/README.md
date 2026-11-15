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

Assign class references to the matching FJumanjiCharacterDefinition soft class fields after the classes exist.

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
Ability assets may use Animation Montages, Ability Tasks, Gameplay Events, Gameplay Cues, Niagara systems, audio and server-validated targeting. These binary content assets are not part of the text-only Stage 05 commit.

## Mobile considerations
- Use short, readable ability effects.
- Provide a clear cooldown state in the HUD.
- Avoid large full-screen flashes.
- Use the same Gameplay Tags on touch buttons and keyboard/gamepad input.
- Profile every effect on the target Android devices.