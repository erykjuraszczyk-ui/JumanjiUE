#include "JumanjiTrialActor.h"

#include "JumanjiAdventureObjective.h"
#include "JumanjiBoardWorldSubsystem.h"
#include "JumanjiGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace
{
    constexpr int32 MaxTrialSteps = 12;
    constexpr int32 MinOptionsPerStep = 2;
    constexpr int32 MaxOptionsPerStep = 8;
}

AJumanjiTrialActor::AJumanjiTrialActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);
}

bool AJumanjiTrialActor::ValidateStepDefinitions() const
{
    if (ObjectiveId.IsNone()
        || Steps.IsEmpty()
        || Steps.Num() > MaxTrialSteps)
    {
        return false;
    }

    TSet<FName> SeenStepIds;

    for (const FJumanjiTrialStepDefinition& Step : Steps)
    {
        if (Step.StepId.IsNone()
            || Step.Prompt.IsEmpty()
            || Step.OptionIds.Num() < MinOptionsPerStep
            || Step.OptionIds.Num() > MaxOptionsPerStep
            || Step.CorrectOptionId.IsNone()
            || !Step.OptionIds.Contains(Step.CorrectOptionId)
            || SeenStepIds.Contains(Step.StepId))
        {
            return false;
        }

        TSet<FName> SeenOptions;
        for (const FName OptionId : Step.OptionIds)
        {
            if (OptionId.IsNone() || SeenOptions.Contains(OptionId))
            {
                return false;
            }

            SeenOptions.Add(OptionId);
        }

        SeenStepIds.Add(Step.StepId);
    }

    return true;
}

bool AJumanjiTrialActor::IsReadyToActivateForObjective(
    FName ExpectedObjectiveId,
    int32 RequiredProgress) const
{
    return TrialState == EJumanjiTrialState::Dormant
        && !ExpectedObjectiveId.IsNone()
        && ObjectiveId == ExpectedObjectiveId
        && RequiredProgress > 0
        && RequiredProgress == Steps.Num()
        && ValidateStepDefinitions();
}

bool AJumanjiTrialActor::CanResetForEncounterRetry() const
{
    return !ObjectiveId.IsNone()
        && ValidateStepDefinitions()
        && static_cast<uint8>(TrialState) <= static_cast<uint8>(EJumanjiTrialState::Completed);
}

bool AJumanjiTrialActor::ResetForEncounterRetry()
{
    if (!HasAuthority() || !CanResetForEncounterRetry())
    {
        return false;
    }

    if (TrialState == EJumanjiTrialState::Dormant && CurrentStepIndex == 0)
    {
        return true;
    }

    TrialState = EJumanjiTrialState::Dormant;
    CurrentStepIndex = 0;
    TrialRevision = TrialRevision < MAX_int32 ? TrialRevision + 1 : 1;

    BroadcastTrialStateChanged();
    ForceNetUpdate();
    return true;
}

bool AJumanjiTrialActor::ActivateTrial()
{
    if (!HasAuthority()
        || !IsReadyToActivateForObjective(ObjectiveId, Steps.Num()))
    {
        return false;
    }

    UWorld* World = GetWorld();
    AJumanjiGameState* BoardGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;

    if (!BoardGameState || !BoardGameState->IsBoardEventActive())
    {
        return false;
    }

    const TArray<FJumanjiAdventureObjectiveState> Objectives =
        BoardGameState->GetAdventureObjectives();

    const FJumanjiAdventureObjectiveState* Objective = Objectives.FindByPredicate(
        [this](const FJumanjiAdventureObjectiveState& Candidate)
        {
            return Candidate.ObjectiveId == ObjectiveId;
        });

    if (!Objective
        || Objective->bCompleted
        || Objective->CurrentProgress != 0
        || Objective->RequiredProgress != Steps.Num())
    {
        return false;
    }

    CurrentStepIndex = 0;
    TrialState = EJumanjiTrialState::Available;
    ++TrialRevision;
    BroadcastTrialStateChanged();
    ForceNetUpdate();
    return true;
}

bool AJumanjiTrialActor::SubmitOption(FName OptionId, int32 ExpectedRevision)
{
    if (!HasAuthority()
        || OptionId.IsNone()
        || ExpectedRevision != TrialRevision
        || (TrialState != EJumanjiTrialState::Available
            && TrialState != EJumanjiTrialState::InProgress))
    {
        return false;
    }

    const FJumanjiTrialStepDefinition* CurrentStep = GetCurrentStep();
    if (!CurrentStep
        || !CurrentStep->OptionIds.Contains(OptionId)
        || CurrentStep->CorrectOptionId != OptionId)
    {
        return false;
    }

    UWorld* World = GetWorld();
    UJumanjiBoardWorldSubsystem* BoardSubsystem = World
        ? World->GetSubsystem<UJumanjiBoardWorldSubsystem>()
        : nullptr;

    if (!BoardSubsystem
        || !BoardSubsystem->AdvanceBoardAdventureObjective(ObjectiveId, 1))
    {
        return false;
    }

    ++CurrentStepIndex;
    TrialState = CurrentStepIndex >= Steps.Num()
        ? EJumanjiTrialState::Completed
        : EJumanjiTrialState::InProgress;

    ++TrialRevision;
    BroadcastTrialStateChanged();
    ForceNetUpdate();
    return true;
}

const FJumanjiTrialStepDefinition* AJumanjiTrialActor::GetCurrentStep() const
{
    return Steps.IsValidIndex(CurrentStepIndex)
        ? &Steps[CurrentStepIndex]
        : nullptr;
}

FText AJumanjiTrialActor::GetCurrentStepPrompt() const
{
    const FJumanjiTrialStepDefinition* CurrentStep = GetCurrentStep();
    if (CurrentStep)
    {
        return CurrentStep->Prompt;
    }

    return TrialState == EJumanjiTrialState::Completed
        ? FText::FromString(TEXT("Próba ukończona"))
        : FText::GetEmpty();
}

TArray<FName> AJumanjiTrialActor::GetCurrentStepOptions() const
{
    const FJumanjiTrialStepDefinition* CurrentStep = GetCurrentStep();
    return CurrentStep ? CurrentStep->OptionIds : TArray<FName>();
}

void AJumanjiTrialActor::BroadcastTrialStateChanged()
{
    OnTrialStateChanged.Broadcast(TrialState, CurrentStepIndex, TrialRevision);
}

void AJumanjiTrialActor::OnRep_TrialRuntimeState()
{
    BroadcastTrialStateChanged();
}

void AJumanjiTrialActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AJumanjiTrialActor, TrialState);
    DOREPLIFETIME(AJumanjiTrialActor, CurrentStepIndex);
    DOREPLIFETIME(AJumanjiTrialActor, TrialRevision);
}