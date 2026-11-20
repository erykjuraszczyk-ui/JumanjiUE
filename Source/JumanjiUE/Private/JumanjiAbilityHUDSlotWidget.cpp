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

    if (AbilityIcon && !bHasCapturedPlaceholderBrush)
    {
        DesignerPlaceholderBrush = AbilityIcon->GetBrush();
        bHasCapturedPlaceholderBrush = true;
    }

    if (!bHasCapturedDefaultTextColors)
    {
        if (AbilityNameText) DefaultAbilityNameColor = AbilityNameText->GetColorAndOpacity();
        if (CooldownText) DefaultCooldownTextColor = CooldownText->GetColorAndOpacity();
        if (ResourceCostText) DefaultResourceCostColor = ResourceCostText->GetColorAndOpacity();
        if (SlotStateText) DefaultSlotStateColor = SlotStateText->GetColorAndOpacity();
        bHasCapturedDefaultTextColors = true;
    }

    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &UJumanjiAbilityHUDSlotWidget::HandleSlotButtonClicked);
    }

    if (bHasReceivedState)
    {
        // NativeConstruct may run after the parent has already pushed the
        // first snapshot. Rebind streamed assets explicitly on every rebuild.
        UpdateTextAndState();
        LoadIconAsync();
        LoadCooldownMaterialAsync();
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

void UJumanjiAbilityHUDSlotWidget::ApplyAccessibilityOptions(
    float InTouchTargetScale,
    bool bInHighContrast,
    bool bInReduceMotion)
{
    TouchTargetScale = FMath::Clamp(
        FMath::IsFinite(InTouchTargetScale) ? InTouchTargetScale : 1.0f,
        0.85f,
        1.5f
    );
    bHighContrastMode = bInHighContrast;
    bReduceMotion = bInReduceMotion;

    if (SlotButton)
    {
        // UMG render transforms are applied to the visual and its hit-test geometry.
        SlotButton->SetRenderScale(FVector2D(TouchTargetScale, TouchTargetScale));
    }

    if (bReduceMotion && ActivationPulseAnimation && IsAnimationPlaying(ActivationPulseAnimation))
    {
        StopAnimation(ActivationPulseAnimation);
    }

    UpdateAccessibilityPresentation();
}

void UJumanjiAbilityHUDSlotWidget::UpdateAccessibilityPresentation()
{
    const FSlateColor HighContrastPrimary(FLinearColor::White);
    const FSlateColor HighContrastSecondary(FLinearColor(1.0f, 0.90f, 0.45f, 1.0f));

    if (AbilityNameText)
    {
        AbilityNameText->SetColorAndOpacity(
            bHighContrastMode ? HighContrastPrimary : DefaultAbilityNameColor
        );
    }
    if (CooldownText)
    {
        CooldownText->SetColorAndOpacity(
            bHighContrastMode ? HighContrastPrimary : DefaultCooldownTextColor
        );
    }
    if (ResourceCostText)
    {
        ResourceCostText->SetColorAndOpacity(
            bHighContrastMode ? HighContrastSecondary : DefaultResourceCostColor
        );
    }
    if (SlotStateText)
    {
        SlotStateText->SetColorAndOpacity(
            bHighContrastMode ? HighContrastSecondary : DefaultSlotStateColor
        );
    }
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
            AbilityIcon->SetBrush(DesignerPlaceholderBrush);
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
        // Restore the original designer brush while the icon streams.
        AbilityIcon->SetBrush(DesignerPlaceholderBrush);
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
        const bool bNeedsMaterialOverlay = CurrentState.bIsOnCooldown
            || (CurrentState.bIsGranted && !CurrentState.bHasRequiredResource);
        CooldownOverlay->SetVisibility(
            bNeedsMaterialOverlay && CooldownDynamicMaterial
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
    if (Feedback == EJumanjiAbilityHUDFeedback::Activated
        && ActivationPulseAnimation
        && !bReduceMotion)
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
    RequestedAudioPath = Sound.ToSoftObjectPath();

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

    const FSoftObjectPath SoundPath = RequestedAudioPath;
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
