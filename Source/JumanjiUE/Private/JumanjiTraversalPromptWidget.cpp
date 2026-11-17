#include "JumanjiTraversalPromptWidget.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "JumanjiCharacter.h"
#include "JumanjiPlayerController.h"
#include "JumanjiTraversalComponent.h"
#include "JumanjiTraversalLink.h"

void UJumanjiTraversalPromptWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UJumanjiTraversalPromptWidget::NativeDestruct()
{
    UnbindTraversalComponent();
    Super::NativeDestruct();
}

void UJumanjiTraversalPromptWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float Now = World->GetTimeSeconds();

    if (PromptViewModel.Status == EJumanjiTraversalPromptStatus::Available
        && IsValid(FocusedTraversalLink))
    {
        APlayerController* OwningController = GetOwningPlayer();
        AJumanjiCharacter* Character = OwningController
            ? Cast<AJumanjiCharacter>(OwningController->GetPawn())
            : nullptr;

        if (!Character
            || (Character->GetTraversalComponent() && Character->GetTraversalComponent()->IsTraversing())
            || !FocusedTraversalLink->CanBeUsedBy(Character))
        {
            ClearFocusedTraversal();
            return;
        }
    }

    if (PromptViewModel.Status == EJumanjiTraversalPromptStatus::PendingServer
        && Now >= RequestDeadlineSeconds)
    {
        PromptViewModel.Status = EJumanjiTraversalPromptStatus::Available;
        PromptViewModel.StatusText = FText::FromString(TEXT("Brak potwierdzenia serwera — spróbuj ponownie"));
        PromptViewModel.bCanRequest = IsValid(FocusedTraversalLink)
            && FocusedTraversalLink->CanBeUsedBy(GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr);
        ++PromptViewModel.Revision;
        BroadcastPrompt();
    }

    if ((PromptViewModel.Status == EJumanjiTraversalPromptStatus::Completed
        || PromptViewModel.Status == EJumanjiTraversalPromptStatus::Interrupted)
        && StatusHideDeadlineSeconds > 0.0f
        && Now >= StatusHideDeadlineSeconds)
    {
        ClearFocusedTraversal();
    }
}

bool UJumanjiTraversalPromptWidget::SetFocusedTraversalLink(AJumanjiTraversalLink* TraversalLink)
{
    APlayerController* OwningController = GetOwningPlayer();
    AJumanjiCharacter* Character = OwningController
        ? Cast<AJumanjiCharacter>(OwningController->GetPawn())
        : nullptr;
    APawn* Pawn = Character;

    if (!IsValid(TraversalLink)
        || !Character
        || (Character->GetTraversalComponent() && Character->GetTraversalComponent()->IsTraversing())
        || !TraversalLink->CanBeUsedBy(Pawn))
    {
        ClearFocusedTraversal();
        return false;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UJumanjiTraversalPresentationSubsystem* PresentationSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiTraversalPresentationSubsystem>()
        : nullptr;

    if (!PresentationSubsystem)
    {
        ClearFocusedTraversal();
        return false;
    }

    FJumanjiTraversalPresentationDefinition Definition;
    if (!PresentationSubsystem->GetPresentationDefinition(
        TraversalLink->GetTraversalId(),
        TraversalLink->GetTraversalType(),
        Definition))
    {
        ClearFocusedTraversal();
        return false;
    }

    UnbindTraversalComponent();
    FocusedTraversalLink = TraversalLink;
    PromptViewModel = FJumanjiTraversalPromptViewModel();
    PromptViewModel.TraversalId = TraversalLink->GetTraversalId();
    PromptViewModel.TraversalType = TraversalLink->GetTraversalType();
    PromptViewModel.Status = EJumanjiTraversalPromptStatus::Available;
    PromptViewModel.DisplayName = Definition.DisplayName;
    PromptViewModel.StatusText = Definition.PromptText;
    PromptViewModel.Icon = Definition.PromptIcon;
    PromptViewModel.bCanRequest = true;
    PromptViewModel.bAllowCameraLook = Definition.bAllowCameraLook;
    PromptViewModel.bShowProgress = Definition.bShowProgress;
    PromptViewModel.DurationSeconds = TraversalLink->GetTraversalDuration();
    PromptViewModel.PromptHoldSeconds = Definition.PromptHoldSeconds;
    ++PromptViewModel.Revision;
    StatusHideDeadlineSeconds = 0.0f;

    BindTraversalComponent();
    SetVisibility(ESlateVisibility::Visible);
    BroadcastPrompt();
    return true;
}

void UJumanjiTraversalPromptWidget::ClearFocusedTraversal()
{
    UnbindTraversalComponent();
    FocusedTraversalLink = nullptr;
    PromptViewModel = FJumanjiTraversalPromptViewModel();
    RequestDeadlineSeconds = 0.0f;
    StatusHideDeadlineSeconds = 0.0f;
    SetVisibility(ESlateVisibility::Collapsed);
    BroadcastPrompt();
}

bool UJumanjiTraversalPromptWidget::RequestFocusedTraversal()
{
    if (!IsValid(FocusedTraversalLink)
        || PromptViewModel.Status != EJumanjiTraversalPromptStatus::Available
        || !PromptViewModel.bCanRequest)
    {
        return false;
    }

    AJumanjiPlayerController* PlayerController =
        Cast<AJumanjiPlayerController>(GetOwningPlayer());

    if (!PlayerController || !PlayerController->RequestTraversalLink(FocusedTraversalLink))
    {
        return false;
    }

    PromptViewModel.Status = EJumanjiTraversalPromptStatus::PendingServer;
    PromptViewModel.StatusText = FText::FromString(TEXT("Oczekiwanie na serwer..."));
    PromptViewModel.bCanRequest = false;
    ++PromptViewModel.Revision;

    RequestDeadlineSeconds = GetWorld()
        ? GetWorld()->GetTimeSeconds() + 3.0f
        : 0.0f;

    BroadcastPrompt();
    return true;
}

void UJumanjiTraversalPromptWidget::BindTraversalComponent()
{
    APlayerController* PlayerController = GetOwningPlayer();
    AJumanjiCharacter* Character = PlayerController
        ? Cast<AJumanjiCharacter>(PlayerController->GetPawn())
        : nullptr;

    BoundTraversalComponent = Character ? Character->GetTraversalComponent() : nullptr;
    if (BoundTraversalComponent)
    {
        BoundTraversalComponent->OnTraversalStateChanged.RemoveDynamic(
            this,
            &UJumanjiTraversalPromptWidget::HandleTraversalStateChanged
        );
        BoundTraversalComponent->OnTraversalStateChanged.AddDynamic(
            this,
            &UJumanjiTraversalPromptWidget::HandleTraversalStateChanged
        );
    }
}

void UJumanjiTraversalPromptWidget::UnbindTraversalComponent()
{
    if (BoundTraversalComponent)
    {
        BoundTraversalComponent->OnTraversalStateChanged.RemoveDynamic(
            this,
            &UJumanjiTraversalPromptWidget::HandleTraversalStateChanged
        );
        BoundTraversalComponent = nullptr;
    }
}

void UJumanjiTraversalPromptWidget::HandleTraversalStateChanged(
    const FJumanjiTraversalReplicatedState& State)
{
    if (State.TraversalId != PromptViewModel.TraversalId)
    {
        return;
    }

    ++PromptViewModel.Revision;

    if (State.bActive)
    {
        PromptViewModel.Status = EJumanjiTraversalPromptStatus::InProgress;
        PromptViewModel.StatusText = FText::FromString(TEXT("Przejście w toku"));
        PromptViewModel.bCanRequest = false;
        RequestDeadlineSeconds = 0.0f;
        StatusHideDeadlineSeconds = 0.0f;
    }
    else
    {
        PromptViewModel.Status = State.bLastTraversalCompleted
            ? EJumanjiTraversalPromptStatus::Completed
            : EJumanjiTraversalPromptStatus::Interrupted;
        PromptViewModel.StatusText = State.bLastTraversalCompleted
            ? FText::FromString(TEXT("Przejście zakończone"))
            : FText::FromString(TEXT("Przejście przerwane — przeszkoda"));
        PromptViewModel.bCanRequest = false;

        const float HoldSeconds = FMath::Clamp(PromptViewModel.PromptHoldSeconds, 0.2f, 3.0f);
        StatusHideDeadlineSeconds = GetWorld()
            ? GetWorld()->GetTimeSeconds() + HoldSeconds
            : 0.0f;
    }

    BroadcastPrompt();
}

void UJumanjiTraversalPromptWidget::BroadcastPrompt()
{
    OnPromptChanged.Broadcast(PromptViewModel);
}
