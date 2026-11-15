#pragma once

#include "CoreMinimal.h"
#include "JumanjiWorldExploration.generated.h"

/** Per-player exploration snapshot replicated by AJumanjiPlayerState. */
USTRUCT(BlueprintType)
struct FJumanjiWorldExplorationState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|World")
    TArray<FName> DiscoveredRegionIds;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|World")
    TArray<FName> VisitedLandmarkIds;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|World")
    int32 Revision = 0;
};
