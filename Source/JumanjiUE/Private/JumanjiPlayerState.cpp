#include "JumanjiPlayerState.h"

#include "Net/UnrealNetwork.h"

AJumanjiPlayerState::AJumanjiPlayerState()
{
    bReplicates = true;
}

void AJumanjiPlayerState::SetSelectedCharacterId(FName CharacterId)
{
    if (!HasAuthority() || CharacterId.IsNone() || SelectedCharacterId == CharacterId)
    {
        return;
    }

    SelectedCharacterId = CharacterId;
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
    ForceNetUpdate();
}

void AJumanjiPlayerState::OnRep_SelectedCharacterId()
{
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
}

void AJumanjiPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiPlayerState, SelectedCharacterId);
}
