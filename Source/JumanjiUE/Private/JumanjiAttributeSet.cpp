#include "JumanjiAttributeSet.h"

#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UJumanjiAttributeSet::UJumanjiAttributeSet()
{
    InitHealth(100.0f);
    InitMaxHealth(100.0f);
    InitEnergy(100.0f);
    InitMaxEnergy(100.0f);
    InitResonance(0.0f);
    InitMaxResonance(100.0f);
    InitAttackPower(10.0f);
    InitDefense(5.0f);
}

void UJumanjiAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    else if (Attribute == GetEnergyAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
    }
    else if (Attribute == GetResonanceAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxResonance());
    }
    else if (Attribute == GetMaxHealthAttribute()
        || Attribute == GetMaxEnergyAttribute()
        || Attribute == GetMaxResonanceAttribute()
        || Attribute == GetAttackPowerAttribute()
        || Attribute == GetDefenseAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
}

void UJumanjiAttributeSet::PostAttributeChange(
    const FGameplayAttribute& Attribute,
    float OldValue,
    float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);

    if (Attribute == GetMaxHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (Attribute == GetMaxEnergyAttribute())
    {
        SetEnergy(FMath::Clamp(GetEnergy(), 0.0f, GetMaxEnergy()));
    }
    else if (Attribute == GetMaxResonanceAttribute())
    {
        SetResonance(FMath::Clamp(GetResonance(), 0.0f, GetMaxResonance()));
    }
}

void UJumanjiAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    const FGameplayAttribute ModifiedAttribute = Data.EvaluatedData.Attribute;

    if (ModifiedAttribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (ModifiedAttribute == GetEnergyAttribute())
    {
        SetEnergy(FMath::Clamp(GetEnergy(), 0.0f, GetMaxEnergy()));
    }
    else if (ModifiedAttribute == GetResonanceAttribute())
    {
        SetResonance(FMath::Clamp(GetResonance(), 0.0f, GetMaxResonance()));
    }
    else if (ModifiedAttribute == GetMaxHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (ModifiedAttribute == GetMaxEnergyAttribute())
    {
        SetEnergy(FMath::Clamp(GetEnergy(), 0.0f, GetMaxEnergy()));
    }
    else if (ModifiedAttribute == GetMaxResonanceAttribute())
    {
        SetResonance(FMath::Clamp(GetResonance(), 0.0f, GetMaxResonance()));
    }
}

void UJumanjiAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, Energy, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, Resonance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, MaxResonance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UJumanjiAttributeSet, Defense, COND_None, REPNOTIFY_Always);
}

void UJumanjiAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, Health, OldValue);
}

void UJumanjiAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, MaxHealth, OldValue);
}

void UJumanjiAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, Energy, OldValue);
}

void UJumanjiAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, MaxEnergy, OldValue);
}

void UJumanjiAttributeSet::OnRep_Resonance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, Resonance, OldValue);
}

void UJumanjiAttributeSet::OnRep_MaxResonance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, MaxResonance, OldValue);
}

void UJumanjiAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, AttackPower, OldValue);
}

void UJumanjiAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UJumanjiAttributeSet, Defense, OldValue);
}
