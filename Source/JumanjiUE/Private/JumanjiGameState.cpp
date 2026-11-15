#include "JumanjiGameState.h"

#include "Net/UnrealNetwork.h"

namespace
{
    constexpr int32 MaxAdventureObjectives = 32;
    constexpr int32 MaxObjectiveProgress = 1000;
}

void AJumanjiGameState::SetBoardEventState(FName EventId, bool bIsActive)
{
    if (!HasAuthority())
    {
        return;
    }

    if (bIsActive && EventId.IsNone())
    {
        return;
    }

    ActiveBoardEventId = bIsActive ? EventId : NAME_None;
    bBoardEventActive = bIsActive;
    ++BoardEventRevision;

    OnBoardEventStateChanged.Broadcast(
        ActiveBoardEventId,
        bBoardEventActive,
        BoardEventRevision
    );

    ForceNetUpdate();
}

bool AJumanjiGameState::BeginAdventureChapter(
    FName ChapterId,
    FName BoardEventId,
    FText DisplayName,
    FText Description)
{
    if (!HasAuthority()
        || ChapterId.IsNone()
        || BoardEventId.IsNone()
        || DisplayName.IsEmpty()
        || Description.IsEmpty()
        || AdventureChapterState.Phase == EJumanjiAdventureChapterPhase::Active)
    {
        return false;
    }

    FJumanjiAdventureChapterState NewChapterState;
    NewChapterState.ChapterId = ChapterId;
    NewChapterState.BoardEventId = BoardEventId;
    NewChapterState.DisplayName = MoveTemp(DisplayName);
    NewChapterState.Description = MoveTemp(Description);
    NewChapterState.Phase = EJumanjiAdventureChapterPhase::Active;
    NewChapterState.Revision = AdventureChapterState.Revision + 1;

    AdventureChapterState = MoveTemp(NewChapterState);
    OnAdventureChapterChanged.Broadcast(AdventureChapterState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::CompleteAdventureChapter(FName ChapterId)
{
    if (!HasAuthority()
        || ChapterId.IsNone()
        || AdventureChapterState.ChapterId != ChapterId
        || AdventureChapterState.Phase != EJumanjiAdventureChapterPhase::Active)
    {
        return false;
    }

    AdventureChapterState.Phase = EJumanjiAdventureChapterPhase::Completed;
    ++AdventureChapterState.Revision;

    OnAdventureChapterChanged.Broadcast(AdventureChapterState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::InitializeAdventureObjectives(
    const TArray<FJumanjiAdventureObjectiveState>& Objectives)
{
    if (!HasAuthority()
        || !bBoardEventActive
        || Objectives.IsEmpty()
        || Objectives.Num() > MaxAdventureObjectives)
    {
        return false;
    }

    TSet<FName> SeenObjectiveIds;
    TArray<FJumanjiAdventureObjectiveState> ValidatedObjectives;
    ValidatedObjectives.Reserve(Objectives.Num());

    for (const FJumanjiAdventureObjectiveState& Objective : Objectives)
    {
        if (Objective.ObjectiveId.IsNone()
            || Objective.DisplayName.IsEmpty()
            || Objective.RequiredProgress < 1
            || Objective.RequiredProgress > MaxObjectiveProgress
            || SeenObjectiveIds.Contains(Objective.ObjectiveId))
        {
            return false;
        }

        SeenObjectiveIds.Add(Objective.ObjectiveId);

        FJumanjiAdventureObjectiveState SanitizedObjective = Objective;
        SanitizedObjective.CurrentProgress = 0;
        SanitizedObjective.bCompleted = false;
        ValidatedObjectives.Add(MoveTemp(SanitizedObjective));
    }

    FJumanjiAdventureObjectiveSet NewObjectiveSet;
    NewObjectiveSet.Objectives = MoveTemp(ValidatedObjectives);
    NewObjectiveSet.Revision = AdventureObjectiveSet.Revision + 1;
    AdventureObjectiveSet = MoveTemp(NewObjectiveSet);

    OnAdventureObjectivesChanged.Broadcast(AdventureObjectiveSet.Revision);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::AddAdventureObjectiveProgress(FName ObjectiveId, int32 ProgressAmount)
{
    if (!HasAuthority()
        || !bBoardEventActive
        || ObjectiveId.IsNone()
        || ProgressAmount < 1
        || ProgressAmount > MaxObjectiveProgress)
    {
        return false;
    }

    for (FJumanjiAdventureObjectiveState& Objective : AdventureObjectiveSet.Objectives)
    {
        if (Objective.ObjectiveId != ObjectiveId)
        {
            continue;
        }

        if (Objective.bCompleted)
        {
            return false;
        }

        Objective.CurrentProgress = FMath::Min(
            Objective.RequiredProgress,
            Objective.CurrentProgress + ProgressAmount
        );
        Objective.bCompleted = Objective.CurrentProgress >= Objective.RequiredProgress;

        ++AdventureObjectiveSet.Revision;
        OnAdventureObjectivesChanged.Broadcast(AdventureObjectiveSet.Revision);
        ForceNetUpdate();
        return true;
    }

    return false;
}

bool AJumanjiGameState::AreAdventureObjectivesComplete() const
{
    if (AdventureObjectiveSet.Objectives.IsEmpty())
    {
        return false;
    }

    for (const FJumanjiAdventureObjectiveState& Objective : AdventureObjectiveSet.Objectives)
    {
        if (!Objective.bCompleted)
        {
            return false;
        }
    }

    return true;
}

void AJumanjiGameState::OnRep_BoardEventState()
{
    OnBoardEventStateChanged.Broadcast(
        ActiveBoardEventId,
        bBoardEventActive,
        BoardEventRevision
    );
}

void AJumanjiGameState::OnRep_AdventureObjectiveSet()
{
    OnAdventureObjectivesChanged.Broadcast(AdventureObjectiveSet.Revision);
}

void AJumanjiGameState::OnRep_AdventureChapterState()
{
    OnAdventureChapterChanged.Broadcast(AdventureChapterState);
}

void AJumanjiGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AJumanjiGameState, ActiveBoardEventId);
    DOREPLIFETIME(AJumanjiGameState, bBoardEventActive);
    DOREPLIFETIME(AJumanjiGameState, BoardEventRevision);
    DOREPLIFETIME(AJumanjiGameState, AdventureChapterState);
    DOREPLIFETIME(AJumanjiGameState, AdventureObjectiveSet);
}
