# JUMANJI 2.111.1 — Stage 03: Great Spirit Presentation Core

## Goal
Connect the replicated character choice from Stage 02 to the in-world third-person character presentation. When a player chooses a Great Spirit, the corresponding skeletal mesh is resolved from the roster definition and applied to that player's pawn.

This is a presentation integration layer. It does not create the nine production models, rigs, animations or the final selection screen.

## Implemented
- AJumanjiCharacter binds to AJumanjiPlayerState::OnSelectedCharacterChanged.
- The character binds at BeginPlay and when PlayerState is replicated through OnRep_PlayerState.
- The selected ID is resolved through UJumanjiCharacterRosterSubsystem.
- If a CharacterMesh soft reference exists and loads successfully, it is applied to the character's skeletal mesh component.
- If the asset reference is empty or unavailable, the existing/default mesh is preserved.
- LastAppliedCharacterId avoids redundant mesh resolution for repeated notifications.

## Runtime flow
1. The player selects a card in the future Great Spirit selection UI.
2. The widget calls AJumanjiPlayerController::RequestSelectCharacter(CharacterId).
3. The server validates the ID against UJumanjiCharacterRosterSubsystem.
4. AJumanjiPlayerState updates SelectedCharacterId and replicates it.
5. AJumanjiCharacter receives the selection delegate event.
6. The character resolves the definition and loads its CharacterMesh soft reference.
7. The mesh is applied locally to the pawn presentation.

The PlayerState remains authoritative for the selected ID. The UI does not set meshes directly.

## Asset naming contract
Import the nine character meshes using stable names:
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_I
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_II
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_III
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_IV
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_V
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_VI
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_VII
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_VIII
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_IX

Portraits should use T_GreatSpirit_I_Portrait through T_GreatSpirit_IX_Portrait. Assign soft references in the roster definitions when the Unreal assets have been imported.

## Skeleton and animation requirements
- Prefer one shared humanoid skeleton where body proportions and rig compatibility permit.
- Use a shared locomotion animation blueprint for idle, walk, run, jump and fall.
- Use per-character animation layers only where a character requires unique movement.
- Keep crowns, feathers, braids and ornaments modular where practical.
- Include LODs and mobile-appropriate material/texture budgets.
- Do not force a shared skeleton if a design genuinely requires a different rig; document the retargeting plan.

## Great Spirit selection screen contract
Create a UMG widget named WBP_GreatSpiritSelection.
- Show nine cards in the same order as the reference.
- Each card shows portrait, Roman numeral, display name, role and element.
- Selected card receives a clear frame and selected-state label.
- Details panel shows description, signature ability and passive concept.
- Confirm calls RequestSelectCharacter with the selected CharacterId.
- Cancel/back returns without changing PlayerState.
- Populate cards using UJumanjiCharacterRosterSubsystem::GetRosterForBlueprint.
- Read initial selection from AJumanjiPlayerState::GetSelectedCharacterId and refresh on OnSelectedCharacterChanged.

The widget is an Unreal Editor asset task; this repository stage provides runtime integration but not a compiled UMG asset.

## Network and performance notes
- PlayerState is the source of truth for the selected character ID.
- The server validates selection before the ID is replicated.
- Mesh soft references are presentation assets; empty references preserve the current mesh.
- This implementation uses LoadSynchronous. Before shipping a large roster, preload selected assets during the lobby/loading phase or replace synchronous loading with asynchronous loading.
- Do not replicate skeletal mesh pointers directly. Replicate stable character IDs and resolve assets locally.
- Ensure all clients have the same cooked character assets and stable IDs.

## Acceptance checklist
- [ ] Project compiles in the target Unreal Engine 5.8 installation.
- [ ] A pawn binds to its PlayerState selection delegate.
- [ ] Great Spirit I remains the default when no other selection is made.
- [ ] A valid selection updates PlayerState on the server.
- [ ] The selected ID replicates to relevant clients.
- [ ] If a mesh asset is assigned, the pawn applies the matching skeletal mesh.
- [ ] If no mesh is assigned, the default mesh remains intact.
- [ ] Repeated notification for the same ID does not repeatedly reload the mesh.
- [ ] All nine cards can be populated by the future UMG widget.
- [ ] Multiplayer test confirms each player's selection changes only that player's pawn.

## Not included
- production character models, portraits, rigs or animation assets;
- actual UMG selection screen asset;
- asynchronous asset preloading;
- full character-specific GAS abilities;
- account persistence, unlocks and progression;
- final Android touch navigation and device profiling.