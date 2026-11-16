#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiPlayerProfileSaveGame.h"
#include "JumanjiPlayerProfileSubsystem.generated.h"

/**
 * Server-process profile persistence for the current single-server prototype.
 *
 * Schema v1 profiles are migrated in memory to v2 on load. The next accepted
 * save writes the v2 snapshot. For a production shared-world MMO this local
 * backend must be replaced by an authenticated transactional profile service.
 */
UCLASS()
class JUMANJIUE_API UJumanjiPlayerProfileSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Profile")
    bool LoadProfile(const FString& ProfileId, FJumanjiPlayerProfileSnapshot& OutSnapshot) const;

    /** A true return means accepted/queued, not that platform I/O has completed. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Profile")
    bool SaveProfile(const FJumanjiPlayerProfileSnapshot& Snapshot);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Profile")
    bool HasProfile(const FString& ProfileId) const;

private:
    static constexpr int32 CurrentSchemaVersion = 2;
    static constexpr int32 LegacySchemaVersion = 1;
    static constexpr int32 MaxRegions = 32;
    static constexpr int32 MaxLandmarks = 128;
    static constexpr int32 MaxCompletedChapters = 64;
    static constexpr int32 MaxCurrencies = 8;
    static constexpr int32 MaxInventoryStacks = 256;
    static constexpr int32 MaxCharacterProgressEntries = 9;
    static constexpr int32 MaxEquippedAbilitySlots = 45;
    static constexpr int32 MaxProcessedTransactions = 2048;

    FString BuildSlotName(const FString& ProfileId) const;
    bool IsValidProfileId(const FString& ProfileId) const;
    bool IsValidLegacyV1Snapshot(const FJumanjiPlayerProfileSnapshot& Snapshot) const;
    bool IsValidSnapshot(const FJumanjiPlayerProfileSnapshot& Snapshot) const;
    bool StartAsyncSave(const FString& SlotName, const FJumanjiPlayerProfileSnapshot& Snapshot);

    void HandleSaveCompleted(const FString& SlotName, const int32 UserIndex, bool bSuccess);

    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UJumanjiPlayerProfileSaveGame>> PendingSaveObjects;

    TMap<FString, FJumanjiPlayerProfileSnapshot> QueuedSnapshots;
    TSet<FString> InFlightSlots;
};
