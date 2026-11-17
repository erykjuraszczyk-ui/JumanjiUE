#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JumanjiTraversalLink.h"
#include "JumanjiTraversalComponent.generated.h"

class ACharacter;

USTRUCT(BlueprintType)
struct FJumanjiTraversalReplicatedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    bool bActive = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    FName TraversalId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    EJumanjiTraversalType TraversalType = EJumanjiTraversalType::None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    FVector_NetQuantize10 StartLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    FVector_NetQuantize10 EndLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    FRotator StartRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    FRotator EndRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    float ServerStartTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    float Duration = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    float ArcHeight = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal")
    int32 Revision = 0;
};

/** Server-authoritative motion through an authored traversal link. */
UCLASS(ClassGroup = (JUMANJI), meta = (BlueprintSpawnableComponent))
class JUMANJIUE_API UJumanjiTraversalComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UJumanjiTraversalComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    bool IsTraversing() const { return TraversalState.bActive; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    FJumanjiTraversalReplicatedState GetTraversalState() const { return TraversalState; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    AJumanjiTraversalLink* GetActiveTraversalLink() const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal")
    bool TryStartTraversal(AJumanjiTraversalLink* Link);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal")
    void CancelTraversal();

protected:
    UPROPERTY(ReplicatedUsing = OnRep_TraversalState, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    FJumanjiTraversalReplicatedState TraversalState;

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Traversal")
    void OnTraversalPresentationChanged(bool bActive, FName TraversalId, EJumanjiTraversalType TraversalType, AJumanjiTraversalLink* Link);

private:
    UFUNCTION()
    void OnRep_TraversalState();

    bool CanStartTraversal(const AJumanjiTraversalLink* Link) const;
    void FinishTraversal(bool bCompleted);
    void RestoreMovementMode();

    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
    uint8 PreviousCustomMovementMode = 0;
    bool bHasPreviousMovementMode = false;
};
