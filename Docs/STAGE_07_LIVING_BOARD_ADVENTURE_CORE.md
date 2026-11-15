# JUMANJI 2.111.1 — Stage 07: Living Board Adventure Core

## Goal

Turn the existing Living Board event flag into a server-authoritative, replicated adventure loop. An adventure starts with a validated objective list, puzzle actors report progress only after a server-side solution check, and the event can finish only after every objective is complete.

This stage is focused on exploration, elemental puzzles, cooperation and environmental discovery. It does not add attack, weapon or damage mechanics.

## Runtime architecture

### AJumanjiGameState

The existing replicated GameState now owns one replicated `FJumanjiAdventureObjectiveSet`:

- an array of objective snapshots;
- the current objective revision.

Keeping the array and revision in one replicated USTRUCT means a client receives a consistent snapshot for the corresponding revision. `OnAdventureObjectivesChanged` notifies Blueprint/UI listeners after the server changes the state or a client receives the replicated snapshot.

Each `FJumanjiAdventureObjectiveState` contains:

- stable `ObjectiveId`;
- localized `DisplayName` and `Description`;
- optional `RequiredElement` Gameplay Tag for presentation and trial setup;
- `RequiredProgress`;
- server-authoritative `CurrentProgress`;
- `bCompleted`.

### UJumanjiBoardWorldSubsystem

New server-authoritative entry points:

- `StartBoardAdventure(EventId, Objectives)` — starts the existing board event and initializes a validated objective snapshot.
- `AdvanceBoardAdventureObjective(ObjectiveId, ProgressAmount)` — advances one incomplete objective on the server.
- `CompleteBoardAdventure(EventId)` — ends the matching board event only when all objectives are complete.

The original `StartBoardEvent` and `EndBoardEvent` functions remain available for simpler board events.

## Validation and authority

- Calls that mutate the adventure state require server authority.
- Event IDs and objective IDs must be non-empty.
- An adventure must contain between 1 and 32 objectives.
- Objective IDs must be unique within the adventure.
- Display names must be non-empty.
- Required progress must be between 1 and 1000.
- Incoming progress must be between 1 and 1000.
- Progress is clamped to the objective requirement; completed objectives cannot be advanced again.
- The adventure cannot complete while any objective remains incomplete.
- The event's result snapshot remains available in GameState after completion, allowing a result screen to read it.

The objective core deliberately does not decide whether a puzzle answer is correct. A trial actor or server-side puzzle controller must validate its own rules first, then call `AdvanceBoardAdventureObjective`. Never treat a client-only widget result as proof that a puzzle was solved.

## First adventure: Emerald Jungle — The Nine Seals

The first chapter can be configured with nine objectives, one themed around each Great Spirit:

| Great Spirit | Objective ID | Trial concept |
|---|---|---|
| I — Root | `TRIAL_ROOT_RUNES` | Restore a sequence of living runes in the correct order |
| II — Tide | `TRIAL_TIDE_CHANNELS` | Route flowing water through a set of channels |
| III — Stone | `TRIAL_STONE_BALANCE` | Balance a group of ancient pressure platforms |
| IV — Storm | `TRIAL_STORM_WINDS` | Align wind vanes with environmental cues |
| V — Ember | `TRIAL_EMBER_SEQUENCE` | Repeat a sequence of glowing symbols |
| VI — Dawn | `TRIAL_DAWN_MIRRORS` | Redirect beams of light toward a seal |
| VII — Shadow | `TRIAL_SHADOW_PATTERN` | Reconstruct a pattern revealed by changing silhouettes |
| VIII — Life | `TRIAL_LIFE_GARDEN` | Restore the correct order of dormant garden nodes |
| IX — Frost | `TRIAL_FROST_GATES` | Synchronize gates using timing and visual clues |

These are design examples, not prebuilt Blueprint assets. The values for `RequiredProgress` depend on how many validated puzzle steps a designer chooses for each trial.

## Blueprint integration contract

Create trial actors under `Content/Adventure/Trials`. Each actor should:

1. expose a clear interaction prompt and readable puzzle state;
2. validate the submitted solution on the server;
3. reject stale, duplicate or out-of-order interactions;
4. call `AdvanceBoardAdventureObjective` only after a successful validation;
5. react to `AJumanjiGameState::OnAdventureObjectivesChanged` for replicated UI feedback.

The guardian in the first chapter is an environmental sentinel: it can patrol, observe, react to completed seals, present clues and change its visual state. It is not required to attack the player. Guardian encounter phases should be driven by objective completion, not health thresholds.

## Example C++ setup

```cpp
TArray<FJumanjiAdventureObjectiveState> Objectives;

FJumanjiAdventureObjectiveState RootTrial;
RootTrial.ObjectiveId = TEXT("TRIAL_ROOT_RUNES");
RootTrial.DisplayName = FText::FromString(TEXT("Próba Korzeni"));
RootTrial.Description = FText::FromString(TEXT("Ułóż trzy runy w prawidłowej kolejności."));
RootTrial.RequiredElement = FGameplayTag::RequestGameplayTag(TEXT("Element.Root"));
RootTrial.RequiredProgress = 3;
Objectives.Add(RootTrial);

// Add the remaining validated trial definitions before starting the chapter.

if (UJumanjiBoardWorldSubsystem* Board = GetWorld()->GetSubsystem<UJumanjiBoardWorldSubsystem>())
{
    Board->StartBoardAdventure(TEXT("EMERALD_JUNGLE_NINE_SEALS"), Objectives);
}
```

After a server-side trial actor validates one rune:

```cpp
Board->AdvanceBoardAdventureObjective(TEXT("TRIAL_ROOT_RUNES"), 1);
```

The chapter can finish only after all configured objectives reach their required progress:

```cpp
Board->CompleteBoardAdventure(TEXT("EMERALD_JUNGLE_NINE_SEALS"));
```

These examples must run in authoritative server logic. A future client interaction layer should forward a request to server-owned game logic, where range, ownership, puzzle state and replay protection are verified.

## Not included

- Finished trial Actor Blueprints or UMG puzzle widgets.
- Nine 3D puzzle rooms, environment meshes, materials, animations, Niagara or audio.
- A persistent quest journal, save-game storage or backend profile persistence.
- Reward inventory, unlock service or anti-replay tokens.
- Full replication, latency and multi-client playtest results.
- A packaged APK/AAB.

## Acceptance checklist

- [ ] Project compiles in the target Unreal Engine 5.8 installation.
- [ ] A server can start a board adventure with 1–32 valid objectives.
- [ ] Empty IDs, duplicate IDs, empty display names and invalid progress limits are rejected.
- [ ] A client cannot initialize or advance authoritative objective state.
- [ ] Objective snapshots replicate to connected clients.
- [ ] Every accepted progress update increments the objective-set revision.
- [ ] Progress never exceeds the required amount.
- [ ] Completed objectives reject additional progress.
- [ ] Completion is rejected while at least one objective is incomplete.
- [ ] Completion ends only the matching active board event.
- [ ] Objective snapshots remain readable after event completion.
- [ ] A two-client test confirms consistent objective state and UI updates.

## Next stage

Stage 08 — Adventure Interaction & Puzzle Actors: server-validated world interactions, reusable trial actor base, puzzle state machines and the first Emerald Jungle playable trial.
