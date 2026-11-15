#include "JumanjiGreatSpiritSelectionWidget.h"

#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiPlayerController.h"
#include "JumanjiPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UJumanjiGreatSpiritSelectionWidget::NativeConstruct()
{
    Super::NativeConstruct();

    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UJumanjiCharacterRosterSubsystem* Roster =
        GameInstance ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>() : nullptr;

    if (!Roster)
    {
        RosterEntries.Reset();
        return;
    }

    Roster->GetRosterForBlueprint(RosterEntries);
    BindToPlayerState();

    if (ConfirmedCharacterId.IsNone())
    {
        PreviewCharacterId = Roster->GetDefaultCharacterId();
        ConfirmedCharacterId = PreviewCharacterId;
    }

    BP_OnRosterReady(RosterEntries, ConfirmedCharacterId);

    FJumanjiCharacterDefinition InitialDefinition;
    if (Roster->GetCharacterDefinition(PreviewCharacterId, InitialDefinition))
    {
        BP_OnCharacterPreviewChanged(InitialDefinition);
    }
}

void UJumanjiGreatSpiritSelectionWidget::NativeDestruct()
{
    UnbindFromPlayerState();
    Super::NativeDestruct();
}

void UJumanjiGreatSpiritSelectionWidget::BindToPlayerState()
{
    UnbindFromPlayerState();

    AJumanjiPlayerState* PlayerState = GetOwningPlayer()
        ? GetOwningPlayer()->GetPlayerState<AJumanjiPlayerState>()
        : nullptr;

    if (!PlayerState)
    {
        return;
    }

    BoundPlayerState = PlayerState;
    PlayerState->OnSelectedCharacterChanged.AddDynamic(
        this, &UJumanjiGreatSpiritSelectionWidget::HandlePlayerStateCharacterChanged);

    ConfirmedCharacterId = PlayerState->GetSelectedCharacterId();
    PreviewCharacterId = ConfirmedCharacterId;
}

void UJumanjiGreatSpiritSelectionWidget::UnbindFromPlayerState()
{
    if (AJumanjiPlayerState* PlayerState = BoundPlayerState.Get())
    {
        PlayerState->OnSelectedCharacterChanged.RemoveDynamic(
            this, &UJumanjiGreatSpiritSelectionWidget::HandlePlayerStateCharacterChanged);
    }

    BoundPlayerState.Reset();
}

bool UJumanjiGreatSpiritSelectionWidget::PreviewCharacter(FName CharacterId)
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UJumanjiCharacterRosterSubsystem* Roster =
        GameInstance ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>() : nullptr;

    if (bAwaitingServerConfirmation || !Roster || !Roster->IsPlayableCharacter(CharacterId))
    {
        return false;
    }

    FJumanjiCharacterDefinition Definition;
    if (!Roster->GetCharacterDefinition(CharacterId, Definition))
    {
        return false;
    }

    PreviewCharacterId = CharacterId;
    BP_OnCharacterPreviewChanged(Definition);
    return true;
}

bool UJumanjiGreatSpiritSelectionWidget::ConfirmSelection()
{
    if (PreviewCharacterId.IsNone() || bAwaitingServerConfirmation)
    {
        return false;
    }

    AJumanjiPlayerController* Controller = Cast<AJumanjiPlayerController>(GetOwningPlayer());
    if (!Controller)
    {
        return false;
    }

    if (PreviewCharacterId == ConfirmedCharacterId)
    {
        BP_OnCharacterSelectionCommitted(ConfirmedCharacterId);
        return true;
    }

    // Set pending before sending the request. On a listen server the
    // PlayerState delegate may fire synchronously inside RequestSelectCharacter.
    bAwaitingServerConfirmation = true;
    BP_OnSelectionRequestSent(PreviewCharacterId);
    Controller->RequestSelectCharacter(PreviewCharacterId);
    return true;
}

void UJumanjiGreatSpiritSelectionWidget::CancelSelection()
{
    // Once a server request has been sent, do not imply it can be cancelled
    // locally. Wait for the replicated result before closing the screen.
    if (bAwaitingServerConfirmation)
    {
        return;
    }

    PreviewCharacterId = ConfirmedCharacterId;

    BP_OnSelectionCancelled();

    if (AJumanjiPlayerController* Controller = Cast<AJumanjiPlayerController>(GetOwningPlayer()))
    {
        Controller->CloseGreatSpiritSelection();
    }
}

void UJumanjiGreatSpiritSelectionWidget::HandlePlayerStateCharacterChanged(FName CharacterId)
{
    if (CharacterId.IsNone())
    {
        return;
    }

    ConfirmedCharacterId = CharacterId;
    PreviewCharacterId = CharacterId;
    bAwaitingServerConfirmation = false;

    BP_OnCharacterSelectionCommitted(CharacterId);

    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UJumanjiCharacterRosterSubsystem* Roster =
        GameInstance ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>() : nullptr;

    FJumanjiCharacterDefinition Definition;
    if (Roster && Roster->GetCharacterDefinition(CharacterId, Definition))
    {
        BP_OnCharacterPreviewChanged(Definition);
    }
}
