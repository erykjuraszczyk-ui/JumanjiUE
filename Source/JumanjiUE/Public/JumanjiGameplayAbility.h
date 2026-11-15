#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "JumanjiGameplayAbility.generated.h"

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

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability")
    FGameplayTag InputTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability")
    EJumanjiAbilitySlot AbilitySlot = EJumanjiAbilitySlot::Active;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability")
    bool bActivateOnGranted = false;
};
