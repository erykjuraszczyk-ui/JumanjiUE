#include "JumanjiGameplayAbility.h"

UJumanjiGameplayAbility::UJumanjiGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}
