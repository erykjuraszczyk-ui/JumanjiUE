#include "JumanjiPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiAdventureHUDWidget.h"
#include "JumanjiAbilityHUDWidget.h"
#include "JumanjiWorldEncounterHUDWidget.h"
#include "JumanjiAbilityCatalogSubsystem.h"
#include "JumanjiGreatSpiritSelectionWidget.h"
#include "JumanjiPlayerState.h"
#include "JumanjiProgressionRewardSubsystem.h"
#include "JumanjiTrialActor.h"
#include "JumanjiWorldAtlasWidget.h"
#include "JumanjiWorldNavigationComponent.h"
#include "JumanjiCharacter.h"
#include "JumanjiTraversalLink.h"
#include "JumanjiTraversalPromptWidget.h"
#include "JumanjiTraversalComponent.h"
#include "JumanjiWorldEncounterDirector.h"

AJumanjiPlayerController::AJumanjiPlayerController()
{
    WorldNavigationComponent = CreateDefaultSubobject<UJumanjiWorldNavigationComponent>(TEXT("WorldNavigationComponent"));
}

void AJumanjiPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    if (AdventureHUDWidgetClass)
    {
        ActiveAdventureHUDWidget = CreateWidget<UJumanjiAdventureHUDWidget>(
            this,
            AdventureHUDWidgetClass
        );

        if (IsValid(ActiveAdventureHUDWidget))
        {
            ActiveAdventureHUDWidget->AddToViewport(50);
        }
    }

    if (AbilityHUDWidgetClass)
    {
        ActiveAbilityHUDWidget = CreateWidget<UJumanjiAbilityHUDWidget>(
            this,
            AbilityHUDWidgetClass
        );

        if (IsValid(ActiveAbilityHUDWidget))
        {
            ActiveAbilityHUDWidget->AddToViewport(60);
        }
    }

    if (WorldEncounterHUDWidgetClass)
    {
        ActiveWorldEncounterHUDWidget = CreateWidget<UJumanjiWorldEncounterHUDWidget>(
            this,
            WorldEncounterHUDWidgetClass
        );

        if (IsValid(ActiveWorldEncounterHUDWidget))
        {
            ActiveWorldEncounterHUDWidget->AddToViewport(65);
        }
    }

    if (TraversalPromptWidgetClass)
    {
        ActiveTraversalPromptWidget = CreateWidget<UJumanjiTraversalPromptWidget>(
            this,
            TraversalPromptWidgetClass
        );

        if (IsValid(ActiveTraversalPromptWidget))
        {
            ActiveTraversalPromptWidget->AddToViewport(70);
        }
    }

    // Some overlap BeginPlay events may run before the prompt widget exists.
    RefreshTraversalCandidateFocus();
}

bool AJumanjiPlayerController::OpenWorldAtlas()
{
    if (!IsLocalController() || !WorldAtlasWidgetClass)
    {
        return false;
    }

    if (IsValid(ActiveWorldAtlasWidget))
    {
        if (!ActiveWorldAtlasWidget->IsInViewport())
        {
            ActiveWorldAtlasWidget->AddToViewport(90);
        }
        return true;
    }

    ActiveWorldAtlasWidget = CreateWidget<UJumanjiWorldAtlasWidget>(
        this,
        WorldAtlasWidgetClass
    );

    if (!IsValid(ActiveWorldAtlasWidget))
    {
        return false;
    }

    ActiveWorldAtlasWidget->AddToViewport(90);

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(ActiveWorldAtlasWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetShowMouseCursor(false);
    return true;
}

void AJumanjiPlayerController::CloseWorldAtlas()
{
    if (IsValid(ActiveWorldAtlasWidget))
    {
        ActiveWorldAtlasWidget->RemoveFromParent();
        ActiveWorldAtlasWidget = nullptr;
    }

    if (IsLocalController())
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        SetShowMouseCursor(false);
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

bool AJumanjiPlayerController::RequestGreatSpiritUpgrade(FName CharacterId, FName RecipeId)
{
    if (CharacterId.IsNone() || RecipeId.IsNone())
    {
        return false;
    }

    if (HasAuthority())
    {
        return ProcessGreatSpiritUpgradeOnServer(CharacterId, RecipeId);
    }

    ServerRequestGreatSpiritUpgrade(CharacterId, RecipeId);
    return true; // The request was queued; the server result is reflected by OnProgressionChanged.
}

void AJumanjiPlayerController::ServerSelectCharacter_Implementation(FName CharacterId)
{
    ApplyCharacterSelectionOnServer(CharacterId);
}

void AJumanjiPlayerController::ServerRequestGreatSpiritUpgrade_Implementation(
    FName CharacterId,
    FName RecipeId)
{
    ProcessGreatSpiritUpgradeOnServer(CharacterId, RecipeId);
}

bool AJumanjiPlayerController::RequestEquipAbilitySlot(
    FName CharacterId,
    FName SlotId,
    FName AbilityId)
{
    if (CharacterId.IsNone() || SlotId.IsNone() || AbilityId.IsNone())
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiAbilityCatalogSubsystem* AbilityCatalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiAbilityCatalogSubsystem>()
        : nullptr;

    if (!AbilityCatalog
        || !AbilityCatalog->IsValidAbilityChoice(CharacterId, SlotId, AbilityId))
    {
        return false;
    }

    if (HasAuthority())
    {
        return ProcessEquipAbilitySlotOnServer(CharacterId, SlotId, AbilityId);
    }

    ServerRequestEquipAbilitySlot(CharacterId, SlotId, AbilityId);
    return true; // Queued only; observe PlayerState.OnProgressionChanged for the accepted result.
}

void AJumanjiPlayerController::ServerRequestEquipAbilitySlot_Implementation(
    FName CharacterId,
    FName SlotId,
    FName AbilityId)
{
    ProcessEquipAbilitySlotOnServer(CharacterId, SlotId, AbilityId);
}

bool AJumanjiPlayerController::ProcessEquipAbilitySlotOnServer(
    FName CharacterId,
    FName SlotId,
    FName AbilityId)
{
    if (!HasAuthority())
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiAbilityCatalogSubsystem* AbilityCatalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiAbilityCatalogSubsystem>()
        : nullptr;

    AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();
    if (!AbilityCatalog
        || !JumanjiPlayerState
        || !AbilityCatalog->IsValidAbilityChoice(CharacterId, SlotId, AbilityId))
    {
        return false;
    }

    return JumanjiPlayerState->SetEquippedAbilitySlot(CharacterId, SlotId, AbilityId);
}


bool AJumanjiPlayerController::ProcessGreatSpiritUpgradeOnServer(
    FName CharacterId,
    FName RecipeId)
{
    if (!HasAuthority() || CharacterId.IsNone() || RecipeId.IsNone())
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UJumanjiProgressionRewardSubsystem* RewardSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiProgressionRewardSubsystem>()
        : nullptr;
    AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();

    return RewardSubsystem
        && JumanjiPlayerState
        && RewardSubsystem->TryUpgradeGreatSpirit(JumanjiPlayerState, CharacterId, RecipeId);
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

bool AJumanjiPlayerController::SetTraversalFocus(AJumanjiTraversalLink* TraversalLink)
{
    if (!IsLocalController() || !IsValid(ActiveTraversalPromptWidget))
    {
        return false;
    }

    return ActiveTraversalPromptWidget->SetFocusedTraversalLink(TraversalLink);
}

void AJumanjiPlayerController::ClearTraversalFocus()
{
    NearbyTraversalCandidates.Reset();

    if (IsLocalController() && IsValid(ActiveTraversalPromptWidget))
    {
        ActiveTraversalPromptWidget->ClearFocusedTraversal();
    }
}

void AJumanjiPlayerController::RegisterTraversalCandidate(AJumanjiTraversalLink* TraversalLink)
{
    if (!IsLocalController() || !IsValid(TraversalLink))
    {
        return;
    }

    NearbyTraversalCandidates.RemoveAll(
        [](const TObjectPtr<AJumanjiTraversalLink>& Candidate)
        {
            return !IsValid(Candidate);
        });

    if (!NearbyTraversalCandidates.Contains(TraversalLink))
    {
        NearbyTraversalCandidates.Add(TraversalLink);
    }

    RefreshTraversalCandidateFocus();
}

void AJumanjiPlayerController::UnregisterTraversalCandidate(AJumanjiTraversalLink* TraversalLink)
{
    if (!IsLocalController() || !IsValid(TraversalLink))
    {
        return;
    }

    NearbyTraversalCandidates.Remove(TraversalLink);
    RefreshTraversalCandidateFocus();
}

void AJumanjiPlayerController::RefreshTraversalCandidateFocus()
{
    if (!IsLocalController() || !IsValid(ActiveTraversalPromptWidget))
    {
        return;
    }

    AJumanjiCharacter* Character = Cast<AJumanjiCharacter>(GetPawn());
    if (!Character)
    {
        ActiveTraversalPromptWidget->ClearFocusedTraversal();
        return;
    }

    UJumanjiTraversalComponent* TraversalComponent = Character->GetTraversalComponent();
    if (TraversalComponent && TraversalComponent->IsTraversing())
    {
        // Keep the active link bound so the widget receives its server-confirmed
        // completion/interruption event even after the overlap volume is exited.
        const FJumanjiTraversalReplicatedState State = TraversalComponent->GetTraversalState();
        if (State.bActive && State.TraversalId == ActiveTraversalPromptWidget->GetPromptViewModel().TraversalId)
        {
            return;
        }

        ActiveTraversalPromptWidget->ClearFocusedTraversal();
        return;
    }

    NearbyTraversalCandidates.RemoveAll(
        [Character](const TObjectPtr<AJumanjiTraversalLink>& Candidate)
        {
            return !IsValid(Candidate) || !Candidate->CanBeUsedBy(Character);
        });

    AJumanjiTraversalLink* BestCandidate = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    const FVector PlayerLocation = Character->GetActorLocation();

    for (const TObjectPtr<AJumanjiTraversalLink>& Candidate : NearbyTraversalCandidates)
    {
        if (!IsValid(Candidate))
        {
            continue;
        }

        const float EntryDistance = FVector::DistSquared(PlayerLocation, Candidate->GetEntryLocation());
        const float ExitDistance = FVector::DistSquared(PlayerLocation, Candidate->GetExitLocation());
        const float CandidateDistance = FMath::Min(EntryDistance, ExitDistance);

        if (CandidateDistance < BestDistanceSquared)
        {
            BestDistanceSquared = CandidateDistance;
            BestCandidate = Candidate;
        }
    }

    if (IsValid(BestCandidate))
    {
        const FJumanjiTraversalPromptViewModel CurrentModel =
            ActiveTraversalPromptWidget->GetPromptViewModel();

        if (CurrentModel.TraversalId != BestCandidate->GetTraversalId()
            || CurrentModel.Status == EJumanjiTraversalPromptStatus::Hidden)
        {
            ActiveTraversalPromptWidget->SetFocusedTraversalLink(BestCandidate);
        }
    }
    else
    {
        ActiveTraversalPromptWidget->ClearFocusedTraversal();
    }
}

bool AJumanjiPlayerController::RequestTraversalLink(AJumanjiTraversalLink* TraversalLink)
{
    if (!IsValid(TraversalLink) || !TraversalLink->CanBeUsedBy(GetPawn()))
    {
        return false;
    }
    if (HasAuthority())
    {
        return ProcessTraversalLinkOnServer(TraversalLink);
    }
    ServerRequestTraversalLink(TraversalLink);
    return true; // Queued only; observe replicated traversal state for acceptance.
}

void AJumanjiPlayerController::ServerRequestTraversalLink_Implementation(AJumanjiTraversalLink* TraversalLink)
{
    ProcessTraversalLinkOnServer(TraversalLink);
}

bool AJumanjiPlayerController::ProcessTraversalLinkOnServer(AJumanjiTraversalLink* TraversalLink)
{
    if (!HasAuthority() || !IsValid(TraversalLink))
    {
        return false;
    }
    AJumanjiCharacter* Character = Cast<AJumanjiCharacter>(GetPawn());
    UJumanjiTraversalComponent* Traversal = Character ? Character->GetTraversalComponent() : nullptr;
    return Traversal && Traversal->TryStartTraversal(TraversalLink);
}

bool AJumanjiPlayerController::RequestStartWorldEncounter(
    AJumanjiWorldEncounterDirector* EncounterDirector)
{
    if (!IsValid(EncounterDirector) || EncounterDirector->GetWorld() != GetWorld())
    {
        return false;
    }

    if (HasAuthority())
    {
        return ProcessStartWorldEncounterOnServer(EncounterDirector);
    }

    ServerRequestStartWorldEncounter(EncounterDirector);
    return true; // Queued only; read replicated GameState encounter state for acceptance.
}

void AJumanjiPlayerController::ServerRequestStartWorldEncounter_Implementation(
    AJumanjiWorldEncounterDirector* EncounterDirector)
{
    ProcessStartWorldEncounterOnServer(EncounterDirector);
}

bool AJumanjiPlayerController::ProcessStartWorldEncounterOnServer(
    AJumanjiWorldEncounterDirector* EncounterDirector)
{
    APawn* ControlledPawn = GetPawn();
    AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();

    if (!HasAuthority()
        || !IsValid(EncounterDirector)
        || EncounterDirector->GetWorld() != GetWorld()
        || !ControlledPawn
        || !JumanjiPlayerState
        || !FMath::IsFinite(MaxWorldEncounterStartDistance)
        || MaxWorldEncounterStartDistance < 100.0f
        || MaxWorldEncounterStartDistance > 1000.0f
        || FVector::DistSquared(
            ControlledPawn->GetActorLocation(),
            EncounterDirector->GetActorLocation())
            > FMath::Square(MaxWorldEncounterStartDistance))
    {
        return false;
    }

    return EncounterDirector->StartEncounter(JumanjiPlayerState);
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
