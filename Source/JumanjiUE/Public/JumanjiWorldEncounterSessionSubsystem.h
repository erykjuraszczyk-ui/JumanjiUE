#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiWorldEncounterSessionSubsystem.generated.h"

class AJumanjiPlayerState;

/**
 * Volatile, server-process encounter membership registry.
 *
 * It tracks profile IDs that participated in the currently running encounter
 * and a short disconnect grace window. It intentionally stores no live actor
 * references and does not write any encounter data into the player profile.
 */
UCLASS(Config = Game)
class JUMANJIUE_API UJumanjiWorldEncounterSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Starts a new in-memory session and registers its initiating profile. */
    bool BeginEncounterSession(FName EncounterId, AJumanjiPlayerState* InitiatingPlayer);

    /** Marks a registered participant disconnected during the active encounter. */
    bool MarkPlayerDisconnected(const AJumanjiPlayerState* PlayerState);

    /**
     * Reattaches a reconnecting profile to the currently active same-server
     * encounter when it disconnected inside the grace window.
     */
    bool TryRejoinPlayer(
        AJumanjiPlayerState* RejoiningPlayer,
        FJumanjiEncounterReconnectState& OutReconnectState
    );

    /** Clears volatile membership when an encounter reaches a terminal state. */
    void EndEncounterSession(FName EncounterId);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter|Session")
    FName GetActiveEncounterId() const { return ActiveEncounterId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter|Session")
    int32 GetParticipantCount() const { return ParticipantProfileIds.Num(); }

protected:
    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Encounter|Session",
        meta = (ClampMin = "30.0", ClampMax = "1800.0"))
    float ReconnectGraceWindowSeconds = 300.0f;

private:
    bool IsValidProfileKey(const FString& ProfileId) const;

    /** These values exist only in server process memory. */
    FName ActiveEncounterId = NAME_None;
    TSet<FString> ParticipantProfileIds;
    TSet<FString> ConnectedProfileIds;
    TMap<FString, FDateTime> DisconnectedAtUtc;
    int32 ReconnectRevision = 0;
};
