#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "JumanjiAdventureObjective.h"
#include "JumanjiBoardWorldSubsystem.generated.h"

/**
 * Server-authoritative entry point for Living Board events and adventures.
 * Puzzle actors validate their solution rules before advancing objective progress.
 */
UCLASS()
class JUMANJIUE_API UJumanjiBoardWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Board")
    bool StartBoardEvent(FName EventId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Board")
    bool EndBoardEvent(FName EventId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool StartBoardAdventure(
        FName EventId,
        const TArray<FJumanjiAdventureObjectiveState>& Objectives
    );

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool AdvanceBoardAdventureObjective(FName ObjectiveId, int32 ProgressAmount = 1);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool CompleteBoardAdventure(FName EventId);
};
