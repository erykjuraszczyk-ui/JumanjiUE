#include "JumanjiCombatDefaults.h"

#include "JumanjiAttributeSet.h"
#include "GameplayTagContainer.h"

namespace
{
    void AddOverrideModifier(
        UGameplayEffect* Effect,
        const FGameplayAttribute& Attribute,
        float Value)
    {
        FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
        Modifier.Attribute = Attribute;
        Modifier.ModifierOp = EGameplayModOp::Override;
        Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
    }
}

UJumanjiDefaultDamageGameplayEffect::UJumanjiDefaultDamageGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FSetByCallerFloat SetByCaller;
    SetByCaller.DataName = FName(TEXT("Data.Damage"));

    FGameplayModifierInfo& DamageModifier = Modifiers.AddDefaulted_GetRef();
    DamageModifier.Attribute = UJumanjiAttributeSet::GetHealthAttribute();
    DamageModifier.ModifierOp = EGameplayModOp::Additive;
    DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

UJumanjiEnemyInitializationGameplayEffect::UJumanjiEnemyInitializationGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    // Keep MaxHealth before Health so the current value is clamped to the new maximum.
    AddOverrideModifier(this, UJumanjiAttributeSet::GetMaxHealthAttribute(), 100.0f);
    AddOverrideModifier(this, UJumanjiAttributeSet::GetHealthAttribute(), 100.0f);
    AddOverrideModifier(this, UJumanjiAttributeSet::GetAttackPowerAttribute(), 8.0f);
    AddOverrideModifier(this, UJumanjiAttributeSet::GetDefenseAttribute(), 2.0f);
}

UJumanjiBossInitializationGameplayEffect::UJumanjiBossInitializationGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    AddOverrideModifier(this, UJumanjiAttributeSet::GetMaxHealthAttribute(), 600.0f);
    AddOverrideModifier(this, UJumanjiAttributeSet::GetHealthAttribute(), 600.0f);
    AddOverrideModifier(this, UJumanjiAttributeSet::GetAttackPowerAttribute(), 18.0f);
    AddOverrideModifier(this, UJumanjiAttributeSet::GetDefenseAttribute(), 12.0f);
}

UJumanjiDefaultPlayerMeleeAbility::UJumanjiDefaultPlayerMeleeAbility()
{
    AbilitySlot = EJumanjiAbilitySlot::PrimaryAttack;
    InputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.Primary")), false);
}

UJumanjiDefaultEnemyMeleeAbility::UJumanjiDefaultEnemyMeleeAbility()
{
    AbilitySlot = EJumanjiAbilitySlot::Active;
    InputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Enemy.Attack")), false);
    BaseDamage = 8.0f;
    TraceDistance = 190.0f;
    TraceRadius = 75.0f;
    AttackPowerCoefficient = 0.8f;
    DefenseCoefficient = 0.7f;
}
