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

    if (ActiveRoute.bHasTarget)
    {
        RefreshNavigationRoute();
    }
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

    if (Region.bHasNavigationAnchor && IsFiniteLocation(Region.NavigationAnchor))
    {
        TargetLocation = Region.NavigationAnchor;
        bFoundLocation = true;
    }
    else if (World)
    {
        for (TActorIterator<AJumanjiWorldRegionVolume> It(World); It; ++It)
        {
            if (It->GetRegionId() == RegionId)
            {
                FVector BoundsExtent = FVector::ZeroVector;
                It->GetActorBounds(false, TargetLocation, BoundsExtent);
                bFoundLocation = IsFiniteLocation(TargetLocation);
                break;
            }
        }
    }

    if (!bFoundLocation)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("JUMANJI navigation: region %s has no configured or loaded destination."),
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

    if (Landmark.bHasWorldLocation && IsFiniteLocation(Landmark.WorldLocation))
    {
        TargetLocation = Landmark.WorldLocation;
        bFoundLocation = true;
    }
    else if (World)
    {
        for (TActorIterator<AJumanjiWorldLandmark> It(World); It; ++It)
        {
            if (It->GetLandmarkId() == LandmarkId && It->GetRegionId() == Landmark.RegionId)
            {
                TargetLocation = It->GetActorLocation();
                bFoundLocation = IsFiniteLocation(TargetLocation);
                break;
            }
        }
    }

    if (!bFoundLocation)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("JUMANJI navigation: landmark %s has no configured or loaded destination."),
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
        || TargetId.IsNone() || !IsFiniteLocation(WorldLocation))
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
        ClearPathIfNeeded();
        return false;
    }

    FNavLocation ProjectedTarget;
    if (!NavigationSystem->ProjectPointToNavigation(
        ActiveRoute.TargetLocation,
        ProjectedTarget,
        NavigationProjectionExtent))
    {
        ClearPathIfNeeded();
        return false;
    }

    UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(
        World,
        Pawn->GetActorLocation(),
        ProjectedTarget.Location,
        Pawn
    );

    if (!IsValid(NavigationPath) || !NavigationPath->IsValid()
        || NavigationPath->PathPoints.Num() < 2)
    {
        ClearPathIfNeeded();
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

void UJumanjiWorldNavigationComponent::ClearPathIfNeeded()
{
    if (!ActiveRoute.bHasPath && ActiveRoute.PathPoints.IsEmpty())
    {
        return;
    }

    ActiveRoute.bHasPath = false;
    ActiveRoute.bPartialPath = false;
    ActiveRoute.PathPoints.Reset();
    ActiveRoute.PathLengthMeters = 0.0f;
    ++ActiveRoute.Revision;
    OnNavigationRouteChanged.Broadcast(ActiveRoute);
}

APlayerController* UJumanjiWorldNavigationComponent::GetOwningPlayerController() const
{
    return Cast<APlayerController>(GetOwner());
}

bool UJumanjiWorldNavigationComponent::IsFiniteLocation(const FVector& Location) const
{
    return FMath::IsFinite(Location.X)
        && FMath::IsFinite(Location.Y)
        && FMath::IsFinite(Location.Z);
}
