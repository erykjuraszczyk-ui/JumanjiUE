#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "JumanjiAdventureObjective.h"
#include "JumanjiAdventureChapter.h"
#include "JumanjiWorldEncounter.h"
#include "JumanjiGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FJumanjiBoardEventStateChanged,
    FName, EventId,
    bool, bIsActive,
    int32, Revision
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiAdventureObjectivesChanged,
    int32, Revision
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiAdventureChapterChanged,
    const FJumanjiAdventureChapterState&, ChapterState
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiWorldEncounterChanged,
    const FJumanjiWorldEncounterState&, EncounterState
);

/** Replicated public session state. The server owns board events and adventure objectives. */
UCLASS()
class JUMANJIUE_API AJumanjiGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Board")
    void SetBoardEventState(FName EventId, bool bIsActive);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Board")
    bool IsBoardEventActive() const { return bBoardEventActive; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Board")
    FName GetActiveBoardEventId() const { return ActiveBoardEventId; }

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool BeginAdventureChapter(
        FName ChapterId,
        FName BoardEventId,
        FText DisplayName,
        FText Description
    );

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool CompleteAdventureChapter(FName ChapterId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Encounter")
    bool BeginWorldEncounter(FName EncounterId, int32 TotalWaves);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Encounter")
    bool UpdateWorldEncounter(
        FName EncounterId,
        EJumanjiWorldEncounterPhase Phase,
        int32 CurrentWave,
        int32 TotalWaves,
        int32 ActiveEnemyCount);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Encounter")
    bool CompleteWorldEncounter(FName EncounterId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Encounter")
    bool FailWorldEncounter(FName EncounterId);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter")
    FJumanjiWorldEncounterState GetWorldEncounterState() const { return WorldEncounterState; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    FJumanjiAdventureChapterState GetAdventureChapterState() const
    {
        return AdventureChapterState;
    }

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool InitializeAdventureObjectives(const TArray<FJumanjiAdventureObjectiveState>& Objectives);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool AddAdventureObjectiveProgress(FName ObjectiveId, int32 ProgressAmount = 1);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    TArray<FJumanjiAdventureObjectiveState> GetAdventureObjectives() const
    {
        return AdventureObjectiveSet.Objectives;
    }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    bool AreAdventureObjectivesComplete() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    int32 GetAdventureObjectiveRevision() const { return AdventureObjectiveSet.Revision; }

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Board")
    FJumanjiBoardEventStateChanged OnBoardEventStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Adventure")
    FJumanjiAdventureObjectivesChanged OnAdventureObjectivesChanged;

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Adventure")
    FJumanjiAdventureChapterChanged OnAdventureChapterChanged;

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Encounter")
    FJumanjiWorldEncounterChanged OnWorldEncounterChanged;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_BoardEventState, BlueprintReadOnly, Category = "JUMANJI|Board")
    FName ActiveBoardEventId = NAME_None;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "JUMANJI|Board")
    bool bBoardEventActive = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "JUMANJI|Board")
    int32 BoardEventRevision = 0;

    /** Chapter identity, phase and revision are replicated as one snapshot. */
    UPROPERTY(ReplicatedUsing = OnRep_AdventureChapterState, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FJumanjiAdventureChapterState AdventureChapterState;

    /** Objective data and revision are replicated together as one snapshot. */
    UPROPERTY(ReplicatedUsing = OnRep_AdventureObjectiveSet, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FJumanjiAdventureObjectiveSet AdventureObjectiveSet;

    UPROPERTY(ReplicatedUsing = OnRep_WorldEncounterState, BlueprintReadOnly, Category = "JUMANJI|Encounter")
    FJumanjiWorldEncounterState WorldEncounterState;

    UFUNCTION()
    void OnRep_BoardEventState();

    UFUNCTION()
    void OnRep_AdventureObjectiveSet();

    UFUNCTION()
    void OnRep_AdventureChapterState();

    UFUNCTION()
    void OnRep_WorldEncounterState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
