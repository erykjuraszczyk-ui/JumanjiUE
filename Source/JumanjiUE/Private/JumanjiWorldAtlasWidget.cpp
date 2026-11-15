#include "JumanjiWorldAtlasWidget.h"

#include "Engine/World.h"
#include "JumanjiPlayerState.h"

void UJumanjiWorldAtlasWidget::NativeConstruct()
{
    Super::NativeConstruct();

    CachedPlayerState = GetJumanjiPlayerState();
    if (IsValid(CachedPlayerState))
    {
        CachedPlayerState->OnWorldExplorationChanged.AddUniqueDynamic(
            this,
            &UJumanjiWorldAtlasWidget::HandleWorldExplorationChanged
        );
    }

    OnWorldAtlasChanged();
}

void UJumanjiWorldAtlasWidget::NativeDestruct()
{
    if (IsValid(CachedPlayerState))
    {
        CachedPlayerState->OnWorldExplorationChanged.RemoveDynamic(
            this,
            &UJumanjiWorldAtlasWidget::HandleWorldExplorationChanged
        );
    }

    CachedPlayerState = nullptr;
    Super::NativeDestruct();
}

FJumanjiWorldExplorationState UJumanjiWorldAtlasWidget::GetExplorationState() const
{
    const AJumanjiPlayerState* PlayerState = GetJumanjiPlayerState();
    return PlayerState
        ? PlayerState->GetWorldExplorationState()
        : FJumanjiWorldExplorationState();
}

bool UJumanjiWorldAtlasWidget::IsRegionDiscovered(FName RegionId) const
{
    const AJumanjiPlayerState* PlayerState = GetJumanjiPlayerState();
    return PlayerState && PlayerState->HasDiscoveredWorldRegion(RegionId);
}

bool UJumanjiWorldAtlasWidget::IsLandmarkVisited(FName LandmarkId) const
{
    const AJumanjiPlayerState* PlayerState = GetJumanjiPlayerState();
    return PlayerState && PlayerState->HasVisitedWorldLandmark(LandmarkId);
}

int32 UJumanjiWorldAtlasWidget::GetDiscoveredRegionCount() const
{
    return GetExplorationState().DiscoveredRegionIds.Num();
}

int32 UJumanjiWorldAtlasWidget::GetVisitedLandmarkCount() const
{
    return GetExplorationState().VisitedLandmarkIds.Num();
}

void UJumanjiWorldAtlasWidget::GetWorldRegions(
    TArray<FJumanjiWorldRegionDefinition>& OutRegions) const
{
    OutRegions.Reset();

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    if (Atlas)
    {
        Atlas->GetWorldRegions(OutRegions);
    }
}

void UJumanjiWorldAtlasWidget::GetWorldLandmarks(
    TArray<FJumanjiWorldLandmarkDefinition>& OutLandmarks) const
{
    OutLandmarks.Reset();

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    if (Atlas)
    {
        Atlas->GetWorldLandmarks(OutLandmarks);
    }
}

void UJumanjiWorldAtlasWidget::HandleWorldExplorationChanged(
    const FJumanjiWorldExplorationState& NewState)
{
    OnWorldAtlasChanged();
}

AJumanjiPlayerState* UJumanjiWorldAtlasWidget::GetJumanjiPlayerState() const
{
    if (IsValid(CachedPlayerState))
    {
        return CachedPlayerState;
    }

    APlayerController* PlayerController = GetOwningPlayer();
    return PlayerController
        ? PlayerController->GetPlayerState<AJumanjiPlayerState>()
        : nullptr;
}
