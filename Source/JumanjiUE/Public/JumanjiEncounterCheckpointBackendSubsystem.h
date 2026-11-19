#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiEncounterCheckpointBackendSubsystem.generated.h"

class FJsonObject;

struct FJumanjiCheckpointBackendResponse
{
    bool bTransportSucceeded = false;
    int32 HttpStatusCode = 0;
    FString ResponseBody;
};

DECLARE_DELEGATE_OneParam(
    FJumanjiCheckpointBackendCallback,
    const FJumanjiCheckpointBackendResponse&);

/**
 * Native-only server adapter for the trusted checkpoint service.
 *
 * The adapter never stores secrets in config or SaveGame. It reads the server
 * bearer credential and shard identity from the server process environment.
 * Every request is rejected in NM_Client. This class does not build snapshots
 * from live actors and does not resume or publish an encounter.
 */
UCLASS(Config = Game)
class JUMANJIUE_API UJumanjiEncounterCheckpointBackendSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    bool AcquireLease(
        FName EncounterId,
        int32 LeaseSeconds,
        FJumanjiCheckpointBackendCallback Callback);

    bool RenewLease(
        FName EncounterId,
        const FString& LeaseToken,
        int64 AuthorityEpoch,
        int32 LeaseSeconds,
        FJumanjiCheckpointBackendCallback Callback);

    bool ReleaseLease(
        FName EncounterId,
        const FString& LeaseToken,
        int64 AuthorityEpoch,
        FJumanjiCheckpointBackendCallback Callback);

    bool WriteCheckpoint(
        FName EncounterId,
        const FString& LeaseToken,
        int64 AuthorityEpoch,
        int64 ExpectedSequence,
        const FString& PayloadJson,
        FJumanjiCheckpointBackendCallback Callback);

    bool ReadCheckpoint(
        FName EncounterId,
        const FString& LeaseToken,
        int64 AuthorityEpoch,
        FJumanjiCheckpointBackendCallback Callback);

private:
    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Checkpoint")
    FString ServiceBaseUrl;

    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Checkpoint", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float RequestTimeoutSeconds = 8.0f;

    bool IsServerRuntime() const;
    bool IsValidEncounterId(FName EncounterId) const;
    bool BuildServerIdentity(FString& OutServerInstanceId, FString& OutShardId) const;
    bool SendJsonRequest(
        const FString& Verb,
        const FString& Route,
        const TSharedRef<FJsonObject>& Body,
        FJumanjiCheckpointBackendCallback Callback) const;

    static TSharedRef<FJsonObject> MakeIdentityBody(
        const FString& ServerInstanceId,
        const FString& ShardId);
    static bool SerializeJsonObject(const TSharedRef<FJsonObject>& Object, FString& OutJson);
    static void CompleteLocalFailure(
        const FJumanjiCheckpointBackendCallback& Callback,
        const FString& ErrorCode,
        const FString& Message);
};
