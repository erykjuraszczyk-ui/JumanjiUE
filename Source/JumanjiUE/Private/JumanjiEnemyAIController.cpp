#include "JumanjiEnemyAIController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "JumanjiAbilitySystemComponent.h"
#include "JumanjiEnemyCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "GameFramework/Pawn.h"

AJumanjiEnemyAIController::AJumanjiEnemyAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickInterval(0.1f);

    PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

    SightConfig->SightRadius = 1400.0f;
    SightConfig->LoseSightRadius = 1700.0f;
    SightConfig->PeripheralVisionAngleDegrees = 75.0f;
    SightConfig->SetMaxAge(4.0f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    PerceptionComponent->ConfigureSense(*SightConfig);
    PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
    PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
        this, &AJumanjiEnemyAIController::HandleTargetPerceptionUpdated);
}

void AJumanjiEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    CurrentTarget.Reset();
    RepathAccumulator = 0.0f;
    AttackRequestAccumulator = 0.0f;
}

void AJumanjiEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!IsValid(Actor) || Actor == GetPawn() || Actor->IsA(AJumanjiEnemyCharacter::StaticClass()))
    {
        return;
    }

    if (Stimulus.WasSuccessfullySensed())
    {
        if (Actor->IsA(APawn::StaticClass()))
        {
            CurrentTarget = Actor;
        }
    }
    else if (CurrentTarget.Get() == Actor)
    {
        CurrentTarget.Reset();
        ClearFocus(EAIFocusPriority::Gameplay);
        StopMovement();
    }
}

void AJumanjiEnemyAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APawn* ControlledPawn = GetPawn();
    AActor* Target = CurrentTarget.Get();

    if (!IsValid(ControlledPawn) || !IsValid(Target))
    {
        return;
    }

    AJumanjiEnemyCharacter* Enemy = Cast<AJumanjiEnemyCharacter>(ControlledPawn);
    if (!Enemy || Enemy->IsDefeated())
    {
        StopMovement();
        return;
    }

    const float Distance = FVector::Dist2D(
        ControlledPawn->GetActorLocation(),
        Target->GetActorLocation());

    SetFocus(Target, EAIFocusPriority::Gameplay);

    RepathAccumulator += DeltaSeconds;
    if (Distance > Enemy->GetEnemyAttackRange())
    {
        AttackRequestAccumulator = 0.0f;
        if (RepathAccumulator >= 0.35f)
        {
            MoveToActor(Target, Enemy->GetEnemyAttackRange() * 0.8f, true, true, true);
            RepathAccumulator = 0.0f;
        }
        return;
    }

    StopMovement();

    AttackRequestAccumulator += DeltaSeconds;
    if (AttackRequestAccumulator >= 0.35f)
    {
        AttackRequestAccumulator = 0.0f;

        const FGameplayTag AttackTag = Enemy->GetEnemyAttackInputTag();
        UJumanjiAbilitySystemComponent* ASC =
            Cast<UJumanjiAbilitySystemComponent>(
                UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ControlledPawn));

        if (ASC && AttackTag.IsValid())
        {
            ASC->TryActivateAbilitiesByInputTag(AttackTag);
        }
    }
}
