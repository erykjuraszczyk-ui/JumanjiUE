#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "JumanjiAdventureObjective.generated.h"

/**
 * Replicated objective snapshot for a Living Board adventure.
 * Puzzle actors own their solution rules; this type stores validated progress.
 */
USTRUCT(BlueprintType)
struct FJumanjiAdventureObjectiveState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Adventure")
    FName ObjectiveId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Adventure")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Adventure")
    FText Description;

    /** Element hint for UI and puzzle setup; enforcement belongs to the trial actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Adventure")
    FGameplayTag RequiredElement;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Adventure", meta = (ClampMin = "1", ClampMax = "1000"))
    int32 RequiredProgress = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    int32 CurrentProgress = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    bool bCompleted = false;
};

/** A single replicated property keeps objective data and its revision together. */
USTRUCT(BlueprintType)
struct FJumanjiAdventureObjectiveSet
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Adventure")
    TArray<FJumanjiAdventureObjectiveState> Objectives;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Adventure")
    int32 Revision = 0;
};
