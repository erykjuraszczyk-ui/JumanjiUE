#include "JumanjiEncounterCheckpointRuntimeSubsystem.h"

#include "JumanjiEncounterCheckpointBackendSubsystem.h"
#include "JumanjiWorldEncounterDirector.h"
#include "JumanjiWorldEncounterSessionSubsystem.h"
#include "JumanjiAdventureChapterDirector.h"
#include "JumanjiAdventureChapter.h"
#include "JumanjiAdventureObjective.h"
#include "JumanjiGameState.h"
#include "JumanjiBossCharacter.h"
#include "JumanjiPlayerState.h"
#include "JumanjiProgressionRewardSubsystem.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "IO/IoHash.h"
#include "HAL/PlatformMisc.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace JumanjiCheckpointRuntime
{
    constexpr int32 LeaseTtlSeconds = 120;
    constexpr float LeaseRenewalIntervalSeconds = 60.0f;
    constexpr int32 CheckpointLifetimeMinutes = 30;
    constexpr int32 MaxParticipants = 64;

    FString ReadEnv(const TCHAR* Key)
    {
        return FPlatformMisc::GetEnvironmentVariable(Key).TrimStartAndEnd();
    }

    bool IsStableId(const FString& Value, int32 MaxLength = 96)
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

    bool IsSafeServerValue(const FString& Value, int32 MaxLength = 128)
    {
        if (Value.Len() < 2 || Value.Len() > MaxLength)
        {
            return false;
        }

        for (const TCHAR Character : Value)
        {
            if (!((Character >= TEXT('A') && Character <= TEXT('Z'))
                || (Character >= TEXT('a') && Character <= TEXT('z'))
                || (Character >= TEXT('0') && Character <= TEXT('9'))
                || Character == TEXT('_') || Character == TEXT('-')
                || Character == TEXT('.')))
            {
                return false;
            }
        }
        return true;
    }

    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& OutValue)
    {
        return Object.IsValid()
            && Object->TryGetStringField(Field, OutValue)
            && !OutValue.IsEmpty();
    }

    bool ReadInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int64& OutValue)
    {
        double Number = 0.0;
        if (!Object.IsValid()
            || !Object->TryGetNumberField(Field, Number)
            || !FMath::IsFinite(Number)
            || FMath::FloorToDouble(Number) != Number
            || Number < static_cast<double>(MIN_int64)
            || Number > static_cast<double>(MAX_int64))
        {
            return false;
        }

        OutValue = static_cast<int64>(Number);
        return true;
    }

    FString SafePointName(EJumanjiEncounterCheckpointSafePoint SafePoint)
    {
        switch (SafePoint)
        {
        case EJumanjiEncounterCheckpointSafePoint::TrialBoundary: return TEXT("TrialBoundary");
        case EJumanjiEncounterCheckpointSafePoint::WaveBoundary: return TEXT("WaveBoundary");
        case EJumanjiEncounterCheckpointSafePoint::BossPhaseBoundary: return TEXT("BossPhaseBoundary");
        case EJumanjiEncounterCheckpointSafePoint::RewardPending: return TEXT("RewardPending");
        default: return FString();
        }
    }

    FString EncounterPhaseName(EJumanjiWorldEncounterPhase Phase)
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

    bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool IsParticipantKeyFormatValid(const FString& Key)
    {
        if (!Key.StartsWith(TEXT("pk_")) || Key.Len() < 23 || Key.Len() > 99)
        {
            return false;
        }

        for (int32 Index = 3; Index < Key.Len(); ++Index)
        {
            const TCHAR Character = Key[Index];
            const bool bAllowed =
                (Character >= TEXT('A') && Character <= TEXT('Z'))
                || (Character >= TEXT('a') && Character <= TEXT('z'))
                || (Character >= TEXT('0') && Character <= TEXT('9'))
                || Character == TEXT('_') || Character == TEXT('-');
            if (!bAllowed)
            {
                return false;
            }
        }
        return true;
    }
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BackendSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointBackendSubsystem>()
        : nullptr;
    ClearLeaseState();
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LeaseRenewalTimerHandle);
    }

    // Do not block shutdown on a network call. The lease expires through the
    // trusted backend TTL if this server process is stopping.
    ClearLeaseState();
    Super::Deinitialize();
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::IsServerRuntime() const
{
    const UWorld* World = GetWorld();
    return World && World->GetNetMode() != NM_Client;
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::ReadRequiredServerMetadata(
    FString& OutServerInstanceId,
    FString& OutShardId,
    FString& OutBuildId,
    FString& OutRulesetVersion,
    FString& OutWorldContentVersion,
    FString& OutParticipantSalt) const
{
    if (!IsServerRuntime())
    {
        return false;
    }

    OutServerInstanceId = JumanjiCheckpointRuntime::ReadEnv(TEXT("JUMANJI_SERVER_INSTANCE_ID"));
    OutShardId = JumanjiCheckpointRuntime::ReadEnv(TEXT("JUMANJI_SHARD_ID"));
    OutBuildId = JumanjiCheckpointRuntime::ReadEnv(TEXT("JUMANJI_SERVER_BUILD_ID"));
    OutRulesetVersion = JumanjiCheckpointRuntime::ReadEnv(TEXT("JUMANJI_RULESET_VERSION"));
    OutWorldContentVersion = JumanjiCheckpointRuntime::ReadEnv(TEXT("JUMANJI_WORLD_CONTENT_VERSION"));
    OutParticipantSalt = JumanjiCheckpointRuntime::ReadEnv(TEXT("JUMANJI_CHECKPOINT_PARTICIPANT_SALT"));

    return JumanjiCheckpointRuntime::IsSafeServerValue(OutServerInstanceId, 96)
        && JumanjiCheckpointRuntime::IsSafeServerValue(OutShardId, 96)
        && JumanjiCheckpointRuntime::IsSafeServerValue(OutBuildId, 128)
        && JumanjiCheckpointRuntime::IsSafeServerValue(OutRulesetVersion, 64)
        && JumanjiCheckpointRuntime::IsSafeServerValue(OutWorldContentVersion, 64)
        && OutParticipantSalt.Len() >= 32;
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::BeginEncounterCheckpointSession(FName EncounterId)
{
    if (!IsServerRuntime()
        || EncounterId.IsNone()
        || EncounterId.ToString().Len() > 64
        || bLeaseAcquiring
        || bLeaseReady
        || bWriteInFlight
        || bReadInFlight
        || !ActiveEncounterId.IsNone())
    {
        return false;
    }

    UJumanjiWorldEncounterSessionSubsystem* SessionSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiWorldEncounterSessionSubsystem>()
        : nullptr;
    if (!SessionSubsystem || !SessionSubsystem->GetActiveSessionId().IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI checkpoint: cannot acquire lease without an active encounter session UUID."));
        return false;
    }

    ClearLeaseState();
    ActiveEncounterId = EncounterId;
    ActiveSessionId = SessionSubsystem->GetActiveSessionId();
    BackendSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointBackendSubsystem>()
        : nullptr;

    if (!BackendSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI checkpoint: backend adapter is unavailable."));
        ClearLeaseState();
        return false;
    }

    bLeaseAcquiring = true;
    const bool bStarted = BackendSubsystem->AcquireLease(
        EncounterId,
        JumanjiCheckpointRuntime::LeaseTtlSeconds,
        FJumanjiCheckpointBackendCallback::CreateUObject(
            this,
            &UJumanjiEncounterCheckpointRuntimeSubsystem::HandleLeaseAcquired));

    if (!bStarted)
    {
        ClearLeaseState();
    }
    return bStarted;
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::EndEncounterCheckpointSession(FName EncounterId)
{
    if (EncounterId.IsNone() || ActiveEncounterId != EncounterId)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LeaseRenewalTimerHandle);
    }

    if (bWriteInFlight || bLeaseAcquiring || bReadInFlight)
    {
        bReleaseLeaseWhenIdle = true;
        return;
    }

    if (bLeaseReady && BackendSubsystem && !LeaseToken.IsEmpty())
    {
        BackendSubsystem->ReleaseLease(
            ActiveEncounterId,
            LeaseToken,
            AuthorityEpoch,
            FJumanjiCheckpointBackendCallback());
    }

    ClearLeaseState();
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::ClearLeaseState()
{
    ActiveEncounterId = NAME_None;
    ActiveSessionId.Invalidate();
    LeaseToken.Reset();
    AuthorityEpoch = 0;
    LastCommittedSequence = 0;
    bLeaseReady = false;
    bLeaseAcquiring = false;
    bWriteInFlight = false;
    bReadInFlight = false;
    bCheckpointWriteBlockedUntilResync = false;
    bRestoreStagingRequested = false;
    bReleaseLeaseWhenIdle = false;
    PendingRestoreDirector.Reset();
    bHasStagedRestoreCandidate = false;
    StagedRestoreCandidate = FJumanjiEncounterCheckpointRestoreCandidate();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LeaseRenewalTimerHandle);
    }
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::HandleLeaseAcquired(
    const FJumanjiCheckpointBackendResponse& Response)
{
    bLeaseAcquiring = false;

    TSharedPtr<FJsonObject> Body;
    FString NewLeaseToken;
    int64 NewEpoch = 0;
    if (!Response.bTransportSucceeded
        || Response.HttpStatusCode != 201
        || !JumanjiCheckpointRuntime::ParseJsonObject(Response.ResponseBody, Body)
        || !JumanjiCheckpointRuntime::ReadString(Body, TEXT("leaseToken"), NewLeaseToken)
        || NewLeaseToken.Len() < 32
        || !JumanjiCheckpointRuntime::ReadInteger(Body, TEXT("authorityEpoch"), NewEpoch)
        || NewEpoch < 1)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI checkpoint: backend lease acquisition failed; snapshot writes remain disabled."));
        bLeaseReady = false;
        if (bRestoreStagingRequested || bReleaseLeaseWhenIdle)
        {
            ClearLeaseState();
        }
        return;
    }

    LeaseToken = MoveTemp(NewLeaseToken);
    AuthorityEpoch = NewEpoch;
    bLeaseReady = true;
    bCheckpointWriteBlockedUntilResync = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            LeaseRenewalTimerHandle,
            this,
            &UJumanjiEncounterCheckpointRuntimeSubsystem::RenewLease,
            JumanjiCheckpointRuntime::LeaseRenewalIntervalSeconds,
            true);
    }

    if (bReleaseLeaseWhenIdle)
    {
        EndEncounterCheckpointSession(ActiveEncounterId);
        return;
    }

    if (bRestoreStagingRequested && PendingRestoreDirector.IsValid())
    {
        bReadInFlight = true;
        const bool bReadStarted = BackendSubsystem->ReadCheckpoint(
            ActiveEncounterId,
            LeaseToken,
            AuthorityEpoch,
            FJumanjiCheckpointBackendCallback::CreateUObject(
                this,
                &UJumanjiEncounterCheckpointRuntimeSubsystem::HandleCheckpointRead,
                PendingRestoreDirector));
        if (!bReadStarted)
        {
            bReadInFlight = false;
            bRestoreStagingRequested = false;
            EndEncounterCheckpointSession(ActiveEncounterId);
        }
    }
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::RenewLease()
{
    if (!bLeaseReady || !BackendSubsystem || LeaseToken.IsEmpty() || ActiveEncounterId.IsNone())
    {
        return;
    }

    BackendSubsystem->RenewLease(
        ActiveEncounterId,
        LeaseToken,
        AuthorityEpoch,
        JumanjiCheckpointRuntime::LeaseTtlSeconds,
        FJumanjiCheckpointBackendCallback::CreateLambda(
            [WeakThis = TWeakObjectPtr<UJumanjiEncounterCheckpointRuntimeSubsystem>(this)]
            (const FJumanjiCheckpointBackendResponse& Response)
            {
                if (!WeakThis.IsValid())
                {
                    return;
                }

                if (!Response.bTransportSucceeded || Response.HttpStatusCode != 200)
                {
                    UE_LOG(LogTemp, Error,
                        TEXT("JUMANJI checkpoint: lease renewal failed; checkpoint writes are fenced off."));
                    WeakThis->bLeaseReady = false;
                    WeakThis->bCheckpointWriteBlockedUntilResync = true;

                    if (UWorld* World = WeakThis->GetWorld())
                    {
                        World->GetTimerManager().ClearTimer(WeakThis->LeaseRenewalTimerHandle);
                    }
                }
            }));
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::BuildOpaqueParticipantKey(
    const FString& PersistentProfileId,
    const FString& Salt,
    FString& OutParticipantKey) const
{
    if (PersistentProfileId.IsEmpty() || Salt.Len() < 32)
    {
        return false;
    }

    const FString Material = Salt + TEXT("|") + PersistentProfileId;
    FTCHARToUTF8 Utf8Material(*Material);
    const FIoHash Hash = FIoHash::HashBuffer(
        Utf8Material.Get(),
        static_cast<uint64>(Utf8Material.Length()));

    // FIoHash is Unreal's BLAKE3-160 content hash. The high-entropy server
    // salt makes this a stable opaque pseudonymous participant key.
    OutParticipantKey = TEXT("pk_") + LexToString(Hash);
    return JumanjiCheckpointRuntime::IsParticipantKeyFormatValid(OutParticipantKey);
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::BuildPayloadJson(
    AJumanjiWorldEncounterDirector* EncounterDirector,
    EJumanjiEncounterCheckpointSafePoint SafePoint,
    FName RewardTransactionId,
    int64 CheckpointSequence,
    FString& OutPayloadJson,
    FString& OutError) const
{
    OutPayloadJson.Reset();
    OutError.Reset();

    if (!IsServerRuntime()
        || !IsValid(EncounterDirector)
        || !EncounterDirector->HasAuthority()
        || EncounterDirector->GetEncounterId() != ActiveEncounterId
        || !ActiveSessionId.IsValid()
        || CheckpointSequence < 1)
    {
        OutError = TEXT("SERVER_CONTEXT_INVALID");
        return false;
    }

    FString ServerInstanceId, ShardId, BuildId, RulesetVersion, WorldContentVersion, ParticipantSalt;
    if (!ReadRequiredServerMetadata(
        ServerInstanceId, ShardId, BuildId, RulesetVersion, WorldContentVersion, ParticipantSalt))
    {
        OutError = TEXT("CHECKPOINT_SERVER_METADATA_MISSING");
        return false;
    }

    UWorld* World = GetWorld();
    AJumanjiGameState* GameState = World ? World->GetGameState<AJumanjiGameState>() : nullptr;
    AJumanjiAdventureChapterDirector* ChapterDirector = EncounterDirector->GetAdventureChapterDirector();
    const UJumanjiWorldEncounterSessionSubsystem* SessionSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiWorldEncounterSessionSubsystem>()
        : nullptr;

    if (!GameState || !ChapterDirector || !SessionSubsystem
        || SessionSubsystem->GetActiveSessionId() != ActiveSessionId)
    {
        OutError = TEXT("CHECKPOINT_WORLD_CONTEXT_MISSING");
        return false;
    }

    const FJumanjiWorldEncounterState EncounterState = GameState->GetWorldEncounterState();
    const FJumanjiAdventureChapterState ChapterState = GameState->GetAdventureChapterState();

    const bool bTrialBoundary = SafePoint == EJumanjiEncounterCheckpointSafePoint::TrialBoundary;
    const bool bExpectedChapterPhase = bTrialBoundary
        ? ChapterState.Phase == EJumanjiAdventureChapterPhase::Active
        : ChapterState.Phase == EJumanjiAdventureChapterPhase::Completed;
    const bool bBoardEventStateValid = bTrialBoundary
        ? (GameState->IsBoardEventActive()
            && GameState->GetActiveBoardEventId() == ChapterDirector->GetBoardEventId())
        : !GameState->IsBoardEventActive();

    if (!JumanjiCheckpointRuntime::IsStableId(ActiveEncounterId.ToString(), 96)
        || !JumanjiCheckpointRuntime::IsStableId(ChapterState.ChapterId.ToString(), 96)
        || !JumanjiCheckpointRuntime::IsStableId(ChapterState.BoardEventId.ToString(), 96)
        || EncounterState.Revision < 1
        || ChapterState.Revision < 1
        || EncounterState.TotalWaves != EncounterDirector->GetTotalWaveCount()
        || EncounterState.TotalWaves < 1
        || EncounterState.TotalWaves > 100
        || EncounterState.EncounterId != ActiveEncounterId
        || ChapterState.ChapterId != ChapterDirector->GetChapterId()
        || ChapterState.BoardEventId != ChapterDirector->GetBoardEventId()
        || !bExpectedChapterPhase
        || !bBoardEventStateValid
        || !ChapterDirector->HasNoTrialsInProgress()
        || (!bTrialBoundary && !GameState->AreAdventureObjectivesComplete()))
    {
        OutError = TEXT("CHECKPOINT_CHAPTER_NOT_AT_SAFE_BOUNDARY");
        return false;
    }

    const FString PhaseName = JumanjiCheckpointRuntime::EncounterPhaseName(EncounterState.Phase);
    const FString KindName = JumanjiCheckpointRuntime::SafePointName(SafePoint);
    if (!JumanjiCheckpointRuntime::IsStableId(EncounterDirector->GetBossDefinitionId().ToString(), 64)
        || !JumanjiCheckpointRuntime::IsStableId(EncounterDirector->GetSpawnSetId().ToString(), 64)
        || PhaseName.IsEmpty() || KindName.IsEmpty())
    {
        OutError = TEXT("CHECKPOINT_SAFE_POINT_UNKNOWN");
        return false;
    }

    if ((SafePoint == EJumanjiEncounterCheckpointSafePoint::TrialBoundary
            && EncounterState.Phase != EJumanjiWorldEncounterPhase::Trials)
        || (SafePoint == EJumanjiEncounterCheckpointSafePoint::WaveBoundary
            && (EncounterState.Phase != EJumanjiWorldEncounterPhase::Combat
                || EncounterState.CurrentWave < 1
                || EncounterState.ActiveEnemyCount != 0))
        || (SafePoint == EJumanjiEncounterCheckpointSafePoint::BossPhaseBoundary
            && EncounterState.Phase != EJumanjiWorldEncounterPhase::Boss)
        || (SafePoint == EJumanjiEncounterCheckpointSafePoint::RewardPending
            && (EncounterState.Phase != EJumanjiWorldEncounterPhase::Reward
                || EncounterState.ActiveEnemyCount != 0
                || RewardTransactionId.IsNone())))
    {
        OutError = TEXT("CHECKPOINT_SAFE_POINT_NOT_STABLE");
        return false;
    }

    AJumanjiBossCharacter* Boss = EncounterState.BossActor;
    if (SafePoint == EJumanjiEncounterCheckpointSafePoint::BossPhaseBoundary)
    {
        if (!IsValid(Boss)
            || Boss->IsDefeated()
            || Boss->GetBossTelegraphState().Stage != EJumanjiBossTelegraphStage::None
            || Boss->GetBossPhase() < 1
            || Boss->GetBossPhase() > 3
            || Boss->GetBossPhaseRevision() < 1)
        {
            OutError = TEXT("CHECKPOINT_BOSS_NOT_AT_PHASE_BOUNDARY");
            return false;
        }
    }

    const TArray<FName> ConfiguredObjectiveIds = ChapterDirector->GetConfiguredObjectiveIds();
    const TArray<FName> ConfiguredTrialIds = ChapterDirector->GetConfiguredTrialIds();
    const TArray<FName> CompletedTrialIds = ChapterDirector->GetCompletedTrialIds();
    const TArray<FName> CompletedTrialObjectiveIds = ChapterDirector->GetCompletedTrialObjectiveIds();
    TSet<FName> ConfiguredObjectives;
    TSet<FName> ConfiguredTrials;
    for (const FName Id : ConfiguredObjectiveIds) { ConfiguredObjectives.Add(Id); }
    for (const FName Id : ConfiguredTrialIds) { ConfiguredTrials.Add(Id); }
    if (ConfiguredObjectives.Num() != ConfiguredObjectiveIds.Num()
        || ConfiguredTrials.Num() != ConfiguredTrialIds.Num())
    {
        OutError = TEXT("CHECKPOINT_DUPLICATE_CONFIGURED_CONTENT_ID");
        return false;
    }

    TArray<FString> CompletedObjectiveNames;
    for (const FJumanjiAdventureObjectiveState& Objective : GameState->GetAdventureObjectives())
    {
        if (Objective.bCompleted)
        {
            if (!ConfiguredObjectives.Contains(Objective.ObjectiveId)
                || !JumanjiCheckpointRuntime::IsStableId(Objective.ObjectiveId.ToString(), 96))
            {
                OutError = TEXT("CHECKPOINT_OBJECTIVE_ID_NOT_ALLOWLISTED");
                return false;
            }
            CompletedObjectiveNames.Add(Objective.ObjectiveId.ToString());
        }
    }

    TArray<FString> CompletedTrialNames;
    for (const FName TrialId : CompletedTrialIds)
    {
        if (!ConfiguredTrials.Contains(TrialId)
            || !JumanjiCheckpointRuntime::IsStableId(TrialId.ToString(), 96))
        {
            OutError = TEXT("CHECKPOINT_TRIAL_ID_NOT_ALLOWLISTED");
            return false;
        }
        CompletedTrialNames.Add(TrialId.ToString());
    }

    if (ConfiguredObjectiveIds.IsEmpty()
        || ConfiguredTrialIds.IsEmpty()
        || CompletedObjectiveNames.Num() != CompletedTrialNames.Num())
    {
        OutError = TEXT("CHECKPOINT_CHAPTER_CONTENT_INCOMPLETE");
        return false;
    }

    if (!ChapterDirector->AreCheckpointTrialIdsConsistent(
        CompletedTrialIds,
        CompletedTrialObjectiveIds))
    {
        OutError = TEXT("CHECKPOINT_TRIAL_OBJECTIVE_STATE_MISMATCH");
        return false;
    }

    if (!bTrialBoundary
        && (CompletedObjectiveNames.Num() != ConfiguredObjectives.Num()
            || CompletedTrialNames.Num() != ConfiguredTrials.Num()))
    {
        OutError = TEXT("CHECKPOINT_CHAPTER_CONTENT_INCOMPLETE");
        return false;
    }

    TArray<AJumanjiPlayerState*> Participants;
    for (APlayerState* BasePlayerState : GameState->PlayerArray)
    {
        AJumanjiPlayerState* PlayerState = Cast<AJumanjiPlayerState>(BasePlayerState);
        if (PlayerState && !PlayerState->GetPersistentProfileId().IsEmpty())
        {
            Participants.Add(PlayerState);
        }
    }

    if (Participants.IsEmpty() || Participants.Num() > JumanjiCheckpointRuntime::MaxParticipants)
    {
        OutError = TEXT("CHECKPOINT_PARTICIPANT_COUNT_INVALID");
        return false;
    }

    // The v1 contract has a single reward transactionId. Do not write a
    // misleading group RewardPending checkpoint for a multi-profile party.
    if (SafePoint == EJumanjiEncounterCheckpointSafePoint::RewardPending && Participants.Num() != 1)
    {
        OutError = TEXT("REWARD_CHECKPOINT_V1_REQUIRES_SINGLE_PARTICIPANT");
        return false;
    }

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetNumberField(TEXT("schemaVersion"), 1);
    Payload->SetStringField(TEXT("checkpointId"), FGuid::NewGuid().ToString());
    Payload->SetStringField(TEXT("encounterId"), ActiveEncounterId.ToString());
    Payload->SetStringField(TEXT("sessionId"), ActiveSessionId.ToString());
    Payload->SetNumberField(TEXT("checkpointSequence"), static_cast<double>(CheckpointSequence));

    const FDateTime Now = FDateTime::UtcNow();
    const FDateTime Expires = Now + FTimespan::FromMinutes(JumanjiCheckpointRuntime::CheckpointLifetimeMinutes);
    Payload->SetStringField(TEXT("createdAtUtc"), Now.ToIso8601());
    Payload->SetStringField(TEXT("expiresAtUtc"), Expires.ToIso8601());

    TSharedRef<FJsonObject> Authority = MakeShared<FJsonObject>();
    Authority->SetStringField(TEXT("serverInstanceId"), ServerInstanceId);
    Authority->SetStringField(TEXT("shardId"), ShardId);
    Authority->SetNumberField(TEXT("authorityEpoch"), static_cast<double>(AuthorityEpoch));
    Authority->SetStringField(TEXT("serverBuildId"), BuildId);
    Authority->SetStringField(TEXT("rulesetVersion"), RulesetVersion);
    Authority->SetStringField(TEXT("worldContentVersion"), WorldContentVersion);
    Payload->SetObjectField(TEXT("authority"), Authority);

    TSharedRef<FJsonObject> SafePointObject = MakeShared<FJsonObject>();
    SafePointObject->SetStringField(TEXT("phase"), PhaseName);
    SafePointObject->SetStringField(TEXT("kind"), KindName);
    SafePointObject->SetNumberField(TEXT("encounterRevision"), EncounterState.Revision);

    TSharedRef<FJsonObject> Chapter = MakeShared<FJsonObject>();
    Chapter->SetStringField(TEXT("chapterId"), ChapterState.ChapterId.ToString());
    Chapter->SetStringField(TEXT("boardEventId"), ChapterState.BoardEventId.ToString());
    Chapter->SetNumberField(TEXT("chapterRevision"), ChapterState.Revision);

    TArray<TSharedPtr<FJsonValue>> ObjectiveValues;
    for (const FString& Id : CompletedObjectiveNames)
    {
        ObjectiveValues.Add(MakeShared<FJsonValueString>(Id));
    }
    Chapter->SetArrayField(TEXT("completedObjectiveIds"), ObjectiveValues);

    TArray<TSharedPtr<FJsonValue>> TrialValues;
    for (const FString& Id : CompletedTrialNames)
    {
        TrialValues.Add(MakeShared<FJsonValueString>(Id));
    }
    Chapter->SetArrayField(TEXT("completedTrialIds"), TrialValues);
    SafePointObject->SetObjectField(TEXT("chapter"), Chapter);

    TSharedRef<FJsonObject> Combat = MakeShared<FJsonObject>();
    Combat->SetNumberField(TEXT("currentWave"), SafePoint == EJumanjiEncounterCheckpointSafePoint::WaveBoundary
        ? EncounterState.CurrentWave : 0);
    Combat->SetNumberField(TEXT("totalWaves"), EncounterState.TotalWaves);

    if (SafePoint == EJumanjiEncounterCheckpointSafePoint::BossPhaseBoundary)
    {
        TSharedRef<FJsonObject> BossSnapshot = MakeShared<FJsonObject>();
        const FString BossPhaseId = FString::Printf(TEXT("BOSS_PHASE_%02d"), Boss->GetBossPhase());
        BossSnapshot->SetStringField(TEXT("definitionId"), EncounterDirector->GetBossDefinitionId().ToString());
        BossSnapshot->SetStringField(TEXT("phaseId"), BossPhaseId);
        BossSnapshot->SetNumberField(TEXT("healthFraction"), Boss->GetBossHealthFraction());
        BossSnapshot->SetNumberField(TEXT("phaseRevision"), Boss->GetBossPhaseRevision());
        Combat->SetObjectField(TEXT("boss"), BossSnapshot);
    }

    SafePointObject->SetObjectField(TEXT("combat"), Combat);
    Payload->SetObjectField(TEXT("safePoint"), SafePointObject);

    TSharedRef<FJsonObject> Deterministic = MakeShared<FJsonObject>();
    Deterministic->SetNumberField(TEXT("encounterSeed"), static_cast<double>(EncounterDirector->GetEncounterSeed()));
    Deterministic->SetStringField(TEXT("spawnSetId"), EncounterDirector->GetSpawnSetId().ToString());
    Payload->SetObjectField(TEXT("deterministic"), Deterministic);

    TArray<TSharedPtr<FJsonValue>> ParticipantValues;
    TSet<FString> SeenParticipantKeys;
    for (const AJumanjiPlayerState* PlayerState : Participants)
    {
        FString ParticipantKey;
        if (!BuildOpaqueParticipantKey(PlayerState->GetPersistentProfileId(), ParticipantSalt, ParticipantKey)
            || SeenParticipantKeys.Contains(ParticipantKey))
        {
            OutError = TEXT("CHECKPOINT_PARTICIPANT_KEY_CREATION_FAILED");
            return false;
        }

        SeenParticipantKeys.Add(ParticipantKey);
        TSharedRef<FJsonObject> Participant = MakeShared<FJsonObject>();
        Participant->SetStringField(TEXT("participantKey"), ParticipantKey);
        const int32 ProfileRevision = PlayerState->GetProgressionState().Revision;
        if (ProfileRevision < 0)
        {
            OutError = TEXT("CHECKPOINT_PROFILE_REVISION_INVALID");
            return false;
        }
        Participant->SetNumberField(TEXT("profileRevision"), ProfileRevision);
        ParticipantValues.Add(MakeShared<FJsonValueObject>(Participant));
    }
    Payload->SetArrayField(TEXT("participants"), ParticipantValues);

    if (SafePoint == EJumanjiEncounterCheckpointSafePoint::RewardPending)
    {
        const UJumanjiProgressionRewardSubsystem* RewardSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UJumanjiProgressionRewardSubsystem>()
            : nullptr;
        const FName ExpectedTransactionId = RewardSubsystem
            ? RewardSubsystem->GetWorldEncounterRewardTransactionId(Participants[0], ActiveEncounterId)
            : NAME_None;
        if (ExpectedTransactionId.IsNone() || ExpectedTransactionId != RewardTransactionId)
        {
            OutError = TEXT("CHECKPOINT_REWARD_RECEIPT_MISMATCH");
            return false;
        }

        TSharedRef<FJsonObject> Reward = MakeShared<FJsonObject>();
        // In schema v1 the stable EncounterId is also the reward definition ID.
        Reward->SetStringField(TEXT("rewardDefinitionId"), ActiveEncounterId.ToString());
        Reward->SetStringField(TEXT("transactionId"), RewardTransactionId.ToString());
        Reward->SetStringField(TEXT("status"), TEXT("PendingLedgerCheck"));
        Payload->SetObjectField(TEXT("reward"), Reward);
    }

    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutPayloadJson);
    if (!FJsonSerializer::Serialize(Payload, Writer))
    {
        OutError = TEXT("CHECKPOINT_JSON_SERIALIZATION_FAILED");
        return false;
    }

    return true;
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::CaptureSafePoint(
    AJumanjiWorldEncounterDirector* EncounterDirector,
    EJumanjiEncounterCheckpointSafePoint SafePoint,
    FName RewardTransactionId)
{
    if (!bLeaseReady
        || !BackendSubsystem
        || bWriteInFlight
        || bReadInFlight
        || bCheckpointWriteBlockedUntilResync
        || LastCommittedSequence >= MAX_int32)
    {
        return false;
    }

    const int64 NextSequence = LastCommittedSequence + 1;
    FString PayloadJson;
    FString Error;
    if (!BuildPayloadJson(EncounterDirector, SafePoint, RewardTransactionId, NextSequence, PayloadJson, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI checkpoint snapshot skipped: %s"), *Error);
        return false;
    }

    bWriteInFlight = true;
    const bool bStarted = BackendSubsystem->WriteCheckpoint(
        ActiveEncounterId,
        LeaseToken,
        AuthorityEpoch,
        LastCommittedSequence,
        PayloadJson,
        FJumanjiCheckpointBackendCallback::CreateLambda(
            [WeakThis = TWeakObjectPtr<UJumanjiEncounterCheckpointRuntimeSubsystem>(this), NextSequence]
            (const FJumanjiCheckpointBackendResponse& Response)
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->HandleCheckpointWrite(Response, NextSequence);
                }
            }));

    if (!bStarted)
    {
        bWriteInFlight = false;
        bCheckpointWriteBlockedUntilResync = true;
        if (bReleaseLeaseWhenIdle)
        {
            EndEncounterCheckpointSession(ActiveEncounterId);
        }
    }
    return bStarted;
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::HandleCheckpointWrite(
    const FJumanjiCheckpointBackendResponse& Response,
    int64 WrittenSequence)
{
    bWriteInFlight = false;
    if (Response.bTransportSucceeded && Response.HttpStatusCode == 200)
    {
        LastCommittedSequence = WrittenSequence;
        bCheckpointWriteBlockedUntilResync = false;
        UE_LOG(LogTemp, Log, TEXT("JUMANJI checkpoint committed: encounter=%s sequence=%lld."),
            *ActiveEncounterId.ToString(), LastCommittedSequence);
        if (bReleaseLeaseWhenIdle)
        {
            EndEncounterCheckpointSession(ActiveEncounterId);
        }
        return;
    }

    // A timeout may occur after the backend committed the CAS. Never retry the
    // same sequence blindly; first read the signed head and reconcile it.
    bCheckpointWriteBlockedUntilResync = true;
    if (bReleaseLeaseWhenIdle)
    {
        EndEncounterCheckpointSession(ActiveEncounterId);
    }
    UE_LOG(LogTemp, Error,
        TEXT("JUMANJI checkpoint write failed or was ambiguous (HTTP %d); writes are blocked until resync."),
        Response.HttpStatusCode);
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::BeginRestoreStaging(
    AJumanjiWorldEncounterDirector* EncounterDirector)
{
    if (!IsServerRuntime()
        || !IsValid(EncounterDirector)
        || !EncounterDirector->HasAuthority()
        || EncounterDirector->GetEncounterId().IsNone()
        || bLeaseAcquiring
        || bLeaseReady
        || bWriteInFlight
        || bReadInFlight
        || !ActiveEncounterId.IsNone()
        || !BackendSubsystem)
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const AJumanjiGameState* GameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    if (!GameState
        || (GameState->GetWorldEncounterState().Phase != EJumanjiWorldEncounterPhase::Inactive
            && GameState->GetWorldEncounterState().Phase != EJumanjiWorldEncounterPhase::Failed))
    {
        return false;
    }

    ClearLeaseState();
    ActiveEncounterId = EncounterDirector->GetEncounterId();
    PendingRestoreDirector = EncounterDirector;
    bRestoreStagingRequested = true;
    bLeaseAcquiring = true;

    const bool bStarted = BackendSubsystem->AcquireLease(
        ActiveEncounterId,
        JumanjiCheckpointRuntime::LeaseTtlSeconds,
        FJumanjiCheckpointBackendCallback::CreateUObject(
            this,
            &UJumanjiEncounterCheckpointRuntimeSubsystem::HandleLeaseAcquired));

    if (!bStarted)
    {
        ClearLeaseState();
    }
    return bStarted;
}

void UJumanjiEncounterCheckpointRuntimeSubsystem::HandleCheckpointRead(
    const FJumanjiCheckpointBackendResponse& Response,
    TWeakObjectPtr<AJumanjiWorldEncounterDirector> EncounterDirector)
{
    bReadInFlight = false;

    if (!Response.bTransportSucceeded || Response.HttpStatusCode != 200
        || !EncounterDirector.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI checkpoint restore staging read failed (HTTP %d)."),
            Response.HttpStatusCode);
        bRestoreStagingRequested = false;
        PendingRestoreDirector.Reset();
        EndEncounterCheckpointSession(ActiveEncounterId);
        return;
    }

    FString Error;
    if (!StageVerifiedBackendResponse(Response.ResponseBody, EncounterDirector.Get(), Error))
    {
        UE_LOG(LogTemp, Error, TEXT("JUMANJI checkpoint restore preflight rejected: %s"), *Error);
        bRestoreStagingRequested = false;
        PendingRestoreDirector.Reset();
        bHasStagedRestoreCandidate = false;
        EndEncounterCheckpointSession(ActiveEncounterId);
        return;
    }

    LastCommittedSequence = StagedRestoreCandidate.CheckpointSequence;
    bCheckpointWriteBlockedUntilResync = false;
    bRestoreStagingRequested = false;
    PendingRestoreDirector.Reset();

    UE_LOG(LogTemp, Log,
        TEXT("JUMANJI checkpoint staged privately: encounter=%s sequence=%lld. No world state has been published."),
        *StagedRestoreCandidate.EncounterId.ToString(),
        StagedRestoreCandidate.CheckpointSequence);
}

bool UJumanjiEncounterCheckpointRuntimeSubsystem::StageVerifiedBackendResponse(
    const FString& ResponseBody,
    AJumanjiWorldEncounterDirector* EncounterDirector,
    FString& OutError)
{
    OutError.Reset();

    TSharedPtr<FJsonObject> ResponseObject;
    if (!JumanjiCheckpointRuntime::ParseJsonObject(ResponseBody, ResponseObject))
    {
        OutError = TEXT("BACKEND_RESPONSE_NOT_JSON");
        return false;
    }

    // Stage 33's trusted service has already revalidated the stored envelope
    // and cryptographically verified its Ed25519 signature before HTTP 200.
    // This client-side preflight checks structure and game/content semantics;
    // the private signing key never exists in the Unreal server process.
    TSharedPtr<FJsonObject> Envelope;
    if (!ResponseObject->TryGetObjectField(TEXT("envelope"), Envelope) || !Envelope.IsValid())
    {
        OutError = TEXT("SIGNED_ENVELOPE_MISSING");
        return false;
    }

    int64 EnvelopeVersion = 0;
    TSharedPtr<FJsonObject> Integrity;
    TSharedPtr<FJsonObject> Payload;
    FString Algorithm, KeyId, Signature, ResponseEncounterId;
    int64 ResponseSequence = 0;
    int64 ResponseAuthorityEpoch = 0;
    if (!JumanjiCheckpointRuntime::ReadString(ResponseObject, TEXT("encounterId"), ResponseEncounterId)
        || !JumanjiCheckpointRuntime::ReadInteger(ResponseObject, TEXT("checkpointSequence"), ResponseSequence)
        || !JumanjiCheckpointRuntime::ReadInteger(ResponseObject, TEXT("authorityEpoch"), ResponseAuthorityEpoch)
        || ResponseSequence < 1
        || ResponseAuthorityEpoch != AuthorityEpoch
        || !Envelope->TryGetObjectField(TEXT("integrity"), Integrity) || !Integrity.IsValid()
        || !Envelope->TryGetObjectField(TEXT("payload"), Payload) || !Payload.IsValid()
        || !JumanjiCheckpointRuntime::ReadInteger(Envelope, TEXT("envelopeVersion"), EnvelopeVersion)
        || EnvelopeVersion != 1
        || !JumanjiCheckpointRuntime::ReadString(Integrity, TEXT("algorithm"), Algorithm)
        || Algorithm != TEXT("Ed25519")
        || !JumanjiCheckpointRuntime::ReadString(Integrity, TEXT("keyId"), KeyId)
        || !JumanjiCheckpointRuntime::IsSafeServerValue(KeyId, 96)
        || !JumanjiCheckpointRuntime::ReadString(Integrity, TEXT("signature"), Signature)
        || Signature.Len() < 64
        || Signature.Len() > 256)
    {
        OutError = TEXT("SIGNED_ENVELOPE_SHAPE_INVALID");
        return false;
    }

    FString ServerInstanceId, ShardId, BuildId, RulesetVersion, WorldContentVersion, ParticipantSalt;
    if (!ReadRequiredServerMetadata(
        ServerInstanceId, ShardId, BuildId, RulesetVersion, WorldContentVersion, ParticipantSalt))
    {
        OutError = TEXT("CHECKPOINT_SERVER_METADATA_MISSING");
        return false;
    }

    FJumanjiEncounterCheckpointRestoreCandidate Candidate;
    FString EncounterIdString, SessionIdString, CreatedAtUtc, ExpiresAtUtc;
    int64 SchemaVersion = 0;
    int64 Sequence = 0;
    if (!JumanjiCheckpointRuntime::ReadInteger(Payload, TEXT("schemaVersion"), SchemaVersion)
        || SchemaVersion != 1
        || !JumanjiCheckpointRuntime::ReadString(Payload, TEXT("encounterId"), EncounterIdString)
        || !JumanjiCheckpointRuntime::ReadString(Payload, TEXT("sessionId"), SessionIdString)
        || !JumanjiCheckpointRuntime::ReadString(Payload, TEXT("createdAtUtc"), CreatedAtUtc)
        || !JumanjiCheckpointRuntime::ReadString(Payload, TEXT("expiresAtUtc"), ExpiresAtUtc)
        || !JumanjiCheckpointRuntime::ReadInteger(Payload, TEXT("checkpointSequence"), Sequence)
        || Sequence < 1)
    {
        OutError = TEXT("CHECKPOINT_PAYLOAD_HEADER_INVALID");
        return false;
    }

    Candidate.EncounterId = FName(*EncounterIdString);
    Candidate.CheckpointSequence = Sequence;
    if (ResponseEncounterId != EncounterIdString
        || ResponseSequence != Sequence
        || Candidate.EncounterId != EncounterDirector->GetEncounterId()
        || !FGuid::Parse(SessionIdString, Candidate.SessionId)
        || !Candidate.SessionId.IsValid())
    {
        OutError = TEXT("CHECKPOINT_ENCOUNTER_OR_SESSION_MISMATCH");
        return false;
    }

    FDateTime CreatedAt, ExpiresAt;
    if (!FDateTime::ParseIso8601(*CreatedAtUtc, CreatedAt)
        || !FDateTime::ParseIso8601(*ExpiresAtUtc, ExpiresAt)
        || ExpiresAt <= FDateTime::UtcNow()
        || ExpiresAt <= CreatedAt
        || ExpiresAt - CreatedAt > FTimespan::FromHours(24))
    {
        OutError = TEXT("CHECKPOINT_TTL_INVALID_OR_EXPIRED");
        return false;
    }

    TSharedPtr<FJsonObject> Authority;
    FString PreviousServerId, PreviousShardId, PreviousBuildId, PreviousRuleset, PreviousContent;
    int64 PreviousEpoch = 0;
    if (!Payload->TryGetObjectField(TEXT("authority"), Authority) || !Authority.IsValid()
        || !JumanjiCheckpointRuntime::ReadString(Authority, TEXT("serverInstanceId"), PreviousServerId)
        || !JumanjiCheckpointRuntime::ReadString(Authority, TEXT("shardId"), PreviousShardId)
        || !JumanjiCheckpointRuntime::ReadString(Authority, TEXT("serverBuildId"), PreviousBuildId)
        || !JumanjiCheckpointRuntime::ReadString(Authority, TEXT("rulesetVersion"), PreviousRuleset)
        || !JumanjiCheckpointRuntime::ReadString(Authority, TEXT("worldContentVersion"), PreviousContent)
        || !JumanjiCheckpointRuntime::ReadInteger(Authority, TEXT("authorityEpoch"), PreviousEpoch)
        || PreviousEpoch < 1
        || PreviousEpoch >= AuthorityEpoch
        || !JumanjiCheckpointRuntime::IsSafeServerValue(PreviousServerId, 96)
        || !JumanjiCheckpointRuntime::IsSafeServerValue(PreviousShardId, 96)
        || PreviousShardId != ShardId
        || PreviousBuildId != BuildId
        || PreviousRuleset != RulesetVersion
        || PreviousContent != WorldContentVersion)
    {
        OutError = TEXT("CHECKPOINT_AUTHORITY_OR_CONTENT_INCOMPATIBLE");
        return false;
    }
    Candidate.PreviousAuthorityEpoch = PreviousEpoch;

    TSharedPtr<FJsonObject> SafePoint;
    TSharedPtr<FJsonObject> Chapter;
    TSharedPtr<FJsonObject> Combat;
    TSharedPtr<FJsonObject> Deterministic;
    FString PhaseString, KindString, ChapterIdString, BoardEventIdString, SpawnSetIdString;
    int64 EncounterRevision = 0, ChapterRevision = 0, CurrentWave = 0, TotalWaves = 0, Seed = 0;
    if (!Payload->TryGetObjectField(TEXT("safePoint"), SafePoint) || !SafePoint.IsValid()
        || !JumanjiCheckpointRuntime::ReadString(SafePoint, TEXT("phase"), PhaseString)
        || !JumanjiCheckpointRuntime::ReadString(SafePoint, TEXT("kind"), KindString)
        || !JumanjiCheckpointRuntime::ReadInteger(SafePoint, TEXT("encounterRevision"), EncounterRevision)
        || !SafePoint->TryGetObjectField(TEXT("chapter"), Chapter) || !Chapter.IsValid()
        || !SafePoint->TryGetObjectField(TEXT("combat"), Combat) || !Combat.IsValid()
        || !Payload->TryGetObjectField(TEXT("deterministic"), Deterministic) || !Deterministic.IsValid()
        || !JumanjiCheckpointRuntime::ReadString(Chapter, TEXT("chapterId"), ChapterIdString)
        || !JumanjiCheckpointRuntime::ReadString(Chapter, TEXT("boardEventId"), BoardEventIdString)
        || !JumanjiCheckpointRuntime::ReadInteger(Chapter, TEXT("chapterRevision"), ChapterRevision)
        || !JumanjiCheckpointRuntime::ReadInteger(Combat, TEXT("currentWave"), CurrentWave)
        || !JumanjiCheckpointRuntime::ReadInteger(Combat, TEXT("totalWaves"), TotalWaves)
        || !JumanjiCheckpointRuntime::ReadInteger(Deterministic, TEXT("encounterSeed"), Seed)
        || !JumanjiCheckpointRuntime::ReadString(Deterministic, TEXT("spawnSetId"), SpawnSetIdString))
    {
        OutError = TEXT("CHECKPOINT_SAFE_POINT_SHAPE_INVALID");
        return false;
    }

    AJumanjiAdventureChapterDirector* ChapterDirector = EncounterDirector->GetAdventureChapterDirector();
    if (!ChapterDirector
        || FName(*ChapterIdString) != ChapterDirector->GetChapterId()
        || FName(*BoardEventIdString) != ChapterDirector->GetBoardEventId()
        || FName(*SpawnSetIdString) != EncounterDirector->GetSpawnSetId()
        || Seed < 0 || Seed > MAX_uint32
        || EncounterRevision < 1 || ChapterRevision < 1
        || CurrentWave < 0 || TotalWaves < 1 || TotalWaves > 100
        || TotalWaves != EncounterDirector->GetTotalWaveCount()
        || CurrentWave > TotalWaves)
    {
        OutError = TEXT("CHECKPOINT_CONTENT_IDS_NOT_ALLOWLISTED");
        return false;
    }

    Candidate.Phase = FName(*PhaseString);
    Candidate.SafePointKind = FName(*KindString);
    Candidate.EncounterRevision = static_cast<int32>(EncounterRevision);
    Candidate.ChapterId = FName(*ChapterIdString);
    Candidate.BoardEventId = FName(*BoardEventIdString);
    Candidate.ChapterRevision = static_cast<int32>(ChapterRevision);
    Candidate.CurrentWave = static_cast<int32>(CurrentWave);
    Candidate.TotalWaves = static_cast<int32>(TotalWaves);
    Candidate.EncounterSeed = static_cast<uint32>(Seed);
    Candidate.SpawnSetId = FName(*SpawnSetIdString);

    const bool bValidBoundary =
        (PhaseString == TEXT("Trials") && KindString == TEXT("TrialBoundary")
            && CurrentWave == 0)
        || (PhaseString == TEXT("Combat") && KindString == TEXT("WaveBoundary")
            && CurrentWave >= 1 && CurrentWave <= TotalWaves)
        || (PhaseString == TEXT("Boss") && KindString == TEXT("BossPhaseBoundary")
            && CurrentWave == 0)
        || (PhaseString == TEXT("Reward") && KindString == TEXT("RewardPending")
            && CurrentWave == 0);
    if (!bValidBoundary)
    {
        OutError = TEXT("CHECKPOINT_PHASE_BOUNDARY_MISMATCH");
        return false;
    }

    const TArray<FName> AllowedObjectives = ChapterDirector->GetConfiguredObjectiveIds();
    const TArray<FName> AllowedTrials = ChapterDirector->GetConfiguredTrialIds();
    TSet<FName> AllowedObjectiveSet;
    TSet<FName> AllowedTrialSet;
    for (const FName Id : AllowedObjectives) { AllowedObjectiveSet.Add(Id); }
    for (const FName Id : AllowedTrials) { AllowedTrialSet.Add(Id); }

    const TArray<TSharedPtr<FJsonValue>>* ObjectiveValues = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* TrialValues = nullptr;
    if (!Chapter->TryGetArrayField(TEXT("completedObjectiveIds"), ObjectiveValues)
        || !ObjectiveValues
        || !Chapter->TryGetArrayField(TEXT("completedTrialIds"), TrialValues)
        || !TrialValues)
    {
        OutError = TEXT("CHECKPOINT_COMPLETED_ID_ARRAYS_MISSING");
        return false;
    }

    TSet<FName> SeenObjectiveIds;
    for (const TSharedPtr<FJsonValue>& Value : *ObjectiveValues)
    {
        if (!Value.IsValid() || Value->Type != EJson::String)
        {
            OutError = TEXT("CHECKPOINT_OBJECTIVE_ID_INVALID");
            return false;
        }
        const FName Id(*Value->AsString());
        if (!AllowedObjectiveSet.Contains(Id) || SeenObjectiveIds.Contains(Id))
        {
            OutError = TEXT("CHECKPOINT_OBJECTIVE_ID_NOT_ALLOWLISTED");
            return false;
        }
        SeenObjectiveIds.Add(Id);
        Candidate.CompletedObjectiveIds.Add(Id);
    }

    TSet<FName> SeenTrialIds;
    for (const TSharedPtr<FJsonValue>& Value : *TrialValues)
    {
        if (!Value.IsValid() || Value->Type != EJson::String)
        {
            OutError = TEXT("CHECKPOINT_TRIAL_ID_INVALID");
            return false;
        }
        const FName Id(*Value->AsString());
        if (!AllowedTrialSet.Contains(Id) || SeenTrialIds.Contains(Id))
        {
            OutError = TEXT("CHECKPOINT_TRIAL_ID_NOT_ALLOWLISTED");
            return false;
        }
        SeenTrialIds.Add(Id);
        Candidate.CompletedTrialIds.Add(Id);
    }

    if (!ChapterDirector->AreCheckpointTrialIdsConsistent(
        Candidate.CompletedTrialIds,
        Candidate.CompletedObjectiveIds))
    {
        OutError = TEXT("CHECKPOINT_TRIAL_OBJECTIVE_STATE_MISMATCH");
        return false;
    }

    if (PhaseString != TEXT("Trials")
        && (SeenObjectiveIds.Num() != AllowedObjectives.Num()
            || SeenTrialIds.Num() != AllowedTrials.Num()))
    {
        OutError = TEXT("CHECKPOINT_CHAPTER_NOT_COMPLETE");
        return false;
    }

    if (PhaseString == TEXT("Boss"))
    {
        TSharedPtr<FJsonObject> BossSnapshot;
        FString DefinitionId, PhaseId;
        int64 PhaseRevision = 0;
        double HealthFraction = -1.0;
        if (!Combat->TryGetObjectField(TEXT("boss"), BossSnapshot) || !BossSnapshot.IsValid()
            || !JumanjiCheckpointRuntime::ReadString(BossSnapshot, TEXT("definitionId"), DefinitionId)
            || !JumanjiCheckpointRuntime::ReadString(BossSnapshot, TEXT("phaseId"), PhaseId)
            || !JumanjiCheckpointRuntime::ReadInteger(BossSnapshot, TEXT("phaseRevision"), PhaseRevision)
            || !BossSnapshot->TryGetNumberField(TEXT("healthFraction"), HealthFraction)
            || !FMath::IsFinite(HealthFraction)
            || HealthFraction < 0.0 || HealthFraction > 1.0
            || FName(*DefinitionId) != EncounterDirector->GetBossDefinitionId()
            || (PhaseId != TEXT("BOSS_PHASE_01")
                && PhaseId != TEXT("BOSS_PHASE_02")
                && PhaseId != TEXT("BOSS_PHASE_03"))
            || PhaseRevision < 1)
        {
            OutError = TEXT("CHECKPOINT_BOSS_SNAPSHOT_INVALID");
            return false;
        }

        Candidate.BossDefinitionId = FName(*DefinitionId);
        Candidate.BossPhaseId = FName(*PhaseId);
        Candidate.BossHealthFraction = static_cast<float>(HealthFraction);
        Candidate.BossPhaseRevision = static_cast<int32>(PhaseRevision);
    }

    const TArray<TSharedPtr<FJsonValue>>* ParticipantValues = nullptr;
    if (!Payload->TryGetArrayField(TEXT("participants"), ParticipantValues)
        || !ParticipantValues
        || ParticipantValues->IsEmpty()
        || ParticipantValues->Num() > JumanjiCheckpointRuntime::MaxParticipants)
    {
        OutError = TEXT("CHECKPOINT_PARTICIPANTS_INVALID");
        return false;
    }

    TSet<FString> SeenParticipantKeys;
    for (const TSharedPtr<FJsonValue>& Value : *ParticipantValues)
    {
        const TSharedPtr<FJsonObject> Participant = Value.IsValid() ? Value->AsObject() : nullptr;
        FString ParticipantKey;
        int64 ProfileRevision = 0;
        if (!Participant.IsValid()
            || !JumanjiCheckpointRuntime::ReadString(Participant, TEXT("participantKey"), ParticipantKey)
            || !JumanjiCheckpointRuntime::IsParticipantKeyFormatValid(ParticipantKey)
            || !JumanjiCheckpointRuntime::ReadInteger(Participant, TEXT("profileRevision"), ProfileRevision)
            || ProfileRevision < 0
            || SeenParticipantKeys.Contains(ParticipantKey))
        {
            OutError = TEXT("CHECKPOINT_PARTICIPANT_ENTRY_INVALID");
            return false;
        }

        SeenParticipantKeys.Add(ParticipantKey);
        Candidate.ParticipantKeys.Add(ParticipantKey);
        Candidate.ParticipantProfileRevisions.Add(ProfileRevision);
    }

    if (PhaseString == TEXT("Reward"))
    {
        TSharedPtr<FJsonObject> Reward;
        FString RewardDefinitionId, TransactionId, Status;
        if (!Payload->TryGetObjectField(TEXT("reward"), Reward) || !Reward.IsValid()
            || !JumanjiCheckpointRuntime::ReadString(Reward, TEXT("rewardDefinitionId"), RewardDefinitionId)
            || !JumanjiCheckpointRuntime::ReadString(Reward, TEXT("transactionId"), TransactionId)
            || !JumanjiCheckpointRuntime::ReadString(Reward, TEXT("status"), Status)
            || Status != TEXT("PendingLedgerCheck")
            || FName(*RewardDefinitionId) != EncounterDirector->GetEncounterId()
            || !JumanjiCheckpointRuntime::IsStableId(TransactionId, 128)
            || Candidate.ParticipantKeys.Num() != 1)
        {
            OutError = TEXT("CHECKPOINT_REWARD_PENDING_INVALID");
            return false;
        }

        Candidate.RewardDefinitionId = FName(*RewardDefinitionId);
        Candidate.RewardTransactionId = FName(*TransactionId);
    }

    // This assignment is the only publication point. A failed preflight leaves
    // the previous staged candidate untouched and never mutates GameState.
    StagedRestoreCandidate = MoveTemp(Candidate);
    bHasStagedRestoreCandidate = true;
    return true;
}

