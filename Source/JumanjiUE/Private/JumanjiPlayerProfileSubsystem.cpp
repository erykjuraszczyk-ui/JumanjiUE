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

    if (!SaveObject || SaveObject->Profile.ProfileId != ProfileId)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI profile: slot for profile '%s' is missing or belongs to another profile."),
            *ProfileId);
        return false;
    }

    const FJumanjiPlayerProfileSnapshot& StoredSnapshot = SaveObject->Profile;

    if (StoredSnapshot.SchemaVersion == LegacySchemaVersion)
    {
        if (!IsValidLegacyV1Snapshot(StoredSnapshot))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("JUMANJI profile: legacy v1 slot for '%s' failed validation."),
                *ProfileId);
            return false;
        }

        // Preserve all v1 identity/exploration fields and add safe v2 defaults.
        // The migrated snapshot is written back on the next profile save.
        OutSnapshot = StoredSnapshot;
        OutSnapshot.SchemaVersion = CurrentSchemaVersion;
        OutSnapshot.Progression = FJumanjiPlayerProgressionState();
        OutSnapshot.ProcessedTransactionIds.Reset();

        UE_LOG(LogTemp, Log,
            TEXT("JUMANJI profile: migrated profile '%s' from schema v1 to v2 in memory."),
            *ProfileId);
        return true;
    }

    if (StoredSnapshot.SchemaVersion != CurrentSchemaVersion
        || !IsValidSnapshot(StoredSnapshot))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI profile: slot for profile '%s' uses an invalid or unsupported schema."),
            *ProfileId);
        return false;
    }

    OutSnapshot = StoredSnapshot;
    return true;
}

bool UJumanjiPlayerProfileSubsystem::SaveProfile(
    const FJumanjiPlayerProfileSnapshot& Snapshot)
{
    if (!IsValidSnapshot(Snapshot))
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI profile: rejected invalid schema v2 snapshot."));
        return false;
    }

    const FString SlotName = BuildSlotName(Snapshot.ProfileId);

    if (InFlightSlots.Contains(SlotName))
    {
        // Coalesce rapid changes: only the latest complete snapshot is retained.
        QueuedSnapshots.Add(SlotName, Snapshot);
        return true;
    }

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

bool UJumanjiPlayerProfileSubsystem::IsValidLegacyV1Snapshot(
    const FJumanjiPlayerProfileSnapshot& Snapshot) const
{
    if (!IsValidProfileId(Snapshot.ProfileId)
        || Snapshot.SchemaVersion != LegacySchemaVersion
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

bool UJumanjiPlayerProfileSubsystem::IsValidSnapshot(
    const FJumanjiPlayerProfileSnapshot& Snapshot) const
{
    const FJumanjiPlayerProgressionState& Progression = Snapshot.Progression;

    if (!IsValidProfileId(Snapshot.ProfileId)
        || Snapshot.SchemaVersion != CurrentSchemaVersion
        || Snapshot.SelectedCharacterId.IsNone()
        || Snapshot.ExplorationRevision < 0
        || Snapshot.DiscoveredRegionIds.Num() > MaxRegions
        || Snapshot.VisitedLandmarkIds.Num() > MaxLandmarks
        || Snapshot.CompletedAdventureChapterIds.Num() > MaxCompletedChapters
        || Progression.AccountLevel < 1
        || Progression.AccountLevel > 100
        || Progression.AccountExperience < 0
        || Progression.AccountExperience > 100000000LL
        || Progression.Revision < 0
        || Progression.Currencies.Num() > MaxCurrencies
        || Progression.Inventory.Num() > MaxInventoryStacks
        || Progression.CharacterProgress.Num() > MaxCharacterProgressEntries
        || Progression.EquippedAbilitySlots.Num() > MaxEquippedAbilitySlots
        || Snapshot.ProcessedTransactionIds.Num() > MaxProcessedTransactions)
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

    if (HasInvalidOrDuplicateIds(Snapshot.DiscoveredRegionIds)
        || HasInvalidOrDuplicateIds(Snapshot.VisitedLandmarkIds)
        || HasInvalidOrDuplicateIds(Snapshot.CompletedAdventureChapterIds)
        || HasInvalidOrDuplicateIds(Snapshot.ProcessedTransactionIds))
    {
        return false;
    }

    TSet<FName> CurrencyIds;
    for (const FJumanjiCurrencyBalance& Currency : Progression.Currencies)
    {
        if (Currency.CurrencyId.IsNone()
            || CurrencyIds.Contains(Currency.CurrencyId)
            || Currency.Amount < 0
            || Currency.Amount > 2000000000LL)
        {
            return false;
        }
        CurrencyIds.Add(Currency.CurrencyId);
    }

    TSet<FName> ItemIds;
    for (const FJumanjiInventoryStack& Stack : Progression.Inventory)
    {
        if (Stack.ItemId.IsNone()
            || ItemIds.Contains(Stack.ItemId)
            || Stack.Quantity < 1
            || Stack.Quantity > 9999)
        {
            return false;
        }
        ItemIds.Add(Stack.ItemId);
    }

    TSet<FName> CharacterIds;
    for (const FJumanjiCharacterProgress& Character : Progression.CharacterProgress)
    {
        if (Character.CharacterId.IsNone()
            || CharacterIds.Contains(Character.CharacterId)
            || Character.Level < 1
            || Character.Level > 80
            || Character.Experience < 0
            || Character.Experience > 100000000LL)
        {
            return false;
        }
        CharacterIds.Add(Character.CharacterId);
    }

    TSet<FString> LoadoutKeys;
    for (const FJumanjiEquippedAbilitySlot& Slot : Progression.EquippedAbilitySlots)
    {
        if (Slot.CharacterId.IsNone() || Slot.SlotId.IsNone() || Slot.AbilityId.IsNone())
        {
            return false;
        }

        const FString Key = Slot.CharacterId.ToString() + TEXT("|") + Slot.SlotId.ToString();
        if (LoadoutKeys.Contains(Key))
        {
            return false;
        }
        LoadoutKeys.Add(Key);
    }

    return true;
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

        // Retry once only when a newer complete snapshot arrived during the failed write.
        if (FJumanjiPlayerProfileSnapshot* QueuedSnapshot = QueuedSnapshots.Find(SlotName))
        {
            const FJumanjiPlayerProfileSnapshot LatestSnapshot = *QueuedSnapshot;
            QueuedSnapshots.Remove(SlotName);

            if (!StartAsyncSave(SlotName, LatestSnapshot))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("JUMANJI profile: failed to start one retry for slot '%s'."),
                    *SlotName);
            }
        }
        return;
    }

    if (FJumanjiPlayerProfileSnapshot* QueuedSnapshot = QueuedSnapshots.Find(SlotName))
    {
        const FJumanjiPlayerProfileSnapshot LatestSnapshot = *QueuedSnapshot;
        QueuedSnapshots.Remove(SlotName);

        if (!StartAsyncSave(SlotName, LatestSnapshot))
        {
            UE_LOG(LogTemp, Error,
                TEXT("JUMANJI profile: failed to start queued save for slot '%s'."),
                *SlotName);
        }
    }
}
