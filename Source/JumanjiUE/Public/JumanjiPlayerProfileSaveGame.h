#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "JumanjiProgressionTypes.h"
#include "JumanjiPlayerProfileSaveGame.generated.h"

/**
 * Versioned, per-account profile snapshot.
 * Persist stable IDs and plain data, never actor/UObject pointers.
 */
USTRUCT(BlueprintType)
struct FJumanjiPlayerProfileSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    int32 SchemaVersion = 2;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    FString ProfileId;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    FName SelectedCharacterId = FName(TEXT("GREAT_SPIRIT_I"));

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    TArray<FName> DiscoveredRegionIds;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    TArray<FName> VisitedLandmarkIds;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    int32 ExplorationRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    TArray<FName> CompletedAdventureChapterIds;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    FJumanjiPlayerProgressionState Progression;

    /** Idempotency keys are persisted with the balances/items they protect. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    TArray<FName> ProcessedTransactionIds;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    FDateTime LastSavedUtc;
};

/** Local server-side SaveGame container for one verified player profile. */
UCLASS()
class JUMANJIUE_API UJumanjiPlayerProfileSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FJumanjiPlayerProfileSnapshot Profile;
};
