#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "JumanjiWorldAtlas.generated.h"

class UTexture2D;
class UStaticMesh;

/** A discoverable region in the Emerald Jungle world atlas. Map coordinates are normalized to 0..1. */
USTRUCT(BlueprintType)
struct FJumanjiWorldRegionDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FName RegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World", meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FGameplayTag ElementTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FName RelatedObjectiveId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FName RecommendedGreatSpiritId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    int32 SortOrder = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FVector2D MapCoordinate = FVector2D(0.5f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    bool bSafeHub = false;

    /** Optional destination for route guidance when the region actor is not loaded. Unreal units are centimeters. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    bool bHasNavigationAnchor = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World", meta = (EditCondition = "bHasNavigationAnchor"))
    FVector NavigationAnchor = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    TSoftObjectPtr<UTexture2D> MapIcon;
};

/** A named point of interest associated with a region. */
USTRUCT(BlueprintType)
struct FJumanjiWorldLandmarkDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FName LandmarkId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FName RegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World", meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    TSoftObjectPtr<UTexture2D> MapIcon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    TSoftObjectPtr<UStaticMesh> LandmarkMesh;

    /** Optional world-space POI destination when its World Partition actor is not loaded. Unreal units are centimeters. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World")
    bool bHasWorldLocation = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|World", meta = (EditCondition = "bHasWorldLocation"))
    FVector WorldLocation = FVector::ZeroVector;
};

/**
 * World-local region/landmark catalog.
 * Uses a built-in ten-region atlas and allows an all-or-nothing DataTable override.
 */
UCLASS(Config = Game)
class JUMANJIUE_API UJumanjiWorldAtlasSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool GetRegionDefinition(FName RegionId, FJumanjiWorldRegionDefinition& OutDefinition) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool GetLandmarkDefinition(FName LandmarkId, FJumanjiWorldLandmarkDefinition& OutDefinition) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool IsKnownRegion(FName RegionId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool IsKnownLandmark(FName LandmarkId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool IsLandmarkInRegion(FName LandmarkId, FName RegionId) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    void GetWorldRegions(TArray<FJumanjiWorldRegionDefinition>& OutRegions) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    void GetWorldLandmarks(TArray<FJumanjiWorldLandmarkDefinition>& OutLandmarks) const;

private:
    void BuildDefaultAtlas();
    bool LoadConfiguredAtlas();

    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|World")
    TSoftObjectPtr<UDataTable> RegionDataTable;

    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|World")
    TSoftObjectPtr<UDataTable> LandmarkDataTable;

    UPROPERTY(Transient)
    TArray<FJumanjiWorldRegionDefinition> Regions;

    UPROPERTY(Transient)
    TArray<FJumanjiWorldLandmarkDefinition> Landmarks;
};
