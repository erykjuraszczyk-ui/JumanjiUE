# JUMANJI 2.111.1 — Stage 02: Gameplay Core

## Scope

Stage 02 adds reusable C++ gameplay foundations for player attributes, GAS ownership, world interaction and the first physical Living Board actor.

## Implemented in source

- Enhanced Input action/context references added to the character; legacy axis bindings remain available as a fallback.
- Character implements IAbilitySystemInterface and owns an AbilitySystemComponent and JumanjiAttributeSet.
- Replicated Health, MaxHealth, Stamina, MaxStamina, Resonance and MaxResonance attributes.
- Server RPC for interaction requests with interface and distance checks.
- JumanjiInteractable interface for board actors, NPCs and world objects.
- JumanjiBoardActor with a simple placeholder mesh and configurable event ID.
- Living Board actor starts the event through JumanjiBoardWorldSubsystem on server authority.

## Enhanced Input assets to create in Unreal Editor

The C++ class exposes optional properties. Create these assets under Content/Input:

1. IA_Move — Input Action, Value Type: Axis2D.
2. IA_Look — Input Action, Value Type: Axis2D.
3. IA_Jump — Input Action, Value Type: Boolean.
4. IA_Interact — Input Action, Value Type: Boolean.
5. IMC_Player — Input Mapping Context.

Suggested bindings:
- IA_Move: WASD and Gamepad Left Stick.
- IA_Look: Mouse Delta and Gamepad Right Stick.
- IA_Jump: Space and Gamepad Face Button Bottom.
- IA_Interact: E and Gamepad Face Button Left.

For keyboard movement, map W/S to the vertical component and A/D to the horizontal component using Enhanced Input Swizzle and Negate modifiers. Configure look axis signs in the mapping context so mouse/gamepad directions feel natural.

Create a Blueprint child of AJumanjiCharacter (for example BP_JumanjiCharacter) and assign IMC_Player plus the four action assets in its Class Defaults. Set this Blueprint as the default pawn in a Blueprint child of AJumanjiGameMode, then set that GameMode in Project Settings → Maps & Modes.

If any action asset is not assigned, the C++ class retains the legacy input binding as a fallback. The virtual joystick setting is only a bootstrap and does not replace a production touch HUD.

## Character Ability System

AJumanjiCharacter owns the AbilitySystemComponent and JumanjiAttributeSet. The component is replicated and uses Mixed effect replication mode. Actor info is initialized on BeginPlay and possession.

Initial values:
- Health: 100 / MaxHealth 100
- Stamina: 100 / MaxStamina 100
- Resonance: 0 / MaxResonance 100

These attributes are replicated foundations. This stage does not yet add damage calculations, stamina regeneration, costs, cooldowns, gameplay abilities or gameplay effects. Implement those in the Relic/Combat stage, with server authority.

## World interaction

Place AJumanjiBoardActor into the prototype map. Set EventId in its Details panel to BOARD_EMERALD_INTRO or another unique event identifier.

The board actor implements JumanjiInteractable. The character performs a camera trace, sends the selected actor to the server, and the server checks:
- target validity;
- interface implementation;
- maximum interaction distance;
- target's CanInteract result.

The board only starts an event on the authoritative server. The current actor uses a simple cube mesh as a placeholder; replace it with a project-owned board mesh and material later.

## Mobile HUD setup

Create a Widget Blueprint based on UserWidget named WBP_JumanjiHUD under Content/UI.

Recommended first layout:
- left: movement joystick;
- right: camera drag region;
- bottom-right: Jump and Interact buttons;
- top-left: Health and Stamina bars;
- top-right: minimap placeholder;
- center-top: current board event and objective text.

In WBP_JumanjiHUD, add a reference to the possessed AJumanjiCharacter and bind the Interact button's OnPressed event to TryInteract. Bind Jump OnPressed to Jump and OnReleased to StopJumping. For production, route movement and look through Enhanced Input actions or a dedicated touch-input component rather than simulating desktop mouse events.

This repository stage does not create binary Widget Blueprint assets; they must be created and saved in Unreal Editor.

## Important scope limits

- No map asset is included.
- No Widget Blueprint or Input Action assets are included.
- No gameplay abilities/effects, damage rules or stamina regeneration are implemented yet.
- No Android APK/AAB is included.
- Source code has not been compiled against the local Unreal installation as part of this remote repository update.

## Acceptance checklist

- [ ] Project compiles in the installed UE 5.8 toolchain.
- [ ] BP_JumanjiCharacter has the four Enhanced Input actions assigned.
- [ ] WASD/gamepad movement and camera look work.
- [ ] Jump action works.
- [ ] E/Gamepad interaction action works.
- [ ] WBP_JumanjiHUD buttons call character actions.
- [ ] Board actor is visible and traceable.
- [ ] Server rejects interactions beyond the configured distance.
- [ ] Board interaction starts only one event on the server.
- [ ] Health, Stamina and Resonance attributes appear in the Ability System Component.

## Next stage

Stage 03 — Relic Combat: gameplay effects, stamina costs, Resonance gain, dodge window, first Relic ability, damage execution and one training enemy.
