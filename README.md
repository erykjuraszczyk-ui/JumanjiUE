# JUMANJI 2.111.1 — Unreal Engine 5.8

Open-world third-person adventure RPG prototype with a shared-world direction and the original Living Board event system.

## Project status

Stage 01 — UE5 Foundation is scaffolded on branch feature/ue5-foundation-2.111.1.

This repository now contains:
- a native Unreal Engine 5.8 C++ project;
- GameMode and replicated GameState;
- a third-person character with camera and starter movement input;
- a server-authoritative Living Board event-state subsystem;
- project configuration, repository hygiene and implementation notes.

This is a development foundation, not a finished game or installable Android build. The first Unreal map, art assets, animations, touch HUD, complete GAS abilities, backend services and packaged APK/AAB still need to be implemented and verified.

## Requirements

- Unreal Engine 5.8;
- C++ compiler/toolchain supported by the installed UE version;
- Android SDK/NDK/JDK configured for UE 5.8 when packaging Android builds.

## Open

1. Clone this repository.
2. Open JumanjiUE.uproject with Unreal Engine 5.8.
3. Generate project files and compile the project when prompted.
4. Create Content/Maps/L_Jumanji_Prototype.umap and set it as the default map.
5. Press Play to test the initial third-person character.

See Docs/STAGE_01_FOUNDATION.md for the detailed setup and acceptance checklist.

## Controls in the initial prototype

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | WASD | Left stick |
| Look | Mouse | Right stick |
| Jump | Space | Bottom face button |

The current input bindings are a bootstrap. Enhanced Input assets and the final Android touch layout are scheduled for the next stage.

## Architecture

- Source/JumanjiUE — runtime C++ module.
- Config — project and input configuration.
- Content — Unreal assets; maps and art are not yet included.
- Docs — implementation and production notes.

## Build verification

The repository scaffold has not been compiled in an Unreal Editor environment during this update. Do not treat the project as build-verified until the acceptance checklist in the stage documentation has been completed locally.
