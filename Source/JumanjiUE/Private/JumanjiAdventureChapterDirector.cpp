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
    }

    BoardGameState->OnAdventureObjectivesChanged.AddUniqueDynamic(
        this,
        &AJumanjiAdventureChapterDirector::HandleAdventureObjectivesChanged
    );
    bListeningForObjectiveChanges = true;

    return true;
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
