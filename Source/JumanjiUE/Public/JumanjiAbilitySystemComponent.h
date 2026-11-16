#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "JumanjiCharacterDefinition.h"
#include "JumanjiAbilityCatalogSubsystem.h"
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

    /** Server-only: replaces the active character's abilities with catalog-resolved saved slots. */
    bool ApplyResolvedCharacterAbilityLoadout(
        FName CharacterId,
        const TArray<FJumanjiResolvedAbility>& ResolvedAbilities);

    /** Server-only: clears the ASC-owned character loadout, including active abilities. */
    void ClearCharacterAbilityLoadout();

    /** Legacy helper retained for non-player prototype callers. */
    void ClearGrantedAbilities(const TArray<FGameplayAbilitySpecHandle>& GrantedHandles);

private:
    UPROPERTY(Transient)
    TArray<FGameplayAbilitySpecHandle> ActiveCharacterAbilityHandles;

    UPROPERTY(Transient)
    FName ActiveCharacterLoadoutId = NAME_None;

    UPROPERTY(Transient)
    TArray<FName> ActiveCharacterAbilityIds;
};
