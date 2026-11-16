#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiProgressionTypes.h"
#include "JumanjiProgressionRewardTypes.h"
#include "JumanjiProgressionCatalogSubsystem.generated.h"

/**
 * Source-controlled validation catalog for currencies, items, level curves and
 * the baseline combat progression model.
 */
UCLASS()
class JUMANJIUE_API UJumanjiProgressionCatalogSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Economy")
    bool GetCurrencyDefinition(FName CurrencyId, FJumanjiCurrencyDefinition& OutDefinition) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Inventory")
    bool GetItemDefinition(FName ItemId, FJumanjiItemDefinition& OutDefinition) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Loadout")
    bool IsValidAbilitySlotId(FName SlotId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Progression")
    int32 GetMaxAccountLevel() const { return MaxAccountLevel; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Progression")
    int32 GetMaxCharacterLevel() const { return MaxCharacterLevel; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Progression")
    int64 GetAccountExperienceForNextLevel(int32 CurrentLevel) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Progression")
    int64 GetCharacterExperienceForNextLevel(int32 CurrentLevel) const;

    /** Prototype level curve. These are base values before temporary Gameplay Effects. */
    void GetCombatProgressionValues(int32 CharacterLevel, FJumanjiCombatProgressionValues& OutValues) const;

private:
    void BuildDefaultCatalog();

    static constexpr int32 MaxAccountLevel = 100;
    static constexpr int32 MaxCharacterLevel = 80;

    UPROPERTY(Transient)
    TArray<FJumanjiCurrencyDefinition> CurrencyDefinitions;

    UPROPERTY(Transient)
    TArray<FJumanjiItemDefinition> ItemDefinitions;

    UPROPERTY(Transient)
    TArray<FName> AbilitySlotIds;
};
