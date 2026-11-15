#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "JumanjiCharacterDefinition.h"
#include "JumanjiAbilitySystemComponent.generated.h"

/**
 * Project ASC: grants a selected Great Spirit's ability loadout and resolves
 * local input tags to granted ability specs.
 */
UCLASS()
class JUMANJIUE_API UJumanjiAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Abilities")
    bool TryActivateAbilitiesByInputTag(FGameplayTag InputTag);

    /** Server-only. Returns handles for every successfully granted ability. */
    void GrantCharacterAbilityLoadout(
        const FJumanjiCharacterDefinition& CharacterDefinition,
        TArray<FGameplayAbilitySpecHandle>& OutGrantedHandles);

    /** Server-only. Clears only the handles granted by the previous loadout. */
    void ClearGrantedAbilities(const TArray<FGameplayAbilitySpecHandle>& GrantedHandles);
};
