#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "JumanjiGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FJumanjiBoardEventStateChanged,
    FName, EventId,
    bool, bIsActive,
    int32, Revision
);

/** Replicated public session state. The server owns the board-event state. */
UCLASS()
class JUMANJIUE_API AJumanjiGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Board")
    void SetBoardEventState(FName EventId, bool bIsActive);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Board")
    bool IsBoardEventActive() const { return bBoardEventActive; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Board")
    FName GetActiveBoardEventId() const { return ActiveBoardEventId; }

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Board")
    FJumanjiBoardEventStateChanged OnBoardEventStateChanged;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_BoardEventState, BlueprintReadOnly, Category = "JUMANJI|Board")
    FName ActiveBoardEventId = NAME_None;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "JUMANJI|Board")
    bool bBoardEventActive = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "JUMANJI|Board")
    int32 BoardEventRevision = 0;

    UFUNCTION()
    void OnRep_BoardEventState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
