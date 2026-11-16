#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiPlayerProfileSaveGame.h"
#include "JumanjiPlayerProfileSubsystem.generated.h"

/**
 * Server-process profile persistence for the current prototype.
 *
 * A profile key must come from a trusted platform/account identity. The client
 * never supplies the slot name or ProfileId. This local SaveGame backend can
 * later be replaced by a remote profile repository without changing PlayerState.
 */
UCLASS()
class JUMANJIUE_API UJumanjiPlayerProfileSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Profile")
    bool LoadProfile(const FString& ProfileId, FJumanjiPlayerProfileSnapshot& OutSnapshot) const;

    /**
     * Queues an asynchronous save. A true return means accepted/queued, not
     * that the platform has completed writing the slot yet.
     */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Profile")
    bool SaveProfile(const FJumanjiPlayerProfileSnapshot& Snapshot);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Profile")
    bool HasProfile(const FString& ProfileId) const;

private:
    static constexpr int32 CurrentSchemaVersion = 1;
    static constexpr int32 MaxRegions = 32;
    static constexpr int32 MaxLandmarks = 128;
    static constexpr int32 MaxCompletedChapters = 64;

    FString BuildSlotName(const FString& ProfileId) const;
    bool IsValidProfileId(const FString& ProfileId) const;
    bool IsValidSnapshot(const FJumanjiPlayerProfileSnapshot& Snapshot) const;
    bool StartAsyncSave(const FString& SlotName, const FJumanjiPlayerProfileSnapshot& Snapshot);

    void HandleSaveCompleted(const FString& SlotName, const int32 UserIndex, bool bSuccess);

    /** Keeps SaveGame UObjects alive until their async write callback returns. */
    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UJumanjiPlayerProfileSaveGame>> PendingSaveObjects;

    /** Latest snapshot received while a write for the same slot is in flight. */
    TMap<FString, FJumanjiPlayerProfileSnapshot> QueuedSnapshots;

    TSet<FString> InFlightSlots;
};
