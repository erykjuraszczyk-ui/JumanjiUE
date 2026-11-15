#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JumanjiAdventureChapter.h"
#include "JumanjiAdventureHUDWidget.generated.h"

class AJumanjiGameState;

/**
 * Blueprint-facing HUD base. It subscribes to replicated GameState changes
 * and asks the Blueprint widget to rebuild its presentation.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiAdventureHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    FJumanjiAdventureChapterState GetChapterState() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    TArray<FJumanjiAdventureObjectiveState> GetAdventureObjectives() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    int32 GetCompletedObjectiveCount() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    int32 GetTotalObjectiveCount() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    bool GetObjectiveProgress(FName ObjectiveId, int32& CurrentProgress, int32& RequiredProgress) const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Implement in WBP_AdventureHUD to refresh the chapter banner and objective list. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Adventure")
    void OnAdventurePresentationChanged();

private:
    UFUNCTION()
    void HandleAdventureObjectivesChanged(int32 Revision);

    UFUNCTION()
    void HandleAdventureChapterChanged(const FJumanjiAdventureChapterState& ChapterState);

    AJumanjiGameState* GetJumanjiGameState() const;

    UPROPERTY(Transient)
    TObjectPtr<AJumanjiGameState> CachedGameState;
};
