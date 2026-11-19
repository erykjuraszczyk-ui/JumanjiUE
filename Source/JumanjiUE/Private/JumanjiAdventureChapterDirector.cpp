#include "JumanjiAdventureChapterDirector.h"

#include "JumanjiBoardWorldSubsystem.h"
#include "JumanjiGameState.h"
#include "JumanjiTrialActor.h"
#include "Engine/World.h"

AJumanjiAdventureChapterDirector::AJumanjiAdventureChapterDirector()
{
    PrimaryActorTick.bCanEverTick = false;
}

bool AJumanjiAdventureChapterDirector::ValidateChapterConfiguration() const
{
    if (ChapterId.IsNone()
        || BoardEventId.IsNone()
        || ChapterTitle.IsEmpty()
        || ChapterDescription.IsEmpty()
        || ChapterObjectives.IsEmpty()
        || ChapterObjectives.Num() != TrialActors.Num())
    {
        return false;
    }

    TSet<FName> SeenObjectiveIds;
    TSet<const AJumanjiTrialActor*> SeenTrialActors;

    for (const FJumanjiAdventureObjectiveState& Objective : ChapterObjectives)
    {
        if (Objective.ObjectiveId.IsNone()
            || Objective.DisplayName.IsEmpty()
            || Objective.RequiredProgress < 1
            || Objective.RequiredProgress > 1000
            || SeenObjectiveIds.Contains(Objective.ObjectiveId))
        {
            return false;
        }

        SeenObjectiveIds.Add(Objective.ObjectiveId);

        const TObjectPtr<AJumanjiTrialActor>* MatchingActor = TrialActors.FindByPredicate(
            [&Objective](const TObjectPtr<AJumanjiTrialActor>& Candidate)
            {
                return IsValid(Candidate) && Candidate->GetObjectiveId() == Objective.ObjectiveId;
            });

        if (!MatchingActor
            || !IsValid(MatchingActor->Get())
            || MatchingActor->Get()->GetWorld() != GetWorld()
            || SeenTrialActors.Contains(MatchingActor->Get())
            || !MatchingActor->Get()->IsReadyToActivateForObjective(
                Objective.ObjectiveId,
                Objective.RequiredProgress))
        {
            return false;
        }

        SeenTrialActors.Add(MatchingActor->Get());
    }

    return SeenTrialActors.Num() == ChapterObjectives.Num();
}

bool AJumanjiAdventureChapterDirector::StartChapter()
{
    if (!HasAuthority() || !ValidateChapterConfiguration())
    {
        return false;
    }

    UWorld* World = GetWorld();
    AJumanjiGameState* BoardGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    UJumanjiBoardWorldSubsystem* BoardSubsystem = World
        ? World->GetSubsystem<UJumanjiBoardWorldSubsystem>()
        : nullptr;

    if (!BoardGameState
        || !BoardSubsystem
        || BoardGameState->GetAdventureChapterState().Phase == EJumanjiAdventureChapterPhase::Active)
    {
        return false;
    }

    if (!BoardSubsystem->StartBoardAdventure(BoardEventId, ChapterObjectives))
    {
        return false;
    }

    if (!BoardGameState->BeginAdventureChapter(
        ChapterId,
        BoardEventId,
        ChapterTitle,
        ChapterDescription))
    {
        BoardSubsystem->EndBoardEvent(BoardEventId);
        return false;
    }

    for (AJumanjiTrialActor* TrialActor : TrialActors)
    {
        if (!IsValid(TrialActor) || !TrialActor->ActivateTrial())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("JUMANJI: Chapter %s could not activate a validated trial actor."),
                *ChapterId.ToString()
            );
            return false;
        }

        TrialActor->OnTrialStateChanged.AddUniqueDynamic(
            this,
            &AJumanjiAdventureChapterDirector::HandleTrialStateChanged);
    }

    BoardGameState->OnAdventureObjectivesChanged.AddUniqueDynamic(
        this,
        &AJumanjiAdventureChapterDirector::HandleAdventureObjectivesChanged
    );
    bListeningForObjectiveChanges = true;

    return true;
}

bool AJumanjiAdventureChapterDirector::ResetChapterForEncounterRetry(FName EncounterId)
{
    if (!HasAuthority()
        || EncounterId.IsNone()
        || ChapterId.IsNone()
        || BoardEventId.IsNone()
        || ChapterObjectives.IsEmpty()
        || ChapterObjectives.Num() != TrialActors.Num())
    {
        return false;
    }

    UWorld* World = GetWorld();
    AJumanjiGameState* BoardGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    UJumanjiBoardWorldSubsystem* BoardSubsystem = World
        ? World->GetSubsystem<UJumanjiBoardWorldSubsystem>()
        : nullptr;

    if (!BoardGameState || !BoardSubsystem)
    {
        return false;
    }

    const FJumanjiWorldEncounterState EncounterState =
        BoardGameState->GetWorldEncounterState();
    const FJumanjiAdventureChapterState ChapterState =
        BoardGameState->GetAdventureChapterState();

    if (EncounterState.EncounterId != EncounterId
        || EncounterState.Phase != EJumanjiWorldEncounterPhase::Failed
        || (!ChapterState.ChapterId.IsNone() && ChapterState.ChapterId != ChapterId)
        || (!ChapterState.BoardEventId.IsNone() && ChapterState.BoardEventId != BoardEventId))
    {
        return false;
    }

    if (BoardGameState->IsBoardEventActive()
        && BoardGameState->GetActiveBoardEventId() != BoardEventId)
    {
        return false;
    }

    TSet<FName> SeenObjectiveIds;
    TSet<const AJumanjiTrialActor*> SeenTrialActors;

    for (const FJumanjiAdventureObjectiveState& Objective : ChapterObjectives)
    {
        if (Objective.ObjectiveId.IsNone()
            || SeenObjectiveIds.Contains(Objective.ObjectiveId))
        {
            return false;
        }

        SeenObjectiveIds.Add(Objective.ObjectiveId);

        const TObjectPtr<AJumanjiTrialActor>* MatchingActor = TrialActors.FindByPredicate(
            [&Objective](const TObjectPtr<AJumanjiTrialActor>& Candidate)
            {
                return IsValid(Candidate) && Candidate->GetObjectiveId() == Objective.ObjectiveId;
            });

        if (!MatchingActor
            || !IsValid(MatchingActor->Get())
            || MatchingActor->Get()->GetWorld() != World
            || MatchingActor->Get()->GetStepCount() != Objective.RequiredProgress
            || SeenTrialActors.Contains(MatchingActor->Get())
            || !MatchingActor->Get()->CanResetForEncounterRetry())
        {
            return false;
        }

        SeenTrialActors.Add(MatchingActor->Get());
    }

    if (SeenTrialActors.Num() != ChapterObjectives.Num())
    {
        return false;
    }

    if (bListeningForObjectiveChanges)
    {
        BoardGameState->OnAdventureObjectivesChanged.RemoveDynamic(
            this,
            &AJumanjiAdventureChapterDirector::HandleAdventureObjectivesChanged
        );
        bListeningForObjectiveChanges = false;
    }

    if (BoardGameState->IsBoardEventActive()
        && !BoardSubsystem->EndBoardEvent(BoardEventId))
    {
        return false;
    }

    for (AJumanjiTrialActor* TrialActor : TrialActors)
    {
        if (!IsValid(TrialActor) || !TrialActor->ResetForEncounterRetry())
        {
            return false;
        }
    }

    return BoardGameState->ResetAdventureChapterForEncounterRetry(
        EncounterId,
        ChapterId,
        BoardEventId
    );
}

bool AJumanjiAdventureChapterDirector::BuildCheckpointProgress(
    TArray<FName>& OutCompletedObjectiveIds,
    TArray<FName>& OutCompletedTrialIds,
    int32& OutChapterRevision) const
{
    OutCompletedObjectiveIds.Reset();
    OutCompletedTrialIds.Reset();
    OutChapterRevision = 0;

    const UWorld* World = GetWorld();
    const AJumanjiGameState* BoardGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    if (!HasAuthority() || !BoardGameState
        || BoardGameState->GetAdventureChapterState().ChapterId != ChapterId
        || (BoardGameState->GetAdventureChapterState().Phase != EJumanjiAdventureChapterPhase::Active
            && BoardGameState->GetAdventureChapterState().Phase != EJumanjiAdventureChapterPhase::Completed)
        || ChapterObjectives.IsEmpty()
        || ChapterObjectives.Num() != TrialActors.Num())
    {
        return false;
    }

    const TArray<FJumanjiAdventureObjectiveState> Objectives =
        BoardGameState->GetAdventureObjectives();
    TMap<FName, const FJumanjiAdventureObjectiveState*> ObjectiveById;
    for (const FJumanjiAdventureObjectiveState& Objective : Objectives)
    {
        if (Objective.ObjectiveId.IsNone() || ObjectiveById.Contains(Objective.ObjectiveId))
        {
            return false;
        }
        ObjectiveById.Add(Objective.ObjectiveId, &Objective);
    }

    TSet<FName> CompletedTrialSet;
    TSet<FName> CompletedObjectiveSet;
    for (const AJumanjiTrialActor* TrialActor : TrialActors)
    {
        if (!IsValid(TrialActor) || TrialActor->GetWorld() != World
            || TrialActor->GetObjectiveId().IsNone()
            || CompletedTrialSet.Contains(TrialActor->GetObjectiveId()))
        {
            return false;
        }

        const FJumanjiAdventureObjectiveState* const* ObjectivePtr =
            ObjectiveById.Find(TrialActor->GetObjectiveId());
        if (!ObjectivePtr || !*ObjectivePtr
            || (*ObjectivePtr)->RequiredProgress != TrialActor->GetStepCount())
        {
            return false;
        }

        const FJumanjiAdventureObjectiveState& Objective = **ObjectivePtr;
        const EJumanjiTrialState TrialState = TrialActor->GetTrialState();

        // Mid-trial snapshots are deliberately forbidden. Available means
        // no option is currently being processed; InProgress means a sequence
        // has begun and must reach its next Completed boundary first.
        if (TrialState == EJumanjiTrialState::InProgress
            || TrialState == EJumanjiTrialState::Dormant)
        {
            return false;
        }

        if (TrialState == EJumanjiTrialState::Completed)
        {
            if (!Objective.bCompleted
                || Objective.CurrentProgress != Objective.RequiredProgress)
            {
                return false;
            }
            CompletedTrialSet.Add(TrialActor->GetObjectiveId());
            CompletedObjectiveSet.Add(TrialActor->GetObjectiveId());
        }
        else if (TrialState == EJumanjiTrialState::Available)
        {
            if (Objective.bCompleted
                || Objective.CurrentProgress >= Objective.RequiredProgress)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    if (CompletedTrialSet.IsEmpty())
    {
        return false;
    }

    for (const FJumanjiAdventureObjectiveState& RequiredObjective : ChapterObjectives)
    {
        if (CompletedObjectiveSet.Contains(RequiredObjective.ObjectiveId))
        {
            OutCompletedObjectiveIds.Add(RequiredObjective.ObjectiveId);
        }
    }

    for (const AJumanjiTrialActor* TrialActor : TrialActors)
    {
        if (TrialActor && CompletedTrialSet.Contains(TrialActor->GetObjectiveId()))
        {
            OutCompletedTrialIds.Add(TrialActor->GetObjectiveId());
        }
    }

    if (OutCompletedObjectiveIds.Num() != OutCompletedTrialIds.Num())
    {
        OutCompletedObjectiveIds.Reset();
        OutCompletedTrialIds.Reset();
        return false;
    }

    if (BoardGameState->GetAdventureChapterState().Phase == EJumanjiAdventureChapterPhase::Completed
        && OutCompletedTrialIds.Num() != TrialActors.Num())
    {
        OutCompletedObjectiveIds.Reset();
        OutCompletedTrialIds.Reset();
        return false;
    }

    OutChapterRevision = BoardGameState->GetAdventureChapterState().Revision;
    return OutChapterRevision > 0;
}

bool AJumanjiAdventureChapterDirector::IsChapterComplete() const
{
    const UWorld* World = GetWorld();
    const AJumanjiGameState* BoardGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;

    if (!BoardGameState)
    {
        return false;
    }

    const FJumanjiAdventureChapterState ChapterState =
        BoardGameState->GetAdventureChapterState();

    return ChapterState.ChapterId == ChapterId
        && ChapterState.Phase == EJumanjiAdventureChapterPhase::Completed;
}

void AJumanjiAdventureChapterDirector::HandleAdventureObjectivesChanged(int32 Revision)
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    AJumanjiGameState* BoardGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    UJumanjiBoardWorldSubsystem* BoardSubsystem = World
        ? World->GetSubsystem<UJumanjiBoardWorldSubsystem>()
        : nullptr;

    if (!BoardGameState
        || !BoardSubsystem
        || !BoardGameState->AreAdventureObjectivesComplete())
    {
        return;
    }

    const FJumanjiAdventureChapterState ChapterState =
        BoardGameState->GetAdventureChapterState();

    if (ChapterState.ChapterId != ChapterId
        || ChapterState.BoardEventId != BoardEventId
        || ChapterState.Phase != EJumanjiAdventureChapterPhase::Active)
    {
        return;
    }

    if (BoardSubsystem->CompleteBoardAdventure(BoardEventId)
        && BoardGameState->CompleteAdventureChapter(ChapterId))
    {
        BoardGameState->OnAdventureObjectivesChanged.RemoveDynamic(
            this,
            &AJumanjiAdventureChapterDirector::HandleAdventureObjectivesChanged
        );
        bListeningForObjectiveChanges = false;
    }
}

void AJumanjiAdventureChapterDirector::HandleTrialStateChanged(
    EJumanjiTrialState TrialState,
    int32 CurrentStepIndex,
    int32 Revision)
{
    if (!HasAuthority() || TrialState != EJumanjiTrialState::Completed)
    {
        return;
    }

    TArray<FName> CompletedObjectiveIds;
    TArray<FName> CompletedTrialIds;
    int32 ChapterRevision = 0;
    if (BuildCheckpointProgress(CompletedObjectiveIds, CompletedTrialIds, ChapterRevision))
    {
        OnCheckpointBoundaryReady.Broadcast();
    }
}

void AJumanjiAdventureChapterDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bListeningForObjectiveChanges)
    {
        if (UWorld* World = GetWorld())
        {
            if (AJumanjiGameState* BoardGameState = World->GetGameState<AJumanjiGameState>())
            {
                BoardGameState->OnAdventureObjectivesChanged.RemoveDynamic(
                    this,
                    &AJumanjiAdventureChapterDirector::HandleAdventureObjectivesChanged
                );
            }
        }

        bListeningForObjectiveChanges = false;
    }

    for (AJumanjiTrialActor* TrialActor : TrialActors)
    {
        if (IsValid(TrialActor))
        {
            TrialActor->OnTrialStateChanged.RemoveDynamic(
                this,
                &AJumanjiAdventureChapterDirector::HandleTrialStateChanged);
        }
    }

    Super::EndPlay(EndPlayReason);
}
