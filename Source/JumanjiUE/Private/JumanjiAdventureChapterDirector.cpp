#include "JumanjiAdventureChapterDirector.h"
#include "JumanjiTrialActor.h"

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

    Super::EndPlay(EndPlayReason);
}


TArray<FName> AJumanjiAdventureChapterDirector::GetConfiguredObjectiveIds() const
{
    TArray<FName> Result;
    Result.Reserve(ChapterObjectives.Num());
    for (const FJumanjiAdventureObjectiveState& Objective : ChapterObjectives)
    {
        if (!Objective.ObjectiveId.IsNone())
        {
            Result.AddUnique(Objective.ObjectiveId);
        }
    }
    return Result;
}

TArray<FName> AJumanjiAdventureChapterDirector::GetConfiguredTrialIds() const
{
    TArray<FName> Result;
    Result.Reserve(TrialActors.Num());
    for (const AJumanjiTrialActor* Trial : TrialActors)
    {
        if (IsValid(Trial) && !Trial->GetCheckpointTrialId().IsNone())
        {
            Result.Add(Trial->GetCheckpointTrialId());
        }
    }
    return Result;
}


TArray<FName> AJumanjiAdventureChapterDirector::GetCompletedTrialIds() const
{
    TArray<FName> Result;
    Result.Reserve(TrialActors.Num());
    for (const AJumanjiTrialActor* Trial : TrialActors)
    {
        if (IsValid(Trial)
            && Trial->GetTrialState() == EJumanjiTrialState::Completed
            && !Trial->GetCheckpointTrialId().IsNone())
        {
            Result.Add(Trial->GetCheckpointTrialId());
        }
    }
    return Result;
}

TArray<FName> AJumanjiAdventureChapterDirector::GetCompletedTrialObjectiveIds() const
{
    TArray<FName> Result;
    Result.Reserve(TrialActors.Num());
    for (const AJumanjiTrialActor* Trial : TrialActors)
    {
        if (IsValid(Trial)
            && Trial->GetTrialState() == EJumanjiTrialState::Completed
            && !Trial->GetObjectiveId().IsNone())
        {
            Result.AddUnique(Trial->GetObjectiveId());
        }
    }
    return Result;
}

bool AJumanjiAdventureChapterDirector::AreCheckpointTrialIdsConsistent(
    const TArray<FName>& CompletedTrialIds,
    const TArray<FName>& CompletedObjectiveIds) const
{
    TSet<FName> CompletedTrialSet;
    TSet<FName> CompletedObjectiveSet;
    for (const FName Id : CompletedTrialIds)
    {
        if (Id.IsNone() || CompletedTrialSet.Contains(Id))
        {
            return false;
        }
        CompletedTrialSet.Add(Id);
    }
    for (const FName Id : CompletedObjectiveIds)
    {
        if (Id.IsNone() || CompletedObjectiveSet.Contains(Id))
        {
            return false;
        }
        CompletedObjectiveSet.Add(Id);
    }

    TSet<FName> ExpectedTrialIds;
    TSet<FName> ExpectedObjectiveIds;
    for (const AJumanjiTrialActor* Trial : TrialActors)
    {
        if (!IsValid(Trial)
            || Trial->GetCheckpointTrialId().IsNone()
            || Trial->GetObjectiveId().IsNone()
            || ExpectedTrialIds.Contains(Trial->GetCheckpointTrialId())
            || ExpectedObjectiveIds.Contains(Trial->GetObjectiveId()))
        {
            return false;
        }

        ExpectedTrialIds.Add(Trial->GetCheckpointTrialId());
        ExpectedObjectiveIds.Add(Trial->GetObjectiveId());

        const bool bTrialCompleted = CompletedTrialSet.Contains(Trial->GetCheckpointTrialId());
        const bool bObjectiveCompleted = CompletedObjectiveSet.Contains(Trial->GetObjectiveId());
        if (bTrialCompleted != bObjectiveCompleted)
        {
            return false;
        }
    }

    // Completed IDs are a subset of chapter content; partial TrialBoundary
    // checkpoints must remain valid while preserving one-to-one ID mapping.
    for (const FName CompletedTrialId : CompletedTrialSet)
    {
        if (!ExpectedTrialIds.Contains(CompletedTrialId))
        {
            return false;
        }
    }
    for (const FName CompletedObjectiveId : CompletedObjectiveSet)
    {
        if (!ExpectedObjectiveIds.Contains(CompletedObjectiveId))
        {
            return false;
        }
    }

    return CompletedTrialSet.Num() == CompletedObjectiveSet.Num();
}


bool AJumanjiAdventureChapterDirector::BuildCheckpointRestoreObjectives(
    const TArray<FName>& CompletedTrialIds,
    const TArray<FName>& CompletedObjectiveIds,
    TArray<FJumanjiAdventureObjectiveState>& OutObjectives,
    bool& bOutChapterComplete,
    FText& OutChapterTitle,
    FText& OutChapterDescription) const
{
    OutObjectives.Reset();
    bOutChapterComplete = false;
    OutChapterTitle = FText::GetEmpty();
    OutChapterDescription = FText::GetEmpty();

    if (ChapterId.IsNone()
        || BoardEventId.IsNone()
        || ChapterTitle.IsEmpty()
        || ChapterDescription.IsEmpty()
        || ChapterObjectives.IsEmpty()
        || ChapterObjectives.Num() > 32
        || ChapterObjectives.Num() != TrialActors.Num()
        || CompletedTrialIds.Num() != CompletedObjectiveIds.Num()
        || !AreCheckpointTrialIdsConsistent(CompletedTrialIds, CompletedObjectiveIds))
    {
        return false;
    }

    TSet<FName> ConfiguredObjectiveIds;
    for (const FJumanjiAdventureObjectiveState& Configured : ChapterObjectives)
    {
        if (Configured.ObjectiveId.IsNone()
            || Configured.DisplayName.IsEmpty()
            || Configured.RequiredProgress < 1
            || Configured.RequiredProgress > 1000
            || ConfiguredObjectiveIds.Contains(Configured.ObjectiveId))
        {
            return false;
        }
        ConfiguredObjectiveIds.Add(Configured.ObjectiveId);
    }

    TSet<FName> TrialObjectiveIds;
    for (const AJumanjiTrialActor* Trial : TrialActors)
    {
        if (!IsValid(Trial)
            || Trial->GetWorld() != GetWorld()
            || Trial->GetObjectiveId().IsNone()
            || TrialObjectiveIds.Contains(Trial->GetObjectiveId()))
        {
            return false;
        }

        const FJumanjiAdventureObjectiveState* MatchingObjective =
            ChapterObjectives.FindByPredicate(
                [Trial](const FJumanjiAdventureObjectiveState& Entry)
                {
                    return Entry.ObjectiveId == Trial->GetObjectiveId();
                });
        if (!MatchingObjective
            || MatchingObjective->RequiredProgress != Trial->GetStepCount())
        {
            return false;
        }
        TrialObjectiveIds.Add(Trial->GetObjectiveId());
    }

    if (ConfiguredObjectiveIds.Num() != TrialObjectiveIds.Num())
    {
        return false;
    }
    for (const FName ObjectiveId : ConfiguredObjectiveIds)
    {
        if (!TrialObjectiveIds.Contains(ObjectiveId))
        {
            return false;
        }
    }

    TSet<FName> CompletedObjectives;
    for (const FName ObjectiveId : CompletedObjectiveIds)
    {
        if (ObjectiveId.IsNone() || CompletedObjectives.Contains(ObjectiveId))
        {
            return false;
        }
        CompletedObjectives.Add(ObjectiveId);
    }

    OutObjectives.Reserve(ChapterObjectives.Num());
    for (const FJumanjiAdventureObjectiveState& Configured : ChapterObjectives)
    {
        if (Configured.ObjectiveId.IsNone()
            || Configured.RequiredProgress < 1
            || Configured.RequiredProgress > 1000)
        {
            OutObjectives.Reset();
            return false;
        }

        FJumanjiAdventureObjectiveState Restored = Configured;
        const bool bCompleted = CompletedObjectives.Contains(Configured.ObjectiveId);
        Restored.CurrentProgress = bCompleted ? Restored.RequiredProgress : 0;
        Restored.bCompleted = bCompleted;
        OutObjectives.Add(MoveTemp(Restored));
    }

    bOutChapterComplete = CompletedObjectives.Num() == ChapterObjectives.Num();
    OutChapterTitle = ChapterTitle;
    OutChapterDescription = ChapterDescription;
    return OutObjectives.Num() == ChapterObjectives.Num();
}

bool AJumanjiAdventureChapterDirector::CanApplyCheckpointTrialStates(
    const TArray<FName>& CompletedTrialIds,
    const TArray<FName>& CompletedObjectiveIds) const
{
    TArray<FJumanjiAdventureObjectiveState> IgnoredObjectives;
    bool bIgnoredComplete = false;
    FText IgnoredTitle;
    FText IgnoredDescription;
    if (!HasAuthority()
        || !BuildCheckpointRestoreObjectives(
            CompletedTrialIds,
            CompletedObjectiveIds,
            IgnoredObjectives,
            bIgnoredComplete,
            IgnoredTitle,
            IgnoredDescription))
    {
        return false;
    }

    for (const AJumanjiTrialActor* Trial : TrialActors)
    {
        if (!IsValid(Trial)
            || Trial->GetWorld() != GetWorld()
            || !Trial->CanApplyCheckpointRestoreState())
        {
            return false;
        }
    }
    return true;
}

bool AJumanjiAdventureChapterDirector::ApplyCheckpointTrialStates(
    const TArray<FName>& CompletedTrialIds,
    const TArray<FName>& CompletedObjectiveIds)
{
    if (!CanApplyCheckpointTrialStates(CompletedTrialIds, CompletedObjectiveIds))
    {
        return false;
    }

    TSet<FName> CompletedTrials;
    for (const FName TrialId : CompletedTrialIds)
    {
        CompletedTrials.Add(TrialId);
    }

    for (AJumanjiTrialActor* Trial : TrialActors)
    {
        if (!IsValid(Trial)
            || !Trial->ApplyCheckpointRestoreState(
                CompletedTrials.Contains(Trial->GetCheckpointTrialId())))
        {
            return false;
        }
    }
    return true;
}

void AJumanjiAdventureChapterDirector::PublishCheckpointTrialStates()
{
    if (!HasAuthority())
    {
        return;
    }

    for (AJumanjiTrialActor* Trial : TrialActors)
    {
        if (IsValid(Trial))
        {
            Trial->PublishCheckpointRestoreState();
        }
    }
}

bool AJumanjiAdventureChapterDirector::BindForCheckpointRestoredActiveChapter()
{
    if (!HasAuthority())
    {
        return false;
    }

    UWorld* World = GetWorld();
    AJumanjiGameState* BoardGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    if (!BoardGameState
        || BoardGameState->GetAdventureChapterState().ChapterId != ChapterId
        || BoardGameState->GetAdventureChapterState().BoardEventId != BoardEventId
        || BoardGameState->GetAdventureChapterState().Phase != EJumanjiAdventureChapterPhase::Active
        || !BoardGameState->IsBoardEventActive()
        || BoardGameState->GetActiveBoardEventId() != BoardEventId)
    {
        return false;
    }

    BoardGameState->OnAdventureObjectivesChanged.AddUniqueDynamic(
        this,
        &AJumanjiAdventureChapterDirector::HandleAdventureObjectivesChanged);
    bListeningForObjectiveChanges = true;
    return true;
}

bool AJumanjiAdventureChapterDirector::HasNoTrialsInProgress() const
{
    if (TrialActors.IsEmpty())
    {
        return false;
    }

    for (const AJumanjiTrialActor* Trial : TrialActors)
    {
        if (!IsValid(Trial) || Trial->GetTrialState() == EJumanjiTrialState::InProgress)
        {
            return false;
        }
    }
    return true;
}
