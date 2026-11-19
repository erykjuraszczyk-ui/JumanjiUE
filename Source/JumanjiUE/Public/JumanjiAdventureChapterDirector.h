#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumanjiAdventureObjective.h"
#include "JumanjiTrialActor.h"
#include "JumanjiAdventureChapterDirector.generated.h"

DECLARE_MULTICAST_DELEGATE(FJumanjiAdventureCheckpointBoundaryReady);

/**
 * Server-side chapter orchestrator. Configure one objective for each placed
 * trial actor. The director starts the Living Board event and completes the
 * chapter only after every objective has been validated by its trial.
 */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiAdventureChapterDirector : public AActor
{
    GENERATED_BODY()

public:
    AJumanjiAdventureChapterDirector();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool StartChapter();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    bool IsChapterComplete() const;

    /** Server-only preparation for retrying the matching failed world encounter. */
    bool ResetChapterForEncounterRetry(FName EncounterId);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    FName GetChapterId() const { return ChapterId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    FName GetBoardEventId() const { return BoardEventId; }

    /** Build a value-only trial/chapter summary after every objective and trial is complete. */
    bool BuildCheckpointProgress(
        TArray<FName>& OutCompletedObjectiveIds,
        TArray<FName>& OutCompletedTrialIds,
        int32& OutChapterRevision) const;

    /** Native server event emitted after a trial actor commits its completed state. */
    FJumanjiAdventureCheckpointBoundaryReady OnCheckpointBoundaryReady;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FName ChapterId = TEXT("EMERALD_JUNGLE_NINE_SEALS");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FName BoardEventId = TEXT("EMERALD_JUNGLE_NINE_SEALS");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FText ChapterTitle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure", meta = (MultiLine = "true"))
    FText ChapterDescription;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    TArray<FJumanjiAdventureObjectiveState> ChapterObjectives;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    TArray<TObjectPtr<AJumanjiTrialActor>> TrialActors;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool ValidateChapterConfiguration() const;

    UFUNCTION()
    void HandleAdventureObjectivesChanged(int32 Revision);

    UFUNCTION()
    void HandleTrialStateChanged(EJumanjiTrialState TrialState, int32 CurrentStepIndex, int32 Revision);

    bool bListeningForObjectiveChanges = false;
};
