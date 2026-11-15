#include "JumanjiPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiGreatSpiritSelectionWidget.h"
#include "JumanjiPlayerState.h"

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

bool AJumanjiPlayerController::OpenGreatSpiritSelection()
{
    if (!IsLocalController() || !GreatSpiritSelectionWidgetClass)
    {
        return false;
    }

    if (IsValid(ActiveGreatSpiritSelectionWidget))
    {
        if (!ActiveGreatSpiritSelectionWidget->IsInViewport())
        {
            ActiveGreatSpiritSelectionWidget->AddToViewport(100);
        }
        return true;
    }

    ActiveGreatSpiritSelectionWidget =
        CreateWidget<UJumanjiGreatSpiritSelectionWidget>(this, GreatSpiritSelectionWidgetClass);

    if (!ActiveGreatSpiritSelectionWidget)
    {
        return false;
    }

    ActiveGreatSpiritSelectionWidget->AddToViewport(100);

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(ActiveGreatSpiritSelectionWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetShowMouseCursor(false);
    return true;
}

void AJumanjiPlayerController::CloseGreatSpiritSelection()
{
    if (IsValid(ActiveGreatSpiritSelectionWidget))
    {
        ActiveGreatSpiritSelectionWidget->RemoveFromParent();
        ActiveGreatSpiritSelectionWidget = nullptr;
    }

    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        SetShowMouseCursor(false);
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
