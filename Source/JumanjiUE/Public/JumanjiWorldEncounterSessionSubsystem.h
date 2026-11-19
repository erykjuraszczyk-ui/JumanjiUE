#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiWorldEncounter.h"
#include "JumanjiWorldEncounterSessionSubsystem.generated.h"

class AJumanjiPlayerState;

/**
 * Volatile, server-process encounter membership registry.
 *
 * It tracks verified profile IDs for one live encounter, a short reconnect
 * grace window, and bounded terminal-result tombstones for disconnected
 * participants. It stores no live actors and never writes profile data.
 */
UCLASS(Config = Game)
class JUMANJIUE_API UJumanjiWorldEncounterSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool BeginEncounterSession(FName EncounterId, AJumanjiPlayerState* InitiatingPlayer);

    /** Server-native restore path; adopts only the verified session UUID and mapped profiles. */
    bool BeginRestoredEncounterSession(
        FName EncounterId,
        FGuid RestoredSessionId,
        const TArray<AJumanjiPlayerState*>& RestoredParticipants);

    bool RegisterConnectedParticipant(AJumanjiPlayerState* PlayerState);
    bool MarkPlayerDisconnected(const AJumanjiPlayerState* PlayerState);

    /**
     * Reattaches a same-server participant inside the grace window.
     * OutFeedbackState remains None for ordinary logins that were not
     * disconnected members of an encounter.
     */
    bool TryRejoinPlayer(
        AJumanjiPlayerState* RejoiningPlayer,
        FJumanjiEncounterReconnectState& OutReconnectState,
        FJumanjiEncounterReconnectFeedbackState& OutFeedbackState
    );

    /** Backward-compatible overload for callers that only need the success snapshot. */
    bool TryRejoinPlayer(
        AJumanjiPlayerState* RejoiningPlayer,
        FJumanjiEncounterReconnectState& OutReconnectState
    );

    void EndEncounterSession(FName EncounterId);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter|Session")
    FName GetActiveEncounterId() const { return ActiveEncounterId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter|Session")
    int32 GetParticipantCount() const { return ParticipantProfileIds.Num(); }

    /** Volatile UUID for the active encounter session; never persisted in a player profile. */
    FGuid GetActiveSessionId() const { return ActiveSessionId; }

protected:
    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Encounter|Session",
        meta = (ClampMin = "30.0", ClampMax = "1800.0"))
    float ReconnectGraceWindowSeconds = 300.0f;

private:
    bool IsValidProfileKey(const FString& ProfileId) const;
    void PruneRecentlyEndedParticipants();
    void StoreFeedbackRevision(FJumanjiEncounterReconnectFeedbackState& OutFeedbackState);

    /** Active-session membership is held only in server process memory. */
    FName ActiveEncounterId = NAME_None;
    FGuid ActiveSessionId;
    TSet<FString> ParticipantProfileIds;
    TSet<FString> ConnectedProfileIds;
    TMap<FString, FDateTime> DisconnectedAtUtc;
    int32 ReconnectRevision = 0;

    /** Bounded, short-lived tombstones let a returning participant learn that the encounter ended. */
    TMap<FString, FName> RecentlyEndedEncounterIds;
    TMap<FString, FDateTime> RecentlyEndedAtUtc;
    TMap<FString, EJumanjiWorldEncounterPhase> RecentlyEndedPhases;

    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Encounter|Session",
        meta = (ClampMin = "60.0", ClampMax = "3600.0"))
    float TerminalNoticeRetentionSeconds = 600.0f;

    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Encounter|Session",
        meta = (ClampMin = "16", ClampMax = "512"))
    int32 MaxTerminalNoticeEntries = 256;
};
