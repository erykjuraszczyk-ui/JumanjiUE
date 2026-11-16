# JUMANJI 2.111.1 — Unreal Engine 5.8

Open-world third-person adventure RPG prototype with a shared-world direction and the original Living Board event system.

## Project status

- **Stage 01 — UE5 Foundation:** project descriptor, C++ targets, third-person character, GameMode/GameState and initial board-event replication.
- **Stage 02 — Gameplay Core:** Enhanced Input integration points, replicated GAS attributes, validated interaction interface, a placeable Living Board actor and configurable local HUD creation.

Development is continuing on branch feature/ue5-foundation-2.111.1.

This is a source-code foundation, not a finished game or installable Android build. Unreal maps, Input Action assets, Widget Blueprints, character art, animation assets, complete gameplay abilities/effects, backend persistence and packaged APK/AAB still need to be created and verified.

## Requirements

- Unreal Engine 5.8;
- C++ compiler/toolchain supported by the installed UE version;
- Android SDK/NDK/JDK configured for UE 5.8 when packaging Android builds.

## Open

1. Clone the repository and check out feature/ue5-foundation-2.111.1.
2. Open JumanjiUE.uproject with Unreal Engine 5.8.
3. Generate project files and compile the project when prompted.
4. Create the prototype map Content/Maps/L_Jumanji_Prototype.umap.
5. Create the Enhanced Input assets described in Content/Input/README.md.
6. Create BP_JumanjiCharacter and assign the input assets.
7. Create WBP_JumanjiHUD and BP_JumanjiPlayerController as described in Content/UI/README.md.
8. Create BP_JumanjiGameMode and set it as the default GameMode.
9. Place AJumanjiBoardActor in the map and set its EventId.
10. Press Play and test movement, interaction and board-event activation.

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | WASD | Left stick |
| Look | Mouse | Right stick |
| Jump | Space | Bottom face button |
| Interact | E | Left face button |

## Architecture

- Source/JumanjiUE — runtime C++ module.
- Config — project and legacy input configuration.
- Content — Unreal assets created in the Editor.
- Docs — implementation and acceptance notes.

## Build verification

The remote source changes have not been compiled in an Unreal Engine 5.8 environment during this update. Complete the acceptance checklists in Docs/STAGE_01_FOUNDATION.md and Docs/STAGE_02_GAMEPLAY_CORE.md before treating the project as build-verified.
