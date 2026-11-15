# JUMANJI 2.111.1 — Stage 09: Emerald Jungle Chapter Flow & Adventure UI

## Goal

Connect the Living Board objective system and the Stage 08 trial actors into one chapter lifecycle. A server-side chapter director validates the chapter configuration, starts the board adventure, activates the placed trials and marks the chapter complete only after every objective is complete. A replicated chapter snapshot and a Blueprint-facing HUD base keep the local presentation synchronized.

This stage focuses on exploration, environmental puzzles, cooperation and readable progression. It adds no attack, weapon or damage mechanics.

## Runtime architecture

### AJumanjiAdventureChapterDirector

Place one director actor in the Emerald Jungle map. Configure:

- ChapterId and BoardEventId;
- localized ChapterTitle and ChapterDescription;
- ChapterObjectives, with one objective for each trial;
- TrialActors, referencing the placed trial actors in the level.

The director validates the configuration before changing runtime state:

- chapter and event IDs must be non-empty;
- title and description must be set;
- objective count must be non-zero and equal to the trial actor count;
- objective IDs must be unique and each objective must have a display name;
- required progress must be between 1 and 1000;
- every objective must have exactly one matching actor in the level;
- each actor must be in the same world, be dormant, have a valid step configuration and have a step count equal to the objective's required progress.

StartChapter is server-authoritative. It calls StartBoardAdventure, publishes the active chapter snapshot, and activates each prevalidated trial. The director subscribes to objective changes. When every objective is complete, it calls CompleteBoardAdventure and changes the chapter phase to Completed.

The director does not accept progress from UI. Trial actors continue to validate the submitted option on the server, and only accepted trial steps advance the objective snapshot.

### AJumanjiGameState chapter snapshot

FJumanjiAdventureChapterState is replicated as one snapshot containing:

- ChapterId and BoardEventId;
- localized DisplayName and Description;
- Inactive, Active or Completed phase;
- monotonically increasing Revision.

BeginAdventureChapter and CompleteAdventureChapter require server authority. The completion method accepts only the currently active matching chapter. OnAdventureChapterChanged notifies HUD listeners on the server and after replication on clients.

The existing objective array remains the source of truth for per-trial progress. Chapter state does not duplicate the objective data.

### UJumanjiAdventureHUDWidget

This abstract UUserWidget base subscribes to:

- AJumanjiGameState::OnAdventureChapterChanged;
- AJumanjiGameState::OnAdventureObjectivesChanged.

It exposes Blueprint-pure getters for the chapter snapshot, objective list, completed/total counts and progress by ObjectiveId. It invokes OnAdventurePresentationChanged when the data changes. Implement that event in WBP_AdventureHUD to refresh the chapter banner, objective rows, progress bars and completion state.

AJumanjiPlayerController creates the configured AdventureHUDWidgetClass for the local controller at BeginPlay. Assign a Widget Blueprint derived from UJumanjiAdventureHUDWidget in the PlayerController Blueprint defaults.

## Emerald Jungle — Nine Seals configuration

Create one placed trial actor for each row. The Root trial is supplied as the native AJumanjiRootRuneTrial; create Blueprint children of AJumanjiTrialActor for the remaining eight and author their step definitions.

| Great Spirit | ObjectiveId | Suggested display name | Required progress |
|---|---|---|---:|
| I — Root | TRIAL_ROOT_RUNES | Próba Korzeni | 3 |
| II — Tide | TRIAL_TIDE_CHANNELS | Próba Przypływu | Match the Tide actor's step count |
| III — Stone | TRIAL_STONE_BALANCE | Próba Kamienia | Match the Stone actor's step count |
| IV — Storm | TRIAL_STORM_WINDS | Próba Burzy | Match the Storm actor's step count |
| V — Ember | TRIAL_EMBER_SEQUENCE | Próba Żaru | Match the Ember actor's step count |
| VI — Dawn | TRIAL_DAWN_MIRRORS | Próba Świtu | Match the Dawn actor's step count |
| VII — Shadow | TRIAL_SHADOW_PATTERN | Próba Cienia | Match the Shadow actor's step count |
| VIII — Life | TRIAL_LIFE_GARDEN | Próba Życia | Match the Life actor's step count |
| IX — Frost | TRIAL_FROST_GATES | Próba Mrozu | Match the Frost actor's step count |

The remaining trial step counts are authoring decisions. Do not enter a placeholder count: RequiredProgress must exactly equal the configured Steps.Num() of its corresponding actor.

## Unreal Editor setup

1. Compile the C++ project in Unreal Engine 5.8.
2. Create the nine trial actors as described in Stage 08. For the first integration test, use the Root Rune actor only and a matching one-objective chapter; add the other eight after their Blueprint steps are configured.
3. Create BP_AdventureChapterDirector_EmeraldJungle as a child of AJumanjiAdventureChapterDirector.
4. Set ChapterTitle to **Szmaragdowa Dżungla — Dziewięć Pieczęci** and enter a short player-facing chapter description.
5. Place the director and trial actors in the same level.
6. Select the placed director instance. Add the chapter objectives and set each ObjectiveId, DisplayName, Description, optional RequiredElement and RequiredProgress.
7. In the TrialActors array, reference each placed trial actor exactly once. Every actor's ObjectiveId must match one objective.
8. Create WBP_AdventureHUD as a child of UJumanjiAdventureHUDWidget. Add a chapter title, description, completed/total counter, objective list and progress bars.
9. In BP_JumanjiPlayerController, set AdventureHUDWidgetClass to WBP_AdventureHUD.
10. From the server-side level start flow or an authoritative Blueprint event, call StartChapter on the placed director.
11. For each objective row, use the widget getters to display current and required progress. Refresh the layout from OnAdventurePresentationChanged.
12. Show a chapter-complete panel only when the replicated chapter phase is Completed.

For an incremental test, start with only the Root Rune trial and a one-objective configuration. The chapter director requires the objective and trial arrays to match; it does not silently invent actors for missing trials.

## Multiplayer and authority rules

- Only the server calls StartChapter.
- The server validates every director reference and objective/trial pairing before starting.
- The UI reads replicated chapter and objective snapshots; it never writes progress.
- Trial submissions still pass through the owning PlayerController and server-side range/revision/answer checks from Stage 08.
- The director ends only its configured BoardEventId and only after all objectives are complete.
- A queued client RPC is not proof of an accepted answer; listen to replicated state.
- The HUD unsubscribes from GameState delegates when it is destructed.

## Acceptance checklist

- [ ] Project compiles in the target Unreal Engine 5.8 installation.
- [ ] The director rejects an empty chapter ID, event ID, title or description.
- [ ] The director rejects duplicate objective IDs and mismatched objective/trial counts.
- [ ] The director rejects missing, duplicate, foreign-world, already-active or invalid trial actors.
- [ ] The director rejects objective progress that differs from its trial's step count.
- [ ] A server can start a valid one-trial chapter.
- [ ] Starting the chapter activates only the configured trial actors.
- [ ] A client cannot start the chapter.
- [ ] Chapter state and revision replicate to a second client.
- [ ] HUD values refresh after objective and chapter snapshot replication.
- [ ] Completing one trial updates its objective but does not complete the chapter while other objectives remain.
- [ ] Completing the final objective ends the matching board event and marks the chapter Completed.
- [ ] A second client sees the same chapter phase and objective progress.
- [ ] The HUD unsubscribes cleanly when removed.

## Not included

- Actual Widget Blueprint .uasset files, map, 3D rooms, materials, meshes, sound or Niagara effects.
- The eight remaining authored Great Spirit trial Blueprint assets.
- Persistent quest journal, SaveGame storage, backend profile service, inventory rewards or account unlock persistence.
- Dedicated-server deployment, automated multiplayer test harness or device profiling.
- Packaged Android APK/AAB.
- Unreal Engine compilation or playtest results from this repository operation.

## Next stage

Stage 10 — Great Spirit Trial Content Pack: finish the eight remaining puzzle definitions, add chapter art/audio hooks, and author the nine trial Blueprint assets for the Emerald Jungle.
