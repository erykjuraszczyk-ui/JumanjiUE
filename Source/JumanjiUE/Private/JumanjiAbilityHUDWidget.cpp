#include "JumanjiAbilityHUDWidget.h"

#include "JumanjiAbilityHUDSlotWidget.h"
#include "AbilitySystemInterface.h"
#include "Engine/GameInstance.h"
#include "JumanjiAbilityPresentationSubsystem.h"
#include "JumanjiAbilitySystemComponent.h"
#include "JumanjiAttributeSet.h"
#include "JumanjiCharacter.h"
#include "JumanjiPlayerState.h"
#include "GameFramework/PlayerController.h"

namespace
{
    const EJumanjiAbilitySlot AbilitySlotOrder[] =
    {
        EJumanjiAbilitySlot::PrimaryAttack,
        EJumanjiAbilitySlot::Active,
        EJumanjiAbilitySlot::Passive,
        EJumanjiAbilitySlot::Ultimate,
        EJumanjiAbilitySlot::Dodge
    };

    FName GetCatalogSlotId(EJumanjiAbilitySlot Slot)
    {
        switch (Slot)
        {
        case EJumanjiAbilitySlot::PrimaryAttack: return FName(TEXT("SLOT_PRIMARY"));
        case EJumanjiAbilitySlot::Active: return FName(TEXT("SLOT_ACTIVE"));
        case EJumanjiAbilitySlot::Passive: return FName(TEXT("SLOT_PASSIVE"));
        case EJumanjiAbilitySlot::Ultimate: return FName(TEXT("SLOT_ULTIMATE"));
        case EJumanjiAbilitySlot::Dodge: return FName(TEXT("SLOT_DODGE"));
        default: return NAME_None;
        }
    }

    FText GetDefaultSlotName(EJumanjiAbilitySlot Slot)
    {
        switch (Slot)
        {
        case EJumanjiAbilitySlot::PrimaryAttack:
            return FText::FromString(TEXT("Podstawowa"));
        case EJumanjiAbilitySlot::Active:
            return FText::FromString(TEXT("Aktywna"));
        case EJumanjiAbilitySlot::Passive:
            return FText::FromString(TEXT("Pasywna"));
        case EJumanjiAbilitySlot::Ultimate:
            return FText::FromString(TEXT("Najwyższa"));
        case EJumanjiAbilitySlot::Dodge:
            return FText::FromString(TEXT("Unik"));
        default:
            return FText::FromString(TEXT("Zdolność"));
        }
    }
}

void UJumanjiAbilityHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    const TArray<UJumanjiAbilityHUDSlotWidget*> SlotWidgets =
    {
        PrimarySlotWidget,
        ActiveSlotWidget,
        PassiveSlotWidget,
        UltimateSlotWidget,
        DodgeSlotWidget
    };

    for (UJumanjiAbilityHUDSlotWidget* SlotWidget : SlotWidgets)
    {
        if (SlotWidget)
        {
            SlotWidget->OnSlotPressed.AddDynamic(
                this,
                &UJumanjiAbilityHUDWidget::HandleAbilitySlotPressed
            );
        }
    }

    OnAbilitySlotFeedback.AddDynamic(
        this,
        &UJumanjiAbilityHUDWidget::HandleAbilitySlotFeedback
    );

    TryBindAbilitySystem();
    RefreshAbilityHUD();
}

void UJumanjiAbilityHUDWidget::NativeDestruct()
{
    const TArray<UJumanjiAbilityHUDSlotWidget*> SlotWidgets =
    {
        PrimarySlotWidget,
        ActiveSlotWidget,
        PassiveSlotWidget,
        UltimateSlotWidget,
        DodgeSlotWidget
    };

    for (UJumanjiAbilityHUDSlotWidget* SlotWidget : SlotWidgets)
    {
        if (SlotWidget)
        {
            SlotWidget->OnSlotPressed.RemoveDynamic(
                this,
                &UJumanjiAbilityHUDWidget::HandleAbilitySlotPressed
            );
        }
    }

    OnAbilitySlotFeedback.RemoveDynamic(
        this,
        &UJumanjiAbilityHUDWidget::HandleAbilitySlotFeedback
    );

    UnbindAbilitySystem();
    TargetFocus.Reset();
    Super::NativeDestruct();
}

void UJumanjiAbilityHUDWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!IsValid(CachedAbilitySystem))
    {
        TryBindAbilitySystem();
    }

    RefreshAccumulator += InDeltaTime;
    const float SafeInterval = FMath::Clamp(RefreshInterval, 0.05f, 1.0f);
    if (RefreshAccumulator >= SafeInterval)
    {
        RefreshAccumulator = 0.0f;
        RefreshAbilityHUD();
    }

    if (TargetFocus.IsValid())
    {
        const APlayerController* PlayerController = GetOwningPlayer();
        const APawn* OwnerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
        if (!OwnerPawn
            || FVector::DistSquared(OwnerPawn->GetActorLocation(), TargetFocus->GetActorLocation())
                > FMath::Square(MaxTargetFocusDistance))
        {
            ClearTargetFocus();
        }
    }
}

void UJumanjiAbilityHUDWidget::TryBindAbilitySystem()
{
    APlayerController* PlayerController = GetOwningPlayer();
    AJumanjiPlayerState* JumanjiPlayerState = PlayerController
        ? PlayerController->GetPlayerState<AJumanjiPlayerState>()
        : nullptr;

    UJumanjiAbilitySystemComponent* NewAbilitySystem = JumanjiPlayerState
        ? JumanjiPlayerState->GetJumanjiAbilitySystemComponent()
        : nullptr;

    if (!NewAbilitySystem || CachedAbilitySystem == NewAbilitySystem)
    {
        return;
    }

    UnbindAbilitySystem();
    CachedAbilitySystem = NewAbilitySystem;

    EnergyChangedHandle = CachedAbilitySystem
        ->GetGameplayAttributeValueChangeDelegate(UJumanjiAttributeSet::GetEnergyAttribute())
        .AddUObject(this, &UJumanjiAbilityHUDWidget::HandleEnergyChanged);

    ResonanceChangedHandle = CachedAbilitySystem
        ->GetGameplayAttributeValueChangeDelegate(UJumanjiAttributeSet::GetResonanceAttribute())
        .AddUObject(this, &UJumanjiAbilityHUDWidget::HandleResonanceChanged);
}

void UJumanjiAbilityHUDWidget::UnbindAbilitySystem()
{
    if (IsValid(CachedAbilitySystem))
    {
        if (EnergyChangedHandle.IsValid())
        {
            CachedAbilitySystem
                ->GetGameplayAttributeValueChangeDelegate(UJumanjiAttributeSet::GetEnergyAttribute())
                .Remove(EnergyChangedHandle);
        }

        if (ResonanceChangedHandle.IsValid())
        {
            CachedAbilitySystem
                ->GetGameplayAttributeValueChangeDelegate(UJumanjiAttributeSet::GetResonanceAttribute())
                .Remove(ResonanceChangedHandle);
        }
    }

    EnergyChangedHandle.Reset();
    ResonanceChangedHandle.Reset();
    CachedAbilitySystem = nullptr;
}

TArray<FJumanjiAbilityHUDSlotState> UJumanjiAbilityHUDWidget::GetAbilitySlotStates() const
{
    TArray<FJumanjiAbilityHUDSlotState> Result;
    Result.Reserve(UE_ARRAY_COUNT(AbilitySlotOrder));

    for (const EJumanjiAbilitySlot Slot : AbilitySlotOrder)
    {
        FJumanjiAbilityHUDSlotState State;
        State.Slot = Slot;
        State.DisplayName = GetDefaultSlotName(Slot);
        State.bIsPassive = Slot == EJumanjiAbilitySlot::Passive;
        Result.Add(State);
    }

    if (!IsValid(CachedAbilitySystem))
    {
        return Result;
    }

    const FGameplayAbilityActorInfo* ActorInfo = CachedAbilitySystem->AbilityActorInfo.Get();
    const FJumanjiAbilityHUDResourceState Resources = GetResourceState();

    const APlayerController* OwningPlayerController = GetOwningPlayer();
    const AJumanjiPlayerState* JumanjiPlayerState = OwningPlayerController
        ? OwningPlayerController->GetPlayerState<AJumanjiPlayerState>()
        : nullptr;
    const FName SelectedCharacterId = JumanjiPlayerState
        ? JumanjiPlayerState->GetSelectedCharacterId()
        : NAME_None;

    const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UJumanjiAbilityPresentationSubsystem* PresentationSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiAbilityPresentationSubsystem>()
        : nullptr;

    for (const FGameplayAbilitySpec& Spec : CachedAbilitySystem->GetActivatableAbilities())
    {
        const UJumanjiGameplayAbility* Ability = Cast<UJumanjiGameplayAbility>(Spec.Ability);
        if (!Ability)
        {
            continue;
        }

        FJumanjiAbilityHUDSlotState* State = Result.FindByPredicate(
            [Ability](const FJumanjiAbilityHUDSlotState& Candidate)
            {
                return Candidate.Slot == Ability->GetAbilitySlot();
            });

        if (!State)
        {
            continue;
        }

        State->bIsGranted = true;
        State->AbilityId = Ability->GetHUDAbilityId();
        State->DisplayName = Ability->GetHUDDisplayName().IsEmpty()
            ? GetDefaultSlotName(State->Slot)
            : Ability->GetHUDDisplayName();
        State->Description = Ability->GetHUDDescription();
        State->InputTag = Ability->GetInputTag();
        State->Icon = Ability->GetHUDIcon();
        State->ResourceType = Ability->GetHUDResourceType();
        State->RequiredResource = FMath::Max(0.0f, Ability->GetHUDResourceCost());

        // Stage 19 data is a presentation override only. Missing rows/assets
        // fall back to the ability class metadata and never block HUD creation.
        if (PresentationSubsystem
            && !State->AbilityId.IsNone()
            && !SelectedCharacterId.IsNone())
        {
            FJumanjiAbilityPresentationData Presentation;
            if (PresentationSubsystem->GetPresentation(State->AbilityId, Presentation)
                && Presentation.CharacterId == SelectedCharacterId
                && Presentation.SlotId == GetCatalogSlotId(State->Slot))
            {
                if (!Presentation.AbilityName.IsEmpty())
                {
                    State->DisplayName = Presentation.AbilityName;
                }
                if (!Presentation.Icon.IsNull())
                {
                    State->Icon = Presentation.Icon;
                }

                State->AccentColor = Presentation.AccentColor;
                State->SecondaryColor = Presentation.SecondaryColor;
                State->CooldownMaterial = Presentation.CooldownMaterial;
                State->ActivatedSound = Presentation.ActivatedSound;
                State->InsufficientResourceSound = Presentation.InsufficientResourceSound;
                State->CooldownSound = Presentation.CooldownSound;
                State->UnavailableSound = Presentation.UnavailableSound;
                State->PresentationCueTag = Presentation.GameplayCueTag;
                State->CooldownSecondsReference = Presentation.CooldownSecondsReference;
            }
        }

        switch (State->ResourceType)
        {
        case EJumanjiAbilityHUDResource::Energy:
            State->CurrentResource = Resources.Energy;
            break;
        case EJumanjiAbilityHUDResource::Resonance:
            State->CurrentResource = Resources.Resonance;
            break;
        case EJumanjiAbilityHUDResource::None:
        default:
            State->CurrentResource = 0.0f;
            break;
        }

        State->bHasRequiredResource = State->ResourceType == EJumanjiAbilityHUDResource::None
            || State->CurrentResource >= State->RequiredResource;

        if (ActorInfo)
        {
            Ability->GetCooldownTimeRemainingAndDuration(
                Spec.Handle,
                ActorInfo,
                State->CooldownRemaining,
                State->CooldownDuration
            );

            FGameplayTagContainer FailureTags;
            State->bCanActivate = Ability->CanActivateAbility(
                Spec.Handle,
                ActorInfo,
                nullptr,
                nullptr,
                &FailureTags
            );
        }

        State->CooldownRemaining = FMath::Max(0.0f, State->CooldownRemaining);
        State->CooldownDuration = FMath::Max(0.0f, State->CooldownDuration);
        State->CooldownPercent = State->CooldownDuration > 0.0f
            ? FMath::Clamp(State->CooldownRemaining / State->CooldownDuration, 0.0f, 1.0f)
            : 0.0f;
        State->bIsOnCooldown = State->CooldownRemaining > 0.01f;

        // A passive is informative only; manual activation is not exposed.
        if (State->bIsPassive)
        {
            State->bCanActivate = false;
        }
    }

    return Result;
}

FJumanjiAbilityHUDResourceState UJumanjiAbilityHUDWidget::GetResourceState() const
{
    FJumanjiAbilityHUDResourceState State;
    if (!IsValid(CachedAbilitySystem))
    {
        return State;
    }

    State.Energy = CachedAbilitySystem->GetNumericAttribute(UJumanjiAttributeSet::GetEnergyAttribute());
    State.MaxEnergy = CachedAbilitySystem->GetNumericAttribute(UJumanjiAttributeSet::GetMaxEnergyAttribute());
    State.Resonance = CachedAbilitySystem->GetNumericAttribute(UJumanjiAttributeSet::GetResonanceAttribute());
    State.MaxResonance = CachedAbilitySystem->GetNumericAttribute(UJumanjiAttributeSet::GetMaxResonanceAttribute());

    State.Energy = FMath::Max(0.0f, State.Energy);
    State.MaxEnergy = FMath::Max(0.0f, State.MaxEnergy);
    State.Resonance = FMath::Max(0.0f, State.Resonance);
    State.MaxResonance = FMath::Max(0.0f, State.MaxResonance);

    State.EnergyPercent = State.MaxEnergy > 0.0f
        ? FMath::Clamp(State.Energy / State.MaxEnergy, 0.0f, 1.0f)
        : 0.0f;
    State.ResonancePercent = State.MaxResonance > 0.0f
        ? FMath::Clamp(State.Resonance / State.MaxResonance, 0.0f, 1.0f)
        : 0.0f;

    return State;
}

bool UJumanjiAbilityHUDWidget::ActivateAbilitySlot(EJumanjiAbilitySlot Slot)
{
    if (Slot == EJumanjiAbilitySlot::Passive || !IsValid(CachedAbilitySystem))
    {
        ReportSlotFeedback(
            Slot,
            EJumanjiAbilityHUDFeedback::Unavailable,
            FText::FromString(TEXT("Ten slot nie ma ręcznego przycisku."))
        );
        return false;
    }

    const TArray<FJumanjiAbilityHUDSlotState> SlotStates = GetAbilitySlotStates();
    const FJumanjiAbilityHUDSlotState* State = SlotStates.FindByPredicate(
        [Slot](const FJumanjiAbilityHUDSlotState& Candidate)
        {
            return Candidate.Slot == Slot;
        });

    if (!State || !State->bIsGranted || !State->InputTag.IsValid())
    {
        ReportSlotFeedback(
            Slot,
            EJumanjiAbilityHUDFeedback::Unavailable,
            FText::FromString(TEXT("Ten slot nie jest skonfigurowany."))
        );
        return false;
    }

    APlayerController* PlayerController = GetOwningPlayer();
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return false;
    }

    AJumanjiCharacter* Character = Cast<AJumanjiCharacter>(PlayerController->GetPawn());
    if (!Character)
    {
        ReportSlotFeedback(
            Slot,
            EJumanjiAbilityHUDFeedback::Unavailable,
            FText::FromString(TEXT("Postać nie jest jeszcze gotowa."))
        );
        return false;
    }

    const bool bActivated = const_cast<AJumanjiCharacter*>(Character)
        ->ActivateAbilityByInputTag(State->InputTag);

    if (bActivated)
    {
        ReportSlotFeedback(
            Slot,
            EJumanjiAbilityHUDFeedback::Activated,
            FText::FromString(TEXT("Aktywowano zdolność."))
        );
        return true;
    }

    if (State->bIsOnCooldown)
    {
        ReportSlotFeedback(
            Slot,
            EJumanjiAbilityHUDFeedback::Cooldown,
            FText::FromString(TEXT("Zdolność jest w trakcie odnowienia."))
        );
    }
    else if (!State->bHasRequiredResource)
    {
        ReportSlotFeedback(
            Slot,
            EJumanjiAbilityHUDFeedback::InsufficientResource,
            State->ResourceType == EJumanjiAbilityHUDResource::Resonance
                ? FText::FromString(TEXT("Za mało rezonansu."))
                : FText::FromString(TEXT("Za mało energii."))
        );
    }
    else
    {
        ReportSlotFeedback(
            Slot,
            EJumanjiAbilityHUDFeedback::Unavailable,
            FText::FromString(TEXT("Zdolność nie może być teraz aktywowana."))
        );
    }

    return false;
}

bool UJumanjiAbilityHUDWidget::SetTargetFocus(AActor* Candidate)
{
    APlayerController* PlayerController = GetOwningPlayer();
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return false;
    }

    const APawn* OwnerPawn = PlayerController->GetPawn();

    if (!IsValid(Candidate)
        || !OwnerPawn
        || Candidate == OwnerPawn
        || !FMath::IsFinite(MaxTargetFocusDistance)
        || MaxTargetFocusDistance <= 0.0f
        || FVector::DistSquared(OwnerPawn->GetActorLocation(), Candidate->GetActorLocation())
            > FMath::Square(MaxTargetFocusDistance))
    {
        return false;
    }

    const IAbilitySystemInterface* TargetAbilityInterface =
        Cast<IAbilitySystemInterface>(Candidate);
    if (!TargetAbilityInterface || !TargetAbilityInterface->GetAbilitySystemComponent())
    {
        return false;
    }

    TargetFocus = Candidate;
    OnAbilityTargetFocusChanged(Candidate);
    return true;
}

void UJumanjiAbilityHUDWidget::ClearTargetFocus()
{
    if (!TargetFocus.IsValid())
    {
        return;
    }

    TargetFocus.Reset();
    OnAbilityTargetFocusChanged(nullptr);
}

void UJumanjiAbilityHUDWidget::HandleEnergyChanged(const FOnAttributeChangeData& ChangeData)
{
    RefreshAbilityHUD();
}

void UJumanjiAbilityHUDWidget::HandleResonanceChanged(const FOnAttributeChangeData& ChangeData)
{
    RefreshAbilityHUD();
}

void UJumanjiAbilityHUDWidget::ApplySlotStatesToBoundWidgets(
    const TArray<FJumanjiAbilityHUDSlotState>& SlotStates)
{
    for (const FJumanjiAbilityHUDSlotState& State : SlotStates)
    {
        UJumanjiAbilityHUDSlotWidget* TargetWidget = nullptr;
        switch (State.Slot)
        {
        case EJumanjiAbilitySlot::PrimaryAttack: TargetWidget = PrimarySlotWidget; break;
        case EJumanjiAbilitySlot::Active: TargetWidget = ActiveSlotWidget; break;
        case EJumanjiAbilitySlot::Passive: TargetWidget = PassiveSlotWidget; break;
        case EJumanjiAbilitySlot::Ultimate: TargetWidget = UltimateSlotWidget; break;
        case EJumanjiAbilitySlot::Dodge: TargetWidget = DodgeSlotWidget; break;
        default: break;
        }

        if (TargetWidget)
        {
            TargetWidget->SetSlotState(State);
        }
    }
}

void UJumanjiAbilityHUDWidget::HandleAbilitySlotPressed(EJumanjiAbilitySlot Slot)
{
    ActivateAbilitySlot(Slot);
}

void UJumanjiAbilityHUDWidget::HandleAbilitySlotFeedback(
    EJumanjiAbilitySlot Slot,
    EJumanjiAbilityHUDFeedback Feedback,
    const FText& Message)
{
    UJumanjiAbilityHUDSlotWidget* TargetWidget = nullptr;
    switch (Slot)
    {
    case EJumanjiAbilitySlot::PrimaryAttack: TargetWidget = PrimarySlotWidget; break;
    case EJumanjiAbilitySlot::Active: TargetWidget = ActiveSlotWidget; break;
    case EJumanjiAbilitySlot::Passive: TargetWidget = PassiveSlotWidget; break;
    case EJumanjiAbilitySlot::Ultimate: TargetWidget = UltimateSlotWidget; break;
    case EJumanjiAbilitySlot::Dodge: TargetWidget = DodgeSlotWidget; break;
    default: break;
    }

    if (TargetWidget)
    {
        TargetWidget->PlayFeedback(Feedback);
    }
}

void UJumanjiAbilityHUDWidget::ReportSlotFeedback(
    EJumanjiAbilitySlot Slot,
    EJumanjiAbilityHUDFeedback Feedback,
    const FText& Message)
{
    OnAbilityInputFeedback(Feedback, Message);
    OnAbilitySlotFeedback.Broadcast(Slot, Feedback, Message);
}

void UJumanjiAbilityHUDWidget::RefreshAbilityHUD()
{
    const TArray<FJumanjiAbilityHUDSlotState> SlotStates = GetAbilitySlotStates();
    const FJumanjiAbilityHUDResourceState ResourceState = GetResourceState();
    ApplySlotStatesToBoundWidgets(SlotStates);
    OnAbilityHUDStateChanged(SlotStates, ResourceState);
}
