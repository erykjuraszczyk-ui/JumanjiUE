#include "JumanjiWorldEncounterDirector.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "JumanjiAdventureChapterDirector.h"
#include "JumanjiAdventureChapter.h"
#include "JumanjiBossCharacter.h"
#include "JumanjiEnemyCharacter.h"
#include "JumanjiGameState.h"
#include "JumanjiPlayerState.h"
#include "JumanjiProgressionRewardSubsystem.h"

AJumanjiWorldEncounterDirector::AJumanjiWorldEncounterDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

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

    if (!GameState
        || GameState->GetAdventureChapterState().Phase == EJumanjiAdventureChapterPhase::Active
        || !GameState->BeginWorldEncounter(EncounterId, Waves.Num()))
    {
        return false;
    }

    bEncounterActive = true;
    ActiveWaveIndex = INDEX_NONE;

    GameState->OnAdventureChapterChanged.AddUniqueDynamic(
        this,
        &AJumanjiWorldEncounterDirector::HandleAdventureChapterChanged);
    bListeningForChapterChanges = true;

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
            GameState->UpdateWorldEncounter(
                EncounterId,
                Phase,
                FMath::Max(0, WaveIndex),
                Waves.Num(),
                FMath::Max(0, ActiveEnemyCount));
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

    bEncounterActive = false;
    if (bListeningForChapterChanges)
    {
        GameState->OnAdventureChapterChanged.RemoveDynamic(
            this,
            &AJumanjiWorldEncounterDirector::HandleAdventureChapterChanged);
        bListeningForChapterChanges = false;
    }

    UnbindActiveEnemies();
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

    PublishEncounterState(EJumanjiWorldEncounterPhase::Failed, 0, ActiveEnemies.Num());

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

    if (bDestroyEncounterEnemiesOnFailure)
    {
        DestroyActiveEnemies();
    }
    else
    {
        UnbindActiveEnemies();
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
    for (AJumanjiEnemyCharacter* Enemy : ActiveEnemies)
    {
        if (IsValid(Enemy))
        {
            Enemy->OnEnemyDefeated.RemoveDynamic(
                this,
                &AJumanjiWorldEncounterDirector::HandleEnemyDefeated);
            Enemy->Destroy();
        }
    }
    ActiveEnemies.Reset();

    if (IsValid(ActiveBoss))
    {
        ActiveBoss->OnEnemyDefeated.RemoveDynamic(
            this,
            &AJumanjiWorldEncounterDirector::HandleEnemyDefeated);
        ActiveBoss->Destroy();
        ActiveBoss = nullptr;
    }
}

void AJumanjiWorldEncounterDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

    UnbindActiveEnemies();
    Super::EndPlay(EndPlayReason);
}
