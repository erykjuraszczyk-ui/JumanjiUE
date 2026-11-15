# JUMANJI 2.111.1 — Stage 08: Emerald Jungle Trial Actors

## Goal

Connect the Stage 07 Living Board objective system to actual world interactions. This stage adds a reusable trial actor, a server-routed interaction request, replicated trial state and the first concrete Emerald Jungle puzzle: the Root Rune Sequence.

The gameplay loop is exploration and environmental problem-solving. No new attack, weapon or damage mechanics are introduced.

## Architecture

### AJumanjiTrialActor

Abstract native base for Blueprint-authored environmental trials.

- Holds an objective ID and a designer-authored list of puzzle steps.
- Each step defines a stable step ID, localized prompt, option IDs and a correct option ID.
- Validates trial configuration before activation: 1–12 steps, unique step IDs, 2–8 unique options per step and a correct option contained in the option list.
- Replicates only the runtime state, current step index and revision.
- Exposes the current prompt and option IDs to UI, but does not expose the correct option through a Blueprint-readable getter.
- Rejects submissions from non-authoritative contexts, completed or dormant trials, invalid options and stale revisions.
- Advances the matching Living Board objective only after a correct answer is validated on the server.

The server does not trust puzzle progress supplied by a widget. The client supplies an option ID and the revision it observed; the server resolves the answer against the trial's authored rules.

### Trial state machine

**Dormant → Available → InProgress → Completed**

- **Dormant:** trial is not accepting input.
- **Available:** objective and puzzle configuration were validated; first step is ready.
- **InProgress:** at least one correct step has been accepted.
- **Completed:** every configured step has been solved.

A wrong option leaves the step and revision unchanged. A successful option advances objective progress by exactly one and increments the trial revision. A request with an old revision is rejected, preventing a delayed duplicate request from advancing the next step.

### AJumanjiPlayerController

RequestTrialInteraction(TrialActor, OptionId, ExpectedRevision) routes input:

1. A locally controlled client invokes the PlayerController's Server RPC.
2. The server checks that the trial actor and controlled pawn are valid.
3. The server checks the pawn-to-trial distance against MaxTrialInteractionDistance (default 350 Unreal units).
4. The trial actor checks state, revision, available option and correct answer.
5. Only a validated answer calls AdvanceBoardAdventureObjective.

A true return on a remote client means that the request was queued; it is not confirmation that the answer was accepted. The client should observe OnTrialStateChanged and the replicated revision for the authoritative result.

The current interaction gate checks distance. A project-specific line-of-sight or interaction-channel test can be added later if the level design requires it.

## First playable puzzle: Root Rune Sequence

AJumanjiRootRuneTrial derives from AJumanjiTrialActor and configures three steps:

| Step | Prompt | Correct option |
|---|---|---|
| 1 | Wybierz runę początku życia. | RUNE_SEED |
| 2 | Wskaż symbol wzrostu. | RUNE_SPROUT |
| 3 | Zamknij sekwencję symbolem dojrzałego drzewa. | RUNE_TREE |

Each step presents three options. Incorrect choices do not reduce progress or apply a penalty. The next step is not available until the current option is accepted by the server.

The authored sequence is a prototype example. Replace its prompts with String Table entries before localization and adjust the visual runes to the final art direction.

## Integration with Stage 07

The adventure objective must match the trial actor:

- Objective ID: TRIAL_ROOT_RUNES
- Required progress: 3
- Initial current progress: 0

The server starts the adventure using StartBoardAdventure, then calls ActivateTrial on the placed Root Rune actor. The actor refuses activation if the matching objective is absent, already completed, has non-zero progress or has a required progress value different from the number of puzzle steps.

After all three steps are solved, the trial replicates Completed and the Living Board objective reaches 3/3. The chapter still uses Stage 07's CompleteBoardAdventure gate; the puzzle actor does not end the whole chapter itself.

## Blueprint and UMG setup

1. Compile the C++ project in Unreal Engine 5.8.
2. Create BP_Trial_RootRunes as a child of AJumanjiRootRuneTrial.
3. Assign the rune mesh, interaction marker, collision/visual components, materials and audio feedback.
4. Place the actor in the Emerald Jungle test map.
5. In the server-side adventure start flow, configure the TRIAL_ROOT_RUNES objective with required progress 3, start the adventure and call ActivateTrial on the actor.
6. Create WBP_TrialInteraction. Read GetCurrentStepPrompt, GetCurrentStepOptions, GetTrialRevision and GetTrialState.
7. For each option button, call the owning AJumanjiPlayerController::RequestTrialInteraction with the actor, selected option ID and latest observed revision.
8. Bind to OnTrialStateChanged to refresh the prompt and available options after the replicated server result.
9. Show a completion marker when the Root Rune actor reaches Completed; keep chapter completion controlled by the Living Board subsystem.

The eight additional Great Spirit trials can be authored as Blueprint children of AJumanjiTrialActor, with their objective IDs and step counts matching the Stage 07 adventure definition.

## Security and multiplayer rules

- Only the server activates a trial or accepts an answer.
- Do not call AdvanceBoardAdventureObjective directly from a client widget.
- The PlayerController is the RPC owner; do not put a client-to-server RPC on an unowned trial actor.
- Every request includes the trial revision observed by the client.
- The server checks the actor reference, pawn, interaction distance and puzzle solution.
- Correct answers are not part of the replicated trial runtime state.
- Do not treat a queued RPC or local button animation as a completed puzzle step.

## Acceptance checklist

- [ ] Project compiles in the target Unreal Engine 5.8 installation.
- [ ] AJumanjiTrialActor is available as an abstract Blueprint parent.
- [ ] Invalid trial configurations are rejected.
- [ ] AJumanjiRootRuneTrial starts only when TRIAL_ROOT_RUNES exists with required progress 3 and current progress 0.
- [ ] A client can submit an option through its owning PlayerController.
- [ ] The server rejects requests from outside the configured interaction distance.
- [ ] The server rejects a stale trial revision.
- [ ] An incorrect option leaves the objective progress unchanged.
- [ ] A correct option advances objective progress exactly once.
- [ ] Replaying the same request with the old revision does not advance progress again.
- [ ] Trial state, step index and revision replicate to a second client.
- [ ] Completing all three rune steps sets the trial to Completed and the objective to 3/3.
- [ ] The chapter cannot complete until every configured Stage 07 objective is complete.
- [ ] Two-client test covers correct, incorrect, delayed and out-of-range interactions.

## Not included

- Finished .uasset Blueprint actors, UMG widgets, maps, meshes, materials, sounds or VFX.
- The eight remaining Great Spirit trial implementations.
- Persistent quest journal, save-game storage, inventory rewards or account backend.
- Packaged Android APK/AAB or verified mobile performance.
- Unreal Engine compilation or multiplayer playtest results from this repository operation.

## Next stage

Stage 09 — Emerald Jungle Chapter Flow & Adventure UI: chapter start orchestration, objective tracker, interaction prompts, puzzle feedback, completion sequence and the first end-to-end playable room.
