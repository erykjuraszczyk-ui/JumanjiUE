#include "JumanjiTraversalComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "JumanjiTraversalLink.h"

namespace
{
    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X)
            && FMath::IsFinite(Value.Y)
            && FMath::IsFinite(Value.Z);
    }
}

UJumanjiTraversalComponent::UJumanjiTraversalComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickInterval = 0.016f;
}

void UJumanjiTraversalComponent::BeginPlay()
{
    Super::BeginPlay();
    SetComponentTickEnabled(false);
}

void UJumanjiTraversalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetOwner() && GetOwner()->HasAuthority() && TraversalState.bActive)
    {
        FinishTraversal(false);
    }

    SetComponentTickEnabled(false);
    Super::EndPlay(EndPlayReason);
}

bool UJumanjiTraversalComponent::CanStartTraversal(const AJumanjiTraversalLink* Link) const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    const APawn* Pawn = Cast<APawn>(GetOwner());

    return GetOwner() && GetOwner()->HasAuthority()
        && !TraversalState.bActive
        && IsValid(Link) && Link->CanBeUsedBy(Pawn)
        && Movement && Movement->IsMovingOnGround();
}

bool UJumanjiTraversalComponent::TryStartTraversal(AJumanjiTraversalLink* Link)
{
    if (!CanStartTraversal(Link))
    {
        return false;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    UWorld* World = GetWorld();

    FTransform StartTransform;
    FTransform EndTransform;
    bool bReverse = false;
    if (!Character || !Movement || !World
        || !Link->GetTraversalEndpoints(Character, StartTransform, EndTransform, bReverse))
    {
        return false;
    }

    const FVector StartLocation = Character->GetActorLocation();
    const FVector EndLocation = EndTransform.GetLocation();

    if (!IsFiniteVector(StartLocation) || !IsFiniteVector(EndLocation)
        || FVector::DistSquared(StartLocation, EndLocation) < FMath::Square(50.0f))
    {
        return false;
    }

    PreviousMovementMode = Movement->MovementMode;
    PreviousCustomMovementMode = Movement->CustomMovementMode;
    bHasPreviousMovementMode = true;
    Movement->StopMovementImmediately();
    Movement->DisableMovement();

    ActiveTraversalLink = Link;
    TraversalState.bActive = true;
    TraversalState.bReverse = bReverse;
    TraversalState.bLastTraversalCompleted = false;
    TraversalState.TraversalId = Link->GetTraversalId();
    TraversalState.TraversalType = Link->GetTraversalType();
    TraversalState.StartLocation = StartLocation;
    TraversalState.EndLocation = EndLocation;
    TraversalState.StartRotation = StartTransform.Rotator();
    TraversalState.EndRotation = EndTransform.Rotator();
    TraversalState.ServerStartTime = World->GetTimeSeconds();
    TraversalState.Duration = Link->GetTraversalDuration();
    TraversalState.ArcHeight = Link->GetTraversalArcHeight();
    TraversalState.Revision = TraversalState.Revision < MAX_int32
        ? TraversalState.Revision + 1
        : 1;

    SetComponentTickEnabled(true);
    Character->ForceNetUpdate();
    NotifyTraversalStateChanged();
    return true;
}

void UJumanjiTraversalComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UWorld* World = GetWorld();
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Character || !World || !TraversalState.bActive)
    {
        SetComponentTickEnabled(false);
        return;
    }

    const float SafeDuration = FMath::Max(0.2f, TraversalState.Duration);
    const float Alpha = FMath::Clamp(
        (World->GetTimeSeconds() - TraversalState.ServerStartTime) / SafeDuration,
        0.0f,
        1.0f
    );

    FTransform SplineTransform;
    const bool bUsingZiplineSpline = TraversalState.TraversalType == EJumanjiTraversalType::Zipline
        && IsValid(ActiveTraversalLink)
        && ActiveTraversalLink->GetTraversalPathTransformAtAlpha(
            Alpha,
            TraversalState.bReverse,
            SplineTransform
        );

    FVector TargetLocation;
    FRotator TargetRotation;

    if (bUsingZiplineSpline)
    {
        TargetLocation = SplineTransform.GetLocation();
        TargetRotation = SplineTransform.Rotator();
    }
    else
    {
        TargetLocation = FMath::Lerp(
            FVector(TraversalState.StartLocation),
            FVector(TraversalState.EndLocation),
            Alpha
        );

        if (TraversalState.TraversalType == EJumanjiTraversalType::ContextJump)
        {
            TargetLocation.Z += FMath::Sin(Alpha * PI)
                * FMath::Clamp(TraversalState.ArcHeight, 0.0f, 1500.0f);
        }

        TargetRotation = FMath::Lerp(
            TraversalState.StartRotation,
            TraversalState.EndRotation,
            Alpha
        );
    }

    FHitResult Hit;
    Character->SetActorLocation(TargetLocation, true, &Hit, ETeleportType::None);
    if (Hit.IsValidBlockingHit())
    {
        FinishTraversal(false);
        return;
    }

    Character->SetActorRotation(TargetRotation);
    if (Alpha >= 1.0f)
    {
        FinishTraversal(true);
    }
}

void UJumanjiTraversalComponent::CancelTraversal()
{
    if (GetOwner() && GetOwner()->HasAuthority() && TraversalState.bActive)
    {
        FinishTraversal(false);
    }
}

void UJumanjiTraversalComponent::FinishTraversal(bool bCompleted)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !TraversalState.bActive)
    {
        SetComponentTickEnabled(false);
        return;
    }

    if (bCompleted)
    {
        FHitResult FinalHit;
        Character->SetActorLocation(
            FVector(TraversalState.EndLocation),
            true,
            &FinalHit,
            ETeleportType::None
        );

        if (FinalHit.IsValidBlockingHit())
        {
            bCompleted = false;
        }
        else
        {
            Character->SetActorRotation(TraversalState.EndRotation);
        }
    }

    RestoreMovementMode();
    TraversalState.bActive = false;
    TraversalState.bLastTraversalCompleted = bCompleted;
    TraversalState.Revision = TraversalState.Revision < MAX_int32
        ? TraversalState.Revision + 1
        : 1;

    SetComponentTickEnabled(false);
    Character->ForceNetUpdate();
    NotifyTraversalStateChanged();

    if (!bCompleted)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("JUMANJI traversal %s was cancelled or blocked; movement restored at the last safe location."),
            *TraversalState.TraversalId.ToString());
    }
}

void UJumanjiTraversalComponent::RestoreMovementMode()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement)
    {
        bHasPreviousMovementMode = false;
        return;
    }

    const EMovementMode RestoreMode = bHasPreviousMovementMode
        ? PreviousMovementMode.GetValue()
        : MOVE_Walking;
    const uint8 RestoreCustomMode = bHasPreviousMovementMode
        ? PreviousCustomMovementMode
        : 0;

    Movement->SetMovementMode(RestoreMode == MOVE_None ? MOVE_Walking : RestoreMode, RestoreCustomMode);
    bHasPreviousMovementMode = false;
}

AJumanjiTraversalLink* UJumanjiTraversalComponent::GetActiveTraversalLink() const
{
    if (IsValid(ActiveTraversalLink)
        && ActiveTraversalLink->GetTraversalId() == TraversalState.TraversalId)
    {
        return ActiveTraversalLink;
    }

    if (TraversalState.TraversalId.IsNone() || !GetWorld())
    {
        return nullptr;
    }

    for (TActorIterator<AJumanjiTraversalLink> It(GetWorld()); It; ++It)
    {
        if (It->GetTraversalId() == TraversalState.TraversalId)
        {
            return *It;
        }
    }

    return nullptr;
}

void UJumanjiTraversalComponent::OnRep_TraversalState()
{
    ActiveTraversalLink = GetActiveTraversalLink();
    NotifyTraversalStateChanged();
}

void UJumanjiTraversalComponent::NotifyTraversalStateChanged()
{
    OnTraversalStateChanged.Broadcast(TraversalState);

    OnTraversalPresentationChanged(
        TraversalState.bActive,
        TraversalState.TraversalId,
        TraversalState.TraversalType,
        GetActiveTraversalLink()
    );
}

void UJumanjiTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UJumanjiTraversalComponent, TraversalState);
}
