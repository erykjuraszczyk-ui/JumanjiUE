#include "JumanjiPlayerState.h"

#include "JumanjiAbilitySystemComponent.h"
#include "JumanjiAttributeSet.h"
#include "JumanjiWorldAtlas.h"
#include "Net/UnrealNetwork.h"

AJumanjiPlayerState::AJumanjiPlayerState()
{
    bReplicates = true;
    NetUpdateFrequency = 100.0f;

    AbilitySystemComponent = CreateDefaultSubobject<UJumanjiAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UJumanjiAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AJumanjiPlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AJumanjiPlayerState::SetSelectedCharacterId(FName CharacterId)
{
    if (!HasAuthority() || CharacterId.IsNone() || SelectedCharacterId == CharacterId)
    {
        return;
    }

    SelectedCharacterId = CharacterId;
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
    ForceNetUpdate();
}

bool AJumanjiPlayerState::DiscoverWorldRegion(FName RegionId)
{
    if (!HasAuthority() || RegionId.IsNone())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    if (!Atlas || !Atlas->IsKnownRegion(RegionId)
        || WorldExplorationState.DiscoveredRegionIds.Contains(RegionId))
    {
        return false;
    }

    WorldExplorationState.DiscoveredRegionIds.Add(RegionId);
    ++WorldExplorationState.Revision;
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiPlayerState::VisitWorldLandmark(FName LandmarkId)
{
    if (!HasAuthority() || LandmarkId.IsNone())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    FJumanjiWorldLandmarkDefinition Landmark;
    if (!Atlas
        || !Atlas->GetLandmarkDefinition(LandmarkId, Landmark)
        || WorldExplorationState.VisitedLandmarkIds.Contains(LandmarkId))
    {
        return false;
    }

    // Reaching a landmark also reveals its containing region.
    DiscoverWorldRegion(Landmark.RegionId);

    WorldExplorationState.VisitedLandmarkIds.Add(LandmarkId);
    ++WorldExplorationState.Revision;
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiPlayerState::HasDiscoveredWorldRegion(FName RegionId) const
{
    return !RegionId.IsNone()
        && WorldExplorationState.DiscoveredRegionIds.Contains(RegionId);
}

bool AJumanjiPlayerState::HasVisitedWorldLandmark(FName LandmarkId) const
{
    return !LandmarkId.IsNone()
        && WorldExplorationState.VisitedLandmarkIds.Contains(LandmarkId);
}

void AJumanjiPlayerState::OnRep_WorldExplorationState()
{
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
}

void AJumanjiPlayerState::OnRep_SelectedCharacterId()
{
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
}

void AJumanjiPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiPlayerState, SelectedCharacterId);
    DOREPLIFETIME(AJumanjiPlayerState, WorldExplorationState);
}
