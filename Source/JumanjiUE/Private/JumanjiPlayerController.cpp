#include "JumanjiPlayerController.h"

#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiPlayerState.h"
#include "Engine/GameInstance.h"

void AJumanjiPlayerController::RequestSelectCharacter(FName CharacterId)
{
    if (CharacterId.IsNone())
    {
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster =
        GameInstance ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>() : nullptr;

    if (!Roster || !Roster->IsPlayableCharacter(CharacterId))
    {
        return;
    }

    if (HasAuthority())
    {
        ApplyCharacterSelectionOnServer(CharacterId);
    }
    else
    {
        ServerSelectCharacter(CharacterId);
    }
}

void AJumanjiPlayerController::ServerSelectCharacter_Implementation(FName CharacterId)
{
    ApplyCharacterSelectionOnServer(CharacterId);
}

void AJumanjiPlayerController::ApplyCharacterSelectionOnServer(FName CharacterId)
{
    if (!HasAuthority())
    {
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster =
        GameInstance ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>() : nullptr;

    if (!Roster || !Roster->IsPlayableCharacter(CharacterId))
    {
        return;
    }

    if (AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>())
    {
        JumanjiPlayerState->SetSelectedCharacterId(CharacterId);
    }
}
