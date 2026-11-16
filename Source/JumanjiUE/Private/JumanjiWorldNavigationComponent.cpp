#include "JumanjiWorldNavigationComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "JumanjiPlayerState.h"
#include "JumanjiWorldAtlas.h"
#include "JumanjiWorldLandmark.h"
#include "JumanjiWorldRegionVolume.h"

UJumanjiWorldNavigationComponent::UJumanjiWorldNavigationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickInterval = 1.0f;
}

void UJumanjiWorldNavigationComponent::BeginPlay()
{
    Super::BeginPlay();

    SetComponentTickInterval(FMath::Clamp(RefreshIntervalSeconds, 0.5f, 10.0f));
    SetComponentTickEnabled(false);
}

void UJumanjiWorldNavigationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SetComponentTickEnabled(false);
    OnNavigationRouteChanged.Clear();

    Super::EndPlay(EndPlayReason);
}

void UJumanjiWorldNavigationComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController || !PlayerController->IsLocalController() || !ActiveRoute.bHasTarget)
    {
        return;
    }

    RefreshNavigationRoute();
}

bool UJumanjiWorldNavigationComponent::SetRegionTarget(FName RegionId)
{
    APlayerController* PlayerController = GetOwningPlayerController();
    AJumanjiPlayerState* PlayerState = PlayerController
        ? PlayerController->GetPlayerState<AJumanjiPlayerState>()
        : nullptr;

    UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    FJumanjiWorldRegionDefinition Region;
    if (!PlayerController || !PlayerController->IsLocalController()
        || !PlayerState || !Atlas
        || !Atlas->GetRegionDefinition(RegionId, Region)
        || !PlayerState->HasDiscoveredWorldRegion(RegionId))
    {
        return false;
    }

    FVector TargetLocation = FVector::ZeroVector;
    bool bFoundLocation = false;

    if (Region.bHasNavigationAnchor && !Region.NavigationAnchor.ContainsNaN())
    {
        TargetLocation = Region.NavigationAnchor;
        bFoundLocation = true;
    }
    else if (World)
    {
        // Fallback for a region volume that is currently loaded by World Partition.
        for (TActorIterator<AJumanjiWorldRegionVolume> It(World); It; ++It)
        {
            if (It->GetRegionId() == RegionId)
            {
                FVector BoundsExtent = FVector::ZeroVector;
                It->GetActorBounds(false, TargetLocation, BoundsExtent);
                bFoundLocation = !TargetLocation.ContainsNaN();
                break;
            }
        }
    }

    if (!bFoundLocation)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("JUMANJI navigation: no world anchor is configured for region %s."),
            *RegionId.ToString());
        return false;
    }

    return SetResolvedTarget(
        EJumanjiNavigationTargetType::Region,
        Region.RegionId,
        Region.DisplayName,
        TargetLocation
    );
}

bool UJumanjiWorldNavigationComponent::SetLandmarkTarget(FName LandmarkId)
{
    APlayerController* PlayerController = GetOwningPlayerController();
    AJumanjiPlayerState* PlayerState = PlayerController
        ? PlayerController->GetPlayerState<AJumanjiPlayerState>()
        : nullptr;

    UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    FJumanjiWorldLandmarkDefinition Landmark;
    if (!PlayerController || !PlayerController->IsLocalController()
        || !PlayerState || !Atlas
        || !Atlas->GetLandmarkDefinition(LandmarkId, Landmark)
        || !PlayerState->HasDiscoveredWorldRegion(Landmark.RegionId))
    {
        return false;
    }

    FVector TargetLocation = FVector::ZeroVector;
    bool bFoundLocation = false;

    if (Landmark.bHasWorldLocation && !Landmark.WorldLocation.ContainsNaN())
    {
        TargetLocation = Landmark.WorldLocation;
        bFoundLocation = true;
    }
    else if (World)
    {
        // If the POI is not in a DataTable yet, use its loaded actor transform.
        for (TActorIterator<AJumanjiWorldLandmark> It(World); It; ++It)
        {
            if (It->GetLandmarkId() == LandmarkId && It->GetRegionId() == Landmark.RegionId)
            {
                TargetLocation = It->GetActorLocation();
                bFoundLocation = !TargetLocation.ContainsNaN();
                break;
            }
        }
    }

    if (!bFoundLocation)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("JUMANJI navigation: no world location is configured for landmark %s."),
            *LandmarkId.ToString());
        return false;
    }

    return SetResolvedTarget(
        EJumanjiNavigationTargetType::Landmark,
        Landmark.LandmarkId,
        Landmark.DisplayName,
        TargetLocation
    );
}

bool UJumanjiWorldNavigationComponent::SetResolvedTarget(
    EJumanjiNavigationTargetType TargetType,
    FName TargetId,
    const FText& DisplayName,
    const FVector& WorldLocation)
{
    APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController || !PlayerController->IsLocalController()
        || TargetType == EJumanjiNavigationTargetType::None
        || TargetId.IsNone() || WorldLocation.ContainsNaN())
    {
        return false;
    }

    const int32 PreviousRevision = ActiveRoute.Revision;
    ActiveRoute = FJumanjiNavigationRoute();
    ActiveRoute.TargetType = TargetType;
    ActiveRoute.TargetId = TargetId;
    ActiveRoute.TargetDisplayName = DisplayName;
    ActiveRoute.TargetLocation = WorldLocation;
    ActiveRoute.bHasTarget = true;
    ActiveRoute.Revision = PreviousRevision;
    bHasResolvedTargetLocation = true;

    SetComponentTickInterval(FMath::Clamp(RefreshIntervalSeconds, 0.5f, 10.0f));
    SetComponentTickEnabled(true);

    if (!RefreshNavigationRoute())
    {
        // The destination is selected even when its navmesh tile is not loaded yet.
        // The component retries on its next interval.
        ++ActiveRoute.Revision;
        OnNavigationRouteChanged.Broadcast(ActiveRoute);
    }

    return true;
}

void UJumanjiWorldNavigationComponent::ClearNavigationTarget()
{
    const int32 PreviousRevision = ActiveRoute.Revision;
    ActiveRoute = FJumanjiNavigationRoute();
    ActiveRoute.Revision = PreviousRevision + 1;
    bHasResolvedTargetLocation = false;
    SetComponentTickEnabled(false);
    OnNavigationRouteChanged.Broadcast(ActiveRoute);
}

bool UJumanjiWorldNavigationComponent::RefreshNavigationRoute()
{
    if (!ActiveRoute.bHasTarget || !bHasResolvedTargetLocation)
    {
        return false;
    }

    APlayerController* PlayerController = GetOwningPlayerController();
    APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    UWorld* World = GetWorld();

    if (!PlayerController || !PlayerController->IsLocalController() || !Pawn || !World)
    {
        return false;
    }

    ActiveRoute.DirectDistanceMeters = FVector::Distance(
        Pawn->GetActorLocation(),
        ActiveRoute.TargetLocation
    ) / 100.0f;

    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    if (!NavigationSystem)
    {
        const bool bHadPath = ActiveRoute.bHasPath || !ActiveRoute.PathPoints.IsEmpty();
        ActiveRoute.bHasPath = false;
        ActiveRoute.bPartialPath = false;
        ActiveRoute.PathPoints.Reset();
        ActiveRoute.PathLengthMeters = 0.0f;
        if (bHadPath)
        {
            ++ActiveRoute.Revision;
            OnNavigationRouteChanged.Broadcast(ActiveRoute);
        }
        return false;
    }

    FNavLocation ProjectedTarget;
    if (!NavigationSystem->ProjectPointToNavigation(
        ActiveRoute.TargetLocation,
        ProjectedTarget,
        NavigationProjectionExtent))
    {
        const bool bHadPath = ActiveRoute.bHasPath || !ActiveRoute.PathPoints.IsEmpty();
        ActiveRoute.bHasPath = false;
        ActiveRoute.bPartialPath = false;
        ActiveRoute.PathPoints.Reset();
        ActiveRoute.PathLengthMeters = 0.0f;
        if (bHadPath)
        {
            ++ActiveRoute.Revision;
            OnNavigationRouteChanged.Broadcast(ActiveRoute);
        }
        return false;
    }

    UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(
        World,
        Pawn->GetActorLocation(),
        ProjectedTarget.Location,
        Pawn
    );

    if (!NavigationPath || !NavigationPath->IsValid() || NavigationPath->PathPoints.Num() < 2)
    {
        const bool bHadPath = ActiveRoute.bHasPath || !ActiveRoute.PathPoints.IsEmpty();
        ActiveRoute.bHasPath = false;
        ActiveRoute.bPartialPath = false;
        ActiveRoute.PathPoints.Reset();
        ActiveRoute.PathLengthMeters = 0.0f;
        if (bHadPath)
        {
            ++ActiveRoute.Revision;
            OnNavigationRouteChanged.Broadcast(ActiveRoute);
        }
        return false;
    }

    ActiveRoute.PathPoints = NavigationPath->PathPoints;
    ActiveRoute.PathLengthMeters = 0.0f;
    for (int32 Index = 1; Index < ActiveRoute.PathPoints.Num(); ++Index)
    {
        ActiveRoute.PathLengthMeters += FVector::Distance(
            ActiveRoute.PathPoints[Index - 1],
            ActiveRoute.PathPoints[Index]
        ) / 100.0f;
    }

    ActiveRoute.bHasPath = true;
    ActiveRoute.bPartialPath = NavigationPath->IsPartial();
    ++ActiveRoute.Revision;
    OnNavigationRouteChanged.Broadcast(ActiveRoute);
    return true;
}

APlayerController* UJumanjiWorldNavigationComponent::GetOwningPlayerController() const
{
    return Cast<APlayerController>(GetOwner());
}
