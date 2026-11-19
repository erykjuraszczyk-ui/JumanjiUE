#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiEncounterCheckpointTypes.h"
#include "JumanjiEncounterCheckpointRuntimeSubsystem.generated.h"

class UJumanjiEncounterCheckpointBackendSubsystem;

/**
 * Server-only logical snapshot builder and private restore staging service.
 *
 * Stage 34 does not spawn actors or publish a resumed GameState. A validated
 * restore plan remains private in this subsystem for a later hydration stage.
 */
UCLASS()
class JUMANJIUE_API UJumanjiEncounterCheckpointRuntimeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Obtain the exclusive lease and reconcile the current backend sequence. */
    bool AcquireEncounterLease(
        FName EncounterId,
        FJumanjiCheckpointRuntimeCallback Callback);

    /** Serialize and persist one explicitly server-confirmed safe-point snapshot. */
    bool CaptureSafePoint(
        const FJumanjiEncounterCheckpointCaptureInput& Input,
        FJumanjiCheckpointRuntimeCallback Callback);

    /** Read, validate and stage the latest signed checkpoint without mutating the world. */
    bool ReadAndStageRestorePlan(
        const FJumanjiEncounterCheckpointRestoreExpectations& Expectations,
        FJumanjiCheckpointRuntimeCallback Callback);

    /** Returns a copy of a fully validated, private, value-only restore plan. */
    bool GetStagedRestorePlan(FJumanjiEncounterCheckpointRestorePlan& OutPlan) const;

    /** Clear only the private staging buffer. Does not mutate GameState or actors. */
    void ClearStagedRestorePlan();

    /** Native integration point for a trusted profile service; no client RPC or Blueprint exposure. */
    void SetParticipantRevisionValidator(TFunction<bool(const FString&, int64)> Validator);

    /** Release the current lease. Never captures implicitly during EndPlay. */
    bool ReleaseEncounterLease(FJumanjiCheckpointRuntimeCallback Callback);

    bool HasActiveLease() const { return bLeaseReady; }
    int64 GetLastCheckpointSequence() const { return LastCheckpointSequence; }

private:
    bool IsServerRuntime() const;
    bool BuildCheckpointPayload(
        const FJumanjiEncounterCheckpointCaptureInput& Input,
        int64 Sequence,
        FString& OutPayloadJson,
        FString& OutErrorCode,
        FString& OutErrorMessage) const;

    bool ParseAndValidateRestoreEnvelope(
        const FString& ResponseBody,
        const FJumanjiEncounterCheckpointRestoreExpectations& Expectations,
        int64 BackendSequence,
        FJumanjiEncounterCheckpointRestorePlan& OutPlan,
        FString& OutErrorCode,
        FString& OutErrorMessage) const;

    bool ValidateCaptureInput(
        const FJumanjiEncounterCheckpointCaptureInput& Input,
        FString& OutErrorCode,
        FString& OutErrorMessage) const;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiEncounterCheckpointBackendSubsystem> BackendSubsystem;

    FName ActiveEncounterId = NAME_None;
    FString LeaseToken;
    int64 AuthorityEpoch = 0;
    int64 LastCheckpointSequence = 0;
    bool bLeaseReady = false;
    bool bLeaseRequestPending = false;
    bool bCaptureRequestPending = false;

    TFunction<bool(const FString&, int64)> ParticipantRevisionValidator;

    bool bHasStagedRestorePlan = false;
    FJumanjiEncounterCheckpointRestorePlan StagedRestorePlan;
};
