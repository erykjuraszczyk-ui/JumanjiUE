#include "JumanjiEncounterCheckpointRuntimeSubsystem.h"

#include "JumanjiEncounterCheckpointBackendSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    FString ReadCheckpointEnvironment(const TCHAR* Name)
    {
        return FPlatformMisc::GetEnvironmentVariable(Name).TrimStartAndEnd();
    }

    bool IsStableContentId(const FString& Value, int32 MaxLength = 96)
    {
        if (Value.Len() < 3 || Value.Len() > MaxLength
            || Value[0] < TEXT('A') || Value[0] > TEXT('Z'))
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

    bool IsOpaqueParticipantKey(const FString& Value)
    {
        if (!Value.StartsWith(TEXT("pk_")) || Value.Len() < 23 || Value.Len() > 99)
        {
            return false;
        }
        for (int32 Index = 3; Index < Value.Len(); ++Index)
        {
            const TCHAR C = Value[Index];
            if (!((C >= TEXT('A') && C <= TEXT('Z'))
                || (C >= TEXT('a') && C <= TEXT('z'))
                || (C >= TEXT('0') && C <= TEXT('9'))
                || C == TEXT('_') || C == TEXT('-')))
            {
                return false;
            }
        }
        return true;
    }

    FString PhaseToString(EJumanjiWorldEncounterPhase Phase)
    {
        switch (Phase)
        {
        case EJumanjiWorldEncounterPhase::Trials: return TEXT("Trials");
        case EJumanjiWorldEncounterPhase::Combat: return TEXT("Combat");
        case EJumanjiWorldEncounterPhase::Boss: return TEXT("Boss");
        case EJumanjiWorldEncounterPhase::Reward: return TEXT("Reward");
        default: return FString();
        }
    }

    FString SafePointKindForPhase(EJumanjiWorldEncounterPhase Phase)
    {
        switch (Phase)
        {
        case EJumanjiWorldEncounterPhase::Trials: return TEXT("TrialBoundary");
        case EJumanjiWorldEncounterPhase::Combat: return TEXT("WaveBoundary");
        case EJumanjiWorldEncounterPhase::Boss: return TEXT("BossPhaseBoundary");
        case EJumanjiWorldEncounterPhase::Reward: return TEXT("RewardPending");
        default: return FString();
        }
    }

    bool ParsePhase(const FString& Value, EJumanjiWorldEncounterPhase& OutPhase)
    {
        if (Value == TEXT("Trials")) { OutPhase = EJumanjiWorldEncounterPhase::Trials; return true; }
        if (Value == TEXT("Combat")) { OutPhase = EJumanjiWorldEncounterPhase::Combat; return true; }
        if (Value == TEXT("Boss")) { OutPhase = EJumanjiWorldEncounterPhase::Boss; return true; }
        if (Value == TEXT("Reward")) { OutPhase = EJumanjiWorldEncounterPhase::Reward; return true; }
        return false;
    }

    bool HasExactKeys(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& Required, const TArray<FString>& Optional = {})
    {
        if (!Object.IsValid())
        {
            return false;
        }
        for (const FString& Key : Required)
        {
            if (!Object->HasField(Key))
            {
                return false;
            }
        }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
        {
            bool bAllowed = Required.Contains(Pair.Key);
            if (!bAllowed)
            {
                bAllowed = Optional.Contains(Pair.Key);
            }
            if (!bAllowed)
            {
                return false;
            }
        }
        return true;
    }

    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out)
    {
        return Object.IsValid() && Object->TryGetStringField(Field, Out) && !Out.IsEmpty();
    }

    bool ReadInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int64& Out, int64 MinValue = 0, int64 MaxValue = MAX_int64)
    {
        if (!Object.IsValid())
        {
            return false;
        }
        double Number = 0.0;
        if (!Object->TryGetNumberField(Field, Number) || !FMath::IsFinite(Number)
            || FMath::FloorToDouble(Number) != Number
            || Number < static_cast<double>(MinValue)
            || Number > static_cast<double>(MaxValue)
            || FMath::Abs(Number) > 9007199254740991.0)
        {
            return false;
        }
        Out = static_cast<int64>(Number);
        return true;
    }

    bool ReadFloat(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& Out, float MinValue, float MaxValue)
    {
        double Number = 0.0;
        if (!Object.IsValid() || !Object->TryGetNumberField(Field, Number)
            || !FMath::IsFinite(Number) || Number < MinValue || Number > MaxValue)
        {
            return false;
        }
        Out = static_cast<float>(Number);
        return true;
    }

    bool ReadUniqueNameArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 MaxItems, TArray<FName>& Out)
    {
        if (!Object.IsValid())
        {
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Field, Values) || !Values || Values->Num() > MaxItems)
        {
            return false;
        }

        TSet<FName> Seen;
        Out.Reset();
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            if (!Value.IsValid() || Value->Type != EJson::String)
            {
                return false;
            }
            const FString StringValue = Value->AsString();
            if (!IsStableContentId(StringValue))
            {
                return false;
            }
            const FName Id(*StringValue);
            if (Seen.Contains(Id))
            {
                return false;
            }
            Seen.Add(Id);
            Out.Add(Id);
        }
        return true;
    }

    TSharedRef<FJsonObject> MakeObject()
    {
        return MakeShared<FJsonObject>();
    }

    void SetNameArray(
        const TSharedRef<FJsonObject>& Object,
        const TCHAR* Field,
        const TArray<FName>& Names)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Reserve(Names.Num());
        for (const FName Name : Names)
        {
            Values.Add(MakeShared<FJsonValueString>(Name.ToString()));
        }
        Object->SetArrayField(Field, Values);
    }

    bool SerializeObject(const TSharedRef<FJsonObject>& Object, FString& OutJson)
    {
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
        return FJsonSerializer::Serialize(Object, Writer);
    }

    void Complete(
        const FJumanjiCheckpointRuntimeCallback& Callback,
        bool bSuccess,
        const TCHAR* Code,
        const FString& Message,
        int64 Sequence = 0)
    {
        FJumanjiCheckpointRuntimeResult Result;
        Result.bSucceeded = bSuccess;
        Result.ErrorCode = Code;
        Result.Message = Message;
        Result.CheckpointSequence = Sequence;
        Callback.ExecuteIfBound(Result);
    }
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::IsServerRuntime() const
{
    const UGameInstance* Instance = GetGameInstance();
    const UWorld* World = Instance ? Instance->GetWorld() : nullptr;
    return World && World->GetNetMode() != NM_Client;
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::SetParticipantRevisionValidator(
    TFunction<bool(const FString&, int64)> Validator)
{
    if (IsServerRuntime())
    {
        ParticipantRevisionValidator = MoveTemp(Validator);
    }
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::AcquireEncounterLease(
    FName EncounterId,
    FJumanjiCheckpointRuntimeCallback Callback)
{
    if (!IsServerRuntime() || EncounterId.IsNone() || bLeaseRequestPending)
    {
        Complete(Callback, false, TEXT("LEASE_REQUEST_REJECTED"),
            TEXT("Checkpoint lease requires a server runtime, a valid encounter and no pending lease request."));
        return false;
    }

    BackendSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointBackendSubsystem>()
        : nullptr;
    if (!BackendSubsystem)
    {
        Complete(Callback, false, TEXT("BACKEND_UNAVAILABLE"), TEXT("Checkpoint backend subsystem is unavailable."));
        return false;
    }

    bLeaseRequestPending = true;
    bLeaseReady = false;
    const TWeakObjectPtr<UJumanjiEncounterCheckpointRuntimeSubsystem> WeakThis(this);
    const bool bStarted = BackendSubsystem->AcquireLease(
        EncounterId,
        120,
        FJumanjiCheckpointBackendCallback::CreateLambda(
            [WeakThis, EncounterId, Callback](const FJumanjiCheckpointBackendResponse& Response)
            {
                UJumanjiEncounterCheckpointRuntimeSubsystem* Self = WeakThis.Get();
                if (!Self)
                {
                    return;
                }

                if (!Response.bTransportSucceeded || Response.HttpStatusCode != 201)
                {
                    Self->bLeaseRequestPending = false;
                    Self->bLeaseReady = false;
                    Complete(Callback, false, TEXT("LEASE_ACQUIRE_FAILED"),
                        FString::Printf(TEXT("Lease endpoint returned HTTP %d."), Response.HttpStatusCode));
                    return;
                }

                TSharedPtr<FJsonObject> Body;
                const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response.ResponseBody);
                if (!FJsonSerializer::Deserialize(Reader, Body) || !Body.IsValid())
                {
                    Self->bLeaseRequestPending = false;
                    Complete(Callback, false, TEXT("LEASE_RESPONSE_INVALID"), TEXT("Lease response is not valid JSON."));
                    return;
                }

                FString Token;
                FString ReturnedEncounter;
                int64 Epoch = 0;
                if (!ReadString(Body, TEXT("leaseToken"), Token)
                    || Token.Len() < 32
                    || !ReadString(Body, TEXT("encounterId"), ReturnedEncounter)
                    || ReturnedEncounter != EncounterId.ToString()
                    || !ReadInteger(Body, TEXT("authorityEpoch"), Epoch, 1))
                {
                    Self->bLeaseRequestPending = false;
                    Complete(Callback, false, TEXT("LEASE_RESPONSE_INVALID"), TEXT("Lease response does not match the requested encounter."));
                    return;
                }

                Self->LeaseToken = Token;
                Self->AuthorityEpoch = Epoch;
                Self->ActiveEncounterId = EncounterId;

                // A newly acquired lease may find no stored checkpoint (HTTP 404).
                Self->BackendSubsystem->ReadCheckpoint(
                    EncounterId,
                    Self->LeaseToken,
                    Self->AuthorityEpoch,
                    FJumanjiCheckpointBackendCallback::CreateLambda(
                        [WeakThis, Callback](const FJumanjiCheckpointBackendResponse& ReadResponse)
                        {
                            UJumanjiEncounterCheckpointRuntimeSubsystem* Current = WeakThis.Get();
                            if (!Current)
                            {
                                return;
                            }

                            if (ReadResponse.bTransportSucceeded && ReadResponse.HttpStatusCode == 404)
                            {
                                Current->LastCheckpointSequence = 0;
                                Current->bLeaseReady = true;
                                Current->bLeaseRequestPending = false;
                                Complete(Callback, true, TEXT("LEASE_READY"),
                                    TEXT("Lease acquired; no earlier checkpoint exists."), 0);
                                return;
                            }

                            if (!ReadResponse.bTransportSucceeded || ReadResponse.HttpStatusCode != 200)
                            {
                                Current->bLeaseRequestPending = false;
                                Current->bLeaseReady = false;
                                Complete(Callback, false, TEXT("CHECKPOINT_HEAD_READ_FAILED"),
                                    FString::Printf(TEXT("Unable to reconcile checkpoint sequence (HTTP %d)."), ReadResponse.HttpStatusCode));
                                return;
                            }

                            TSharedPtr<FJsonObject> Head;
                            const TSharedRef<TJsonReader<>> HeadReader = TJsonReaderFactory<>::Create(ReadResponse.ResponseBody);
                            int64 Sequence = 0;
                            int64 ReadEpoch = 0;
                            FString ReadEncounter;
                            if (!FJsonSerializer::Deserialize(HeadReader, Head) || !Head.IsValid()
                                || !ReadInteger(Head, TEXT("checkpointSequence"), Sequence, 1)
                                || !ReadInteger(Head, TEXT("authorityEpoch"), ReadEpoch, 1)
                                || !ReadString(Head, TEXT("encounterId"), ReadEncounter)
                                || ReadEncounter != Current->ActiveEncounterId.ToString()
                                || ReadEpoch != Current->AuthorityEpoch)
                            {
                                Current->bLeaseRequestPending = false;
                                Current->bLeaseReady = false;
                                Complete(Callback, false, TEXT("CHECKPOINT_HEAD_INVALID"),
                                    TEXT("Backend checkpoint head did not match the active lease."));
                                return;
                            }

                            Current->LastCheckpointSequence = Sequence;
                            Current->bLeaseReady = true;
                            Current->bLeaseRequestPending = false;
                            Complete(Callback, true, TEXT("LEASE_READY"),
                                TEXT("Lease acquired and checkpoint sequence reconciled."), Sequence);
                        }));
            }));

    if (!bStarted)
    {
        bLeaseRequestPending = false;
        bLeaseReady = false;
        return false;
    }
    return true;
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::ValidateCaptureInput(
    const FJumanjiEncounterCheckpointCaptureInput& Input,
    FString& OutErrorCode,
    FString& OutErrorMessage) const
{
    auto Fail = [&OutErrorCode, &OutErrorMessage](const TCHAR* Code, const TCHAR* Message)
    {
        OutErrorCode = Code;
        OutErrorMessage = Message;
        return false;
    };

    if (!IsServerRuntime() || !Input.bServerConfirmedSafeBoundary)
    {
        return Fail(TEXT("NOT_SERVER_CONFIRMED"), TEXT("Only a server-confirmed safe boundary may be captured."));
    }

    if (Input.EncounterId.IsNone() || Input.EncounterId != ActiveEncounterId
        || !Input.SessionId.IsValid()
        || Input.ChapterId.IsNone() || Input.BoardEventId.IsNone()
        || Input.EncounterRevision < 1 || Input.ChapterRevision < 1
        || !IsStableContentId(Input.EncounterId.ToString())
        || !IsStableContentId(Input.ChapterId.ToString())
        || !IsStableContentId(Input.BoardEventId.ToString())
        || !IsStableContentId(Input.SpawnSetId.ToString()))
    {
        return Fail(TEXT("IDENTITY_INVALID"), TEXT("Checkpoint content/session identity is incomplete or inconsistent."));
    }

    if (!Input.bNoActiveTraversal || !Input.bNoTrialInProgress || !Input.bNoPendingSpawn)
    {
        return Fail(TEXT("UNSAFE_WORLD_STATE"), TEXT("Traversal, trial interaction or spawning is still in progress."));
    }

    if (Input.Participants.IsEmpty() || Input.Participants.Num() > 64)
    {
        return Fail(TEXT("PARTICIPANTS_INVALID"), TEXT("A checkpoint requires 1 to 64 trusted participants."));
    }

    TSet<FString> ParticipantKeys;
    for (const FJumanjiCheckpointParticipant& Participant : Input.Participants)
    {
        if (!IsOpaqueParticipantKey(Participant.ParticipantKey)
            || Participant.ProfileRevision < 0
            || ParticipantKeys.Contains(Participant.ParticipantKey))
        {
            return Fail(TEXT("PARTICIPANTS_INVALID"), TEXT("Participant keys must be unique opaque keys with non-negative profile revisions."));
        }
        ParticipantKeys.Add(Participant.ParticipantKey);
    }

    TSet<FName> SeenObjectives;
    for (const FName Id : Input.CompletedObjectiveIds)
    {
        if (Id.IsNone() || !IsStableContentId(Id.ToString()) || SeenObjectives.Contains(Id))
        {
            return Fail(TEXT("CHAPTER_PROGRESS_INVALID"), TEXT("Completed objective IDs must be unique stable content IDs."));
        }
        SeenObjectives.Add(Id);
    }
    TSet<FName> SeenTrials;
    for (const FName Id : Input.CompletedTrialIds)
    {
        if (Id.IsNone() || !IsStableContentId(Id.ToString()) || SeenTrials.Contains(Id))
        {
            return Fail(TEXT("CHAPTER_PROGRESS_INVALID"), TEXT("Completed trial IDs must be unique stable content IDs."));
        }
        SeenTrials.Add(Id);
    }

    switch (Input.Phase)
    {
    case EJumanjiWorldEncounterPhase::Trials:
        if (Input.CurrentWave != 0 || Input.TotalWaves < 1
            || !Input.bNoActiveEnemies
            || Input.CompletedObjectiveIds.IsEmpty() || Input.CompletedTrialIds.IsEmpty())
        {
            return Fail(TEXT("TRIAL_BARRIER_INVALID"), TEXT("Trial boundary requires committed chapter progress and currentWave=0."));
        }
        break;

    case EJumanjiWorldEncounterPhase::Combat:
        if (Input.CurrentWave < 1 || Input.TotalWaves < Input.CurrentWave
            || !Input.bNoPendingSpawn || !Input.bNoActiveEnemies)
        {
            return Fail(TEXT("WAVE_BARRIER_INVALID"), TEXT("Wave boundary requires a completed wave and no pending spawn."));
        }
        break;

    case EJumanjiWorldEncounterPhase::Boss:
        if (!Input.bNoActiveBossTelegraph
            || Input.BossDefinitionId.IsNone()
            || !IsStableContentId(Input.BossDefinitionId.ToString())
            || Input.BossPhase < 1 || Input.BossPhase > 3
            || Input.BossPhaseRevision < 1
            || !FMath::IsFinite(Input.BossHealthFraction)
            || Input.BossHealthFraction < 0.0f || Input.BossHealthFraction > 1.0f
            || Input.CurrentWave != 0)
        {
            return Fail(TEXT("BOSS_BARRIER_INVALID"), TEXT("Boss checkpoint requires a quiescent boss phase and bounded logical health snapshot."));
        }
        break;

    case EJumanjiWorldEncounterPhase::Reward:
        if (!Input.bNoActiveEnemies || !Input.bRewardTransactionStable
            || Input.RewardDefinitionId.IsNone()
            || Input.RewardTransactionId.IsNone()
            || !IsStableContentId(Input.RewardDefinitionId.ToString())
            || !IsStableContentId(Input.RewardTransactionId.ToString())
            || Input.CurrentWave != 0)
        {
            return Fail(TEXT("REWARD_BARRIER_INVALID"), TEXT("Reward checkpoint requires stable reward IDs before ledger reconciliation."));
        }
        break;

    default:
        return Fail(TEXT("PHASE_NOT_RESUMABLE"), TEXT("Only Trials, Combat, Boss and Reward phases are checkpointable."));
    }

    return true;
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::BuildCheckpointPayload(
    const FJumanjiEncounterCheckpointCaptureInput& Input,
    int64 Sequence,
    FString& OutPayloadJson,
    FString& OutErrorCode,
    FString& OutErrorMessage) const
{
    if (Sequence < 1 || Sequence > 9007199254740991LL)
    {
        OutErrorCode = TEXT("SEQUENCE_INVALID");
        OutErrorMessage = TEXT("Checkpoint sequence is outside the JSON safe integer range.");
        return false;
    }

    const FString ServerInstanceId = ReadCheckpointEnvironment(TEXT("JUMANJI_SERVER_INSTANCE_ID"));
    const FString ShardId = ReadCheckpointEnvironment(TEXT("JUMANJI_SHARD_ID"));
    const FString BuildId = ReadCheckpointEnvironment(TEXT("JUMANJI_SERVER_BUILD_ID"));
    const FString RulesetVersion = ReadCheckpointEnvironment(TEXT("JUMANJI_RULESET_VERSION"));
    const FString WorldContentVersion = ReadCheckpointEnvironment(TEXT("JUMANJI_WORLD_CONTENT_VERSION"));
    if (ServerInstanceId.IsEmpty() || ShardId.IsEmpty() || BuildId.IsEmpty()
        || RulesetVersion.IsEmpty() || WorldContentVersion.IsEmpty())
    {
        OutErrorCode = TEXT("SERVER_METADATA_MISSING");
        OutErrorMessage = TEXT("Server identity, build, ruleset and world content versions must be configured in the server process.");
        return false;
    }

    TSharedRef<FJsonObject> Payload = MakeObject();
    Payload->SetNumberField(TEXT("schemaVersion"), 1);
    Payload->SetStringField(TEXT("checkpointId"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
    Payload->SetStringField(TEXT("encounterId"), Input.EncounterId.ToString());
    Payload->SetStringField(TEXT("sessionId"), Input.SessionId.ToString(EGuidFormats::DigitsWithHyphens));
    Payload->SetNumberField(TEXT("checkpointSequence"), static_cast<double>(Sequence));

    const FDateTime Now = FDateTime::UtcNow();
    Payload->SetStringField(TEXT("createdAtUtc"), Now.ToIso8601());
    Payload->SetStringField(TEXT("expiresAtUtc"), (Now + FTimespan::FromMinutes(30)).ToIso8601());

    TSharedRef<FJsonObject> Authority = MakeObject();
    Authority->SetStringField(TEXT("serverInstanceId"), ServerInstanceId);
    Authority->SetStringField(TEXT("shardId"), ShardId);
    Authority->SetNumberField(TEXT("authorityEpoch"), static_cast<double>(AuthorityEpoch));
    Authority->SetStringField(TEXT("serverBuildId"), BuildId);
    Authority->SetStringField(TEXT("rulesetVersion"), RulesetVersion);
    Authority->SetStringField(TEXT("worldContentVersion"), WorldContentVersion);
    Payload->SetObjectField(TEXT("authority"), Authority);

    TSharedRef<FJsonObject> SafePoint = MakeObject();
    const FString PhaseString = PhaseToString(Input.Phase);
    SafePoint->SetStringField(TEXT("phase"), PhaseString);
    SafePoint->SetStringField(TEXT("kind"), SafePointKindForPhase(Input.Phase));
    SafePoint->SetNumberField(TEXT("encounterRevision"), Input.EncounterRevision);

    TSharedRef<FJsonObject> Chapter = MakeObject();
    Chapter->SetStringField(TEXT("chapterId"), Input.ChapterId.ToString());
    Chapter->SetStringField(TEXT("boardEventId"), Input.BoardEventId.ToString());
    Chapter->SetNumberField(TEXT("chapterRevision"), Input.ChapterRevision);
    SetNameArray(Chapter, TEXT("completedObjectiveIds"), Input.CompletedObjectiveIds);
    SetNameArray(Chapter, TEXT("completedTrialIds"), Input.CompletedTrialIds);
    SafePoint->SetObjectField(TEXT("chapter"), Chapter);

    TSharedRef<FJsonObject> Combat = MakeObject();
    Combat->SetNumberField(TEXT("currentWave"), Input.CurrentWave);
    Combat->SetNumberField(TEXT("totalWaves"), Input.TotalWaves);
    if (Input.Phase == EJumanjiWorldEncounterPhase::Boss)
    {
        TSharedRef<FJsonObject> Boss = MakeObject();
        Boss->SetStringField(TEXT("definitionId"), Input.BossDefinitionId.ToString());
        Boss->SetStringField(TEXT("phaseId"), FString::Printf(TEXT("BOSS_PHASE_%02d"), Input.BossPhase));
        Boss->SetNumberField(TEXT("healthFraction"), Input.BossHealthFraction);
        Boss->SetNumberField(TEXT("phaseRevision"), Input.BossPhaseRevision);
        Combat->SetObjectField(TEXT("boss"), Boss);
    }
    SafePoint->SetObjectField(TEXT("combat"), Combat);
    Payload->SetObjectField(TEXT("safePoint"), SafePoint);

    TSharedRef<FJsonObject> Deterministic = MakeObject();
    Deterministic->SetNumberField(TEXT("encounterSeed"), static_cast<double>(Input.EncounterSeed));
    Deterministic->SetStringField(TEXT("spawnSetId"), Input.SpawnSetId.ToString());
    Payload->SetObjectField(TEXT("deterministic"), Deterministic);

    TArray<TSharedPtr<FJsonValue>> Participants;
    for (const FJumanjiCheckpointParticipant& Participant : Input.Participants)
    {
        TSharedRef<FJsonObject> Entry = MakeObject();
        Entry->SetStringField(TEXT("participantKey"), Participant.ParticipantKey);
        Entry->SetNumberField(TEXT("profileRevision"), static_cast<double>(Participant.ProfileRevision));
        Participants.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Payload->SetArrayField(TEXT("participants"), Participants);

    if (Input.Phase == EJumanjiWorldEncounterPhase::Reward)
    {
        TSharedRef<FJsonObject> Reward = MakeObject();
        Reward->SetStringField(TEXT("rewardDefinitionId"), Input.RewardDefinitionId.ToString());
        Reward->SetStringField(TEXT("transactionId"), Input.RewardTransactionId.ToString());
        Reward->SetStringField(TEXT("status"), TEXT("PendingLedgerCheck"));
        Payload->SetObjectField(TEXT("reward"), Reward);
    }

    return SerializeObject(Payload, OutPayloadJson);
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::CaptureSafePoint(
    const FJumanjiEncounterCheckpointCaptureInput& Input,
    FJumanjiCheckpointRuntimeCallback Callback)
{
    if (!bLeaseReady || !IsServerRuntime())
    {
        Complete(Callback, false, TEXT("LEASE_NOT_READY"), TEXT("No active trusted checkpoint lease is available."));
        return false;
    }

    FString ErrorCode;
    FString ErrorMessage;
    if (!ValidateCaptureInput(Input, ErrorCode, ErrorMessage))
    {
        Complete(Callback, false, *ErrorCode, ErrorMessage);
        return false;
    }

    FString PayloadJson;
    const int64 NextSequence = LastCheckpointSequence + 1;
    if (!BuildCheckpointPayload(Input, NextSequence, PayloadJson, ErrorCode, ErrorMessage))
    {
        Complete(Callback, false, *ErrorCode, ErrorMessage);
        return false;
    }

    const TWeakObjectPtr<UJumanjiEncounterCheckpointRuntimeSubsystem> WeakThis(this);
    const bool bStarted = BackendSubsystem->RenewLease(
        ActiveEncounterId,
        LeaseToken,
        AuthorityEpoch,
        120,
        FJumanjiCheckpointBackendCallback::CreateLambda(
            [WeakThis, PayloadJson, NextSequence, Callback](const FJumanjiCheckpointBackendResponse& RenewResponse)
            {
                UJumanjiEncounterCheckpointRuntimeSubsystem* Self = WeakThis.Get();
                if (!Self)
                {
                    return;
                }
                if (!RenewResponse.bTransportSucceeded || RenewResponse.HttpStatusCode != 200)
                {
                    Self->bLeaseReady = false;
                    Complete(Callback, false, TEXT("LEASE_RENEW_FAILED"), TEXT("Safe-point capture was not written because the lease could not be renewed."));
                    return;
                }

                Self->BackendSubsystem->WriteCheckpoint(
                    Self->ActiveEncounterId,
                    Self->LeaseToken,
                    Self->AuthorityEpoch,
                    Self->LastCheckpointSequence,
                    PayloadJson,
                    FJumanjiCheckpointBackendCallback::CreateLambda(
                        [WeakThis, NextSequence, Callback](const FJumanjiCheckpointBackendResponse& WriteResponse)
                        {
                            UJumanjiEncounterCheckpointRuntimeSubsystem* Current = WeakThis.Get();
                            if (!Current)
                            {
                                return;
                            }
                            if (!WriteResponse.bTransportSucceeded || WriteResponse.HttpStatusCode != 201)
                            {
                                if (WriteResponse.HttpStatusCode == 409)
                                {
                                    Current->bLeaseReady = false;
                                }
                                Complete(Callback, false, TEXT("CHECKPOINT_WRITE_FAILED"),
                                    FString::Printf(TEXT("Checkpoint write returned HTTP %d; sequence was not advanced."), WriteResponse.HttpStatusCode));
                                return;
                            }

                            Current->LastCheckpointSequence = NextSequence;
                            Complete(Callback, true, TEXT("CHECKPOINT_COMMITTED"),
                                TEXT("Signed logical checkpoint was committed by the trusted backend."), NextSequence);
                        }));
            }));

    if (!bStarted)
    {
        // Backend adapter invokes the supplied callback for local request-start failures.
        return false;
    }
    return true;
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::ParseAndValidateRestoreEnvelope(
    const FString& ResponseBody,
    const FJumanjiEncounterCheckpointRestoreExpectations& Expectations,
    int64 BackendSequence,
    FJumanjiEncounterCheckpointRestorePlan& OutPlan,
    FString& OutErrorCode,
    FString& OutErrorMessage) const
{
    auto Fail = [&OutErrorCode, &OutErrorMessage](const TCHAR* Code, const TCHAR* Message)
    {
        OutErrorCode = Code;
        OutErrorMessage = Message;
        return false;
    };

    TSharedPtr<FJsonObject> Response;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, Response) || !Response.IsValid()
        || !HasExactKeys(Response, {TEXT("encounterId"), TEXT("checkpointSequence"), TEXT("authorityEpoch"), TEXT("envelope")}))
    {
        return Fail(TEXT("RESTORE_RESPONSE_INVALID"), TEXT("Checkpoint read response has an invalid top-level shape."));
    }

    FString ResponseEncounter;
    int64 ResponseSequence = 0;
    int64 ResponseEpoch = 0;
    const TSharedPtr<FJsonObject>* EnvelopePtr = nullptr;
    if (!ReadString(Response, TEXT("encounterId"), ResponseEncounter)
        || ResponseEncounter != Expectations.EncounterId.ToString()
        || !ReadInteger(Response, TEXT("checkpointSequence"), ResponseSequence, 1)
        || ResponseSequence != BackendSequence
        || !ReadInteger(Response, TEXT("authorityEpoch"), ResponseEpoch, 1)
        || ResponseEpoch != AuthorityEpoch
        || !Response->TryGetObjectField(TEXT("envelope"), EnvelopePtr)
        || !EnvelopePtr || !EnvelopePtr->IsValid())
    {
        return Fail(TEXT("RESTORE_HEAD_MISMATCH"), TEXT("Backend response head does not match the active lease or expected encounter."));
    }

    const TSharedPtr<FJsonObject> Envelope = *EnvelopePtr;
    const TArray<FString> EnvelopeKeys = {TEXT("envelopeVersion"), TEXT("payload"), TEXT("integrity")};
    const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
    const TSharedPtr<FJsonObject>* IntegrityPtr = nullptr;
    int64 EnvelopeVersion = 0;
    if (!HasExactKeys(Envelope, EnvelopeKeys)
        || !ReadInteger(Envelope, TEXT("envelopeVersion"), EnvelopeVersion, 1, 1)
        || !Envelope->TryGetObjectField(TEXT("payload"), PayloadPtr)
        || !PayloadPtr || !PayloadPtr->IsValid()
        || !Envelope->TryGetObjectField(TEXT("integrity"), IntegrityPtr)
        || !IntegrityPtr || !IntegrityPtr->IsValid())
    {
        return Fail(TEXT("ENVELOPE_INVALID"), TEXT("Checkpoint envelope does not match the closed v1 schema."));
    }

    const TSharedPtr<FJsonObject> Payload = *PayloadPtr;
    const TSharedPtr<FJsonObject> Integrity = *IntegrityPtr;
    if (!HasExactKeys(Integrity, {TEXT("algorithm"), TEXT("keyId"), TEXT("signature")}))
    {
        return Fail(TEXT("INTEGRITY_INVALID"), TEXT("Checkpoint integrity fields are invalid."));
    }

    FString Algorithm;
    FString KeyId;
    FString Signature;
    if (!ReadString(Integrity, TEXT("algorithm"), Algorithm)
        || Algorithm != TEXT("Ed25519")
        || !ReadString(Integrity, TEXT("keyId"), KeyId)
        || !ReadString(Integrity, TEXT("signature"), Signature)
        || Signature.Len() < 64 || Signature.Len() > 256)
    {
        return Fail(TEXT("INTEGRITY_INVALID"), TEXT("Checkpoint signature metadata is malformed."));
    }

    const FString ExpectedKeyId = ReadCheckpointEnvironment(TEXT("JUMANJI_CHECKPOINT_EXPECTED_KEY_ID"));
    if (ExpectedKeyId.IsEmpty() || KeyId != ExpectedKeyId)
    {
        return Fail(TEXT("SIGNING_KEY_NOT_APPROVED"), TEXT("Checkpoint signing key ID is not approved by this server."));
    }

    const TArray<FString> PayloadRequired = {
        TEXT("schemaVersion"), TEXT("checkpointId"), TEXT("encounterId"), TEXT("sessionId"),
        TEXT("checkpointSequence"), TEXT("createdAtUtc"), TEXT("expiresAtUtc"), TEXT("authority"),
        TEXT("safePoint"), TEXT("deterministic"), TEXT("participants")
    };
    const TSharedPtr<FJsonObject>* AuthorityPtr = nullptr;
    const TSharedPtr<FJsonObject>* SafePointPtr = nullptr;
    const TSharedPtr<FJsonObject>* DeterministicPtr = nullptr;
    int64 SchemaVersion = 0;
    FString CheckpointIdString;
    FString SessionIdString;
    FString EncounterIdString;
    FString CreatedAt;
    FString ExpiresAt;
    if (!HasExactKeys(Payload, PayloadRequired, {TEXT("reward")})
        || !ReadInteger(Payload, TEXT("schemaVersion"), SchemaVersion, 1, 1)
        || !ReadString(Payload, TEXT("checkpointId"), CheckpointIdString)
        || !ReadString(Payload, TEXT("sessionId"), SessionIdString)
        || !ReadString(Payload, TEXT("encounterId"), EncounterIdString)
        || !ReadString(Payload, TEXT("createdAtUtc"), CreatedAt)
        || !ReadString(Payload, TEXT("expiresAtUtc"), ExpiresAt)
        || !Payload->TryGetObjectField(TEXT("authority"), AuthorityPtr) || !AuthorityPtr || !AuthorityPtr->IsValid()
        || !Payload->TryGetObjectField(TEXT("safePoint"), SafePointPtr) || !SafePointPtr || !SafePointPtr->IsValid()
        || !Payload->TryGetObjectField(TEXT("deterministic"), DeterministicPtr) || !DeterministicPtr || !DeterministicPtr->IsValid())
    {
        return Fail(TEXT("PAYLOAD_INVALID"), TEXT("Checkpoint payload is missing required fields or contains unknown fields."));
    }

    FGuid CheckpointGuid;
    FGuid SessionGuid;
    if (!FGuid::Parse(CheckpointIdString, CheckpointGuid)
        || !FGuid::Parse(SessionIdString, SessionGuid)
        || EncounterIdString != Expectations.EncounterId.ToString())
    {
        return Fail(TEXT("PAYLOAD_IDENTITY_INVALID"), TEXT("Checkpoint IDs are invalid or do not match the requested encounter."));
    }

    FDateTime CreatedTime;
    FDateTime ExpiresTime;
    if (!FDateTime::ParseIso8601(*CreatedAt, CreatedTime)
        || !FDateTime::ParseIso8601(*ExpiresAt, ExpiresTime)
        || ExpiresTime <= FDateTime::UtcNow()
        || ExpiresTime <= CreatedTime
        || ExpiresTime - CreatedTime > FTimespan::FromHours(24))
    {
        return Fail(TEXT("CHECKPOINT_EXPIRED_OR_TIME_INVALID"), TEXT("Checkpoint timestamps are invalid or expired."));
    }

    const TSharedPtr<FJsonObject> Authority = *AuthorityPtr;
    const TArray<FString> AuthorityKeys = {
        TEXT("serverInstanceId"), TEXT("shardId"), TEXT("authorityEpoch"),
        TEXT("serverBuildId"), TEXT("rulesetVersion"), TEXT("worldContentVersion")
    };
    FString ServerId, ShardId, BuildId, Ruleset, ContentVersion;
    int64 SourceEpoch = 0, CheckpointSequence = 0;
    if (!HasExactKeys(Authority, AuthorityKeys)
        || !ReadString(Authority, TEXT("serverInstanceId"), ServerId)
        || !ReadString(Authority, TEXT("shardId"), ShardId)
        || !ReadInteger(Authority, TEXT("authorityEpoch"), SourceEpoch, 1)
        || !ReadString(Authority, TEXT("serverBuildId"), BuildId)
        || !ReadString(Authority, TEXT("rulesetVersion"), Ruleset)
        || !ReadString(Authority, TEXT("worldContentVersion"), ContentVersion)
        || !ReadInteger(Payload, TEXT("checkpointSequence"), CheckpointSequence, 1)
        || CheckpointSequence != BackendSequence
        || SourceEpoch >= AuthorityEpoch)
    {
        return Fail(TEXT("AUTHORITY_OR_SEQUENCE_INVALID"), TEXT("Checkpoint source authority or sequence is stale."));
    }

    if (BuildId != Expectations.ServerBuildId
        || Ruleset != Expectations.RulesetVersion
        || ContentVersion != Expectations.WorldContentVersion)
    {
        return Fail(TEXT("CONTENT_VERSION_MISMATCH"), TEXT("Build, ruleset or world content version requires an approved migration."));
    }

    const TSharedPtr<FJsonObject> SafePoint = *SafePointPtr;
    if (!HasExactKeys(SafePoint, {
        TEXT("phase"), TEXT("kind"), TEXT("encounterRevision"), TEXT("chapter"), TEXT("combat")
    }))
    {
        return Fail(TEXT("SAFE_POINT_INVALID"), TEXT("Safe point object contains missing or unsupported fields."));
    }

    FString PhaseString, KindString;
    int64 EncounterRevision = 0;
    const TSharedPtr<FJsonObject>* ChapterPtr = nullptr;
    const TSharedPtr<FJsonObject>* CombatPtr = nullptr;
    if (!ReadString(SafePoint, TEXT("phase"), PhaseString)
        || !ReadString(SafePoint, TEXT("kind"), KindString)
        || !ReadInteger(SafePoint, TEXT("encounterRevision"), EncounterRevision, 1)
        || !SafePoint->TryGetObjectField(TEXT("chapter"), ChapterPtr) || !ChapterPtr || !ChapterPtr->IsValid()
        || !SafePoint->TryGetObjectField(TEXT("combat"), CombatPtr) || !CombatPtr || !CombatPtr->IsValid())
    {
        return Fail(TEXT("SAFE_POINT_INVALID"), TEXT("Safe point phase or revision is malformed."));
    }

    EJumanjiWorldEncounterPhase Phase;
    if (!ParsePhase(PhaseString, Phase) || SafePointKindForPhase(Phase) != KindString)
    {
        return Fail(TEXT("SAFE_POINT_KIND_MISMATCH"), TEXT("Safe point kind is not valid for its encounter phase."));
    }

    const TSharedPtr<FJsonObject> Chapter = *ChapterPtr;
    FString ChapterIdString, BoardEventIdString;
    int64 ChapterRevision = 0;
    if (!HasExactKeys(Chapter, {
        TEXT("chapterId"), TEXT("boardEventId"), TEXT("chapterRevision"),
        TEXT("completedObjectiveIds"), TEXT("completedTrialIds")
    })
        || !ReadString(Chapter, TEXT("chapterId"), ChapterIdString)
        || !ReadString(Chapter, TEXT("boardEventId"), BoardEventIdString)
        || !ReadInteger(Chapter, TEXT("chapterRevision"), ChapterRevision, 1)
        || ChapterIdString != Expectations.ChapterId.ToString()
        || BoardEventIdString != Expectations.BoardEventId.ToString())
    {
        return Fail(TEXT("CHAPTER_MISMATCH"), TEXT("Checkpoint chapter or board event is not compatible with the configured world encounter."));
    }

    TArray<FName> CompletedObjectives, CompletedTrials;
    if (!ReadUniqueNameArray(Chapter, TEXT("completedObjectiveIds"), 64, CompletedObjectives)
        || !ReadUniqueNameArray(Chapter, TEXT("completedTrialIds"), 9, CompletedTrials)
        || Expectations.AllowedObjectiveIds.IsEmpty()
        || Expectations.AllowedTrialIds.IsEmpty())
    {
        return Fail(TEXT("CHAPTER_PROGRESS_INVALID"), TEXT("Completed objective/trial IDs or current content allowlists are invalid."));
    }

    const TSet<FName> AllowedObjectives(Expectations.AllowedObjectiveIds);
    const TSet<FName> AllowedTrials(Expectations.AllowedTrialIds);
    for (const FName Id : CompletedObjectives)
    {
        if (!AllowedObjectives.Contains(Id))
        {
            return Fail(TEXT("OBJECTIVE_NOT_ALLOWLISTED"), TEXT("Checkpoint contains an objective absent from the current chapter definition."));
        }
    }
    for (const FName Id : CompletedTrials)
    {
        if (!AllowedTrials.Contains(Id))
        {
            return Fail(TEXT("TRIAL_NOT_ALLOWLISTED"), TEXT("Checkpoint contains a trial absent from the current chapter definition."));
        }
    }

    const TSharedPtr<FJsonObject> Combat = *CombatPtr;
    if (!HasExactKeys(Combat, {TEXT("currentWave"), TEXT("totalWaves")}, {TEXT("boss")}))
    {
        return Fail(TEXT("COMBAT_SNAPSHOT_INVALID"), TEXT("Combat snapshot contains unknown fields."));
    }

    int64 CurrentWave = 0, TotalWaves = 0;
    if (!ReadInteger(Combat, TEXT("currentWave"), CurrentWave, 0, 100)
        || !ReadInteger(Combat, TEXT("totalWaves"), TotalWaves, 0, 100)
        || CurrentWave > TotalWaves
        || ((Phase == EJumanjiWorldEncounterPhase::Combat) && CurrentWave < 1)
        || ((Phase != EJumanjiWorldEncounterPhase::Combat) && CurrentWave != 0))
    {
        return Fail(TEXT("WAVE_SNAPSHOT_INVALID"), TEXT("Checkpoint wave numbers are outside the allowed phase range."));
    }

    const TSharedPtr<FJsonObject>* DeterministicPtrValue = DeterministicPtr;
    const TSharedPtr<FJsonObject> Deterministic = *DeterministicPtrValue;
    int64 Seed = 0;
    FString SpawnSet;
    if (!HasExactKeys(Deterministic, {TEXT("encounterSeed"), TEXT("spawnSetId")})
        || !ReadInteger(Deterministic, TEXT("encounterSeed"), Seed, 0, 4294967295LL)
        || !ReadString(Deterministic, TEXT("spawnSetId"), SpawnSet)
        || SpawnSet != Expectations.SpawnSetId.ToString()
        || !IsStableContentId(SpawnSet))
    {
        return Fail(TEXT("DETERMINISTIC_CONTENT_MISMATCH"), TEXT("Encounter seed or allowlisted spawn set is invalid."));
    }

    const TArray<TSharedPtr<FJsonValue>>* ParticipantValues = nullptr;
    if (!Payload->TryGetArrayField(TEXT("participants"), ParticipantValues)
        || !ParticipantValues || ParticipantValues->Num() < 1 || ParticipantValues->Num() > 64
        || !ParticipantRevisionValidator)
    {
        return Fail(TEXT("PARTICIPANTS_NOT_VERIFIED"), TEXT("Trusted participant identity/revision validation is unavailable."));
    }

    TArray<FJumanjiCheckpointParticipant> Participants;
    TSet<FString> SeenParticipantKeys;
    for (const TSharedPtr<FJsonValue>& ParticipantValue : *ParticipantValues)
    {
        const TSharedPtr<FJsonObject> Participant = ParticipantValue.IsValid()
            ? ParticipantValue->AsObject()
            : nullptr;
        FString ParticipantKey;
        int64 ProfileRevision = 0;
        if (!HasExactKeys(Participant, {TEXT("participantKey"), TEXT("profileRevision")})
            || !ReadString(Participant, TEXT("participantKey"), ParticipantKey)
            || !IsOpaqueParticipantKey(ParticipantKey)
            || !ReadInteger(Participant, TEXT("profileRevision"), ProfileRevision, 0)
            || SeenParticipantKeys.Contains(ParticipantKey)
            || !ParticipantRevisionValidator(ParticipantKey, ProfileRevision))
        {
            return Fail(TEXT("PARTICIPANT_PREFLIGHT_FAILED"), TEXT("A participant is duplicate, malformed or not approved by the trusted profile resolver."));
        }
        SeenParticipantKeys.Add(ParticipantKey);
        FJumanjiCheckpointParticipant Entry;
        Entry.ParticipantKey = ParticipantKey;
        Entry.ProfileRevision = ProfileRevision;
        Participants.Add(MoveTemp(Entry));
    }

    OutPlan = FJumanjiEncounterCheckpointRestorePlan();
    OutPlan.CheckpointId = CheckpointGuid;
    OutPlan.SessionId = SessionGuid;
    OutPlan.EncounterId = Expectations.EncounterId;
    OutPlan.ChapterId = Expectations.ChapterId;
    OutPlan.BoardEventId = Expectations.BoardEventId;
    OutPlan.Phase = Phase;
    OutPlan.CheckpointSequence = CheckpointSequence;
    OutPlan.SourceAuthorityEpoch = SourceEpoch;
    OutPlan.EncounterRevision = static_cast<int32>(EncounterRevision);
    OutPlan.ChapterRevision = static_cast<int32>(ChapterRevision);
    OutPlan.CompletedObjectiveIds = MoveTemp(CompletedObjectives);
    OutPlan.CompletedTrialIds = MoveTemp(CompletedTrials);
    OutPlan.CurrentWave = static_cast<int32>(CurrentWave);
    OutPlan.TotalWaves = static_cast<int32>(TotalWaves);
    OutPlan.EncounterSeed = static_cast<uint32>(Seed);
    OutPlan.SpawnSetId = FName(*SpawnSet);

    if (Phase == EJumanjiWorldEncounterPhase::Boss)
    {
        const TSharedPtr<FJsonObject>* BossPtr = nullptr;
        FString DefinitionId, PhaseId;
        int64 PhaseRevision = 0;
        if (!Combat->TryGetObjectField(TEXT("boss"), BossPtr) || !BossPtr || !BossPtr->IsValid()
            || !HasExactKeys(*BossPtr, {
                TEXT("definitionId"), TEXT("phaseId"), TEXT("healthFraction"), TEXT("phaseRevision")
            })
            || !ReadString(*BossPtr, TEXT("definitionId"), DefinitionId)
            || !ReadString(*BossPtr, TEXT("phaseId"), PhaseId)
            || DefinitionId != Expectations.BossDefinitionId.ToString()
            || !ReadInteger(*BossPtr, TEXT("phaseRevision"), PhaseRevision, 1))
        {
            return Fail(TEXT("BOSS_PREFLIGHT_FAILED"), TEXT("Boss logical snapshot does not match the allowlisted boss definition."));
        }

        const FString ExpectedPhaseId = PhaseId;
        if (ExpectedPhaseId != TEXT("BOSS_PHASE_01")
            && ExpectedPhaseId != TEXT("BOSS_PHASE_02")
            && ExpectedPhaseId != TEXT("BOSS_PHASE_03"))
        {
            return Fail(TEXT("BOSS_PHASE_INVALID"), TEXT("Boss phase ID is not supported."));
        }
        float HealthFraction = 0.0f;
        if (!ReadFloat(*BossPtr, TEXT("healthFraction"), HealthFraction, 0.0f, 1.0f))
        {
            return Fail(TEXT("BOSS_HEALTH_INVALID"), TEXT("Boss health fraction is outside the supported range."));
        }
        OutPlan.BossDefinitionId = FName(*DefinitionId);
        OutPlan.BossPhase = ExpectedPhaseId == TEXT("BOSS_PHASE_01") ? 1
            : ExpectedPhaseId == TEXT("BOSS_PHASE_02") ? 2 : 3;
        OutPlan.BossHealthFraction = HealthFraction;
        OutPlan.BossPhaseRevision = static_cast<int32>(PhaseRevision);
    }
    else if (Combat->HasField(TEXT("boss")))
    {
        return Fail(TEXT("BOSS_SNAPSHOT_NOT_ALLOWED"), TEXT("Boss data is allowed only at a BossPhaseBoundary."));
    }

    if (Phase == EJumanjiWorldEncounterPhase::Reward)
    {
        const TSharedPtr<FJsonObject>* RewardPtr = nullptr;
        FString RewardDefinitionId, TransactionId, Status;
        if (!Payload->TryGetObjectField(TEXT("reward"), RewardPtr) || !RewardPtr || !RewardPtr->IsValid()
            || !HasExactKeys(*RewardPtr, {
                TEXT("rewardDefinitionId"), TEXT("transactionId"), TEXT("status")
            })
            || !ReadString(*RewardPtr, TEXT("rewardDefinitionId"), RewardDefinitionId)
            || !ReadString(*RewardPtr, TEXT("transactionId"), TransactionId)
            || !ReadString(*RewardPtr, TEXT("status"), Status)
            || Status != TEXT("PendingLedgerCheck")
            || !IsStableContentId(RewardDefinitionId, 96)
            || !IsStableContentId(TransactionId, 128)
            || Expectations.AllowedRewardDefinitionId.IsNone()
            || RewardDefinitionId != Expectations.AllowedRewardDefinitionId.ToString())
        {
            return Fail(TEXT("REWARD_PREFLIGHT_FAILED"), TEXT("Reward checkpoint lacks an approved stable idempotency reference."));
        }
        OutPlan.RewardDefinitionId = FName(*RewardDefinitionId);
        OutPlan.RewardTransactionId = FName(*TransactionId);
    }
    else if (Payload->HasField(TEXT("reward")))
    {
        return Fail(TEXT("REWARD_SNAPSHOT_NOT_ALLOWED"), TEXT("Reward data is allowed only in the Reward phase."));
    }

    return true;
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::ReadAndStageRestorePlan(
    const FJumanjiEncounterCheckpointRestoreExpectations& Expectations,
    FJumanjiCheckpointRuntimeCallback Callback)
{
    if (!IsServerRuntime() || !bLeaseReady || Expectations.EncounterId != ActiveEncounterId
        || Expectations.ChapterId.IsNone() || Expectations.BoardEventId.IsNone()
        || Expectations.SpawnSetId.IsNone() || Expectations.BossDefinitionId.IsNone()
        || Expectations.AllowedRewardDefinitionId.IsNone()
        || Expectations.AllowedObjectiveIds.IsEmpty() || Expectations.AllowedTrialIds.IsEmpty()
        || Expectations.ServerBuildId.IsEmpty() || Expectations.RulesetVersion.IsEmpty()
        || Expectations.WorldContentVersion.IsEmpty())
    {
        Complete(Callback, false, TEXT("RESTORE_PRECONDITIONS_MISSING"),
            TEXT("A server lease and complete current-content expectations are required."));
        return false;
    }

    const TWeakObjectPtr<UJumanjiEncounterCheckpointRuntimeSubsystem> WeakThis(this);
    return BackendSubsystem->ReadCheckpoint(
        ActiveEncounterId,
        LeaseToken,
        AuthorityEpoch,
        FJumanjiCheckpointBackendCallback::CreateLambda(
            [WeakThis, Expectations, Callback](const FJumanjiCheckpointBackendResponse& Response)
            {
                UJumanjiEncounterCheckpointRuntimeSubsystem* Self = WeakThis.Get();
                if (!Self)
                {
                    return;
                }
                if (!Response.bTransportSucceeded || Response.HttpStatusCode != 200)
                {
                    Complete(Callback, false, TEXT("CHECKPOINT_READ_FAILED"),
                        FString::Printf(TEXT("Backend returned HTTP %d; no restore plan was staged."), Response.HttpStatusCode));
                    return;
                }

                TSharedPtr<FJsonObject> Head;
                const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response.ResponseBody);
                int64 Sequence = 0;
                if (!FJsonSerializer::Deserialize(Reader, Head) || !Head.IsValid()
                    || !ReadInteger(Head, TEXT("checkpointSequence"), Sequence, 1))
                {
                    Complete(Callback, false, TEXT("CHECKPOINT_HEAD_INVALID"), TEXT("Backend response head is malformed."));
                    return;
                }

                FJumanjiEncounterCheckpointRestorePlan Plan;
                FString ErrorCode, ErrorMessage;
                if (!Self->ParseAndValidateRestoreEnvelope(
                    Response.ResponseBody,
                    Expectations,
                    Sequence,
                    Plan,
                    ErrorCode,
                    ErrorMessage))
                {
                    Self->bHasStagedRestorePlan = false;
                    Self->StagedRestorePlan = FJumanjiEncounterCheckpointRestorePlan();
                    Complete(Callback, false, *ErrorCode, ErrorMessage);
                    return;
                }

                Self->StagedRestorePlan = MoveTemp(Plan);
                Self->bHasStagedRestorePlan = true;
                Complete(Callback, true, TEXT("RESTORE_PLAN_STAGED"),
                    TEXT("Checkpoint passed schema, signature-service, version, allowlist and participant preflight. Live world state was not changed."),
                    Self->StagedRestorePlan.CheckpointSequence);
            }));
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::GetStagedRestorePlan(
    FJumanjiEncounterCheckpointRestorePlan& OutPlan) const
{
    if (!IsServerRuntime() || !bHasStagedRestorePlan)
    {
        return false;
    }
    OutPlan = StagedRestorePlan;
    return true;
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::ClearStagedRestorePlan()
{
    if (IsServerRuntime())
    {
        bHasStagedRestorePlan = false;
        StagedRestorePlan = FJumanjiEncounterCheckpointRestorePlan();
    }
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::ReleaseEncounterLease(
    FJumanjiCheckpointRuntimeCallback Callback)
{
    if (!bLeaseReady || !BackendSubsystem)
    {
        Complete(Callback, false, TEXT("LEASE_NOT_ACTIVE"), TEXT("No active checkpoint lease can be released."));
        return false;
    }

    const FName ReleasingEncounter = ActiveEncounterId;
    const FString ReleasingToken = LeaseToken;
    const int64 ReleasingEpoch = AuthorityEpoch;
    const TWeakObjectPtr<UJumanjiEncounterCheckpointRuntimeSubsystem> WeakThis(this);
    return BackendSubsystem->ReleaseLease(
        ReleasingEncounter,
        ReleasingToken,
        ReleasingEpoch,
        FJumanjiCheckpointBackendCallback::CreateLambda(
            [WeakThis, Callback](const FJumanjiCheckpointBackendResponse& Response)
            {
                UJumanjiEncounterCheckpointRuntimeSubsystem* Self = WeakThis.Get();
                if (!Self)
                {
                    return;
                }
                const bool bSuccess = Response.bTransportSucceeded && Response.HttpStatusCode == 200;
                if (bSuccess)
                {
                    Self->bLeaseReady = false;
                    Self->bLeaseRequestPending = false;
                    Self->LeaseToken.Reset();
                    Self->AuthorityEpoch = 0;
                    Self->ActiveEncounterId = NAME_None;
                }
                Complete(Callback, bSuccess,
                    bSuccess ? TEXT("LEASE_RELEASED") : TEXT("LEASE_RELEASE_FAILED"),
                    FString::Printf(TEXT("Lease release returned HTTP %d."), Response.HttpStatusCode),
                    Self->LastCheckpointSequence);
            }));
}
