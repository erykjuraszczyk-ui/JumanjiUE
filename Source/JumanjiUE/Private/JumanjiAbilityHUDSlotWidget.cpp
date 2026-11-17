#include "JumanjiAbilityHUDSlotWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

void UJumanjiAbilityHUDSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &UJumanjiAbilityHUDSlotWidget::HandleSlotButtonClicked);
    }

    if (bHasReceivedState)
    {
        SetSlotState(CurrentState);
    }
}

void UJumanjiAbilityHUDSlotWidget::NativeDestruct()
{
    if (SlotButton)
    {
        SlotButton->OnClicked.RemoveDynamic(this, &UJumanjiAbilityHUDSlotWidget::HandleSlotButtonClicked);
    }

    if (IconLoadHandle.IsValid())
    {
        IconLoadHandle->CancelHandle();
        IconLoadHandle.Reset();
    }
    if (MaterialLoadHandle.IsValid())
    {
        MaterialLoadHandle->CancelHandle();
        MaterialLoadHandle.Reset();
    }
    if (AudioLoadHandle.IsValid())
    {
        AudioLoadHandle->CancelHandle();
        AudioLoadHandle.Reset();
    }

    Super::NativeDestruct();
}

void UJumanjiAbilityHUDSlotWidget::SetSlotState(const FJumanjiAbilityHUDSlotState& NewState)
{
    const bool bIconChanged = CurrentState.Icon.ToSoftObjectPath() != NewState.Icon.ToSoftObjectPath();
    const bool bMaterialChanged = CurrentState.CooldownMaterial.ToSoftObjectPath()
        != NewState.CooldownMaterial.ToSoftObjectPath();

    CurrentState = NewState;
    bHasReceivedState = true;

    if (SlotButton)
    {
        SlotButton->SetIsEnabled(CurrentState.bIsGranted && !CurrentState.bIsPassive);
    }

    UpdateTextAndState();

    if (bIconChanged)
    {
        LoadIconAsync();
    }
    if (bMaterialChanged)
    {
        LoadCooldownMaterialAsync();
    }

    UpdateCooldownMaterial();
}

void UJumanjiAbilityHUDSlotWidget::HandleSlotButtonClicked()
{
    if (!CurrentState.bIsGranted || CurrentState.bIsPassive)
    {
        return;
    }

    OnSlotPressed.Broadcast(CurrentState.Slot);
}

void UJumanjiAbilityHUDSlotWidget::LoadIconAsync()
{
    RequestedIconPath = CurrentState.Icon.ToSoftObjectPath();

    if (IconLoadHandle.IsValid())
    {
        IconLoadHandle->CancelHandle();
        IconLoadHandle.Reset();
    }

    if (RequestedIconPath.IsNull())
    {
        if (AbilityIcon)
        {
            // Keep the designer-assigned placeholder brush.
            AbilityIcon->SetOpacity(0.45f);
        }
        return;
    }

    if (UTexture2D* LoadedIcon = CurrentState.Icon.Get())
    {
        if (AbilityIcon)
        {
            AbilityIcon->SetBrushFromTexture(LoadedIcon, true);
            AbilityIcon->SetOpacity(1.0f);
        }
        return;
    }

    if (AbilityIcon)
    {
        // The widget's default brush is the placeholder while the icon streams.
        AbilityIcon->SetOpacity(0.45f);
    }

    const FSoftObjectPath PathToLoad = RequestedIconPath;
    const TWeakObjectPtr<UJumanjiAbilityHUDSlotWidget> WeakThis(this);
    IconLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        PathToLoad,
        FStreamableDelegate::CreateLambda([WeakThis, PathToLoad]()
        {
            UJumanjiAbilityHUDSlotWidget* Widget = WeakThis.Get();
            if (!Widget || Widget->RequestedIconPath != PathToLoad || !Widget->AbilityIcon)
            {
                return;
            }

            UTexture2D* LoadedIcon = Cast<UTexture2D>(PathToLoad.ResolveObject());
            if (LoadedIcon)
            {
                Widget->AbilityIcon->SetBrushFromTexture(LoadedIcon, true);
                Widget->AbilityIcon->SetOpacity(1.0f);
            }
        })
    );
}

void UJumanjiAbilityHUDSlotWidget::LoadCooldownMaterialAsync()
{
    RequestedMaterialPath = CurrentState.CooldownMaterial.ToSoftObjectPath();
    CooldownDynamicMaterial = nullptr;

    if (MaterialLoadHandle.IsValid())
    {
        MaterialLoadHandle->CancelHandle();
        MaterialLoadHandle.Reset();
    }

    if (RequestedMaterialPath.IsNull())
    {
        UpdateCooldownMaterial();
        return;
    }

    if (UMaterialInterface* LoadedMaterial = CurrentState.CooldownMaterial.Get())
    {
        CooldownDynamicMaterial = UMaterialInstanceDynamic::Create(LoadedMaterial, this);
        if (CooldownOverlay && CooldownDynamicMaterial)
        {
            CooldownOverlay->SetBrushFromMaterial(CooldownDynamicMaterial);
        }
        UpdateCooldownMaterial();
        return;
    }

    const FSoftObjectPath PathToLoad = RequestedMaterialPath;
    const TWeakObjectPtr<UJumanjiAbilityHUDSlotWidget> WeakThis(this);
    MaterialLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        PathToLoad,
        FStreamableDelegate::CreateLambda([WeakThis, PathToLoad]()
        {
            UJumanjiAbilityHUDSlotWidget* Widget = WeakThis.Get();
            if (!Widget || Widget->RequestedMaterialPath != PathToLoad)
            {
                return;
            }

            UMaterialInterface* LoadedMaterial = Cast<UMaterialInterface>(PathToLoad.ResolveObject());
            if (!LoadedMaterial)
            {
                return;
            }

            Widget->CooldownDynamicMaterial = UMaterialInstanceDynamic::Create(LoadedMaterial, Widget);
            if (Widget->CooldownOverlay && Widget->CooldownDynamicMaterial)
            {
                Widget->CooldownOverlay->SetBrushFromMaterial(Widget->CooldownDynamicMaterial);
            }
            Widget->UpdateCooldownMaterial();
        })
    );
}

void UJumanjiAbilityHUDSlotWidget::UpdateCooldownMaterial()
{
    const float CooldownPercent = FMath::Clamp(CurrentState.CooldownPercent, 0.0f, 1.0f);
    const float DisabledAmount = (!CurrentState.bIsGranted || !CurrentState.bHasRequiredResource)
        ? 1.0f
        : 0.0f;

    if (CooldownDynamicMaterial)
    {
        CooldownDynamicMaterial->SetScalarParameterValue(CooldownPercentParameter, CooldownPercent);
        CooldownDynamicMaterial->SetVectorParameterValue(AccentColorParameter, CurrentState.AccentColor);
        CooldownDynamicMaterial->SetScalarParameterValue(DisabledAmountParameter, DisabledAmount);
    }

    if (CooldownFallbackBar)
    {
        CooldownFallbackBar->SetPercent(CooldownPercent);
        CooldownFallbackBar->SetVisibility(
            CurrentState.bIsOnCooldown && !CooldownDynamicMaterial
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed
        );
    }

    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(
            CurrentState.bIsOnCooldown && CooldownDynamicMaterial
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed
        );
    }
}

void UJumanjiAbilityHUDSlotWidget::UpdateTextAndState()
{
    if (AbilityNameText)
    {
        AbilityNameText->SetText(CurrentState.DisplayName);
    }

    if (CooldownText)
    {
        CooldownText->SetText(
            CurrentState.bIsOnCooldown
                ? FText::AsNumber(FMath::CeilToInt(CurrentState.CooldownRemaining))
                : FText::GetEmpty()
        );
        CooldownText->SetVisibility(
            CurrentState.bIsOnCooldown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed
        );
    }

    if (ResourceCostText)
    {
        FText CostText = FText::GetEmpty();
        if (CurrentState.ResourceType != EJumanjiAbilityHUDResource::None
            && CurrentState.RequiredResource > 0.0f)
        {
            const TCHAR* ResourceName = CurrentState.ResourceType == EJumanjiAbilityHUDResource::Resonance
                ? TEXT("Rezonans")
                : TEXT("Energia");
            CostText = FText::FromString(FString::Printf(
                TEXT("%s: %.0f"),
                ResourceName,
                CurrentState.RequiredResource
            ));
        }
        ResourceCostText->SetText(CostText);
        ResourceCostText->SetVisibility(
            CostText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible
        );
    }

    if (SlotStateText)
    {
        FText StateText = FText::GetEmpty();
        if (!CurrentState.bIsGranted)
        {
            StateText = FText::FromString(TEXT("Niedostępna"));
        }
        else if (CurrentState.bIsPassive)
        {
            StateText = FText::FromString(TEXT("Pasywna"));
        }
        else if (CurrentState.bIsOnCooldown)
        {
            StateText = FText::FromString(TEXT("Odnowienie"));
        }
        else if (!CurrentState.bHasRequiredResource)
        {
            StateText = FText::FromString(TEXT("Brak zasobu"));
        }
        SlotStateText->SetText(StateText);
        SlotStateText->SetVisibility(
            StateText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible
        );
    }

    UpdateCooldownMaterial();
}

void UJumanjiAbilityHUDSlotWidget::PlayFeedback(EJumanjiAbilityHUDFeedback Feedback)
{
    if (Feedback == EJumanjiAbilityHUDFeedback::Activated && ActivationPulseAnimation)
    {
        PlayAnimation(ActivationPulseAnimation);
    }

    const TSoftObjectPtr<USoundBase>* SoundToPlay = nullptr;
    switch (Feedback)
    {
    case EJumanjiAbilityHUDFeedback::Activated:
        SoundToPlay = &CurrentState.ActivatedSound;
        break;
    case EJumanjiAbilityHUDFeedback::InsufficientResource:
        SoundToPlay = &CurrentState.InsufficientResourceSound;
        break;
    case EJumanjiAbilityHUDFeedback::Cooldown:
        SoundToPlay = &CurrentState.CooldownSound;
        break;
    case EJumanjiAbilityHUDFeedback::Unavailable:
        SoundToPlay = &CurrentState.UnavailableSound;
        break;
    default:
        break;
    }

    if (SoundToPlay)
    {
        PlaySoundAsync(*SoundToPlay);
    }
}

void UJumanjiAbilityHUDSlotWidget::PlaySoundAsync(const TSoftObjectPtr<USoundBase>& Sound)
{
    if (AudioLoadHandle.IsValid())
    {
        AudioLoadHandle->CancelHandle();
        AudioLoadHandle.Reset();
    }

    if (USoundBase* LoadedSound = Sound.Get())
    {
        UGameplayStatics::PlaySound2D(this, LoadedSound);
        return;
    }

    const FSoftObjectPath SoundPath = Sound.ToSoftObjectPath();
    RequestedAudioPath = SoundPath;
    if (SoundPath.IsNull())
    {
        return;
    }

    const TWeakObjectPtr<UJumanjiAbilityHUDSlotWidget> WeakThis(this);
    AudioLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        SoundPath,
        FStreamableDelegate::CreateLambda([WeakThis, SoundPath]()
        {
            UJumanjiAbilityHUDSlotWidget* Widget = WeakThis.Get();
            if (!Widget || Widget->RequestedAudioPath != SoundPath)
            {
                return;
            }

            if (USoundBase* LoadedSound = Cast<USoundBase>(SoundPath.ResolveObject()))
            {
                UGameplayStatics::PlaySound2D(Widget, LoadedSound);
            }
        })
    );
}
