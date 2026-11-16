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

/**
 * Replicated character attributes used by combat, exploration and Relic systems.
 * Gameplay effects should modify these values; clients must not authoritatively
 * assign combat results.
 */
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

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_Stamina)
    FGameplayAttributeData Stamina;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_MaxStamina)
    FGameplayAttributeData MaxStamina;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, MaxStamina)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_Resonance)
    FGameplayAttributeData Resonance;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, Resonance)

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Attributes", ReplicatedUsing = OnRep_MaxResonance)
    FGameplayAttributeData MaxResonance;
    JUMANJI_ATTRIBUTE_ACCESSORS(UJumanjiAttributeSet, MaxResonance)

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Stamina(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Resonance(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxResonance(const FGameplayAttributeData& OldValue);
};

#undef JUMANJI_ATTRIBUTE_ACCESSORS
