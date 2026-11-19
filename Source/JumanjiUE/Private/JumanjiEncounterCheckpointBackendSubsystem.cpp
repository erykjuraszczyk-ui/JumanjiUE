#include "JumanjiEncounterCheckpointBackendSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/World.h"

namespace
{
    FString ReadServerEnvironment(const TCHAR* VariableName)
    {
        return FPlatformMisc::GetEnvironmentVariable(VariableName).TrimStartAndEnd();
    }

    bool IsSafeServerId(const FString& Value)
    {
        if (Value.Len() < 3 || Value.Len() > 96)
        {
            return false;
        }

        for (const TCHAR Character : Value)
        {
            const bool bAllowed =
                (Character >= TEXT('A') && Character <= TEXT('Z'))
                || (Character >= TEXT('a') && Character <= TEXT('z'))
                || (Character >= TEXT('0') && Character <= TEXT('9'))
                || Character == TEXT('_')
                || Character == TEXT('-');
            if (!bAllowed)
            {
                return false;
            }
        }

        return true;
    }

    bool IsAllowedEndpoint(const FString& Url)
    {
        if (Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
        {
            return true;
        }

        // Plain HTTP is permitted only for explicit loopback development.
        return Url.StartsWith(TEXT("http://127.0.0.1:"), ESearchCase::IgnoreCase)
            || Url.StartsWith(TEXT("http://localhost:"), ESearchCase::IgnoreCase)
            || Url.Equals(TEXT("http://127.0.0.1"), ESearchCase::IgnoreCase)
            || Url.Equals(TEXT("http://localhost"), ESearchCase::IgnoreCase);
    }

    void AddLeaseFields(
        const TSharedRef<FJsonObject>& Body,
        const FString& LeaseToken,
        int64 AuthorityEpoch)
    {
        Body->SetStringField(TEXT("leaseToken"), LeaseToken);
        Body->SetNumberField(TEXT("authorityEpoch"), static_cast<double>(AuthorityEpoch));
    }
}

bool UJumanjiEncounterCheckpointBackendSubsystem::IsServerRuntime() const
{
    const UWorld* World = GetWorld();
    return World && World->GetNetMode() != NM_Client;
}

bool UJumanjiEncounterCheckpointBackendSubsystem::IsValidEncounterId(FName EncounterId) const
{
    const FString Value = EncounterId.ToString();
    if (Value.Len() < 3 || Value.Len() > 96)
    {
        return false;
    }

    for (const TCHAR Character : Value)
    {
        if (!((Character >= TEXT('A') && Character <= TEXT('Z'))
            || (Character >= TEXT('0') && Character <= TEXT('9'))
            || Character == TEXT('_')))
        {
            return false;
        }
    }

    return true;
}

bool UJumanjiEncounterCheckpointBackendSubsystem::BuildServerIdentity(
    FString& OutServerInstanceId,
    FString& OutShardId) const
{
    if (!IsServerRuntime())
    {
        return false;
    }

    OutServerInstanceId = ReadServerEnvironment(TEXT("JUMANJI_SERVER_INSTANCE_ID"));
    OutShardId = ReadServerEnvironment(TEXT("JUMANJI_SHARD_ID"));
    return IsSafeServerId(OutServerInstanceId) && IsSafeServerId(OutShardId);
}

TSharedRef<FJsonObject> UJumanjiEncounterCheckpointBackendSubsystem::MakeIdentityBody(
    const FString& ServerInstanceId,
    const FString& ShardId)
{
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("serverInstanceId"), ServerInstanceId);
    Body->SetStringField(TEXT("shardId"), ShardId);
    return Body;
}

bool UJumanjiEncounterCheckpointBackendSubsystem::SerializeJsonObject(
    const TSharedRef<FJsonObject>& Object,
    FString& OutJson)
{
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
    return FJsonSerializer::Serialize(Object, Writer);
}

void UJumanjiEncounterCheckpointBackendSubsystem::CompleteLocalFailure(
    const FJumanjiCheckpointBackendCallback& Callback,
    const FString& ErrorCode,
    const FString& Message)
{
    FJumanjiCheckpointBackendResponse Result;
    Result.ResponseBody = FString::Printf(
        TEXT("{\"error\":\"%s\",\"message\":\"%s\"}"),
        *ErrorCode,
        *Message);
    Callback.ExecuteIfBound(Result);
}

bool UJumanjiEncounterCheckpointBackendSubsystem::SendJsonRequest(
    const FString& Verb,
    const FString& Route,
    const TSharedRef<FJsonObject>& Body,
    FJumanjiCheckpointBackendCallback Callback) const
{
    if (!IsServerRuntime())
    {
        CompleteLocalFailure(Callback, TEXT("SERVER_ONLY"), TEXT("Checkpoint service calls are server-only"));
        return false;
    }

    const FString ServerToken = ReadServerEnvironment(TEXT("JUMANJI_CHECKPOINT_SERVER_TOKEN"));
    if (ServerToken.Len() < 32)
    {
        CompleteLocalFailure(Callback, TEXT("SERVER_CREDENTIAL_MISSING"), TEXT("Server credential is not configured"));
        return false;
    }

    FString BaseUrl = ServiceBaseUrl;
    BaseUrl.TrimStartAndEndInline();
    BaseUrl.RemoveFromEnd(TEXT("/"));
    if (BaseUrl.IsEmpty() || !IsAllowedEndpoint(BaseUrl))
    {
        CompleteLocalFailure(Callback, TEXT("ENDPOINT_INVALID"), TEXT("Use HTTPS or a loopback HTTP endpoint"));
        return false;
    }

    FString BodyJson;
    if (!SerializeJsonObject(Body, BodyJson))
    {
        CompleteLocalFailure(Callback, TEXT("REQUEST_SERIALIZATION_FAILED"), TEXT("Request JSON could not be serialized"));
        return false;
    }

    const FString Url = BaseUrl + Route;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(Verb);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString(TEXT("Bearer ")) + ServerToken);
    Request->SetTimeout(FMath::Clamp(RequestTimeoutSeconds, 1.0f, 30.0f));
    Request->SetContentAsString(BodyJson);

    Request->OnProcessRequestComplete().BindLambda(
        [Callback = MoveTemp(Callback)](
            FHttpRequestPtr CompletedRequest,
            FHttpResponsePtr Response,
            bool bWasSuccessful) mutable
        {
            FJumanjiCheckpointBackendResponse Result;
            Result.bTransportSucceeded = bWasSuccessful && Response.IsValid();
            if (Response.IsValid())
            {
                Result.HttpStatusCode = Response->GetResponseCode();
                Result.ResponseBody = Response->GetContentAsString();
            }
            Callback.ExecuteIfBound(Result);
        });

    return Request->ProcessRequest();
}

bool UJumanjiEncounterCheckpointBackendSubsystem::AcquireLease(
    FName EncounterId,
    int32 LeaseSeconds,
    FJumanjiCheckpointBackendCallback Callback)
{
    FString ServerId;
    FString ShardId;
    if (!IsValidEncounterId(EncounterId) || !BuildServerIdentity(ServerId, ShardId)
        || LeaseSeconds < 15 || LeaseSeconds > 120)
    {
        CompleteLocalFailure(Callback, TEXT("REQUEST_INVALID"), TEXT("EncounterId, server identity or lease TTL is invalid"));
        return false;
    }

    TSharedRef<FJsonObject> Body = MakeIdentityBody(ServerId, ShardId);
    Body->SetNumberField(TEXT("ttlSeconds"), LeaseSeconds);
    const FString Route = FString::Printf(
        TEXT("/v1/encounters/%s/leases"),
        *EncounterId.ToString());
    return SendJsonRequest(TEXT("POST"), Route, Body, MoveTemp(Callback));
}

bool UJumanjiEncounterCheckpointBackendSubsystem::RenewLease(
    FName EncounterId,
    const FString& LeaseToken,
    int64 AuthorityEpoch,
    int32 LeaseSeconds,
    FJumanjiCheckpointBackendCallback Callback)
{
    FString ServerId;
    FString ShardId;
    if (!IsValidEncounterId(EncounterId) || !BuildServerIdentity(ServerId, ShardId)
        || LeaseToken.Len() < 32 || AuthorityEpoch < 1
        || LeaseSeconds < 15 || LeaseSeconds > 120)
    {
        CompleteLocalFailure(Callback, TEXT("REQUEST_INVALID"), TEXT("Lease identity, epoch or TTL is invalid"));
        return false;
    }

    TSharedRef<FJsonObject> Body = MakeIdentityBody(ServerId, ShardId);
    AddLeaseFields(Body, LeaseToken, AuthorityEpoch);
    Body->SetNumberField(TEXT("ttlSeconds"), LeaseSeconds);
    const FString Route = FString::Printf(
        TEXT("/v1/encounters/%s/leases/renew"),
        *EncounterId.ToString());
    return SendJsonRequest(TEXT("POST"), Route, Body, MoveTemp(Callback));
}

bool UJumanjiEncounterCheckpointBackendSubsystem::ReleaseLease(
    FName EncounterId,
    const FString& LeaseToken,
    int64 AuthorityEpoch,
    FJumanjiCheckpointBackendCallback Callback)
{
    FString ServerId;
    FString ShardId;
    if (!IsValidEncounterId(EncounterId) || !BuildServerIdentity(ServerId, ShardId)
        || LeaseToken.Len() < 32 || AuthorityEpoch < 1)
    {
        CompleteLocalFailure(Callback, TEXT("REQUEST_INVALID"), TEXT("Lease identity or epoch is invalid"));
        return false;
    }

    TSharedRef<FJsonObject> Body = MakeIdentityBody(ServerId, ShardId);
    AddLeaseFields(Body, LeaseToken, AuthorityEpoch);
    const FString Route = FString::Printf(
        TEXT("/v1/encounters/%s/leases/release"),
        *EncounterId.ToString());
    return SendJsonRequest(TEXT("POST"), Route, Body, MoveTemp(Callback));
}

bool UJumanjiEncounterCheckpointBackendSubsystem::WriteCheckpoint(
    FName EncounterId,
    const FString& LeaseToken,
    int64 AuthorityEpoch,
    int64 ExpectedSequence,
    const FString& PayloadJson,
    FJumanjiCheckpointBackendCallback Callback)
{
    FString ServerId;
    FString ShardId;
    if (!IsValidEncounterId(EncounterId) || !BuildServerIdentity(ServerId, ShardId)
        || LeaseToken.Len() < 32 || AuthorityEpoch < 1 || ExpectedSequence < 0)
    {
        CompleteLocalFailure(Callback, TEXT("REQUEST_INVALID"), TEXT("Encounter, lease or sequence is invalid"));
        return false;
    }

    TSharedPtr<FJsonObject> Payload;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
    if (!FJsonSerializer::Deserialize(Reader, Payload) || !Payload.IsValid())
    {
        CompleteLocalFailure(Callback, TEXT("PAYLOAD_INVALID"), TEXT("PayloadJson must contain a JSON object"));
        return false;
    }

    TSharedRef<FJsonObject> Body = MakeIdentityBody(ServerId, ShardId);
    AddLeaseFields(Body, LeaseToken, AuthorityEpoch);
    Body->SetNumberField(TEXT("expectedSequence"), static_cast<double>(ExpectedSequence));
    Body->SetObjectField(TEXT("payload"), Payload.ToSharedRef());

    const FString Route = FString::Printf(
        TEXT("/v1/encounters/%s/checkpoint"),
        *EncounterId.ToString());
    return SendJsonRequest(TEXT("PUT"), Route, Body, MoveTemp(Callback));
}

bool UJumanjiEncounterCheckpointBackendSubsystem::ReadCheckpoint(
    FName EncounterId,
    const FString& LeaseToken,
    int64 AuthorityEpoch,
    FJumanjiCheckpointBackendCallback Callback)
{
    FString ServerId;
    FString ShardId;
    if (!IsValidEncounterId(EncounterId) || !BuildServerIdentity(ServerId, ShardId)
        || LeaseToken.Len() < 32 || AuthorityEpoch < 1)
    {
        CompleteLocalFailure(Callback, TEXT("REQUEST_INVALID"), TEXT("Encounter, lease or epoch is invalid"));
        return false;
    }

    TSharedRef<FJsonObject> Body = MakeIdentityBody(ServerId, ShardId);
    AddLeaseFields(Body, LeaseToken, AuthorityEpoch);
    const FString Route = FString::Printf(
        TEXT("/v1/encounters/%s/checkpoint/read"),
        *EncounterId.ToString());
    return SendJsonRequest(TEXT("POST"), Route, Body, MoveTemp(Callback));
}
