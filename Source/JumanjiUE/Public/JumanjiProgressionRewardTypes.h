#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "JumanjiProgressionTypes.h"
#include "JumanjiProgressionRewardTypes.generated.h"

/** DataTable row for one chapter completion reward package. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiChapterRewardDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Rewards")
    FName ChapterId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Rewards")
    int64 AccountExperienceReward = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Rewards")
    int64 ActiveCharacterExperienceReward = 0;

    /** Reward deltas must be positive. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Rewards")
    TArray<FJumanjiCurrencyDelta> CurrencyRewards;

    /** Reward deltas must be positive. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Rewards")
    TArray<FJumanjiItemDelta> ItemRewards;
};


/** One-time exploration reward granted on the first successful traversal of a stable link ID. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiTraversalRewardDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|TraversalRewards")
    FName TraversalId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|TraversalRewards")
    FName CurrencyId = FName(TEXT("CURRENCY_EXPLORER_COINS"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|TraversalRewards", meta = (ClampMin = "1"))
    int64 CurrencyAmount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|TraversalRewards")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|TraversalRewards", meta = (ClampMin = "0"))
    int32 ItemAmount = 0;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiCurrencyCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade")
    FName CurrencyId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade", meta = (ClampMin = "1"))
    int64 Amount = 0;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiItemCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade", meta = (ClampMin = "1"))
    int32 Amount = 0;
};

/** DataTable row for a server-validated Great Spirit upgrade purchase. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiSpiritUpgradeRecipe : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade")
    FName RecipeId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade")
    FName CharacterId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade")
    int64 CharacterExperienceReward = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade")
    TArray<FJumanjiCurrencyCost> CurrencyCosts;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Upgrade")
    TArray<FJumanjiItemCost> ItemCosts;
};

/** Base combat values derived from the selected Great Spirit's persisted level. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiCombatProgressionValues
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    float MaxHealth = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    float MaxEnergy = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    float MaxResonance = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    float AttackPower = 10.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Progression")
    float Defense = 5.0f;
};
