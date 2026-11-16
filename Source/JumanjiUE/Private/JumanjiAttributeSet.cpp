#include "JumanjiAttributeSet.h"

#include "Net/UnrealNetwork.h"

UJumanjiAttributeSet::UJumanjiAttributeSet()
{
    InitMaxHealth(100.0f);
    InitHealth(100.0f);

    InitMaxStamina(100.0f);
    InitStamina(100.0f);

    InitMaxResonance(100.0f);
    InitResonance(0.0f);
}

void UJumanjiAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, Resonance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, MaxResonance, COND_None, REPNOTIFY_Always);
}

void UJumanjiAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, Health, OldValue);
}

void UJumanjiAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, MaxHealth, OldValue);
}

void UJumanjiAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, Stamina, OldValue);
}

void UJumanjiAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, MaxStamina, OldValue);
}

void UJumanjiAttributeSet::OnRep_Resonance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, Resonance, OldValue);
}

void UJumanjiAttributeSet::OnRep_MaxResonance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, MaxResonance, OldValue);
}
