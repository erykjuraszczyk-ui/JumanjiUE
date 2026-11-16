#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "JumanjiGameplayAbility.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EJumanjiAbilityHUDResource : uint8
{
    None UMETA(DisplayName = "None"),
    Energy UMETA(DisplayName = "Energy"),
    Resonance UMETA(DisplayName = "Resonance")
};

UENUM(BlueprintType)
enum class EJumanjiAbilitySlot : uint8
{
    PrimaryAttack UMETA(DisplayName = "Primary Attack"),
    Active UMETA(DisplayName = "Active Ability"),
    Passive UMETA(DisplayName = "Passive"),
    Ultimate UMETA(DisplayName = "Ultimate"),
    Dodge UMETA(DisplayName = "Dodge")
};

/**
 * Shared base for Great Spirit abilities.
 * Designers assign input tags, cooldown Gameplay Effects, cost Gameplay Effects,
 * activation/block tags and animation/effect tasks in derived Blueprint classes.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UJumanjiGameplayAbility();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability")
    FGameplayTag GetInputTag() const { return InputTag; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability")
    EJumanjiAbilitySlot GetAbilitySlot() const { return AbilitySlot; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability")
    bool ShouldActivateOnGranted() const { return bActivateOnGranted; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability|HUD")
    FText GetHUDDisplayName() const { return HUDDisplayName; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability|HUD")
    FText GetHUDDescription() const { return HUDDescription; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability|HUD")
    TSoftObjectPtr<UTexture2D> GetHUDIcon() const { return HUDIcon; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability|HUD")
    EJumanjiAbilityHUDResource GetHUDResourceType() const { return HUDResourceType; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability|HUD")
    float GetHUDResourceCost() const { return HUDResourceCost; }

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability")
    FGameplayTag InputTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability")
    EJumanjiAbilitySlot AbilitySlot = EJumanjiAbilitySlot::Active;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability")
    bool bActivateOnGranted = false;

    /** Presentation metadata; mirror the configured Gameplay Effect cost. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|HUD")
    FText HUDDisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|HUD", meta = (MultiLine = "true"))
    FText HUDDescription;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|HUD")
    TSoftObjectPtr<UTexture2D> HUDIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|HUD")
    EJumanjiAbilityHUDResource HUDResourceType = EJumanjiAbilityHUDResource::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|HUD", meta = (ClampMin = "0.0"))
    float HUDResourceCost = 0.0f;
};
