#pragma once

#include "CoreMinimal.h"
#include "JumanjiProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class EJumanjiInventoryItemCategory : uint8
{
    UpgradeMaterial UMETA(DisplayName = "Upgrade Material"),
    Quest UMETA(DisplayName = "Quest Item"),
    Consumable UMETA(DisplayName = "Consumable"),
    Relic UMETA(DisplayName = "Relic")
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiCurrencyDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency")
    FName CurrencyId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency")
    int64 StartingBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency")
    int64 MaxBalance = 2000000000LL;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiItemDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    EJumanjiInventoryItemCategory Category = EJumanjiInventoryItemCategory::UpgradeMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 MaxStack = 9999;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bCanBeConsumed = false;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiCurrencyBalance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Economy")
    FName CurrencyId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Economy")
    int64 Amount = 0;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiInventoryStack
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Inventory")
    FName ItemId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Inventory")
    int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiCharacterProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    FName CharacterId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    int32 Level = 1;

    /** Experience accumulated toward the next character level. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    int64 Experience = 0;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiEquippedAbilitySlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Loadout")
    FName CharacterId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Loadout")
    FName SlotId = NAME_None;

    /** Stable designer ID; never store a raw UObject pointer in the profile. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Loadout")
    FName AbilityId = NAME_None;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiPlayerProgressionState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    int32 AccountLevel = 1;

    /** Experience accumulated toward the next account level. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    int64 AccountExperience = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Economy")
    TArray<FJumanjiCurrencyBalance> Currencies;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Inventory")
    TArray<FJumanjiInventoryStack> Inventory;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    TArray<FJumanjiCharacterProgress> CharacterProgress;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Loadout")
    TArray<FJumanjiEquippedAbilitySlot> EquippedAbilitySlots;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    int32 Revision = 0;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiCurrencyDelta
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    FName CurrencyId = NAME_None;

    /** Positive grants currency; negative spends currency. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    int64 Delta = 0;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiItemDelta
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    FName ItemId = NAME_None;

    /** Positive adds items; negative consumes items. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    int32 Delta = 0;
};

/**
 * A single all-or-nothing server-side profile transaction.
 * TransactionId is an idempotency key supplied by trusted server gameplay code.
 */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiProfileTransaction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    FName TransactionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    int64 AccountExperienceDelta = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    FName CharacterId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    int64 CharacterExperienceDelta = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    TArray<FJumanjiCurrencyDelta> CurrencyDeltas;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transaction")
    TArray<FJumanjiItemDelta> ItemDeltas;
};
