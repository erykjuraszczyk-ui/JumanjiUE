# Content/Adventure/Trials — Blueprint authoring

## First room

Create BP_Trial_RootRunes as a child of the native AJumanjiRootRuneTrial class. The native class already provides the three-step Seed → Sprout → Tree sequence and validates answers on the server.

Recommended components for the Blueprint:

- Static Mesh or Skeletal Mesh for the rune stone.
- Scene Component for the interaction prompt anchor.
- Optional mesh components for the three visual runes.
- Audio/VFX components for correct-answer feedback.
- Optional overlap volume for local prompt visibility. The overlap is presentation only; the server still validates the interaction distance.

## Adventure objective

The Stage 07 adventure definition must include:

- ObjectiveId = TRIAL_ROOT_RUNES
- RequiredProgress = 3
- CurrentProgress = 0

After the server calls StartBoardAdventure, call ActivateTrial on the placed actor. Do not activate it on a client.

## Widget contract

WBP_TrialInteraction reads the following from the trial actor:

- GetCurrentStepPrompt
- GetCurrentStepOptions
- GetCurrentStepIndex
- GetStepCount
- GetTrialRevision
- GetTrialState

When a player selects an option, call the owning PlayerController's RequestTrialInteraction with:

- the AJumanjiTrialActor reference;
- the selected option ID;
- the latest GetTrialRevision value.

Bind to OnTrialStateChanged and redraw the widget from the replicated actor state. On a remote client, a successful return from RequestTrialInteraction means only that the RPC was queued. It does not mean the server accepted the option.

## Authoring the other eight trials

Create Blueprint children of AJumanjiTrialActor and set the inherited objective ID and step definitions in Class Defaults. Every objective must have a RequiredProgress equal to the number of steps in that actor. Each step needs a unique StepId, non-empty prompt, 2–8 unique option IDs and one correct option that is included in the option list.

Use String Tables for production localization. Keep the correct-option field out of runtime UI graphs. The server validates the answer; the widget presents options and reads only the current prompt/options/state.

## Multiplayer checklist

- Host and client both see the same trial state, step index and revision.
- A client outside the configured interaction radius cannot submit an accepted option.
- Wrong options do not change Living Board objective progress.
- Correct options advance progress once.
- A replay with the old revision is rejected.
- Completing the trial does not complete the chapter until all chapter objectives are complete.
