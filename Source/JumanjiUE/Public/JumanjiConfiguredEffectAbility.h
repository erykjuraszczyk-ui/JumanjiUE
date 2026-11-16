#pragma once

#include "CoreMinimal.h"
#include "JumanjiGameplayAbility.h"
#include "JumanjiConfiguredEffectAbility.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilitySystemComponent;

UENUM(BlueprintType)
enum class EJumanjiAbilityTargetingMode : uint8
{
    /** Apply only the configured self effect. */
    Self UMETA(DisplayName = "Self"),

    /** Server-side sphere sweep in front of the owning character. */
    ForwardSweep UMETA(DisplayName = "Forward Sweep"),

    /** Server-side sphere overlap around the owning character. */
    RadialPulse UMETA(DisplayName = "Radial Pulse"),

    /** Move the owning character forward and optionally apply a self effect. */
    Dash UMETA(DisplayName = "Dash")
};

UENUM(BlueprintType)
enum class EJumanjiAbilityTargetTeam : uint8
{
    /** Only the owning actor. */
    SelfOnly UMETA(DisplayName = "Self Only"),

    /** Actors on the same prototype PvE side as the owner. */
    Friendly UMETA(DisplayName = "Friendly"),

    /** Actors on the opposing prototype PvE side. */
    Hostile UMETA(DisplayName = "Hostile"),

    /** Any actor with a compatible Ability System Component. */
    Any UMETA(DisplayName = "Any")
};

/**
 * Reusable server-authoritative execution base for Great Spirit abilities.
 *
 * Designers create Blueprint children and configure the slot/input contract,
 * cost/cooldown Gameplay Effects, targeting, target/self effects, montage and
 * Gameplay Cue. Effect amounts are supplied through SetByCaller tags.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiConfiguredEffectAbility : public UJumanjiGameplayAbility
{
    GENERATED_BODY()

public:
    UJumanjiConfiguredEffectAbility();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    void ExecuteConfiguredAction(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo);

    bool IsTargetAllowed(const AActor* SourceActor, const AActor* TargetActor) const;

    bool ApplyConfiguredEffect(
        UAbilitySystemComponent* SourceASC,
        UAbilitySystemComponent* TargetASC,
        AActor* SourceActor,
        AActor* TargetActor,
        const FHitResult* Hit,
        TSubclassOf<UGameplayEffect> EffectClass,
        FGameplayTag MagnitudeTag,
        float BaseMagnitude,
        float AttackPowerCoefficient,
        float& OutAppliedMagnitude,
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo) const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Targeting")
    EJumanjiAbilityTargetingMode TargetingMode = EJumanjiAbilityTargetingMode::ForwardSweep;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Targeting")
    EJumanjiAbilityTargetTeam TargetTeam = EJumanjiAbilityTargetTeam::Hostile;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Targeting", meta = (ClampMin = "25.0"))
    float TraceDistance = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Targeting", meta = (ClampMin = "5.0"))
    float TraceRadius = 90.0f;

    /** Zero means no explicit target cap. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Targeting", meta = (ClampMin = "0"))
    int32 MaxTargets = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Effect")
    TSubclassOf<UGameplayEffect> TargetEffectClass;

    /** Must match a SetByCaller modifier in TargetEffectClass when one is used. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Effect", meta = (Categories = "Data"))
    FGameplayTag TargetMagnitudeTag;

    /** Signed magnitude: negative for damage, positive for healing or a positive attribute modifier. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Effect")
    float TargetBaseMagnitude = -10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Effect", meta = (ClampMin = "0.0"))
    float TargetAttackPowerCoefficient = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Self Effect")
    TSubclassOf<UGameplayEffect> SelfEffectClass;

    /** Must match a SetByCaller modifier in SelfEffectClass when one is used. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Self Effect", meta = (Categories = "Data"))
    FGameplayTag SelfMagnitudeTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Self Effect")
    float SelfBaseMagnitude = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Self Effect", meta = (ClampMin = "0.0"))
    float SelfAttackPowerCoefficient = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Movement", meta = (ClampMin = "0.0"))
    float DashSpeed = 850.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Movement")
    float DashVerticalImpulse = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Presentation")
    TObjectPtr<UAnimMontage> ActivationMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Presentation", meta = (ClampMin = "0.1"))
    float MontagePlayRate = 1.0f;

    /** Optional cue executed on each successfully affected target. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Ability|Presentation", meta = (Categories = "GameplayCue"))
    FGameplayTag GameplayCueTag;

    /** Blueprint extension point for additional server-side per-target behavior. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Ability")
    void BP_OnConfiguredTargetApplied(AActor* TargetActor, float AppliedMagnitude);
};
