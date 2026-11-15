#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "JumanjiEnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
struct FAIStimulus;

/**
 * Lightweight prototype AI: acquire a player pawn, pursue it, and request
 * the enemy's tagged GAS attack while inside its configured attack range.
 */
UCLASS()
class JUMANJIUE_API AJumanjiEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    AJumanjiEnemyAIController();

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION()
    void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
    UPROPERTY(VisibleAnywhere, Category = "JUMANJI|AI")
    TObjectPtr<UAIPerceptionComponent> JumanjiPerceptionComponent;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    TWeakObjectPtr<AActor> CurrentTarget;

    UPROPERTY(EditDefaultsOnly, Category = "JUMANJI|AI", meta = (ClampMin = "0.25"))
    float AttackRequestInterval = 1.0f;

    float RepathAccumulator = 0.0f;
    float AttackRequestAccumulator = 0.0f;
};
