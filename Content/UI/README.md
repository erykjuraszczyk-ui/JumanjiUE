# Great Spirit Selection UI

The C++ base widget is UJumanjiGreatSpiritSelectionWidget. Create WBP_GreatSpiritSelection as a Widget Blueprint child of this class after compiling the project.

Create a reusable WBP_GreatSpiritCard for each of the nine roster entries. Use the roster data for names, roles, elements and color accents. If a portrait is not assigned, show a numbered placeholder rather than reusing another character's art.

See Docs/STAGE_04_GREAT_SPIRIT_SELECT_UI.md for the layout, Blueprint graph wiring, preview-scene setup and acceptance checklist.

The actual .uasset widgets, portrait textures and preview render-target assets are not included yet.

## Adventure HUD

Create WBP_AdventureHUD as a Widget Blueprint child of UJumanjiAdventureHUDWidget. Configure this class as AdventureHUDWidgetClass on BP_JumanjiPlayerController. The local PlayerController creates it at BeginPlay.

Implement the OnAdventurePresentationChanged event to redraw the HUD from:
- GetChapterState;
- GetAdventureObjectives;
- GetCompletedObjectiveCount;
- GetTotalObjectiveCount;
- GetObjectiveProgress.

The widget base subscribes to replicated GameState chapter/objective changes and unsubscribes in NativeDestruct. It is a presentation layer only; it must not write puzzle or objective progress.
