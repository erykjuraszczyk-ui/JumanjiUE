#include "JumanjiAdventureHUDWidget.h"

#include "JumanjiGameState.h"
#include "Engine/World.h"

void UJumanjiAdventureHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    CachedGameState = GetJumanjiGameState();
    if (IsValid(CachedGameState))
    {
        CachedGameState->OnAdventureObjectivesChanged.AddUniqueDynamic(
            this,
            &UJumanjiAdventureHUDWidget::HandleAdventureObjectivesChanged
        );
        CachedGameState->OnAdventureChapterChanged.AddUniqueDynamic(
            this,
            &UJumanjiAdventureHUDWidget::HandleAdventureChapterChanged
        );
    }

    OnAdventurePresentationChanged();
}

void UJumanjiAdventureHUDWidget::NativeDestruct()
{
    if (IsValid(CachedGameState))
    {
        CachedGameState->OnAdventureObjectivesChanged.RemoveDynamic(
            this,
            &UJumanjiAdventureHUDWidget::HandleAdventureObjectivesChanged
        );
        CachedGameState->OnAdventureChapterChanged.RemoveDynamic(
            this,
            &UJumanjiAdventureHUDWidget::HandleAdventureChapterChanged
        );
    }

    CachedGameState = nullptr;
    Super::NativeDestruct();
}

FJumanjiAdventureChapterState UJumanjiAdventureHUDWidget::GetChapterState() const
{
    const AJumanjiGameState* BoardGameState = GetJumanjiGameState();
    return BoardGameState
        ? BoardGameState->GetAdventureChapterState()
        : FJumanjiAdventureChapterState();
}

TArray<FJumanjiAdventureObjectiveState> UJumanjiAdventureHUDWidget::GetAdventureObjectives() const
{
    const AJumanjiGameState* BoardGameState = GetJumanjiGameState();
    return BoardGameState
        ? BoardGameState->GetAdventureObjectives()
        : TArray<FJumanjiAdventureObjectiveState>();
}

int32 UJumanjiAdventureHUDWidget::GetCompletedObjectiveCount() const
{
    int32 CompletedCount = 0;

    for (const FJumanjiAdventureObjectiveState& Objective : GetAdventureObjectives())
    {
        CompletedCount += Objective.bCompleted ? 1 : 0;
    }

    return CompletedCount;
}

int32 UJumanjiAdventureHUDWidget::GetTotalObjectiveCount() const
{
    return GetAdventureObjectives().Num();
}

bool UJumanjiAdventureHUDWidget::GetObjectiveProgress(
    FName ObjectiveId,
    int32& CurrentProgress,
    int32& RequiredProgress) const
{
    CurrentProgress = 0;
    RequiredProgress = 0;

    if (ObjectiveId.IsNone())
    {
        return false;
    }

    for (const FJumanjiAdventureObjectiveState& Objective : GetAdventureObjectives())
    {
        if (Objective.ObjectiveId == ObjectiveId)
        {
            CurrentProgress = Objective.CurrentProgress;
            RequiredProgress = Objective.RequiredProgress;
            return true;
        }
    }

    return false;
}

void UJumanjiAdventureHUDWidget::HandleAdventureObjectivesChanged(int32 Revision)
{
    OnAdventurePresentationChanged();
}

void UJumanjiAdventureHUDWidget::HandleAdventureChapterChanged(
    const FJumanjiAdventureChapterState& ChapterState)
{
    OnAdventurePresentationChanged();
}

AJumanjiGameState* UJumanjiAdventureHUDWidget::GetJumanjiGameState() const
{
    if (IsValid(CachedGameState))
    {
        return CachedGameState;
    }

    const UWorld* World = GetWorld();
    return World ? World->GetGameState<AJumanjiGameState>() : nullptr;
}
