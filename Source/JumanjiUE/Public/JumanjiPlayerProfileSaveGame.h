#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "JumanjiPlayerProfileSaveGame.generated.h"

/**
 * Versioned, per-account profile snapshot.
 * The arrays are IDs rather than UObject references so the save remains stable
 * when maps, actors or DataTable assets are unloaded.
 */
USTRUCT(BlueprintType)
struct FJumanjiPlayerProfileSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Profile")
    int32 SchemaVersion = 1;

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
