#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "JumanjiBoardWorldSubsystem.generated.h"

/**
 * Server-authoritative entry point for Living Board events.
 * Event content, objectives and reward validation will be data-driven later.
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
};
