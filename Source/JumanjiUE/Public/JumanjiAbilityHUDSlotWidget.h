#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "JumanjiAbilityHUDWidget.h"
#include "JumanjiAbilityHUDSlotWidget.generated.h"

class UButton;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProgressBar;
class USoundBase;
class UTextBlock;
class UTexture2D;
class UWidgetAnimation;
class FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiAbilitySlotPressedEvent,
    EJumanjiAbilitySlot,
    Slot
);

/**
 * Reusable visual and touch-input component for one Great Spirit ability slot.
 * Gameplay remains owned by the parent HUD and GAS.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiAbilityHUDSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD|Slot")
    void SetSlotState(const FJumanjiAbilityHUDSlotState& NewState);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability HUD|Slot")
    FJumanjiAbilityHUDSlotState GetSlotState() const { return CurrentState; }

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD|Slot")
    void PlayFeedback(EJumanjiAbilityHUDFeedback Feedback);

    /** Applies local touch-target, contrast and motion preferences. Does not affect gameplay. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD|Accessibility")
    void ApplyAccessibilityOptions(float TouchTargetScale, bool bHighContrast, bool bReduceMotion);

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Ability HUD|Slot")
    FJumanjiAbilitySlotPressedEvent OnSlotPressed;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SlotButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> AbilityIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> CooldownOverlay;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> CooldownFallbackBar;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> AbilityNameText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CooldownText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ResourceCostText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SlotStateText;

    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> ActivationPulseAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability HUD|Slot")
    FName CooldownPercentParameter = TEXT("CooldownPercent");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability HUD|Slot")
    FName AccentColorParameter = TEXT("AccentColor");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability HUD|Slot")
    FName DisabledAmountParameter = TEXT("DisabledAmount");

private:
    UFUNCTION()
    void HandleSlotButtonClicked();

    void LoadIconAsync();
    void LoadCooldownMaterialAsync();
    void PlaySoundAsync(const TSoftObjectPtr<USoundBase>& Sound);
    void UpdateCooldownMaterial();
    void UpdateTextAndState();
    void UpdateAccessibilityPresentation();

    UPROPERTY(Transient)
    FJumanjiAbilityHUDSlotState CurrentState;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CooldownDynamicMaterial;

    TSharedPtr<FStreamableHandle> IconLoadHandle;
    TSharedPtr<FStreamableHandle> MaterialLoadHandle;
    TSharedPtr<FStreamableHandle> AudioLoadHandle;

    FSlateBrush DesignerPlaceholderBrush;
    FSoftObjectPath RequestedIconPath;
    FSoftObjectPath RequestedMaterialPath;
    FSoftObjectPath RequestedAudioPath;
    bool bHasReceivedState = false;
    bool bHasCapturedPlaceholderBrush = false;
    bool bHasCapturedDefaultTextColors = false;
    bool bHighContrastMode = false;
    bool bReduceMotion = false;
    float TouchTargetScale = 1.0f;
    FSlateColor DefaultAbilityNameColor;
    FSlateColor DefaultCooldownTextColor;
    FSlateColor DefaultResourceCostColor;
    FSlateColor DefaultSlotStateColor;
};
