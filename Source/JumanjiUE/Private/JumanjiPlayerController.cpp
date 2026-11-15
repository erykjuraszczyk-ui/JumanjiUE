#include "JumanjiPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiAdventureHUDWidget.h"
#include "JumanjiGreatSpiritSelectionWidget.h"
#include "JumanjiPlayerState.h"
#include "JumanjiTrialActor.h"

void AJumanjiPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController() || !AdventureHUDWidgetClass)
    {
        return;
    }

    ActiveAdventureHUDWidget = CreateWidget<UJumanjiAdventureHUDWidget>(
        this,
        AdventureHUDWidgetClass
    );

    if (IsValid(ActiveAdventureHUDWidget))
    {
        ActiveAdventureHUDWidget->AddToViewport(50);
    }
}

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

bool AJumanjiPlayerController::RequestTrialInteraction(
    AJumanjiTrialActor* TrialActor,
    FName OptionId,
    int32 ExpectedRevision)
{
    if (!IsValid(TrialActor) || OptionId.IsNone() || ExpectedRevision < 0)
    {
        return false;
    }

    if (HasAuthority())
    {
        return ProcessTrialInteractionOnServer(TrialActor, OptionId, ExpectedRevision);
    }

    ServerRequestTrialInteraction(TrialActor, OptionId, ExpectedRevision);
    return true;
}

void AJumanjiPlayerController::ServerSelectCharacter_Implementation(FName CharacterId)
{
    ApplyCharacterSelectionOnServer(CharacterId);
}

void AJumanjiPlayerController::ServerRequestTrialInteraction_Implementation(
    AJumanjiTrialActor* TrialActor,
    FName OptionId,
    int32 ExpectedRevision)
{
    ProcessTrialInteractionOnServer(TrialActor, OptionId, ExpectedRevision);
}

bool AJumanjiPlayerController::ProcessTrialInteractionOnServer(
    AJumanjiTrialActor* TrialActor,
    FName OptionId,
    int32 ExpectedRevision)
{
    APawn* ControlledPawn = GetPawn();
    if (!HasAuthority()
        || !IsValid(TrialActor)
        || !ControlledPawn
        || OptionId.IsNone()
        || !FMath::IsFinite(MaxTrialInteractionDistance)
        || MaxTrialInteractionDistance <= 0.0f)
    {
        return false;
    }

    const float DistanceSquared = FVector::DistSquared(
        ControlledPawn->GetActorLocation(),
        TrialActor->GetActorLocation()
    );

    if (DistanceSquared > FMath::Square(MaxTrialInteractionDistance))
    {
        return false;
    }

    return TrialActor->SubmitOption(OptionId, ExpectedRevision);
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
