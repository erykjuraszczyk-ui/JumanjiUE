#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JumanjiGameplayAbility.h"
#include "JumanjiAbilityHUDWidget.generated.h"

class UJumanjiAbilitySystemComponent;
class UJumanjiAttributeSet;
class UTexture2D;

UENUM(BlueprintType)
enum class EJumanjiAbilityHUDFeedback : uint8
{
    Activated UMETA(DisplayName = "Activated"),
    InsufficientResource UMETA(DisplayName = "Insufficient Resource"),
    Cooldown UMETA(DisplayName = "On Cooldown"),
    Unavailable UMETA(DisplayName = "Unavailable")
};

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
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    FGameplayTag InputTag;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability HUD")
    TSoftObjectPtr<UTexture2D> Icon;

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

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability HUD", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float RefreshInterval = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability HUD", meta = (ClampMin = "100.0", ClampMax = "5000.0"))
    float MaxTargetFocusDistance = 2500.0f;

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

    void HandleEnergyChanged(const FOnAttributeChangeData& ChangeData);
    void HandleResonanceChanged(const FOnAttributeChangeData& ChangeData);

    FDelegateHandle EnergyChangedHandle;
    FDelegateHandle ResonanceChangedHandle;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiAbilitySystemComponent> CachedAbilitySystem;

    TWeakObjectPtr<AActor> TargetFocus;
    float RefreshAccumulator = 0.0f;
};
