#include "JumanjiWorldEncounterDirector.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "JumanjiAdventureChapterDirector.h"
#include "JumanjiAdventureChapter.h"
#include "JumanjiBossCharacter.h"
#include "JumanjiEnemyCharacter.h"
#include "GameFramework/PlayerState.h"
#include "JumanjiGameState.h"
#include "JumanjiPlayerState.h"
#include "JumanjiProgressionRewardSubsystem.h"
#include "JumanjiWorldEncounterSessionSubsystem.h"
#include "JumanjiEncounterCheckpointRuntimeSubsystem.h"

AJumanjiWorldEncounterDirector::AJumanjiWorldEncounterDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    // The client sends this actor reference through a PlayerController server RPC.
    // Keep the persistent encounter director addressable and relevant to clients.
    bReplicates = true;
    bAlwaysRelevant = true;
    NetUpdateFrequency = 10.0f;

    RequiredTraversalIds = {
        FName(TEXT("EJ_FROST_GATE_LIFT_01")),
        FName(TEXT("EJ_FROST_GATE_JUMP_01"))
    };
}

bool AJumanjiWorldEncounterDirector::ValidateConfiguration() const
{
    if (EncounterId.IsNone()
        || RequiredChapterId.IsNone()
        || !IsValid(AdventureChapterDirector)
        || AdventureChapterDirector->GetWorld() != GetWorld()
        || AdventureChapterDirector->GetChapterId() != RequiredChapterId
        || Waves.IsEmpty()
        || Waves.Num() > 16
        || !BossClass
        || !IsValid(BossSpawnPoint)
        || BossSpawnPoint->GetWorld() != GetWorld())
    {
        return false;
    }

    TSet<FName> SeenWaveIds;
    int32 TotalEnemySpawns = 0;

    for (const FJumanjiEmeraldJungleEncounterWave& Wave : Waves)
    {
        if (Wave.WaveId.IsNone()
            || SeenWaveIds.Contains(Wave.WaveId)
            || Wave.Spawns.IsEmpty())
        {
            return false;
        }

        SeenWaveIds.Add(Wave.WaveId);

        for (const FJumanjiEncounterEnemySpawn& Spawn : Wave.Spawns)
        {
            if (!Spawn.EnemyClass
                || !Spawn.EnemyClass->IsChildOf(AJumanjiEnemyCharacter::StaticClass())
                || !IsValid(Spawn.SpawnPoint)
                || Spawn.SpawnPoint->GetWorld() != GetWorld()
                || Spawn.Count < 1
                || Spawn.Count > 8
                || !FMath::IsFinite(Spawn.SpreadRadius)
                || Spawn.SpreadRadius < 0.0f
                || Spawn.SpreadRadius > 500.0f)
            {
                return false;
            }

            TotalEnemySpawns += Spawn.Count;
            if (TotalEnemySpawns > 64)
            {
                return false;
            }
        }
    }

    return true;
}

bool AJumanjiWorldEncounterDirector::ValidatePlayerPrerequisites(
    const AJumanjiPlayerState* PlayerState) const
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || PlayerState->GetPersistentProfileId().IsEmpty())
    {
        return false;
    }

    if (!RequiredRegionId.IsNone()
        && !PlayerState->HasDiscoveredWorldRegion(RequiredRegionId))
    {
        return false;
    }

    if (!RequiredLandmarkId.IsNone()
        && !PlayerState->HasVisitedWorldLandmark(RequiredLandmarkId))
    {
        return false;
    }

    const UJumanjiProgressionRewardSubsystem* RewardSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UJumanjiProgressionRewardSubsystem>()
            : nullptr;

    if (!RewardSubsystem)
    {
        return false;
    }

    for (const FName TraversalId : RequiredTraversalIds)
    {
        if (TraversalId.IsNone()
            || !RewardSubsystem->HasCompletedTraversalReward(PlayerState, TraversalId))
        {
            return false;
        }
    }

    return true;
}

bool AJumanjiWorldEncounterDirector::CanRetryEncounter() const
{
    const UWorld* World = GetWorld();
    const AJumanjiGameState* GameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;

    if (!GameState)
    {
        return false;
    }

    const FJumanjiWorldEncounterState State = GameState->GetWorldEncounterState();
    return State.EncounterId == EncounterId
        && State.Phase == EJumanjiWorldEncounterPhase::Failed;
}

bool AJumanjiWorldEncounterDirector::StartEncounter(AJumanjiPlayerState* InitiatingPlayer)
{
    if (!HasAuthority()
        || bEncounterActive
        || !ValidateConfiguration()
        || !ValidatePlayerPrerequisites(InitiatingPlayer))
    {
        return false;
    }

    UWorld* World = GetWorld();
    AJumanjiGameState* GameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;

    if (!GameState)
    {
        return false;
    }

    const FJumanjiWorldEncounterState PreviousEncounterState =
        GameState->GetWorldEncounterState();
    const bool bIsRetry = PreviousEncounterState.Phase == EJumanjiWorldEncounterPhase::Failed;

    // An active chapter is allowed only when it belongs to the failed encounter
    // being reset below. Normal duplicate starts remain rejected.

    if (!bIsRetry
        && GameState->GetAdventureChapterState().Phase == EJumanjiAdventureChapterPhase::Active)
    {
        return false;
    }

    if (bIsRetry)
    {
        if (PreviousEncounterState.EncounterId != EncounterId
            || !AdventureChapterDirector->ResetChapterForEncounterRetry(EncounterId))
        {
            return false;
        }

        // This destroys leftovers even when the failure policy previously chose
        // to unbind rather than immediately destroy the spawned actors.
        DestroyActiveEnemies();
    }
    else if (PreviousEncounterState.Phase != EJumanjiWorldEncounterPhase::Inactive)
    {
        // Completed encounters remain terminal and cannot be replayed as rewards.
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UJumanjiWorldEncounterSessionSubsystem* SessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiWorldEncounterSessionSubsystem>()
        : nullptr;

    if (!SessionSubsystem
        || !SessionSubsystem->BeginEncounterSession(EncounterId, InitiatingPlayer))
    {
        return false;
    }

    // The world encounter is shared by the players currently connected to this
    // server. Register only server-known PlayerStates with a bound profile key.
    for (APlayerState* BasePlayerState : GameState->PlayerArray)
    {
        if (AJumanjiPlayerState* Participant = Cast<AJumanjiPlayerState>(BasePlayerState))
        {
            if (Participant != InitiatingPlayer)
            {
                SessionSubsystem->RegisterConnectedParticipant(Participant);
            }
        }
    }

    if (!GameState->BeginWorldEncounter(EncounterId, Waves.Num()))
    {
        SessionSubsystem->EndEncounterSession(EncounterId);
        return false;
    }

    bEncounterActive = true;
    ActiveWaveIndex = INDEX_NONE;

    if (UJumanjiEncounterCheckpointRuntimeSubsystem* CheckpointSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiEncounterCheckpointRuntimeSubsystem>()
        : nullptr)
    {
        // Checkpointing is optional for local development. A missing backend or
        // server secret never blocks the gameplay encounter from starting.
        if (!CheckpointSubsystem->BeginEncounterCheckpointSession(EncounterId))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("JUMANJI checkpoint: lease was not started for encounter '%s'; this session has no durable checkpoints."),
                *EncounterId.ToString());
        }
    }

    GameState->OnAdventureChapterChanged.AddUniqueDynamic(
        this,
        &AJumanjiWorldEncounterDirector::HandleAdventureChapterChanged);
    bListeningForChapterChanges = true;

    GameState->OnAdventureObjectivesChanged.AddUniqueDynamic(
        this,
        &AJumanjiWorldEncounterDirector::HandleAdventureObjectivesChanged);
    bListeningForObjectiveChanges = true;

    PublishEncounterState(EJumanjiWorldEncounterPhase::Trials, 0, 0);

    if (!AdventureChapterDirector->StartChapter())
    {
        FailEncounter(FName(TEXT("CHAPTER_START_REJECTED")));
        return false;
    }

    return true;
}

void AJumanjiWorldEncounterDirector::HandleAdventureChapterChanged(
    const FJumanjiAdventureChapterState& ChapterState)
{
    if (!HasAuthority()
        || !bEncounterActive
        || ChapterState.ChapterId != RequiredChapterId
        || ChapterState.Phase != EJumanjiAdventureChapterPhase::Completed
        || ActiveWaveIndex != INDEX_NONE)
    {
        return;
    }

    SpawnWave(0);
}

void AJumanjiWorldEncounterDirector::HandleAdventureObjectivesChanged(int32 ObjectiveRevision)
{
    if (!HasAuthority()
        || !bEncounterActive
        || ActiveWaveIndex != INDEX_NONE)
    {
        return;
    }

    const UWorld* World = GetWorld();
    const AJumanjiGameState* GameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    const FJumanjiAdventureChapterState ChapterState = GameState
        ? GameState->GetAdventureChapterState()
        : FJumanjiAdventureChapterState();

    if (!GameState
        || ChapterState.ChapterId != RequiredChapterId
        || ChapterState.Phase != EJumanjiAdventureChapterPhase::Active
        || !IsValid(AdventureChapterDirector)
        || !AdventureChapterDirector->HasNoTrialsInProgress())
    {
        return;
    }

    if (UJumanjiEncounterCheckpointRuntimeSubsystem* CheckpointSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointRuntimeSubsystem>()
        : nullptr)
    {
        CheckpointSubsystem->CaptureSafePoint(
            this,
            EJumanjiEncounterCheckpointSafePoint::TrialBoundary);
    }
}

void AJumanjiWorldEncounterDirector::SpawnWave(int32 WaveIndex)
{
    if (!HasAuthority() || !bEncounterActive)
    {
        return;
    }

    if (!Waves.IsValidIndex(WaveIndex))
    {
        SpawnBoss();
        return;
    }

    ActiveWaveIndex = WaveIndex;
    ActiveEnemies.Reset();

    PublishEncounterState(
        EJumanjiWorldEncounterPhase::Combat,
        WaveIndex + 1,
        0);

    const FJumanjiEmeraldJungleEncounterWave& Wave = Waves[WaveIndex];

    for (const FJumanjiEncounterEnemySpawn& Spawn : Wave.Spawns)
    {
        if (!Spawn.EnemyClass || !IsValid(Spawn.SpawnPoint))
        {
            FailEncounter(FName(TEXT("INVALID_ENEMY_SPAWN")));
            return;
        }

        for (int32 SpawnIndex = 0; SpawnIndex < Spawn.Count; ++SpawnIndex)
        {
            FVector Offset = FVector::ZeroVector;
            if (Spawn.Count > 1 && Spawn.SpreadRadius > 0.0f)
            {
                const float Angle = (2.0f * PI * SpawnIndex) / Spawn.Count;
                Offset = FVector(
                    FMath::Cos(Angle) * Spawn.SpreadRadius,
                    FMath::Sin(Angle) * Spawn.SpreadRadius,
                    0.0f);
            }

            FTransform SpawnTransform = Spawn.SpawnPoint->GetActorTransform();
            SpawnTransform.AddToTranslation(Offset);

            FActorSpawnParameters SpawnParameters;
            SpawnParameters.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
            SpawnParameters.Owner = this;

            AJumanjiEnemyCharacter* Enemy = GetWorld()->SpawnActor<AJumanjiEnemyCharacter>(
                Spawn.EnemyClass,
                SpawnTransform,
                SpawnParameters);

            if (!IsValid(Enemy))
            {
                FailEncounter(FName(TEXT("ENEMY_SPAWN_FAILED")));
                return;
            }

            Enemy->OnEnemyDefeated.AddUniqueDynamic(
                this,
                &AJumanjiWorldEncounterDirector::HandleEnemyDefeated);
            ActiveEnemies.Add(Enemy);
            SpawnedEncounterActors.Add(Enemy);
        }
    }

    PublishEncounterState(
        EJumanjiWorldEncounterPhase::Combat,
        WaveIndex + 1,
        ActiveEnemies.Num());

    if (ActiveEnemies.IsEmpty())
    {
        FailEncounter(FName(TEXT("EMPTY_ENEMY_WAVE")));
    }
}

void AJumanjiWorldEncounterDirector::SpawnBoss()
{
    if (!HasAuthority() || !bEncounterActive)
    {
        return;
    }

    if (!BossClass || !IsValid(BossSpawnPoint))
    {
        FailEncounter(FName(TEXT("BOSS_CONFIGURATION_INVALID")));
        return;
    }

    PublishEncounterState(
        EJumanjiWorldEncounterPhase::Boss,
        0,
        1);

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    SpawnParameters.Owner = this;

    ActiveBoss = GetWorld()->SpawnActor<AJumanjiBossCharacter>(
        BossClass,
        BossSpawnPoint->GetActorTransform(),
        SpawnParameters);

    if (!IsValid(ActiveBoss))
    {
        FailEncounter(FName(TEXT("BOSS_SPAWN_FAILED")));
        return;
    }

    ActiveBoss->OnEnemyDefeated.AddUniqueDynamic(
        this,
        &AJumanjiWorldEncounterDirector::HandleEnemyDefeated);
    SpawnedEncounterActors.Add(ActiveBoss);

    AJumanjiGameState* GameState = GetWorld()
        ? GetWorld()->GetGameState<AJumanjiGameState>()
        : nullptr;
    if (!GameState || !GameState->SetWorldEncounterBoss(EncounterId, ActiveBoss))
    {
        FailEncounter(FName(TEXT("BOSS_REFERENCE_REJECTED")));
        return;
    }

    ActiveBoss->OnBossPhaseChanged.AddUniqueDynamic(
        this,
        &AJumanjiWorldEncounterDirector::HandleBossPhaseChanged);
}

void AJumanjiWorldEncounterDirector::HandleBossPhaseChanged(uint8 NewPhase, int32 PhaseRevision)
{
    if (!HasAuthority()
        || !bEncounterActive
        || !IsValid(ActiveBoss)
        || NewPhase < 2
        || NewPhase > 3
        || PhaseRevision < 1
        || ActiveBoss->GetBossTelegraphState().Stage != EJumanjiBossTelegraphStage::None)
    {
        return;
    }

    if (UJumanjiEncounterCheckpointRuntimeSubsystem* CheckpointSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointRuntimeSubsystem>()
        : nullptr)
    {
        CheckpointSubsystem->CaptureSafePoint(
            this,
            EJumanjiEncounterCheckpointSafePoint::BossPhaseBoundary);
    }
}

void AJumanjiWorldEncounterDirector::HandleEnemyDefeated(AJumanjiEnemyCharacter* Enemy)
{
    if (!HasAuthority() || !bEncounterActive || !IsValid(Enemy))
    {
        return;
    }

    Enemy->OnEnemyDefeated.RemoveDynamic(
        this,
        &AJumanjiWorldEncounterDirector::HandleEnemyDefeated);

    if (Enemy == ActiveBoss)
    {
        ActiveBoss = nullptr;
        CompleteEncounter();
        return;
    }

    ActiveEnemies.Remove(Enemy);
    PublishEncounterState(
        EJumanjiWorldEncounterPhase::Combat,
        ActiveWaveIndex + 1,
        ActiveEnemies.Num());

    if (ActiveEnemies.IsEmpty())
    {
        if (UJumanjiEncounterCheckpointRuntimeSubsystem* CheckpointSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointRuntimeSubsystem>()
            : nullptr)
        {
            CheckpointSubsystem->CaptureSafePoint(
                this,
                EJumanjiEncounterCheckpointSafePoint::WaveBoundary);
        }

        SpawnWave(ActiveWaveIndex + 1);
    }
}

void AJumanjiWorldEncounterDirector::PublishEncounterState(
    EJumanjiWorldEncounterPhase Phase,
    int32 WaveIndex,
    int32 ActiveEnemyCount)
{
    if (!HasAuthority())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (AJumanjiGameState* GameState = World->GetGameState<AJumanjiGameState>())
        {
            if (Phase == EJumanjiWorldEncounterPhase::Failed)
            {
                GameState->FailWorldEncounter(EncounterId);
            }
            else
            {
                GameState->UpdateWorldEncounter(
                    EncounterId,
                    Phase,
                    FMath::Max(0, WaveIndex),
                    Waves.Num(),
                    FMath::Max(0, ActiveEnemyCount));
            }
        }
    }

    BP_OnEncounterPhaseChanged(Phase);
}

void AJumanjiWorldEncounterDirector::CompleteEncounter()
{
    if (!HasAuthority() || !bEncounterActive)
    {
        return;
    }

    PublishEncounterState(EJumanjiWorldEncounterPhase::Reward, 0, 0);

    UWorld* World = GetWorld();
    UJumanjiProgressionRewardSubsystem* RewardSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiProgressionRewardSubsystem>()
        : nullptr;
    AJumanjiGameState* GameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;

    if (!RewardSubsystem || !GameState)
    {
        FailEncounter(FName(TEXT("REWARD_SYSTEM_UNAVAILABLE")));
        return;
    }

    TArray<AJumanjiPlayerState*> RewardParticipants;
    for (APlayerState* BasePlayerState : GameState->PlayerArray)
    {
        AJumanjiPlayerState* Candidate = Cast<AJumanjiPlayerState>(BasePlayerState);
        if (Candidate && !Candidate->GetPersistentProfileId().IsEmpty())
        {
            RewardParticipants.Add(Candidate);
        }
    }

    if (RewardParticipants.Num() == 1)
    {
        const FName RewardReceiptId = RewardSubsystem->GetWorldEncounterRewardTransactionId(
            RewardParticipants[0], EncounterId);

        if (UJumanjiEncounterCheckpointRuntimeSubsystem* CheckpointSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointRuntimeSubsystem>()
            : nullptr)
        {
            CheckpointSubsystem->CaptureSafePoint(
                this,
                EJumanjiEncounterCheckpointSafePoint::RewardPending,
                RewardReceiptId);
        }
    }
    else
    {
        // Schema v1 has one reward transactionId. Do not write a partial
        // multi-participant RewardPending checkpoint.
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI checkpoint: RewardPending snapshot skipped; schema v1 requires a single participant."));
    }

    for (APlayerState* BasePlayerState : GameState->PlayerArray)
    {
        AJumanjiPlayerState* PlayerState = Cast<AJumanjiPlayerState>(BasePlayerState);
        if (!PlayerState || PlayerState->GetPersistentProfileId().IsEmpty())
        {
            continue;
        }

        if (!RewardSubsystem->GrantWorldEncounterCompletionReward(PlayerState, EncounterId))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("JUMANJI encounter: reward application failed for profile '%s' and encounter '%s'."),
                *PlayerState->GetPersistentProfileId(),
                *EncounterId.ToString());
        }
    }

    if (!GameState->CompleteWorldEncounter(EncounterId))
    {
        FailEncounter(FName(TEXT("ENCOUNTER_COMPLETION_REJECTED")));
        return;
    }

    if (UJumanjiEncounterCheckpointRuntimeSubsystem* CheckpointSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointRuntimeSubsystem>()
        : nullptr)
    {
        CheckpointSubsystem->EndEncounterCheckpointSession(EncounterId);
    }

    if (UJumanjiWorldEncounterSessionSubsystem* SessionSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UJumanjiWorldEncounterSessionSubsystem>()
            : nullptr)
    {
        SessionSubsystem->EndEncounterSession(EncounterId);
    }

    bEncounterActive = false;
    if (bListeningForChapterChanges)
    {
        GameState->OnAdventureChapterChanged.RemoveDynamic(
            this,
            &AJumanjiWorldEncounterDirector::HandleAdventureChapterChanged);
        bListeningForChapterChanges = false;
    }
    if (bListeningForObjectiveChanges)
    {
        GameState->OnAdventureObjectivesChanged.RemoveDynamic(
            this,
            &AJumanjiWorldEncounterDirector::HandleAdventureObjectivesChanged);
        bListeningForObjectiveChanges = false;
    }

    UnbindActiveEnemies();
    SpawnedEncounterActors.Reset();
    BP_OnEncounterPhaseChanged(EJumanjiWorldEncounterPhase::Completed);
    BP_OnEncounterCompleted();
}

void AJumanjiWorldEncounterDirector::FailEncounter(FName ReasonId)
{
    if (!HasAuthority() || !bEncounterActive)
    {
        return;
    }

    bEncounterActive = false;

    if (bDestroyEncounterEnemiesOnFailure)
    {
        DestroyActiveEnemies();
    }
    else
    {
        UnbindActiveEnemies();
    }

    if (UWorld* World = GetWorld())
    {
        if (AJumanjiGameState* GameState = World->GetGameState<AJumanjiGameState>())
        {
            GameState->SetWorldEncounterBoss(EncounterId, nullptr);
        }
    }

    PublishEncounterState(EJumanjiWorldEncounterPhase::Failed, 0, 0);

    if (UJumanjiEncounterCheckpointRuntimeSubsystem* CheckpointSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointRuntimeSubsystem>()
        : nullptr)
    {
        CheckpointSubsystem->EndEncounterCheckpointSession(EncounterId);
    }

    if (UJumanjiWorldEncounterSessionSubsystem* SessionSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UJumanjiWorldEncounterSessionSubsystem>()
            : nullptr)
    {
        SessionSubsystem->EndEncounterSession(EncounterId);
    }

    if (bListeningForChapterChanges)
    {
        if (UWorld* World = GetWorld())
        {
            if (AJumanjiGameState* GameState = World->GetGameState<AJumanjiGameState>())
            {
                GameState->OnAdventureChapterChanged.RemoveDynamic(
                    this,
                    &AJumanjiWorldEncounterDirector::HandleAdventureChapterChanged);
            }
        }
        bListeningForChapterChanges = false;
    }

    if (bListeningForObjectiveChanges)
    {
        if (UWorld* World = GetWorld())
        {
            if (AJumanjiGameState* GameState = World->GetGameState<AJumanjiGameState>())
            {
                GameState->OnAdventureObjectivesChanged.RemoveDynamic(
                    this,
                    &AJumanjiWorldEncounterDirector::HandleAdventureObjectivesChanged);
            }
        }
        bListeningForObjectiveChanges = false;
    }

    BP_OnEncounterFailed(ReasonId);
}

void AJumanjiWorldEncounterDirector::UnbindActiveEnemies()
{
    for (AJumanjiEnemyCharacter* Enemy : ActiveEnemies)
    {
        if (IsValid(Enemy))
        {
            Enemy->OnEnemyDefeated.RemoveDynamic(
                this,
                &AJumanjiWorldEncounterDirector::HandleEnemyDefeated);
        }
    }
    ActiveEnemies.Reset();

    if (IsValid(ActiveBoss))
    {
        ActiveBoss->OnEnemyDefeated.RemoveDynamic(
            this,
            &AJumanjiWorldEncounterDirector::HandleEnemyDefeated);
        ActiveBoss = nullptr;
    }
}

void AJumanjiWorldEncounterDirector::DestroyActiveEnemies()
{
    // Track all encounter-owned actors, not just living enemies: defeated
    // actors can remain in their short cosmetic lifespan when a failure occurs.
    for (AActor* SpawnedActor : SpawnedEncounterActors)
    {
        if (!IsValid(SpawnedActor))
        {
            continue;
        }

        if (AJumanjiEnemyCharacter* Enemy = Cast<AJumanjiEnemyCharacter>(SpawnedActor))
        {
            Enemy->OnEnemyDefeated.RemoveDynamic(
                this,
                &AJumanjiWorldEncounterDirector::HandleEnemyDefeated);
        }

        SpawnedActor->Destroy();
    }

    SpawnedEncounterActors.Reset();
    ActiveEnemies.Reset();
    ActiveBoss = nullptr;
}

void AJumanjiWorldEncounterDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UJumanjiWorldEncounterSessionSubsystem* SessionSubsystem =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UJumanjiWorldEncounterSessionSubsystem>()
            : nullptr)
    {
        SessionSubsystem->EndEncounterSession(EncounterId);
    }

    if (bListeningForChapterChanges)
    {
        if (UWorld* World = GetWorld())
        {
            if (AJumanjiGameState* GameState = World->GetGameState<AJumanjiGameState>())
            {
                GameState->OnAdventureChapterChanged.RemoveDynamic(
                    this,
                    &AJumanjiWorldEncounterDirector::HandleAdventureChapterChanged);
            }
        }
        bListeningForChapterChanges = false;
    }

    if (bListeningForObjectiveChanges)
    {
        if (UWorld* World = GetWorld())
        {
            if (AJumanjiGameState* GameState = World->GetGameState<AJumanjiGameState>())
            {
                GameState->OnAdventureObjectivesChanged.RemoveDynamic(
                    this,
                    &AJumanjiWorldEncounterDirector::HandleAdventureObjectivesChanged);
            }
        }
        bListeningForObjectiveChanges = false;
    }

    if (UJumanjiEncounterCheckpointRuntimeSubsystem* CheckpointSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiEncounterCheckpointRuntimeSubsystem>()
        : nullptr)
    {
        CheckpointSubsystem->EndEncounterCheckpointSession(EncounterId);
    }

    UnbindActiveEnemies();
    Super::EndPlay(EndPlayReason);
}
