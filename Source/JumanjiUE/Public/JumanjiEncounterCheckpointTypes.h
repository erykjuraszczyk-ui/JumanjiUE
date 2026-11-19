#pragma once

#include "CoreMinimal.h"
#include "JumanjiWorldEncounter.h"
#include "JumanjiEncounterCheckpointTypes.generated.h"

/** Opaque identity supplied by a trusted profile service; never a raw ProfileId. */
USTRUCT()
struct FJumanjiCheckpointParticipant
{
    GENERATED_BODY()

    UPROPERTY()
    FString ParticipantKey;

    UPROPERTY()
    int64 ProfileRevision = 0;
};

/**
 * Plain logical capture input. It intentionally contains no UObject, actor,
 * transform, velocity, AI, timer, inventory, wallet or raw profile identity.
 * Construct this only from authoritative server state after a safe barrier.
 */
USTRUCT()
struct FJumanjiEncounterCheckpointCaptureInput
{
    GENERATED_BODY()

    UPROPERTY()
    FName EncounterId = NAME_None;

    UPROPERTY()
    FGuid SessionId;

    UPROPERTY()
    FName ChapterId = NAME_None;

    UPROPERTY()
    FName BoardEventId = NAME_None;

    UPROPERTY()
    int32 EncounterRevision = 0;

    UPROPERTY()
    int32 ChapterRevision = 0;

    UPROPERTY()
    TArray<FName> CompletedObjectiveIds;

    UPROPERTY()
    TArray<FName> CompletedTrialIds;

    UPROPERTY()
    EJumanjiWorldEncounterPhase Phase = EJumanjiWorldEncounterPhase::Inactive;

    UPROPERTY()
    int32 CurrentWave = 0;

    UPROPERTY()
    int32 TotalWaves = 0;

    UPROPERTY()
    uint32 EncounterSeed = 0;

    UPROPERTY()
    FName SpawnSetId = NAME_None;

    UPROPERTY()
    FName BossDefinitionId = NAME_None;

    UPROPERTY()
    int32 BossPhase = 0;

    UPROPERTY()
    float BossHealthFraction = 0.0f;

    UPROPERTY()
    int32 BossPhaseRevision = 0;

    UPROPERTY()
    FName RewardDefinitionId = NAME_None;

    UPROPERTY()
    FName RewardTransactionId = NAME_None;

    UPROPERTY()
    TArray<FJumanjiCheckpointParticipant> Participants;

    /** These are server-derived barrier checks, not client-supplied claims. */
    UPROPERTY()
    bool bServerConfirmedSafeBoundary = false;

    UPROPERTY()
    bool bNoActiveTraversal = false;

    UPROPERTY()
    bool bNoTrialInProgress = false;

    UPROPERTY()
    bool bNoPendingSpawn = false;

    UPROPERTY()
    bool bNoActiveEnemies = false;

    UPROPERTY()
    bool bNoActiveBossTelegraph = false;

    UPROPERTY()
    bool bRewardTransactionStable = false;
};

/** Current server content/build identity required for restore compatibility. */
USTRUCT()
struct FJumanjiEncounterCheckpointRestoreExpectations
{
    GENERATED_BODY()

    UPROPERTY()
    FName EncounterId = NAME_None;

    UPROPERTY()
    FName ChapterId = NAME_None;

    UPROPERTY()
    FName BoardEventId = NAME_None;

    UPROPERTY()
    FName SpawnSetId = NAME_None;

    UPROPERTY()
    FName BossDefinitionId = NAME_None;

    UPROPERTY()
    FString ServerBuildId;

    UPROPERTY()
    FString RulesetVersion;

    UPROPERTY()
    FString WorldContentVersion;
};

/**
 * Private restore staging object. This is a value-only plan; it never contains
 * actor pointers, object paths, transforms, timers or mutable economy state.
 */
USTRUCT()
struct FJumanjiEncounterCheckpointRestorePlan
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid CheckpointId;

    UPROPERTY()
    FGuid SessionId;

    UPROPERTY()
    FName EncounterId = NAME_None;

    UPROPERTY()
    FName ChapterId = NAME_None;

    UPROPERTY()
    FName BoardEventId = NAME_None;

    UPROPERTY()
    EJumanjiWorldEncounterPhase Phase = EJumanjiWorldEncounterPhase::Inactive;

    UPROPERTY()
    int64 CheckpointSequence = 0;

    UPROPERTY()
    int64 SourceAuthorityEpoch = 0;

    UPROPERTY()
    int32 EncounterRevision = 0;

    UPROPERTY()
    int32 ChapterRevision = 0;

    UPROPERTY()
    TArray<FName> CompletedObjectiveIds;

    UPROPERTY()
    TArray<FName> CompletedTrialIds;

    UPROPERTY()
    int32 CurrentWave = 0;

    UPROPERTY()
    int32 TotalWaves = 0;

    UPROPERTY()
    uint32 EncounterSeed = 0;

    UPROPERTY()
    FName SpawnSetId = NAME_None;

    UPROPERTY()
    FName BossDefinitionId = NAME_None;

    UPROPERTY()
    int32 BossPhase = 0;

    UPROPERTY()
    float BossHealthFraction = 0.0f;

    UPROPERTY()
    int32 BossPhaseRevision = 0;

    UPROPERTY()
    FName RewardDefinitionId = NAME_None;

    UPROPERTY()
    FName RewardTransactionId = NAME_None;

    UPROPERTY()
    TArray<FJumanjiCheckpointParticipant> Participants;
};

struct FJumanjiCheckpointRuntimeResult
{
    bool bSucceeded = false;
    FString ErrorCode;
    FString Message;
    int64 CheckpointSequence = 0;
};

DECLARE_DELEGATE_OneParam(
    FJumanjiCheckpointRuntimeCallback,
    const FJumanjiCheckpointRuntimeResult&);
