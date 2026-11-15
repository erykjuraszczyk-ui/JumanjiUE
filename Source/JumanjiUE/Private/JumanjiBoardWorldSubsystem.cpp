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
