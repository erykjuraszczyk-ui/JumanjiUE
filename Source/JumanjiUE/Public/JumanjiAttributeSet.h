#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "JumanjiAttributeSet.generated.h"

#define JUMANJI_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Replicated combat attributes owned by the player's AbilitySystemComponent. */
UCLASS()
class JUMANJIUE_API UJumanjiAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UJumanjiAttributeSet();

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_Energy)
    FGameplayAttributeData Energy;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, Energy)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_MaxEnergy)
    FGameplayAttributeData MaxEnergy;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, MaxEnergy)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_Resonance)
    FGameplayAttributeData Resonance;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, Resonance)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_MaxResonance)
    FGameplayAttributeData MaxResonance;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, MaxResonance)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_AttackPower)
    FGameplayAttributeData AttackPower;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, AttackPower)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_Defense)
    FGameplayAttributeData Defense;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, Defense)

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Energy(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Resonance(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxResonance(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Defense(const FGameplayAttributeData& OldValue);
};

#undef JUMANJI_ATTRIBUTE_ACCESSORS
