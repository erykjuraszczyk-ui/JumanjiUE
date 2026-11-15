#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumanjiWorldLandmark.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
struct FHitResult;

/**
 * Discoverable point of interest. The server records the visit in the
 * overlapping player's replicated PlayerState exploration snapshot.
 */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiWorldLandmark : public AActor
{
    GENERATED_BODY()

public:
    AJumanjiWorldLandmark();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    FName GetLandmarkId() const { return LandmarkId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    FName GetRegionId() const { return RegionId; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    TObjectPtr<USphereComponent> DiscoverySphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    TObjectPtr<UStaticMeshComponent> LandmarkVisual;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|World")
    FName LandmarkId = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|World")
    FName RegionId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|World", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
    float DiscoveryRadius = 350.0f;

private:
    UFUNCTION()
    void HandleDiscoveryOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );
};
