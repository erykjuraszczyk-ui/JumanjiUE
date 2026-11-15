#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JumanjiWorldAtlas.h"
#include "JumanjiWorldExploration.h"
#include "JumanjiWorldAtlasWidget.generated.h"

class AJumanjiPlayerState;

/** Blueprint-facing world map and exploration journal base widget. */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiWorldAtlasWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    FJumanjiWorldExplorationState GetExplorationState() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool IsRegionDiscovered(FName RegionId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool IsLandmarkVisited(FName LandmarkId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    int32 GetDiscoveredRegionCount() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    int32 GetVisitedLandmarkCount() const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    void GetWorldRegions(TArray<FJumanjiWorldRegionDefinition>& OutRegions) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    void GetWorldLandmarks(TArray<FJumanjiWorldLandmarkDefinition>& OutLandmarks) const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Implement in WBP_WorldAtlas to refresh map pins and exploration counters. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|World")
    void OnWorldAtlasChanged();

private:
    UFUNCTION()
    void HandleWorldExplorationChanged(const FJumanjiWorldExplorationState& NewState);

    AJumanjiPlayerState* GetJumanjiPlayerState() const;

    UPROPERTY(Transient)
    TObjectPtr<AJumanjiPlayerState> CachedPlayerState;
};
