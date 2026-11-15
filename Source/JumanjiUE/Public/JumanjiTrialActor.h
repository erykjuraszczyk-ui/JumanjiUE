#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumanjiTrialActor.generated.h"

UENUM(BlueprintType)
enum class EJumanjiTrialState : uint8
{
    Dormant UMETA(DisplayName = "Dormant"),
    Available UMETA(DisplayName = "Available"),
    InProgress UMETA(DisplayName = "In Progress"),
    Completed UMETA(DisplayName = "Completed")
};

/**
 * Authoring data for one step of a non-combat environmental trial.
 * CorrectOptionId is used by the server and is deliberately not exposed as a
 * Blueprint-readable runtime value.
 */
USTRUCT(BlueprintType)
struct FJumanjiTrialStepDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "JUMANJI|Trial")
    FName StepId = NAME_None;

    UPROPERTY(EditAnywhere, Category = "JUMANJI|Trial")
    FText Prompt;

    UPROPERTY(EditAnywhere, Category = "JUMANJI|Trial")
    TArray<FName> OptionIds;

    UPROPERTY(EditAnywhere, Category = "JUMANJI|Trial")
    FName CorrectOptionId = NAME_None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FJumanjiTrialStateChanged,
    EJumanjiTrialState, TrialState,
    int32, CurrentStepIndex,
    int32, Revision
);

/**
 * Reusable server-authoritative base for Living Board environmental trials.
 * Clients submit an option through their owning PlayerController. The server
 * validates range, revision and the configured answer before advancing the
 * matching Living Board objective.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API AJumanjiTrialActor : public AActor
{
    GENERATED_BODY()

public:
    AJumanjiTrialActor();

    /** Call on the server after StartBoardAdventure has initialized objectives. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Trial")
    bool ActivateTrial();

    /** Server-only validation entry point; PlayerController routes client requests here. */
    bool SubmitOption(FName OptionId, int32 ExpectedRevision);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Trial")
    FName GetObjectiveId() const { return ObjectiveId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Trial")
    EJumanjiTrialState GetTrialState() const { return TrialState; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Trial")
    int32 GetCurrentStepIndex() const { return CurrentStepIndex; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Trial")
    int32 GetStepCount() const { return Steps.Num(); }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Trial")
    int32 GetTrialRevision() const { return TrialRevision; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Trial")
    FText GetCurrentStepPrompt() const;

    /** Returns choices for the current step without exposing the expected answer. */
    UFUNCTION(BlueprintPure, Category = "JUMANJI|Trial")
    TArray<FName> GetCurrentStepOptions() const;

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Trial")
    FJumanjiTrialStateChanged OnTrialStateChanged;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "JUMANJI|Trial")
    FName ObjectiveId = NAME_None;

    UPROPERTY(EditDefaultsOnly, Category = "JUMANJI|Trial")
    TArray<FJumanjiTrialStepDefinition> Steps;

    UPROPERTY(ReplicatedUsing = OnRep_TrialRuntimeState, BlueprintReadOnly, Category = "JUMANJI|Trial")
    EJumanjiTrialState TrialState = EJumanjiTrialState::Dormant;

    UPROPERTY(ReplicatedUsing = OnRep_TrialRuntimeState, BlueprintReadOnly, Category = "JUMANJI|Trial")
    int32 CurrentStepIndex = 0;

    UPROPERTY(ReplicatedUsing = OnRep_TrialRuntimeState, BlueprintReadOnly, Category = "JUMANJI|Trial")
    int32 TrialRevision = 0;

    UFUNCTION()
    void OnRep_TrialRuntimeState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    bool ValidateStepDefinitions() const;
    const FJumanjiTrialStepDefinition* GetCurrentStep() const;
    void BroadcastTrialStateChanged();
};