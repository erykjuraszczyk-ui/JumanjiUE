#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiGameplayAbility.h"
#include "JumanjiProgressionTypes.h"
#include "JumanjiAbilityCatalogSubsystem.generated.h"

class UDataTable;
class UGameplayAbility;

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiAbilityCatalogEntry : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    FName AbilityId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    FName CharacterId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    FName SlotId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    EJumanjiAbilitySlot AbilitySlot = EJumanjiAbilitySlot::Active;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    FGameplayTag InputTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    TSoftClassPtr<UGameplayAbility> AbilityClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    bool bEnabled = true;
};

/** Runtime result passed to the ASC after the catalog validates the saved ID. */
struct JUMANJIUE_API FJumanjiResolvedAbility
{
    FName AbilityId = NAME_None;
    FName SlotId = NAME_None;
    EJumanjiAbilitySlot AbilitySlot = EJumanjiAbilitySlot::Active;
    FGameplayTag InputTag;
    TSoftClassPtr<UGameplayAbility> AbilityClass;
};

/**
 * Allowlisted AbilityId -> Gameplay Ability class catalog.
 *
 * The server resolves saved logical IDs through this catalog. Profile data
 * never contains raw UObject references or client-supplied class paths.
 */
UCLASS(Config = Game)
class JUMANJIUE_API UJumanjiAbilityCatalogSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Abilities")
    bool GetAbilityDefinition(FName AbilityId, FJumanjiAbilityCatalogEntry& OutEntry) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Abilities")
    void GetAbilityChoices(FName CharacterId, FName SlotId, TArray<FJumanjiAbilityCatalogEntry>& OutChoices) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Abilities")
    bool IsValidAbilityChoice(FName CharacterId, FName SlotId, FName AbilityId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Abilities")
    FName GetDefaultAbilityId(FName CharacterId, FName SlotId) const;

    /** Returns only catalog-approved classes for the selected character. */
    bool ResolveEquippedLoadout(
        FName CharacterId,
        const TArray<FJumanjiEquippedAbilitySlot>& SavedSlots,
        TArray<FJumanjiResolvedAbility>& OutResolvedAbilities) const;

private:
    void EnsureCatalogReady() const;
    void BuildDefaultCatalog();
    void ApplyConfiguredDataTable();
    bool IsEntryClassCompatible(const FJumanjiAbilityCatalogEntry& Entry) const;
    static bool GetSlotContract(
        FName SlotId,
        EJumanjiAbilitySlot& OutAbilitySlot,
        FGameplayTag& OutInputTag);
    static FName MakeDefaultAbilityId(int32 SpiritNumber, FName SlotId);
    void AddOrReplaceEntry(const FJumanjiAbilityCatalogEntry& Entry);

    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Abilities")
    TSoftObjectPtr<UDataTable> ConfiguredAbilityDataTable;

    UPROPERTY(Transient)
    TArray<FJumanjiAbilityCatalogEntry> AbilityEntries;

    mutable bool bCatalogReady = false;
};
