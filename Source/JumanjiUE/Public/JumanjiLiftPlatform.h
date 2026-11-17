#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumanjiLiftPlatform.generated.h"

class ACharacter;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UJumanjiTraversalComponent;

UENUM(BlueprintType)
enum class EJumanjiLiftPlatformState : uint8
{
    AtLower UMETA(DisplayName = "At Lower Stop"),
    MovingUp UMETA(DisplayName = "Moving Up"),
    AtUpper UMETA(DisplayName = "At Upper Stop"),
    MovingDown UMETA(DisplayName = "Moving Down"),
    Blocked UMETA(DisplayName = "Blocked")
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiLiftPlatformReplicatedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Lift")
    EJumanjiLiftPlatformState State = EJumanjiLiftPlatformState::AtLower;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Lift")
    bool bReverse = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Lift")
    float ServerStartTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Lift")
    float Duration = 4.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Lift")
    int32 Revision = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Lift")
    int32 PassengerCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiLiftPlatformStateChanged,
    const FJumanjiLiftPlatformReplicatedState&, State
);

USTRUCT()
struct JUMANJIUE_API FJumanjiLiftPassenger
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<ACharacter> Character;

    UPROPERTY()
    TObjectPtr<UJumanjiTraversalComponent> TraversalComponent;
};

/**
 * Server-driven, replicated lift platform.
 *
 * The platform moves as a replicated actor. Characters are attached to its
 * root while riding, so multiple riders share the same physical platform path.
 */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiLiftPlatform : public AActor
{
    GENERATED_BODY()

public:
    AJumanjiLiftPlatform();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Lift")
    EJumanjiLiftPlatformState GetLiftState() const { return LiftState.State; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Lift")
    FJumanjiLiftPlatformReplicatedState GetReplicatedLiftState() const { return LiftState; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Lift")
    int32 GetPassengerCount() const { return LiftState.PassengerCount; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Lift")
    bool CanAcceptPassenger(bool bReverse) const;

    /** Called only by a server-authoritative traversal component. */
    bool TryBoardCharacter(
        ACharacter* Character,
        UJumanjiTraversalComponent* TraversalComponent,
        bool bReverse
    );

    /** Detaches one rider when traversal is cancelled or its component ends. */
    void RemovePassenger(ACharacter* Character, bool bRestoreMovement);

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Lift")
    FJumanjiLiftPlatformStateChanged OnLiftStateChanged;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Lift")
    TObjectPtr<UBoxComponent> PlatformCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Lift")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Lift")
    TObjectPtr<UStaticMeshComponent> PlatformMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Lift")
    TObjectPtr<UBoxComponent> BoardingVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Lift")
    TObjectPtr<USceneComponent> LowerStopAnchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Lift")
    TObjectPtr<USceneComponent> UpperStopAnchor;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Lift",
        meta = (ClampMin = "0.5", ClampMax = "60.0"))
    float TravelDurationSeconds = 4.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Lift",
        meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxPassengers = 4;

    /** Further riders may join shortly after departure if they are inside BoardingVolume. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Lift",
        meta = (ClampMin = "0.0", ClampMax = "0.25"))
    float BoardingWindowAlpha = 0.12f;

    UPROPERTY(ReplicatedUsing = OnRep_LiftState, BlueprintReadOnly, Category = "JUMANJI|Lift")
    FJumanjiLiftPlatformReplicatedState LiftState;

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Lift")
    void OnLiftPresentationChanged(const FJumanjiLiftPlatformReplicatedState& State);

private:
    UFUNCTION()
    void OnRep_LiftState();

    bool IsCharacterInsideBoardingVolume(const ACharacter* Character) const;
    void StartTravel(bool bReverse);
    void UpdateLiftMotion();
    void FinishTravel(bool bCompleted);
    void NotifyStateChanged();
    void DetachAllPassengers(bool bCompleted);

    FVector InitialActorLocation = FVector::ZeroVector;
    FVector LowerStopLocation = FVector::ZeroVector;
    FVector UpperStopLocation = FVector::ZeroVector;
    bool bAnchorsInitialized = false;

    UPROPERTY(Transient)
    TArray<FJumanjiLiftPassenger> Passengers;
};
