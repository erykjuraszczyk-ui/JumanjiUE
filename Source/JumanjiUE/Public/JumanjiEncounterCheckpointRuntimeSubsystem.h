#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "JumanjiEncounterCheckpointRuntimeSubsystem.generated.h"

class AJumanjiWorldEncounterDirector;
class UJumanjiEncounterCheckpointBackendSubsystem;

UENUM()
enum class EJumanjiEncounterCheckpointSafePoint : uint8
{
    TrialBoundary,
    WaveBoundary,
    BossPhaseBoundary,
    RewardPending
};

enum class EJumanjiEncounterRecoveryOutcome : uint8
{
    None,
    SessionStarted,
    LeaseAcquired,
    LeaseRenewed,
    LeaseAcquireFailed,
    LeaseRenewalFailed,
    CheckpointWriteStarted,
    CheckpointWriteCommitted,
    CheckpointWriteFailed,
    RestoreReadStarted,
    RestoreReadFailed,
    RestoreCandidateStaged,
    RestoreWaitingForParticipants,
    RestoreCommitted,
    RestoreRejected,
    RestoreTimedOut,
    OperatorRetryRequested,
    OperatorAbortCompleted,
    LeaseReleased
};

/** Reasons accepted by the trusted host's operator-abort integration. */
enum class EJumanjiEncounterRecoveryAbortReason : uint8
{
    OperatorCancelled,
    MaintenanceWindow,
    ConfigurationCorrection,
    IncidentContainment
};

/**
 * Secret-free, process-local recovery diagnostics. This is a native C++ value
 * object: it is not replicated, Blueprint-reflected, persisted or sent to clients.
 */
struct FJumanjiEncounterRecoveryDiagnostics
{
    FGuid CorrelationId;
    FName EncounterId = NAME_None;
    EJumanjiEncounterRecoveryOutcome LastOutcome = EJumanjiEncounterRecoveryOutcome::None;
    FName LastErrorCode = NAME_None;
    FName LastOperatorAbortReason = NAME_None;
    int32 LastHttpStatusCode = 0;
    int64 CheckpointSequence = 0;
    int64 AuthorityEpoch = 0;
    bool bLeaseAcquiring = false;
    bool bLeaseReady = false;
    bool bReadInFlight = false;
    bool bWriteInFlight = false;
    bool bCheckpointWriteBlockedUntilResync = false;
    bool bHasStagedRestoreCandidate = false;
    bool bWaitingForParticipants = false;
    bool bRestoreCommitInProgress = false;
    bool bReleaseLeaseWhenIdle = false;
    double RecoveryAgeSeconds = 0.0;
    uint64 LeaseAcquisitionAttempts = 0;
    uint64 LeaseAcquisitionFailures = 0;
    uint64 LeaseRenewalAttempts = 0;
    uint64 LeaseRenewalFailures = 0;
    uint64 CheckpointWriteAttempts = 0;
    uint64 CheckpointWriteFailures = 0;
    uint64 RestoreReadAttempts = 0;
    uint64 RestoreReadFailures = 0;
    uint64 RestoreCommitAttempts = 0;
    uint64 RestoreCommitFailures = 0;
    uint64 RestoreCommitSuccesses = 0;
    uint64 ParticipantWaitTimeouts = 0;
    uint64 OperatorRetryRequests = 0;
    uint64 OperatorAbortRequests = 0;
};

/**
 * Private, validated logical restore plan. It deliberately contains no UObject,
 * actor reference, transform, AI state, timer, profile ID, inventory or wallet.
 * The candidate is replaced atomically only after every preflight check passes.
 */
struct JUMANJIUE_API FJumanjiEncounterCheckpointRestoreCandidate
{
    FName EncounterId = NAME_None;
    FGuid SessionId;
    int64 CheckpointSequence = 0;
    int64 PreviousAuthorityEpoch = 0;
    FName Phase = NAME_None;
    FName SafePointKind = NAME_None;
    int32 EncounterRevision = 0;
    FName ChapterId = NAME_None;
    FName BoardEventId = NAME_None;
    int32 ChapterRevision = 0;
    TArray<FName> CompletedObjectiveIds;
    TArray<FName> CompletedTrialIds;
    int32 CurrentWave = 0;
    int32 TotalWaves = 0;
    FName BossDefinitionId = NAME_None;
    FName BossPhaseId = NAME_None;
    float BossHealthFraction = 0.0f;
    int32 BossPhaseRevision = 0;
    uint32 EncounterSeed = 0;
    FName SpawnSetId = NAME_None;
    TArray<FString> ParticipantKeys;
    TArray<int64> ParticipantProfileRevisions;
    FName RewardDefinitionId = NAME_None;
    FName RewardTransactionId = NAME_None;
};

/**
 * Server-only bridge between the live encounter director and the trusted
 * Stage 33 checkpoint backend. Captures only logical state at explicit,
 * server-approved safe points and stages verified reads without publishing
 * partially recovered gameplay state.
 */
UCLASS()
class JUMANJIUE_API UJumanjiEncounterCheckpointRuntimeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Starts a lease for a newly-started encounter. Failure never fabricates a local checkpoint. */
    bool BeginEncounterCheckpointSession(FName EncounterId);

    /** Best-effort lease release after a terminal encounter state. */
    void EndEncounterCheckpointSession(FName EncounterId);

    /** Build and submit one schema-v1 logical snapshot at a stable safe point. */
    bool CaptureSafePoint(
        AJumanjiWorldEncounterDirector* EncounterDirector,
        EJumanjiEncounterCheckpointSafePoint SafePoint,
        FName RewardTransactionId = NAME_None);

    /** Acquire a recovery lease, read the signed checkpoint and preflight it into private staging. */
    bool BeginRestoreStaging(AJumanjiWorldEncounterDirector* EncounterDirector);

    /**
     * Retry the staged server restore after another participant has completed
     * authentication and profile restoration. A missing participant is pending,
     * not a permanent checkpoint failure.
     */
    bool TryCommitStagedRestore(AJumanjiWorldEncounterDirector* EncounterDirector);

    bool IsWaitingForRestoreParticipants() const { return bWaitingForRestoreParticipants; }

    bool HasStagedRestoreCandidate() const { return bHasStagedRestoreCandidate; }
    const FJumanjiEncounterCheckpointRestoreCandidate& GetStagedRestoreCandidate() const
    {
        return StagedRestoreCandidate;
    }

    bool IsCheckpointLeaseReady() const { return bLeaseReady; }
    int64 GetLastCommittedSequence() const { return LastCommittedSequence; }

    /** Secret-free process-local counters and state. Native C++ only. */
    FJumanjiEncounterRecoveryDiagnostics GetRecoveryDiagnostics() const;

    /**
     * Trusted host/operator integration only. Caller must authenticate and
     * authorize the operator before invoking this native server-only method.
     */
    bool RequestOperatorRetry(
        AJumanjiWorldEncounterDirector* EncounterDirector,
        FString& OutError);

    /** Cancels only a staged restore waiting for authenticated participants. */
    bool RequestOperatorAbort(
        EJumanjiEncounterRecoveryAbortReason Reason,
        FString& OutError);

private:
    void RecordRecoveryEvent(
        EJumanjiEncounterRecoveryOutcome Outcome,
        FName ErrorCode = NAME_None,
        int32 HttpStatusCode = 0);
    static FName RecoveryOutcomeCode(EJumanjiEncounterRecoveryOutcome Outcome);
    static bool IsValidOperatorAbortReason(EJumanjiEncounterRecoveryAbortReason Reason);

    bool IsServerRuntime() const;
    bool ReadRequiredServerMetadata(
        FString& OutServerInstanceId,
        FString& OutShardId,
        FString& OutBuildId,
        FString& OutRulesetVersion,
        FString& OutWorldContentVersion,
        FString& OutParticipantSalt) const;

    bool BuildPayloadJson(
        AJumanjiWorldEncounterDirector* EncounterDirector,
        EJumanjiEncounterCheckpointSafePoint SafePoint,
        FName RewardTransactionId,
        int64 CheckpointSequence,
        FString& OutPayloadJson,
        FString& OutError) const;

    bool BuildOpaqueParticipantKey(
        const FString& PersistentProfileId,
        const FString& Salt,
        FString& OutParticipantKey) const;

    bool StageVerifiedBackendResponse(
        const FString& ResponseBody,
        AJumanjiWorldEncounterDirector* EncounterDirector,
        FString& OutError);

    /** Reconciles live identities/profiles, then asks the director to materialize and publish. */
    bool CommitStagedRestore(
        AJumanjiWorldEncounterDirector* EncounterDirector,
        FString& OutError);

    void HandleLeaseAcquired(const struct FJumanjiCheckpointBackendResponse& Response);
    void HandleCheckpointRead(const struct FJumanjiCheckpointBackendResponse& Response,
        TWeakObjectPtr<AJumanjiWorldEncounterDirector> EncounterDirector);
    void HandleCheckpointWrite(const struct FJumanjiCheckpointBackendResponse& Response, int64 WrittenSequence);
    void HandleRestoreParticipantWaitExpired();
    void RenewLease();
    void ClearLeaseState();

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiEncounterCheckpointBackendSubsystem> BackendSubsystem;

    FGuid RecoveryCorrelationId;
    double RecoveryStartedAtSeconds = 0.0;
    EJumanjiEncounterRecoveryOutcome LastRecoveryOutcome = EJumanjiEncounterRecoveryOutcome::None;
    FName LastRecoveryErrorCode = NAME_None;
    FName LastOperatorAbortReason = NAME_None;
    int32 LastRecoveryHttpStatusCode = 0;
    uint64 LeaseAcquisitionAttempts = 0;
    uint64 LeaseAcquisitionFailures = 0;
    uint64 LeaseRenewalAttempts = 0;
    uint64 LeaseRenewalFailures = 0;
    uint64 CheckpointWriteAttempts = 0;
    uint64 CheckpointWriteFailures = 0;
    uint64 RestoreReadAttempts = 0;
    uint64 RestoreReadFailures = 0;
    uint64 RestoreCommitAttempts = 0;
    uint64 RestoreCommitFailures = 0;
    uint64 RestoreCommitSuccesses = 0;
    uint64 ParticipantWaitTimeouts = 0;
    uint64 OperatorRetryRequests = 0;
    uint64 OperatorAbortRequests = 0;
    int64 LastKnownCheckpointSequence = 0;
    int64 LastKnownAuthorityEpoch = 0;

    FName ActiveEncounterId = NAME_None;
    FGuid ActiveSessionId;
    FString LeaseToken;
    int64 AuthorityEpoch = 0;
    int64 LastCommittedSequence = 0;
    bool bLeaseReady = false;
    bool bLeaseAcquiring = false;
    bool bWriteInFlight = false;
    bool bReadInFlight = false;
    bool bCheckpointWriteBlockedUntilResync = false;
    bool bRestoreStagingRequested = false;
    bool bReleaseLeaseWhenIdle = false;
    bool bHasStagedRestoreCandidate = false;
    bool bRestoreCommitInProgress = false;
    bool bWaitingForRestoreParticipants = false;

    TWeakObjectPtr<AJumanjiWorldEncounterDirector> PendingRestoreDirector;
    FTimerHandle LeaseRenewalTimerHandle;
    FTimerHandle RestoreParticipantWaitTimerHandle;
    FJumanjiEncounterCheckpointRestoreCandidate StagedRestoreCandidate;
};