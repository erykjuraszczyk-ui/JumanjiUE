#pragma once

#include "CoreMinimal.h"
#include "JumanjiGameplayAbility.h"
#include "JumanjiMeleeAttackAbility.generated.h"

class UGameplayEffect;

/**
 * Server-authoritative short-range melee ability.
 *
 * The ability performs a forward sphere sweep on the server and applies a
 * configured Gameplay Effect to each unique target with an Ability System.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiMeleeAttackAbility : public UJumanjiGameplayAbility
{
    GENERATED_BODY()

public:
    UJumanjiMeleeAttackAbility();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    void ApplyMeleeDamage(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Combat", meta = (ClampMin = "25.0"))
    float TraceDistance = 220.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Combat", meta = (ClampMin = "5.0"))
    float TraceRadius = 90.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Combat", meta = (ClampMin = "0.0"))
    float BaseDamage = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Combat", meta = (ClampMin = "0.0"))
    float AttackPowerCoefficient = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Combat", meta = (ClampMin = "0.0"))
    float DefenseCoefficient = 0.75f;

    /** Gameplay Effect must apply a SetByCaller modifier to Health using Data.Damage. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Combat")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Combat")
    FName DamageSetByCallerTagName = FName(TEXT("Data.Damage"));
};
