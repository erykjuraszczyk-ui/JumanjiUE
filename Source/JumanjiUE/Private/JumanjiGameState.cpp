#include "JumanjiGameState.h"

#include "Net/UnrealNetwork.h"

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

void AJumanjiGameState::OnRep_BoardEventState()
{
    OnBoardEventStateChanged.Broadcast(
        ActiveBoardEventId,
        bBoardEventActive,
        BoardEventRevision
    );
}

void AJumanjiGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AJumanjiGameState, ActiveBoardEventId);
    DOREPLIFETIME(AJumanjiGameState, bBoardEventActive);
    DOREPLIFETIME(AJumanjiGameState, BoardEventRevision);
}
