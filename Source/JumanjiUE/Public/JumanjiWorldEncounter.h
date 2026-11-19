#pragma once

#include "CoreMinimal.h"
#include "JumanjiWorldEncounter.generated.h"

class AJumanjiBossCharacter;

UENUM(BlueprintType)
enum class EJumanjiWorldEncounterPhase : uint8
{
    Inactive UMETA(DisplayName = "Inactive"),
    Trials UMETA(DisplayName = "Great Spirit Trials"),
    Combat UMETA(DisplayName = "Enemy Waves"),
    Boss UMETA(DisplayName = "Boss Encounter"),
    Reward UMETA(DisplayName = "Reward Processing"),
    Completed UMETA(DisplayName = "Completed"),
    Failed UMETA(DisplayName = "Failed")
};

/** Shared, replicated encounter progress. The server is the only writer. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiWorldEncounterState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    FName EncounterId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    EJumanjiWorldEncounterPhase Phase = EJumanjiWorldEncounterPhase::Inactive;

    /** One-based during combat; zero while trials, boss, reward or terminal state is active. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    int32 CurrentWave = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    int32 TotalWaves = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    int32 ActiveEnemyCount = 0;

    /** Replicated actor reference for the encounter HUD. Set by the server when the boss spawns. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    TObjectPtr<AJumanjiBossCharacter> BossActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    int32 Revision = 0;
};


/**
 * One-shot notice for a PlayerState that rejoined its still-running encounter.
 * This is transient replicated session state, never part of the profile SaveGame.
 */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiEncounterReconnectState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    bool bWasReconnected = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    FName EncounterId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    EJumanjiWorldEncounterPhase Phase = EJumanjiWorldEncounterPhase::Inactive;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    int32 EncounterRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    int32 ReconnectRevision = 0;
};


/** Outcome shown to the reconnecting local player. None is silent for normal logins. */
UENUM(BlueprintType)
enum class EJumanjiEncounterReconnectOutcome : uint8
{
    None UMETA(DisplayName = "No reconnect notice"),
    Reconnected UMETA(DisplayName = "Reconnected"),
    GraceWindowExpired UMETA(DisplayName = "Reconnect window expired"),
    EncounterEnded UMETA(DisplayName = "Encounter ended while away")
};

/**
 * Transient UI feedback for one PostLogin attempt. It is replicated to the
 * returning PlayerState, but is never serialized into a player profile.
 */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiEncounterReconnectFeedbackState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    EJumanjiEncounterReconnectOutcome Outcome = EJumanjiEncounterReconnectOutcome::None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    FName EncounterId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    EJumanjiWorldEncounterPhase Phase = EJumanjiWorldEncounterPhase::Inactive;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    int32 Revision = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter|Reconnect")
    float DisplayDurationSeconds = 6.0f;
};
