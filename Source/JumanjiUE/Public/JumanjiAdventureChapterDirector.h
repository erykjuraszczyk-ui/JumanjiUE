#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumanjiAdventureObjective.h"
#include "JumanjiAdventureChapterDirector.generated.h"

class AJumanjiTrialActor;

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

    bool bListeningForObjectiveChanges = false;
};
