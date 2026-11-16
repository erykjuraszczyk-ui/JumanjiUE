#pragma once

#include "CoreMinimal.h"
#include "Engine/Volume.h"
#include "JumanjiWorldRegionVolume.generated.h"

/**
 * Server-side discovery boundary for one atlas region.
 * Place and scale this volume around the entrance/area of a region.
 */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiWorldRegionVolume : public AVolume
{
    GENERATED_BODY()

public:
    AJumanjiWorldRegionVolume();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    FName GetRegionId() const { return RegionId; }

protected:
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|World")
    FName RegionId = NAME_None;

private:
    UFUNCTION()
    void HandleActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);
};
