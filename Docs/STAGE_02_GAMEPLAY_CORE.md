# JUMANJI 2.111.1 — Stage 02: Gameplay Core

## Scope

Stage 02 adds reusable C++ gameplay foundations for input, GAS ownership, attributes, world interaction, board activation and local HUD creation.

## Implemented in source

- Enhanced Input mapping-context and action references on the player character.
- Legacy input bindings retained as a fallback.
- Character implements IAbilitySystemInterface and owns a replicated AbilitySystemComponent.
- JumanjiAttributeSet defines replicated Health, MaxHealth, Stamina, MaxStamina, Resonance and MaxResonance.
- Ability actor info is initialized on server possession and client controller replication.
- Server RPC for interaction requests with target/interface/distance validation.
- JumanjiInteractable interface for board actors, NPCs, doors and world objects.
- JumanjiBoardActor with a simple placeholder mesh and configurable EventId.
- Living Board actor starts an event through JumanjiBoardWorldSubsystem on server authority.
- JumanjiPlayerController creates an assigned HUD Widget Blueprint for the local player.

## Enhanced Input assets to create in Unreal Editor

See Content/Input/README.md. Create:
- IA_Move — Axis2D;
- IA_Look — Axis2D;
- IA_Jump — Boolean;
- IA_Interact — Boolean;
- IMC_Player — Input Mapping Context.

Create BP_JumanjiCharacter as a child of AJumanjiCharacter and assign IMC_Player plus the four Input Actions in Class Defaults. When an action is not assigned, its legacy input binding remains active.

## Character Ability System

AJumanjiCharacter owns the AbilitySystemComponent and JumanjiAttributeSet. The component is replicated and uses Mixed effect replication mode. Actor info is initialized in BeginPlay, server possession and client controller replication.

Initial values:
- Health: 100 / MaxHealth 100
- Stamina: 100 / MaxStamina 100
- Resonance: 0 / MaxResonance 100

These attributes are replicated foundations. This stage does not yet add damage calculations, stamina regeneration, ability grants, stamina costs, cooldowns, gameplay abilities or gameplay effects. Implement those in Stage 03 with server authority.

## World interaction

Place AJumanjiBoardActor into the prototype map. Set EventId in its Details panel to BOARD_EMERALD_INTRO or another unique event identifier.

The character performs a camera line trace. The server RPC checks:
- target validity;
- interface implementation;
- maximum interaction distance;
- target's CanInteract result.

The board actor only starts an event on the authoritative server. The current actor uses a simple cube mesh as a placeholder; replace it with a project-owned board mesh and material later.

## Mobile HUD

AJumanjiPlayerController creates the Widget Blueprint assigned to HUDWidgetClass for the local player. See Content/UI/README.md for the editor setup.

Create:
1. WBP_JumanjiHUD — Widget Blueprint based on UserWidget.
2. BP_JumanjiPlayerController — Blueprint child of AJumanjiPlayerController; assign WBP_JumanjiHUD to HUDWidgetClass.
3. BP_JumanjiGameMode — Blueprint child of AJumanjiGameMode; set BP_JumanjiPlayerController and BP_JumanjiCharacter as its classes.

Recommended HUD layout:
- left: movement joystick;
- right: camera drag region;
- bottom-right: Jump and Interact buttons;
- top-left: Health and Stamina bars;
- top-right: minimap placeholder;
- center-top: active board event and objective.

Bind the Interact button's OnPressed event to TryInteract on the possessed AJumanjiCharacter. Bind Jump OnPressed to Jump and OnReleased to StopJumping. For movement and camera look, use touch-aware input actions/a dedicated touch-input component. Do not simulate mouse input for the movement joystick.

The Widget Blueprint and Input Action assets are binary editor assets; they must be created and saved in Unreal Editor.

## Important scope limits

- No map asset is included.
- No Input Action, Mapping Context or Widget Blueprint assets are included.
- No Relic gameplay abilities/effects, damage execution or stamina regeneration are implemented yet.
- No Android APK/AAB is included.
- Source has not been compiled against the local Unreal installation during this remote repository update.

## Acceptance checklist

- [ ] Project compiles in the installed UE 5.8 toolchain.
- [ ] BP_JumanjiCharacter has its four Enhanced Input actions assigned.
- [ ] WASD/gamepad movement and camera look work.
- [ ] Jump action works.
- [ ] E/Gamepad interaction action works.
- [ ] WBP_JumanjiHUD appears for the local player.
- [ ] HUD Interact button calls TryInteract.
- [ ] Board actor is visible and traceable.
- [ ] Server rejects interaction requests outside the configured distance.
- [ ] Board interaction starts at most one active event on the server.
- [ ] Health, Stamina and Resonance attributes are visible through the Ability System Component.

## Next stage

Stage 03 — Relic Combat: gameplay effects, stamina costs, Resonance gain, dodge window, first Relic ability, damage execution and one training enemy.
