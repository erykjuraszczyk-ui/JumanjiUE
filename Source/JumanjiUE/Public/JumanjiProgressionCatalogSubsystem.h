#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiProgressionTypes.h"
#include "JumanjiProgressionCatalogSubsystem.generated.h"

/**
 * Small source-controlled catalog used by the prototype to validate profile
 * currency and item IDs. It can later be backed by validated DataTables or
 * Primary Data Assets without changing PlayerState's transaction API.
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
