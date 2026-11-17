#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JumanjiGameplayAbility.h"
#include "JumanjiAbilityHUDWidget.generated.h"

class UJumanjiAbilitySystemComponent;
class UJumanjiAttributeSet;
class UTexture2D;
class UMaterialInterface;
class USoundBase;
class UJumanjiAbilityHUDSlotWidget;

UENUM(BlueprintType)
enum class EJumanjiAbilityHUDFeedback : uint8
{
    Activated UMETA(DisplayName = "Activated"),
    InsufficientResource UMETA(DisplayName = "Insufficient Resource"),
    Cooldown UMETA(DisplayName = "On Cooldown"),
    Unavailable UMETA(DisplayName = "Unavailable")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FJumanjiAbilitySlotFeedbackEvent,
    EJumanjiAbilitySlot, Slot,
    EJumanjiAbilityHUDFeedback, Feedback,
    const FText&, Message
);

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiAbilityHUDResourceState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float Energy = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float MaxEnergy = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float EnergyPercent = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float Resonance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float MaxResonance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float ResonancePercent = 0.0f;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiAbilityHUDSlotState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    EJumanjiAbilitySlot Slot = EJumanjiAbilitySlot::PrimaryAttack;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FName AbilityId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FGameplayTag InputTag;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FLinearColor AccentColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FLinearColor SecondaryColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    TSoftObjectPtr<UMaterialInterface> CooldownMaterial;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    TSoftObjectPtr<USoundBase> ActivatedSound;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    TSoftObjectPtr<USoundBase> InsufficientResourceSound;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    TSoftObjectPtr<USoundBase> CooldownSound;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    TSoftObjectPtr<USoundBase> UnavailableSound;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FGameplayTag PresentationCueTag;

    /** Reference only; do not use it as a runtime cooldown timer. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float CooldownSecondsReference = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    EJumanjiAbilityHUDResource ResourceType = EJumanjiAbilityHUDResource::None;

    /** Presentation metadata only. GAS Gameplay Effects remain authoritative for cost. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float RequiredResource = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float CurrentResource = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float CooldownRemaining = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float CooldownDuration = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    float CooldownPercent = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    bool bIsGranted = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    bool bIsPassive = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    bool bIsOnCooldown = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    bool bHasRequiredResource = true;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    bool bCanActivate = false;
};

/**
 * Local-only presentation layer for the five Great Spirit ability slots.
 * The widget never spends resources or applies effects; GAS owns activation,
 * cooldown and cost validation.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiAbilityHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability HUD")
    TArray<FJumanjiAbilityHUDSlotState> GetAbilitySlotStates() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability HUD")
    FJumanjiAbilityHUDResourceState GetResourceState() const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD")
    bool ActivateAbilitySlot(EJumanjiAbilitySlot Slot);

    /** Sets a local HUD focus marker. This is not trusted gameplay target data. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD")
    bool SetTargetFocus(AActor* Candidate);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD")
    void ClearTargetFocus();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability HUD")
    AActor* GetTargetFocus() const { return TargetFocus.Get(); }

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Ability HUD")
    FJumanjiAbilitySlotFeedbackEvent OnAbilitySlotFeedback;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability HUD", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float RefreshInterval = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability HUD", meta = (ClampMin = "100.0", ClampMax = "5000.0"))
    float MaxTargetFocusDistance = 2500.0f;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UJumanjiAbilityHUDSlotWidget> PrimarySlotWidget;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UJumanjiAbilityHUDSlotWidget> ActiveSlotWidget;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UJumanjiAbilityHUDSlotWidget> PassiveSlotWidget;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UJumanjiAbilityHUDSlotWidget> UltimateSlotWidget;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UJumanjiAbilityHUDSlotWidget> DodgeSlotWidget;

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Ability HUD")
    void OnAbilityHUDStateChanged(
        const TArray<FJumanjiAbilityHUDSlotState>& SlotStates,
        const FJumanjiAbilityHUDResourceState& ResourceState
    );

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Ability HUD")
    void OnAbilityInputFeedback(EJumanjiAbilityHUDFeedback Feedback, const FText& Message);

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Ability HUD")
    void OnAbilityTargetFocusChanged(AActor* TargetActor);

private:
    void TryBindAbilitySystem();
    void UnbindAbilitySystem();
    void RefreshAbilityHUD();
    void ApplySlotStatesToBoundWidgets(const TArray<FJumanjiAbilityHUDSlotState>& SlotStates);
    void ReportSlotFeedback(EJumanjiAbilitySlot Slot, EJumanjiAbilityHUDFeedback Feedback, const FText& Message);

    UFUNCTION()
    void HandleAbilitySlotPressed(EJumanjiAbilitySlot Slot);

    UFUNCTION()
    void HandleAbilitySlotFeedback(EJumanjiAbilitySlot Slot, EJumanjiAbilityHUDFeedback Feedback, const FText& Message);

    void HandleEnergyChanged(const FOnAttributeChangeData& ChangeData);
    void HandleResonanceChanged(const FOnAttributeChangeData& ChangeData);

    FDelegateHandle EnergyChangedHandle;
    FDelegateHandle ResonanceChangedHandle;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiAbilitySystemComponent> CachedAbilitySystem;

    TWeakObjectPtr<AActor> TargetFocus;
    float RefreshAccumulator = 0.0f;
};
