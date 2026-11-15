#include "JumanjiMeleeAttackAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "JumanjiAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UJumanjiMeleeAttackAbility::UJumanjiMeleeAttackAbility()
{
    // The server owns the trace and damage decision. Clients request activation
    // through GAS; no client-side hit result is accepted as authoritative.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UJumanjiMeleeAttackAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (ActorInfo->AvatarActor->HasAuthority())
    {
        ApplyMeleeDamage(ActorInfo);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UJumanjiMeleeAttackAbility::ApplyMeleeDamage(
    const FGameplayAbilityActorInfo* ActorInfo)
{
    if (!DamageEffectClass || !ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return;
    }

    AActor* AvatarActor = ActorInfo->AvatarActor.Get();
    UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

    if (!World || !AvatarActor || !SourceASC || !AvatarActor->HasAuthority())
    {
        return;
    }

    const FGameplayTag DamageTag =
        FGameplayTag::RequestGameplayTag(DamageSetByCallerTagName, false);

    if (!DamageTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("JUMANJI: missing SetByCaller tag for melee damage."));
        return;
    }

    const FVector Start = AvatarActor->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
    const FVector End = Start + AvatarActor->GetActorForwardVector() * TraceDistance;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JumanjiMeleeAttack), false, AvatarActor);
    QueryParams.AddIgnoredActor(AvatarActor);

    TArray<FHitResult> HitResults;
    const bool bHitAnything = World->SweepMultiByChannel(
        HitResults,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(TraceRadius),
        QueryParams
    );

    if (!bHitAnything)
    {
        return;
    }

    const float SourceAttackPower =
        SourceASC->GetNumericAttribute(UJumanjiAttributeSet::GetAttackPowerAttribute());

    TSet<AActor*> DamagedActors;

    for (const FHitResult& Hit : HitResults)
    {
        AActor* TargetActor = Hit.GetActor();
        if (!IsValid(TargetActor) || TargetActor == AvatarActor || DamagedActors.Contains(TargetActor))
        {
            continue;
        }

        UAbilitySystemComponent* TargetASC =
            UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

        if (!TargetASC || TargetASC == SourceASC)
        {
            continue;
        }

        const float TargetHealth =
            TargetASC->GetNumericAttribute(UJumanjiAttributeSet::GetHealthAttribute());

        if (TargetHealth <= 0.0f)
        {
            continue;
        }

        const float TargetDefense =
            TargetASC->GetNumericAttribute(UJumanjiAttributeSet::GetDefenseAttribute());

        const float RawDamage = BaseDamage + (SourceAttackPower * AttackPowerCoefficient);
        const float FinalDamage = FMath::Max(1.0f, RawDamage - (TargetDefense * DefenseCoefficient));

        FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
        EffectContext.AddSourceObject(AvatarActor);
        EffectContext.AddHitResult(Hit, true);

        FGameplayEffectSpecHandle SpecHandle =
            SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(Handle, ActorInfo), EffectContext);

        if (!SpecHandle.IsValid())
        {
            continue;
        }

        // Health modifiers in GE_Damage_Base use this negative SetByCaller value.
        SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, -FinalDamage);
        TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

        const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(
            FName(TEXT("Event.Combat.Hit")), false);

        if (HitEventTag.IsValid())
        {
            FGameplayEventData HitEvent;
            HitEvent.EventTag = HitEventTag;
            HitEvent.Instigator = AvatarActor;
            HitEvent.Target = TargetActor;
            HitEvent.EventMagnitude = FinalDamage;
            HitEvent.ContextHandle = EffectContext;
            TargetASC->HandleGameplayEvent(HitEventTag, &HitEvent);
        }

        DamagedActors.Add(TargetActor);
    }
}
