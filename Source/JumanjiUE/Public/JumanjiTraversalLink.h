#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavLinkProxy.h"
#include "JumanjiTraversalLink.generated.h"

class APawn;
class USceneComponent;
class USplineComponent;
class USphereComponent;
class UPrimitiveComponent;
class USceneComponent;
class UAnimMontage;
class AJumanjiLiftPlatform;

UENUM(BlueprintType)
enum class EJumanjiTraversalType : uint8
{
    None,
    ContextJump,
    Climb,
    Zipline,
    Lift
};

/** ANavLinkProxy with authored anchors and server-validated player interaction. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiTraversalLink : public ANavLinkProxy
{
    GENERATED_BODY()

public:
    AJumanjiTraversalLink();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    FName GetTraversalId() const { return TraversalId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    EJumanjiTraversalType GetTraversalType() const { return TraversalType; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    FVector GetEntryLocation() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    FVector GetExitLocation() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    float GetTraversalDuration() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    float GetTraversalArcHeight() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    TSoftObjectPtr<UAnimMontage> GetTraversalMontage() const { return TraversalMontage; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    bool IsTraversalEnabled() const { return bTraversalEnabled; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    bool HasAuthoredTraversalSpline() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    AJumanjiLiftPlatform* GetLiftPlatform() const { return LiftPlatform; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    bool CanBeUsedBy(const APawn* Pawn) const;

    bool GetTraversalEndpoints(
        const APawn* Pawn,
        FTransform& OutStart,
        FTransform& OutEnd,
        bool& bOutReverse
    ) const;

    /** Returns a world-space spline pose for a Zipline link, or false to use anchor interpolation. */
    bool GetTraversalPathTransformAtAlpha(
        float Alpha,
        bool bReverse,
        FTransform& OutTransform
    ) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal")
    bool SetTraversalEnabled(bool bEnabled);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    TObjectPtr<USceneComponent> EntryAnchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    TObjectPtr<USceneComponent> ExitAnchor;

    /** Query-only sphere used to register nearby links with the local controller. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    TObjectPtr<USphereComponent> FocusVolume;

    /** Optional authored path for Zipline. Add/edit points in the link Blueprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    TObjectPtr<USplineComponent> TraversalSpline;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    FName TraversalId = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    EJumanjiTraversalType TraversalType = EJumanjiTraversalType::ContextJump;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_TraversalEnabled, Category = "JUMANJI|Traversal")
    bool bTraversalEnabled = true;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    bool bBidirectional = false;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    bool bExposeSmartLinkToNavigation = true;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    bool bUseSplinePath = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal",
        meta = (ClampMin = "100.0", ClampMax = "1000.0"))
    float MaxInteractionDistance = 250.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal",
        meta = (ClampMin = "0.2", ClampMax = "20.0"))
    float TraversalDuration = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal",
        meta = (ClampMin = "0.0", ClampMax = "1500.0"))
    float ArcHeight = 220.0f;

    /** Legacy per-link animation override; manifest data takes priority in the presentation Blueprint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal")
    TSoftObjectPtr<UAnimMontage> TraversalMontage;

private:
    UFUNCTION()
    void OnRep_TraversalEnabled();

    UFUNCTION()
    void HandleFocusVolumeBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    UFUNCTION()
    void HandleFocusVolumeEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );

    void UpdateSmartLink();
    void UpdateFocusVolume();
};
