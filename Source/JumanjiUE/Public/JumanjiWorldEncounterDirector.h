#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumanjiAdventureChapter.h"
#include "JumanjiWorldEncounter.h"
#include "JumanjiWorldEncounterDirector.generated.h"

class AActor;
class AJumanjiAdventureChapterDirector;
class AJumanjiBossCharacter;
class AJumanjiEnemyCharacter;
class AJumanjiPlayerState;
class AJumanjiBossCharacter;

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiEncounterEnemySpawn
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    TSubclassOf<AJumanjiEnemyCharacter> EnemyClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    TObjectPtr<AActor> SpawnPoint = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Encounter", meta = (ClampMin = "1", ClampMax = "8"))
    int32 Count = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Encounter", meta = (ClampMin = "0.0", ClampMax = "500.0"))
    float SpreadRadius = 120.0f;
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiEmeraldJungleEncounterWave
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    FName WaveId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    TArray<FJumanjiEncounterEnemySpawn> Spawns;
};

/**
 * Server-owned vertical-slice orchestrator:
 * discovered traversal gate -> nine-trial chapter -> enemy waves -> boss -> profile reward.
 *
 * Place this actor in the persistent/always-loaded level. Wave enemies and the
 * boss are spawned by the server from authored classes and world-space anchors.
 */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiWorldEncounterDirector : public AActor
{
    GENERATED_BODY()

public:
    AJumanjiWorldEncounterDirector();

    /** Server-only entry point. The PlayerController validates proximity before calling it. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "JUMANJI|Encounter")
    bool StartEncounter(AJumanjiPlayerState* InitiatingPlayer);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter")
    FName GetEncounterId() const { return EncounterId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter")
    bool IsEncounterActive() const { return bEncounterActive; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter")
    bool CanRetryEncounter() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter|Checkpoint")
    FName GetCheckpointSpawnSetId() const { return CheckpointSpawnSetId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter|Checkpoint")
    FName GetCheckpointBossDefinitionId() const { return CheckpointBossDefinitionId; }

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    FName EncounterId = FName(TEXT("ENCOUNTER_EMERALD_JUNGLE_CITADEL"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    FName RequiredChapterId = FName(TEXT("EMERALD_JUNGLE_NINE_SEALS"));

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    TObjectPtr<AJumanjiAdventureChapterDirector> AdventureChapterDirector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter|Exploration")
    FName RequiredRegionId = FName(TEXT("REGION_FROST_GATE"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter|Exploration")
    FName RequiredLandmarkId = FName(TEXT("LANDMARK_FROST_GATE"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter|Exploration")
    TArray<FName> RequiredTraversalIds;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter|Combat")
    TArray<FJumanjiEmeraldJungleEncounterWave> Waves;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter|Boss")
    TSubclassOf<AJumanjiBossCharacter> BossClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter|Checkpoint")
    FName CheckpointSpawnSetId = FName(TEXT("EMERALD_CITADEL_WAVESET_A"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter|Checkpoint")
    FName CheckpointBossDefinitionId = FName(TEXT("BOSS_EMERALD_JUNGLE_GUARDIAN"));

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter|Boss")
    TObjectPtr<AActor> BossSpawnPoint;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    bool bDestroyEncounterEnemiesOnFailure = true;

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Encounter")
    void BP_OnEncounterPhaseChanged(EJumanjiWorldEncounterPhase NewPhase);

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Encounter")
    void BP_OnEncounterCompleted();

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Encounter")
    void BP_OnEncounterFailed(FName ReasonId);

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool ValidateConfiguration() const;
    bool ValidatePlayerPrerequisites(const AJumanjiPlayerState* PlayerState) const;
    UFUNCTION()
    void HandleAdventureChapterChanged(const FJumanjiAdventureChapterState& ChapterState);
    void SpawnWave(int32 WaveIndex);
    void SpawnBoss();
    UFUNCTION()
    void HandleEnemyDefeated(AJumanjiEnemyCharacter* Enemy);

    void HandleBossCheckpointBoundaryReady(uint8 Phase, float HealthFraction, uint32 PhaseRevision);
    void RequestCheckpointCapture(EJumanjiWorldEncounterPhase Phase, int32 CurrentWave);
    void ReleaseCheckpointLease();
    void PublishEncounterState(EJumanjiWorldEncounterPhase Phase, int32 WaveIndex, int32 ActiveEnemyCount);
    void CompleteEncounter();
    void FailEncounter(FName ReasonId);
    void UnbindActiveEnemies();
    void DestroyActiveEnemies();

    UPROPERTY(Transient)
    TArray<TObjectPtr<AJumanjiEnemyCharacter>> ActiveEnemies;

    UPROPERTY(Transient)
    TObjectPtr<AJumanjiBossCharacter> ActiveBoss;

    /** Every actor spawned by this encounter, including defeated actors in their cleanup lifespan. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedEncounterActors;

    int32 ActiveWaveIndex = INDEX_NONE;
    FGuid EncounterSessionId;
    uint32 EncounterSeed = 0;
    FDelegateHandle BossCheckpointDelegateHandle;
    bool bEncounterActive = false;
    bool bListeningForChapterChanges = false;
};
