# Content/Adventure — asset authoring guide

## Folder layout

Create the following folders in Unreal Editor:

- /Game/Adventure/Trials — puzzle and trial Actor Blueprints.
- /Game/Adventure/Guardian — non-hostile guardian presentation and behavior.
- /Game/Adventure/UI — objective tracker and puzzle widgets.
- /Game/Adventure/Maps — adventure maps and test rooms.
- /Game/Adventure/Data — objective and chapter Data Assets/Data Tables.

## Naming

- BP_Trial_RootRunes
- BP_Trial_TideChannels
- BP_Trial_StoneBalance
- BP_Trial_StormWinds
- BP_Trial_EmberSequence
- BP_Trial_DawnMirrors
- BP_Trial_ShadowPattern
- BP_Trial_LifeGarden
- BP_Trial_FrostGates
- BP_Guardian_EmeraldSentinel
- WBP_AdventureObjectiveTracker
- WBP_TrialInteraction

## Runtime contract

The native objective state is stored by AJumanjiGameState and replicated to clients. Use UJumanjiBoardWorldSubsystem for the adventure lifecycle:

1. Server creates the objective array and calls StartBoardAdventure.
2. A trial actor owns its puzzle solution rules and validates them on the server.
3. After a correct solution, server calls AdvanceBoardAdventureObjective.
4. UI listens to AJumanjiGameState::OnAdventureObjectivesChanged and reads GetAdventureObjectives().
5. Server calls CompleteBoardAdventure after all objectives are complete.

The subsystem rejects non-authoritative mutations. Do not increment progress directly from a client widget. Do not assume that a required-element tag automatically validates the selected Great Spirit; the trial actor must perform that check if a puzzle requires a particular spirit.

## Stage 08 native trial runtime

- AJumanjiTrialActor is the abstract replicated base for environmental puzzles.
- AJumanjiRootRuneTrial is the first concrete trial, with a three-step Seed → Sprout → Tree sequence.
- AJumanjiPlayerController::RequestTrialInteraction sends a client request through the owning PlayerController.
- The server verifies the controlled pawn, interaction distance, trial revision and submitted option before advancing Living Board progress.
- The trial replicates state, current step index and revision. It does not replicate its correct-option field as runtime state.

See Docs/STAGE_08_EMERALD_JUNGLE_TRIAL_ACTORS.md and Content/Adventure/Trials/README.md for the full C++/Blueprint integration contract.

## First playtest room

Build a small Emerald Jungle room with:

- Player Start and a clear route through the area;
- BP_Trial_RootRunes with three ordered rune steps;
- a non-hostile guardian that observes the room and responds to puzzle completion;
- objective tracker with current/required progress;
- a completion marker activated after the board event ends.

Keep the first room small and readable. Optimize the interaction loop before creating all nine rooms.

## Asset status

The repository includes the native C++ objective system and trial interaction runtime, plus the AJumanjiRootRuneTrial sequence defaults. It does not contain the actual Blueprint .uasset files, 3D art, sound, map or packaged game. Those assets must be authored in Unreal Editor and committed through Git LFS where appropriate.
