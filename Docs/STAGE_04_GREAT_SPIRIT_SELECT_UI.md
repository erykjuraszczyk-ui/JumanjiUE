# JUMANJI 2.111.1 — Stage 04: Great Spirit Select UI

## Goal
Provide a responsive selection-screen foundation for the nine Great Spirits. The screen is driven by a C++ UUserWidget base class and the roster subsystem. Designers build the visible layout in Unreal Editor as a Widget Blueprint.

## Added in this stage
- UJumanjiGreatSpiritSelectionWidget, a Blueprintable/Abstract UUserWidget base.
- Roster access for the nine Great Spirits.
- Preview selection separate from server-confirmed selection.
- Validation of preview IDs against the roster subsystem.
- Confirmation through AJumanjiPlayerController::RequestSelectCharacter.
- PlayerState delegate binding so the UI updates after the server-confirmed replicated selection.
- Cancel flow that restores the confirmed selection and closes the screen.
- AJumanjiPlayerController::OpenGreatSpiritSelection and CloseGreatSpiritSelection.
- Configurable Widget Blueprint class property on the controller.
- UMG module dependency in the runtime module.

## Layout specification
Design for landscape mobile first, with a responsive fallback for portrait screens.

### Landscape layout (recommended 16:9)
- Left panel (~36% width): nine cards, ordered I–IX. Use a 3×3 grid on phones/tablets; keep consistent card size and a clear selected state.
- Center panel (~40% width): large selected-character preview. Use a dedicated preview actor in a small preview scene and a SceneCapture2D rendered into a Render Target, or use a portrait fallback until 3D assets exist.
- Right panel (~24% width): Great Spirit name, title, element, role, short description, signature ability and passive ability.
- Bottom action row: Confirm and Back/Cancel. Keep touch targets large and outside display cutout/safe areas.

The supplied reference uses a 5+4 two-row collage. For mobile usability the in-game screen should preserve the same numbered order but may use a 3×3 grid. Do not distort the source portraits to force them into the original collage layout.

### Portrait layout
- Place the nine cards in a 3×3 grid at the top or in a scrollable grid.
- Put the preview and information panel below.
- Keep Confirm and Back fixed near the bottom safe area.
- Do not shrink the card text below comfortable mobile readability.

## Create the actual Widget Blueprint in Unreal Editor
The repository contains the C++ widget base, not a compiled .uasset layout. Create the following assets after the project builds.

### 1. Card widget
Create WBP_GreatSpiritCard:
- Parent class: UserWidget.
- Root: Button or a Border containing a Button.
- Children: portrait Image, number Text, display-name Text, role/element Text and selected-frame Border.
- Expose variables for CharacterId, portrait, number, name, role, element and selected state.
- Add an Event Dispatcher OnCardSelected(CharacterId).
- On click, broadcast the CharacterId.

### 2. Main selection widget
Create WBP_GreatSpiritSelection:
- Parent class: UJumanjiGreatSpiritSelectionWidget.
- Add a UniformGridPanel named RosterGrid.
- Add a preview region named CharacterPreview.
- Add text fields for name, title, description, element, role, signature ability and passive ability.
- Add buttons named ConfirmButton and BackButton.
- Add a selected-state indicator and pending/confirmation text.

### 3. Populate the nine cards
Implement BP_OnRosterReady(Entries, InitiallySelectedId):
1. Clear RosterGrid.
2. Loop over Entries.
3. Create WBP_GreatSpiritCard for each entry.
4. Assign the entry's ID, SpiritNumber, DisplayName, PortraitTexture, Role and Element.
5. Add each card to the grid using SpiritNumber: column = (SpiritNumber - 1) % 3, row = (SpiritNumber - 1) / 3.
6. Bind OnCardSelected to the parent widget's PreviewCharacter.
7. Apply the initial selected frame using InitiallySelectedId.

If PortraitTexture is not assigned, display a designed placeholder with the character's number and primary/secondary colors. Do not use a random portrait from another character.

### 4. Preview and details
Implement BP_OnCharacterPreviewChanged(Definition):
- update selected card highlight;
- set name, title and description;
- show role and element;
- show signature and passive ability names;
- use primary/secondary colors for accents;
- update the portrait.

For the 3D preview, use a dedicated preview scene:
- create a preview actor with a SkeletalMeshComponent;
- assign the selected definition's CharacterMesh if the soft reference is valid;
- place a SceneCapture2D camera facing the preview actor;
- render to a TextureRenderTarget2D;
- display that render target in the UMG preview image;
- keep a static portrait fallback when no skeletal mesh is configured.

The 3D preview must not spawn a second gameplay pawn or modify the player's live character.

### 5. Confirm and cancel
- Confirm button calls ConfirmSelection.
- While IsAwaitingServerConfirmation is true, disable repeated confirmation and show a pending indicator.
- Implement BP_OnSelectionRequestSent(CharacterId) to show the pending state.
- Implement BP_OnCharacterSelectionCommitted(CharacterId) to update the selected frame, clear pending state and show confirmation.
- Back button calls CancelSelection.
- Implement BP_OnSelectionCancelled for any closing animation; if using an animation, defer calling CancelSelection until the animation completes or provide a separate close animation flow.

The C++ widget only requests selection. The server remains authoritative and the UI is updated when PlayerState reports the accepted ID.

## Configure the controller to create the Widget Blueprint
1. Compile the C++ project in Unreal Editor.
2. Create BP_JumanjiPlayerController, parent class JumanjiPlayerController.
3. In Class Defaults, set GreatSpiritSelectionWidgetClass to WBP_GreatSpiritSelection.
4. Create BP_JumanjiGameMode, parent class JumanjiGameMode.
5. Set its Player Controller Class to BP_JumanjiPlayerController.
6. Set the project default game mode to BP_JumanjiGameMode, or use it in the relevant map's World Settings.
7. Call OpenGreatSpiritSelection from the main menu or character-select entry point.

If the widget class is unset, OpenGreatSpiritSelection safely returns false.

## C++ API
UJumanjiGreatSpiritSelectionWidget:
- PreviewCharacter(CharacterId) — validate and preview a roster entry.
- ConfirmSelection() — send the selected ID through the owning PlayerController.
- CancelSelection() — revert preview to confirmed choice and close the widget.
- GetPreviewCharacterId(), GetConfirmedCharacterId(), IsAwaitingServerConfirmation().
- GetRosterEntries(OutEntries).

Blueprint events: BP_OnRosterReady, BP_OnCharacterPreviewChanged, BP_OnCharacterSelectionCommitted, BP_OnSelectionCancelled and BP_OnSelectionRequestSent.

AJumanjiPlayerController:
- OpenGreatSpiritSelection().
- CloseGreatSpiritSelection().
- RequestSelectCharacter(CharacterId).

## Acceptance criteria
- [ ] Project compiles in the target Unreal Engine 5.8 installation.
- [ ] The Widget Blueprint can be created with UJumanjiGreatSpiritSelectionWidget as parent.
- [ ] The widget receives exactly nine roster entries.
- [ ] The card grid preserves Great Spirit I–IX order.
- [ ] A valid card click changes the preview and details without changing PlayerState.
- [ ] Confirm sends the preview ID through PlayerController.
- [ ] The screen displays pending state until PlayerState confirms the choice.
- [ ] The committed ID updates from the replicated PlayerState delegate.
- [ ] Cancel returns to the committed selection and closes the screen.
- [ ] Missing portrait or mesh assets show placeholders without breaking the widget.
- [ ] The screen is usable with touch on an Android device.
- [ ] The 3D preview, when configured, does not alter the live player pawn.

## Not included in this repository stage
- compiled UMG Widget Blueprint .uasset;
- binary portrait textures;
- production 3D preview models and render-target assets;
- final animations/transitions and sound effects;
- localization and accessibility validation;
- Android APK/AAB build or device performance results.

These are editor-authored asset and validation tasks; source code alone cannot create the finished binary widget and art assets.