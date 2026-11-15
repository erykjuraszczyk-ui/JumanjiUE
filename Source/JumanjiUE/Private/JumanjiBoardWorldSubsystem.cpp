#include "JumanjiBoardWorldSubsystem.h"

#include "JumanjiGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

bool UJumanjiBoardWorldSubsystem::StartBoardEvent(FName EventId)
{
    UWorld* World = GetWorld();
    if (!World || EventId.IsNone() || !World->GetAuthGameMode())
    {
        return false;
    }

    AJumanjiGameState* BoardGameState = World->GetGameState<AJumanjiGameState>();
    if (!BoardGameState || BoardGameState->IsBoardEventActive())
    {
        return false;
    }

    BoardGameState->SetBoardEventState(EventId, true);
    return BoardGameState->IsBoardEventActive()
        && BoardGameState->GetActiveBoardEventId() == EventId;
}

bool UJumanjiBoardWorldSubsystem::EndBoardEvent(FName EventId)
{
    UWorld* World = GetWorld();
    if (!World || EventId.IsNone() || !World->GetAuthGameMode())
    {
        return false;
    }

    AJumanjiGameState* BoardGameState = World->GetGameState<AJumanjiGameState>();
    if (!BoardGameState
        || !BoardGameState->IsBoardEventActive()
        || BoardGameState->GetActiveBoardEventId() != EventId)
    {
        return false;
    }

    BoardGameState->SetBoardEventState(NAME_None, false);
    return !BoardGameState->IsBoardEventActive();
}

bool UJumanjiBoardWorldSubsystem::StartBoardAdventure(
    FName EventId,
    const TArray<FJumanjiAdventureObjectiveState>& Objectives)
{
    if (Objectives.IsEmpty() || !StartBoardEvent(EventId))
    {
        return false;
    }

    UWorld* World = GetWorld();
    AJumanjiGameState* BoardGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;

    if (!BoardGameState || !BoardGameState->InitializeAdventureObjectives(Objectives))
    {
        EndBoardEvent(EventId);
        return false;
    }

    return true;
}

bool UJumanjiBoardWorldSubsystem::AdvanceBoardAdventureObjective(
    FName ObjectiveId,
    int32 ProgressAmount)
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode())
    {
        return false;
    }

    AJumanjiGameState* BoardGameState = World->GetGameState<AJumanjiGameState>();
    if (!BoardGameState || !BoardGameState->IsBoardEventActive())
    {
        return false;
    }

    return BoardGameState->AddAdventureObjectiveProgress(ObjectiveId, ProgressAmount);
}

bool UJumanjiBoardWorldSubsystem::CompleteBoardAdventure(FName EventId)
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode())
    {
        return false;
    }

    AJumanjiGameState* BoardGameState = World->GetGameState<AJumanjiGameState>();
    if (!BoardGameState
        || !BoardGameState->IsBoardEventActive()
        || BoardGameState->GetActiveBoardEventId() != EventId
        || !BoardGameState->AreAdventureObjectivesComplete())
    {
        return false;
    }

    return EndBoardEvent(EventId);
}
