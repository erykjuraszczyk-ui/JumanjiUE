#include "JumanjiConfiguredEffectAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "JumanjiAttributeSet.h"
#include "JumanjiEnemyCharacter.h"

namespace
{
    struct FJumanjiAbilityTargetCandidate
    {
        AActor* Actor = nullptr;
        FHitResult Hit;
        bool bHasHit = false;
    };
}

UJumanjiConfiguredEffectAbility::UJumanjiConfiguredEffectAbility()
{
    // Target acquisition and effect application are resolved by the server.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UJumanjiConfiguredEffectAbility::ActivateAbility(
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
        ExecuteConfiguredAction(Handle, ActorInfo);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UJumanjiConfiguredEffectAbility::ExecuteConfiguredAction(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo)
{
    AActor* SourceActor = ActorInfo && ActorInfo->AvatarActor.IsValid()
        ? ActorInfo->AvatarActor.Get()
        : nullptr;
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

    if (!SourceActor || !SourceASC || !SourceActor->HasAuthority())
    {
        return;
    }

    if (ActivationMontage)
    {
        if (ACharacter* Character = Cast<ACharacter>(SourceActor))
        {
            Character->PlayAnimMontage(ActivationMontage, MontagePlayRate);
        }
    }

    if (TargetingMode == EJumanjiAbilityTargetingMode::Dash)
    {
        if (ACharacter* Character = Cast<ACharacter>(SourceActor))
        {
            const FVector LaunchVelocity =
                (SourceActor->GetActorForwardVector() * DashSpeed)
                + (FVector::UpVector * DashVerticalImpulse);
            Character->LaunchCharacter(LaunchVelocity, true, true);
        }
    }

    // Self effects are independent from target acquisition. This supports
    // personal buffs on an area ability as well as self-only abilities.
    if (SelfEffectClass)
    {
        ApplyConfiguredEffect(
            SourceASC,
            SourceASC,
            SourceActor,
            SourceActor,
            nullptr,
            SelfEffectClass,
            SelfMagnitudeTag,
            SelfBaseMagnitude,
            SelfAttackPowerCoefficient,
            Handle,
            ActorInfo);
    }

    if (!TargetEffectClass || TargetingMode == EJumanjiAbilityTargetingMode::Dash)
    {
        return;
    }

    UWorld* World = SourceActor->GetWorld();
    if (!World)
    {
        return;
    }

    TArray<FJumanjiAbilityTargetCandidate> Candidates;

    if (TargetingMode == EJumanjiAbilityTargetingMode::Self)
    {
        FJumanjiAbilityTargetCandidate Candidate;
        Candidate.Actor = SourceActor;
        Candidates.Add(Candidate);
    }
    else if (TargetingMode == EJumanjiAbilityTargetingMode::ForwardSweep)
    {
        const FVector Start = SourceActor->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
        const FVector End = Start + SourceActor->GetActorForwardVector() * TraceDistance;

        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JumanjiConfiguredAbility), false, SourceActor);
        QueryParams.AddIgnoredActor(SourceActor);

        TArray<FHitResult> HitResults;
        World->SweepMultiByChannel(
            HitResults,
            Start,
            End,
            FQuat::Identity,
            ECC_Pawn,
            FCollisionShape::MakeSphere(TraceRadius),
            QueryParams);

        for (const FHitResult& Hit : HitResults)
        {
            FJumanjiAbilityTargetCandidate Candidate;
            Candidate.Actor = Hit.GetActor();
            Candidate.Hit = Hit;
            Candidate.bHasHit = true;
            Candidates.Add(Candidate);
        }
    }
    else if (TargetingMode == EJumanjiAbilityTargetingMode::RadialPulse)
    {
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JumanjiConfiguredAbility), false, SourceActor);
        QueryParams.AddIgnoredActor(SourceActor);

        TArray<FOverlapResult> Overlaps;
        World->OverlapMultiByChannel(
            Overlaps,
            SourceActor->GetActorLocation(),
            FQuat::Identity,
            ECC_Pawn,
            FCollisionShape::MakeSphere(TraceRadius),
            QueryParams);

        for (const FOverlapResult& Overlap : Overlaps)
        {
            FJumanjiAbilityTargetCandidate Candidate;
            Candidate.Actor = Overlap.GetActor();
            Candidates.Add(Candidate);
        }
    }

    TSet<AActor*> ProcessedActors;
    int32 AppliedTargetCount = 0;

    for (const FJumanjiAbilityTargetCandidate& Candidate : Candidates)
    {
        AActor* TargetActor = Candidate.Actor;
        if (!IsValid(TargetActor)
            || ProcessedActors.Contains(TargetActor)
            || !IsTargetAllowed(SourceActor, TargetActor))
        {
            continue;
        }

        UAbilitySystemComponent* TargetASC =
            UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
        if (!TargetASC)
        {
            continue;
        }

        const bool bApplied = ApplyConfiguredEffect(
            SourceASC,
            TargetASC,
            SourceActor,
            TargetActor,
            Candidate.bHasHit ? &Candidate.Hit : nullptr,
            TargetEffectClass,
            TargetMagnitudeTag,
            TargetBaseMagnitude,
            TargetAttackPowerCoefficient,
            Handle,
            ActorInfo);

        if (!bApplied)
        {
            continue;
        }

        ProcessedActors.Add(TargetActor);
        ++AppliedTargetCount;
        BP_OnConfiguredTargetApplied(TargetActor, TargetBaseMagnitude);

        if (MaxTargets > 0 && AppliedTargetCount >= MaxTargets)
        {
            break;
        }
    }
}

bool UJumanjiConfiguredEffectAbility::IsTargetAllowed(
    const AActor* SourceActor,
    const AActor* TargetActor) const
{
    if (!SourceActor || !TargetActor)
    {
        return false;
    }

    if (TargetTeam == EJumanjiAbilityTargetTeam::Any)
    {
        return true;
    }

    if (TargetTeam == EJumanjiAbilityTargetTeam::SelfOnly)
    {
        return SourceActor == TargetActor;
    }

    // The first content pack uses the project's PvE convention:
    // player-controlled pawns are one side and enemy characters are the other.
    // A production faction/team subsystem can replace this predicate later.
    const bool bSourceIsEnemy = SourceActor->IsA(AJumanjiEnemyCharacter::StaticClass());
    const bool bTargetIsEnemy = TargetActor->IsA(AJumanjiEnemyCharacter::StaticClass());
    const bool bFriendly = (bSourceIsEnemy == bTargetIsEnemy);

    return TargetTeam == EJumanjiAbilityTargetTeam::Friendly
        ? bFriendly
        : !bFriendly;
}

bool UJumanjiConfiguredEffectAbility::ApplyConfiguredEffect(
    UAbilitySystemComponent* SourceASC,
    UAbilitySystemComponent* TargetASC,
    AActor* SourceActor,
    AActor* TargetActor,
    const FHitResult* Hit,
    TSubclassOf<UGameplayEffect> EffectClass,
    FGameplayTag MagnitudeTag,
    float BaseMagnitude,
    float AttackPowerCoefficient,
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (!SourceASC || !TargetASC || !SourceActor || !TargetActor || !EffectClass)
    {
        return false;
    }

    FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
    EffectContext.AddSourceObject(SourceActor);
    if (Hit)
    {
        EffectContext.AddHitResult(*Hit, true);
    }

    const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
        EffectClass,
        GetAbilityLevel(Handle, ActorInfo),
        EffectContext);

    if (!SpecHandle.IsValid())
    {
        return false;
    }

    float AppliedMagnitude = BaseMagnitude;
    if (AttackPowerCoefficient > 0.0f)
    {
        const float AttackPower = SourceASC->GetNumericAttribute(
            UJumanjiAttributeSet::GetAttackPowerAttribute());
        AppliedMagnitude += AttackPower * AttackPowerCoefficient;
    }

    if (MagnitudeTag.IsValid())
    {
        SpecHandle.Data->SetSetByCallerMagnitude(MagnitudeTag, AppliedMagnitude);
    }

    TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

    if (GameplayCueTag.IsValid())
    {
        TargetASC->ExecuteGameplayCue(GameplayCueTag, EffectContext);
    }

    return true;
}
