# JUMANJI 2.111.1 — Stage 01: UE5 Foundation

## Goal

Establish a native Unreal Engine 5.8 C++ project baseline for the first playable third-person prototype.

## Added in this stage

- Unreal Engine 5.8 project descriptor.
- Runtime and Editor C++ targets.
- Core engine, Enhanced Input and Gameplay Ability System module dependencies.
- Third-person character with spring-arm camera.
- Starter keyboard/gamepad axis bindings and Unreal virtual joystick setting.
- GameMode and replicated GameState classes.
- Server-authoritative Living Board event state.
- World subsystem used to start and end a single active board event.
- Repository rules for generated files and Unreal binary assets.

## Important scope limits

This is a source-code foundation, not a finished game build.

The repository does not yet contain:
- a saved Unreal map (.umap);
- character or environment art assets;
- animation blueprints or Enhanced Input action assets;
- a touch-first HUD;
- a complete Gameplay Ability System implementation;
- a dedicated-server deployment;
- authentication, profile storage or cloud persistence;
- Android signing keys or a packaged APK/AAB.

The project has not been compiled in an Unreal Editor environment as part of this repository update. A local UE 5.8 build is required to verify engine and toolchain compatibility.

## Open the project

1. Install Unreal Engine 5.8 from the Epic Games Launcher.
2. Install the C++ toolchain supported by the UE 5.8 installation on your operating system.
3. Clone this repository and check out branch feature/ue5-foundation-2.111.1.
4. Open JumanjiUE.uproject.
5. Allow Unreal Editor to generate project files and compile the JumanjiUE module.
6. If the editor prompts for a rebuild, choose Yes. If compilation fails, capture the first compiler error and its surrounding context.
7. Create Content/Maps/L_Jumanji_Prototype.umap.
8. Add a floor, traversal obstacles, a PlayerStart and basic lighting.
9. Set the prototype map under Project Settings → Maps & Modes.
10. Press Play and verify movement, camera rotation and jump.

## Controls in the first prototype

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | WASD | Left stick |
| Look | Mouse | Right stick |
| Jump | Space | Bottom face button |

The project input configuration enables Unreal's virtual joystick option. The production Android control layout, button placement, sensitivity settings and accessibility options are part of the mobile UI stage.

## Living Board foundation

The server owns the active event state through AJumanjiGameState.

UJumanjiBoardWorldSubsystem exposes:
- StartBoardEvent(EventId)
- EndBoardEvent(EventId)

The subsystem rejects empty event IDs, client-side calls, attempts to start a second active event, and attempts to end a different event. The GameState replicates the active event ID, active flag and revision counter to connected clients.

This is only the event-state foundation. Event phases, objective progress, time limits, reward tables, persistence and anti-replay protection must be implemented before public multiplayer use.

## Android packaging

Before Android packaging:
1. Configure the Android SDK, NDK and JDK versions required by the installed UE 5.8 release.
2. Set the package identifier and supported architectures in Project Settings → Platforms → Android.
3. Use ARM64 for the production target.
4. Set minimum and target SDK versions according to current Google Play requirements and the selected Unreal version.
5. Configure signing outside the repository. Never commit keystores or passwords.
6. Package a Development build and test it on a physical Android 13+ device before creating a Shipping build.

No APK/AAB is included in this stage.

## Acceptance checklist

- [ ] Unreal Editor opens JumanjiUE.uproject.
- [ ] C++ project generation completes.
- [ ] JumanjiUEEditor builds successfully.
- [ ] Prototype map is created and set as the default map.
- [ ] Third-person movement and camera work.
- [ ] Jump works with keyboard and gamepad.
- [ ] The board event starts only on authority.
- [ ] A second simultaneous board event is rejected.
- [ ] Ending the active event clears the replicated state.
- [ ] A Development build launches on a physical Android device.

## Next stage

Stage 02 — Gameplay Core: Enhanced Input assets, mobile HUD, interaction interface, character attributes, GAS integration and the first playable Living Board objective sequence.
