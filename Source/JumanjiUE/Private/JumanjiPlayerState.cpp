#include "JumanjiPlayerState.h"

#include "JumanjiAbilitySystemComponent.h"
#include "JumanjiAttributeSet.h"
#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiPlayerProfileSubsystem.h"
#include "JumanjiWorldAtlas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace JumanjiProfile
{
    constexpr int32 MaxRegions = 32;
    constexpr int32 MaxLandmarks = 128;
    constexpr int32 MaxCompletedChapters = 64;
}

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
    PersistProfileSnapshot();
}

void AJumanjiPlayerState::SetPersistentProfileId(const FString& InProfileId)
{
    if (!HasAuthority())
    {
        return;
    }

    const FString CleanProfileId = InProfileId.TrimStartAndEnd();
    if (CleanProfileId.IsEmpty() || CleanProfileId.Len() > 256)
    {
        return;
    }

    if (!PersistentProfileId.IsEmpty() && PersistentProfileId != CleanProfileId)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI profile: refusing to change an already-bound profile identity."));
        return;
    }

    PersistentProfileId = CleanProfileId;
}

void AJumanjiPlayerState::SaveProfileNow()
{
    PersistProfileSnapshot();
}

bool AJumanjiPlayerState::ApplyProfileSnapshot(
    const FJumanjiPlayerProfileSnapshot& Snapshot)
{
    if (!HasAuthority()
        || PersistentProfileId.IsEmpty()
        || Snapshot.ProfileId != PersistentProfileId
        || Snapshot.SchemaVersion != 1
        || Snapshot.DiscoveredRegionIds.Num() > JumanjiProfile::MaxRegions
        || Snapshot.VisitedLandmarkIds.Num() > JumanjiProfile::MaxLandmarks
        || Snapshot.CompletedAdventureChapterIds.Num() > JumanjiProfile::MaxCompletedChapters
        || Snapshot.ExplorationRevision < 0)
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    if (!Atlas)
    {
        return false;
    }

    SelectedCharacterId = Roster && Roster->IsPlayableCharacter(Snapshot.SelectedCharacterId)
        ? Snapshot.SelectedCharacterId
        : FName(TEXT("GREAT_SPIRIT_I"));

    TArray<FName> SafeRegions;
    TSet<FName> RegionIds;
    for (const FName RegionId : Snapshot.DiscoveredRegionIds)
    {
        if (!RegionId.IsNone() && Atlas->IsKnownRegion(RegionId) && !RegionIds.Contains(RegionId))
        {
            RegionIds.Add(RegionId);
            SafeRegions.Add(RegionId);
        }
    }

    TArray<FName> SafeLandmarks;
    TSet<FName> LandmarkIds;
    for (const FName LandmarkId : Snapshot.VisitedLandmarkIds)
    {
        FJumanjiWorldLandmarkDefinition Landmark;
        if (LandmarkId.IsNone()
            || LandmarkIds.Contains(LandmarkId)
            || !Atlas->GetLandmarkDefinition(LandmarkId, Landmark))
            {
                continue;
            }

        // Visiting a landmark always implies discovery of its parent region.
        if (!RegionIds.Contains(Landmark.RegionId))
        {
            RegionIds.Add(Landmark.RegionId);
            SafeRegions.Add(Landmark.RegionId);
        }

        LandmarkIds.Add(LandmarkId);
        SafeLandmarks.Add(LandmarkId);
    }

    TArray<FName> SafeCompletedChapters;
    TSet<FName> CompletedChapterIds;
    for (const FName ChapterId : Snapshot.CompletedAdventureChapterIds)
    {
        if (!ChapterId.IsNone()
            && !CompletedChapterIds.Contains(ChapterId)
            && SafeCompletedChapters.Num() < JumanjiProfile::MaxCompletedChapters)
        {
            CompletedChapterIds.Add(ChapterId);
            SafeCompletedChapters.Add(ChapterId);
        }
    }

    WorldExplorationState.DiscoveredRegionIds = MoveTemp(SafeRegions);
    WorldExplorationState.VisitedLandmarkIds = MoveTemp(SafeLandmarks);
    WorldExplorationState.Revision = FMath::Max(0, Snapshot.ExplorationRevision);
    CompletedAdventureChapterIds = MoveTemp(SafeCompletedChapters);

    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
    OnCompletedAdventureChaptersChanged.Broadcast(CompletedAdventureChapterIds);
    ForceNetUpdate();
    return true;
}

bool AJumanjiPlayerState::MarkAdventureChapterCompleted(FName ChapterId)
{
    if (!HasAuthority() || ChapterId.IsNone() || CompletedAdventureChapterIds.Contains(ChapterId))
    {
        return false;
    }

    if (CompletedAdventureChapterIds.Num() >= JumanjiProfile::MaxCompletedChapters)
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI profile: completed chapter limit reached."));
        return false;
    }

    CompletedAdventureChapterIds.Add(ChapterId);
    OnCompletedAdventureChaptersChanged.Broadcast(CompletedAdventureChapterIds);
    ForceNetUpdate();
    PersistProfileSnapshot();
    return true;
}

bool AJumanjiPlayerState::HasCompletedAdventureChapter(FName ChapterId) const
{
    return !ChapterId.IsNone() && CompletedAdventureChapterIds.Contains(ChapterId);
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
    PersistProfileSnapshot();
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

    bool bChanged = false;

    // Reaching a landmark also reveals its containing region.
    if (!WorldExplorationState.DiscoveredRegionIds.Contains(Landmark.RegionId))
    {
        WorldExplorationState.DiscoveredRegionIds.Add(Landmark.RegionId);
        bChanged = true;
    }

    WorldExplorationState.VisitedLandmarkIds.Add(LandmarkId);
    bChanged = true;

    if (bChanged)
    {
        ++WorldExplorationState.Revision;
        OnWorldExplorationChanged.Broadcast(WorldExplorationState);
        ForceNetUpdate();
        PersistProfileSnapshot();
    }

    return bChanged;
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

FJumanjiPlayerProfileSnapshot AJumanjiPlayerState::BuildProfileSnapshot() const
{
    FJumanjiPlayerProfileSnapshot Snapshot;
    Snapshot.SchemaVersion = 1;
    Snapshot.ProfileId = PersistentProfileId;
    Snapshot.SelectedCharacterId = SelectedCharacterId;
    Snapshot.DiscoveredRegionIds = WorldExplorationState.DiscoveredRegionIds;
    Snapshot.VisitedLandmarkIds = WorldExplorationState.VisitedLandmarkIds;
    Snapshot.ExplorationRevision = WorldExplorationState.Revision;
    Snapshot.CompletedAdventureChapterIds = CompletedAdventureChapterIds;
    Snapshot.LastSavedUtc = FDateTime::UtcNow();
    return Snapshot;
}

void AJumanjiPlayerState::PersistProfileSnapshot()
{
    if (!HasAuthority() || PersistentProfileId.IsEmpty())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UJumanjiPlayerProfileSubsystem* ProfileSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiPlayerProfileSubsystem>()
        : nullptr;

    if (!ProfileSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI profile: persistence subsystem is unavailable."));
        return;
    }

    if (!ProfileSubsystem->SaveProfile(BuildProfileSnapshot()))
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI profile: snapshot was rejected before save could be queued."));
    }
}

void AJumanjiPlayerState::OnRep_WorldExplorationState()
{
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
}

void AJumanjiPlayerState::OnRep_SelectedCharacterId()
{
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
}

void AJumanjiPlayerState::OnRep_CompletedAdventureChapterIds()
{
    OnCompletedAdventureChaptersChanged.Broadcast(CompletedAdventureChapterIds);
}

void AJumanjiPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiPlayerState, SelectedCharacterId);
    DOREPLIFETIME(AJumanjiPlayerState, WorldExplorationState);
    DOREPLIFETIME(AJumanjiPlayerState, CompletedAdventureChapterIds);
}
