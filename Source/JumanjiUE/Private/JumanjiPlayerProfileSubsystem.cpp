#include "JumanjiPlayerProfileSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"

bool UJumanjiPlayerProfileSubsystem::LoadProfile(
    const FString& ProfileId,
    FJumanjiPlayerProfileSnapshot& OutSnapshot) const
{
    if (!IsValidProfileId(ProfileId))
    {
        return false;
    }

    const FString SlotName = BuildSlotName(ProfileId);
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        return false;
    }

    USaveGame* LoadedObject = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
    const UJumanjiPlayerProfileSaveGame* SaveObject =
        Cast<UJumanjiPlayerProfileSaveGame>(LoadedObject);

    if (!SaveObject
        || SaveObject->Profile.ProfileId != ProfileId
        || SaveObject->Profile.SchemaVersion != CurrentSchemaVersion
        || !IsValidSnapshot(SaveObject->Profile))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI profile: slot for profile '%s' is invalid or uses an unsupported schema."),
            *ProfileId);
        return false;
    }

    OutSnapshot = SaveObject->Profile;
    return true;
}

bool UJumanjiPlayerProfileSubsystem::SaveProfile(
    const FJumanjiPlayerProfileSnapshot& Snapshot)
{
    if (!IsValidSnapshot(Snapshot))
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI profile: rejected invalid profile snapshot."));
        return false;
    }

    const FString SlotName = BuildSlotName(Snapshot.ProfileId);

    if (InFlightSlots.Contains(SlotName))
    {
        // Coalesce rapid discoveries/updates: keep only the newest complete snapshot.
        QueuedSnapshots.Add(SlotName, Snapshot);
        return true;
    }

    // A fresh snapshot supersedes a queued retry from a previously failed write.
    QueuedSnapshots.Remove(SlotName);
    return StartAsyncSave(SlotName, Snapshot);
}

bool UJumanjiPlayerProfileSubsystem::HasProfile(const FString& ProfileId) const
{
    return IsValidProfileId(ProfileId)
        && UGameplayStatics::DoesSaveGameExist(BuildSlotName(ProfileId), 0);
}

FString UJumanjiPlayerProfileSubsystem::BuildSlotName(const FString& ProfileId) const
{
    const FString NormalizedProfileId = ProfileId.TrimStartAndEnd();
    const FString ProfileHash = FMD5::HashAnsiString(*NormalizedProfileId);
    return FString::Printf(TEXT("JumanjiProfile_%s"), *ProfileHash);
}

bool UJumanjiPlayerProfileSubsystem::IsValidProfileId(const FString& ProfileId) const
{
    return !ProfileId.TrimStartAndEnd().IsEmpty()
        && ProfileId.Len() <= 256;
}

bool UJumanjiPlayerProfileSubsystem::IsValidSnapshot(
    const FJumanjiPlayerProfileSnapshot& Snapshot) const
{
    if (!IsValidProfileId(Snapshot.ProfileId)
        || Snapshot.SchemaVersion != CurrentSchemaVersion
        || Snapshot.SelectedCharacterId.IsNone()
        || Snapshot.ExplorationRevision < 0
        || Snapshot.DiscoveredRegionIds.Num() > MaxRegions
        || Snapshot.VisitedLandmarkIds.Num() > MaxLandmarks
        || Snapshot.CompletedAdventureChapterIds.Num() > MaxCompletedChapters)
    {
        return false;
    }

    const auto HasInvalidOrDuplicateIds = [](const TArray<FName>& Ids)
    {
        TSet<FName> SeenIds;
        for (const FName Id : Ids)
        {
            if (Id.IsNone() || SeenIds.Contains(Id))
            {
                return true;
            }
            SeenIds.Add(Id);
        }
        return false;
    };

    return !HasInvalidOrDuplicateIds(Snapshot.DiscoveredRegionIds)
        && !HasInvalidOrDuplicateIds(Snapshot.VisitedLandmarkIds)
        && !HasInvalidOrDuplicateIds(Snapshot.CompletedAdventureChapterIds);
}

bool UJumanjiPlayerProfileSubsystem::StartAsyncSave(
    const FString& SlotName,
    const FJumanjiPlayerProfileSnapshot& Snapshot)
{
    UJumanjiPlayerProfileSaveGame* SaveObject =
        Cast<UJumanjiPlayerProfileSaveGame>(
            UGameplayStatics::CreateSaveGameObject(UJumanjiPlayerProfileSaveGame::StaticClass())
        );

    if (!SaveObject)
    {
        UE_LOG(LogTemp, Error, TEXT("JUMANJI profile: failed to create SaveGame object."));
        return false;
    }

    SaveObject->Profile = Snapshot;
    SaveObject->Profile.SchemaVersion = CurrentSchemaVersion;
    SaveObject->Profile.LastSavedUtc = FDateTime::UtcNow();

    PendingSaveObjects.Add(SlotName, SaveObject);
    InFlightSlots.Add(SlotName);

    UGameplayStatics::AsyncSaveGameToSlot(
        SaveObject,
        SlotName,
        0,
        FAsyncSaveGameToSlotDelegate::CreateUObject(
            this,
            &UJumanjiPlayerProfileSubsystem::HandleSaveCompleted
        )
    );

    return true;
}

void UJumanjiPlayerProfileSubsystem::HandleSaveCompleted(
    const FString& SlotName,
    const int32 UserIndex,
    bool bSuccess)
{
    PendingSaveObjects.Remove(SlotName);
    InFlightSlots.Remove(SlotName);

    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error,
            TEXT("JUMANJI profile: asynchronous save failed for slot '%s' (user index %d)."),
            *SlotName,
            UserIndex);
        // The PlayerState still owns the current in-memory snapshot. A later
        // milestone or logout call will submit a fresh write.
        QueuedSnapshots.Remove(SlotName);
        return;
    }

    if (FJumanjiPlayerProfileSnapshot* QueuedSnapshot = QueuedSnapshots.Find(SlotName))
    {
        const FJumanjiPlayerProfileSnapshot LatestSnapshot = *QueuedSnapshot;
        QueuedSnapshots.Remove(SlotName);

        if (!StartAsyncSave(SlotName, LatestSnapshot))
        {
            UE_LOG(LogTemp, Error,
                TEXT("JUMANJI profile: failed to start the queued save for slot '%s'."),
                *SlotName);
        }
    }
}
