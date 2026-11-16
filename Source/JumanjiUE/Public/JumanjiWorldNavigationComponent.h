#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JumanjiWorldNavigationComponent.generated.h"

class APlayerController;

/** The type of world-atlas destination currently selected by the local player. */
UENUM(BlueprintType)
enum class EJumanjiNavigationTargetType : uint8
{
    None,
    Region,
    Landmark
};

/** Local, presentation-only route data. It is not replicated to other players. */
USTRUCT(BlueprintType)
struct FJumanjiNavigationRoute
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    EJumanjiNavigationTargetType TargetType = EJumanjiNavigationTargetType::None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    FName TargetId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    FText TargetDisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    bool bHasTarget = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    bool bHasPath = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    bool bPartialPath = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    TArray<FVector> PathPoints;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    float PathLengthMeters = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    float DirectDistanceMeters = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Navigation")
    int32 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiNavigationRouteChanged,
    const FJumanjiNavigationRoute&, Route
);

/**
 * Per-local-player route guidance for the World Atlas.
 *
 * Route calculation is client-side UI work. Discovery remains server-authoritative
 * through AJumanjiPlayerState and the existing world-atlas actors.
 */
UCLASS(ClassGroup = (JUMANJI), meta = (BlueprintSpawnableComponent))
class JUMANJIUE_API UJumanjiWorldNavigationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UJumanjiWorldNavigationComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    /** Selects a discovered region as the local navigation destination. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Navigation")
    bool SetRegionTarget(FName RegionId);

    /** Selects a landmark in a region already discovered by this player. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Navigation")
    bool SetLandmarkTarget(FName LandmarkId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Navigation")
    void ClearNavigationTarget();

    /** Rebuilds the path against the currently loaded navigation data. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Navigation")
    bool RefreshNavigationRoute();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Navigation")
    bool HasNavigationTarget() const { return ActiveRoute.bHasTarget; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Navigation")
    FJumanjiNavigationRoute GetActiveRoute() const { return ActiveRoute; }

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Navigation")
    FJumanjiNavigationRouteChanged OnNavigationRouteChanged;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Navigation",
        meta = (ClampMin = "100.0", ClampMax = "5000.0"))
    FVector NavigationProjectionExtent = FVector(600.0f, 600.0f, 1200.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Navigation",
        meta = (ClampMin = "0.5", ClampMax = "10.0"))
    float RefreshIntervalSeconds = 1.0f;

private:
    bool SetResolvedTarget(
        EJumanjiNavigationTargetType TargetType,
        FName TargetId,
        const FText& DisplayName,
        const FVector& WorldLocation
    );

    APlayerController* GetOwningPlayerController() const;

    UPROPERTY(Transient)
    FJumanjiNavigationRoute ActiveRoute;

    bool bHasResolvedTargetLocation = false;
};
