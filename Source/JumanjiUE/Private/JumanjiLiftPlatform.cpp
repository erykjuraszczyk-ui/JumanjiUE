#include "JumanjiLiftPlatform.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionShape.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "JumanjiTraversalComponent.h"
#include "Net/UnrealNetwork.h"

AJumanjiLiftPlatform::AJumanjiLiftPlatform()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bReplicates = true;
    SetReplicateMovement(true);
    NetUpdateFrequency = 30.0f;
    MinNetUpdateFrequency = 10.0f;

    PlatformCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PlatformCollision"));
    SetRootComponent(PlatformCollision);
    PlatformCollision->SetBoxExtent(FVector(240.0f, 240.0f, 60.0f));
    PlatformCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PlatformCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetupAttachment(PlatformCollision);

    PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
    PlatformMesh->SetupAttachment(SceneRoot);
    // PlatformCollision is the sole physical root. The mesh is visual only,
    // avoiding duplicate collision surfaces and duplicate sweep contacts.
    PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlatformMesh->SetIsReplicated(false);

    BoardingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardingVolume"));
    BoardingVolume->SetupAttachment(SceneRoot);
    BoardingVolume->SetBoxExtent(FVector(220.0f, 220.0f, 180.0f));
    BoardingVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoardingVolume->SetCollisionObjectType(ECC_WorldDynamic);
    BoardingVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    BoardingVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    BoardingVolume->SetGenerateOverlapEvents(true);

    LowerStopAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("LowerStopAnchor"));
    LowerStopAnchor->SetupAttachment(SceneRoot);

    UpperStopAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("UpperStopAnchor"));
    UpperStopAnchor->SetupAttachment(SceneRoot);
    UpperStopAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 1200.0f));
}

void AJumanjiLiftPlatform::BeginPlay()
{
    Super::BeginPlay();

    InitialActorLocation = GetActorLocation();
    LowerStopLocation = LowerStopAnchor
        ? LowerStopAnchor->GetComponentLocation()
        : InitialActorLocation;
    UpperStopLocation = UpperStopAnchor
        ? UpperStopAnchor->GetComponentLocation()
        : InitialActorLocation + FVector(0.0f, 0.0f, 1200.0f);

    bAnchorsInitialized = FVector::DistSquared(LowerStopLocation, UpperStopLocation)
        >= FMath::Square(100.0f);

    if (!bAnchorsInitialized)
    {
        UE_LOG(LogTemp, Error,
            TEXT("JUMANJI lift %s has invalid lower/upper stop anchors; platform will remain disabled."),
            *GetName());
        LiftState.State = EJumanjiLiftPlatformState::Blocked;
    }
}

void AJumanjiLiftPlatform::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        DetachAllPassengers(false);
    }

    Super::EndPlay(EndPlayReason);
}

bool AJumanjiLiftPlatform::CanAcceptPassenger(bool bReverse) const
{
    if (!HasAuthority() || !bAnchorsInitialized || Passengers.Num() >= FMath::Clamp(MaxPassengers, 1, 8))
    {
        return false;
    }

    const EJumanjiLiftPlatformState DesiredMovingState = bReverse
        ? EJumanjiLiftPlatformState::MovingDown
        : EJumanjiLiftPlatformState::MovingUp;

    if (LiftState.State == DesiredMovingState)
    {
        const float Duration = FMath::Max(0.5f, LiftState.Duration);
        const float Alpha = GetWorld()
            ? FMath::Clamp((GetWorld()->GetTimeSeconds() - LiftState.ServerStartTime) / Duration, 0.0f, 1.0f)
            : 1.0f;

        return Alpha <= FMath::Clamp(BoardingWindowAlpha, 0.0f, 0.25f);
    }

    return bReverse
        ? LiftState.State == EJumanjiLiftPlatformState::AtUpper
        : LiftState.State == EJumanjiLiftPlatformState::AtLower;
}

bool AJumanjiLiftPlatform::IsCharacterInsideBoardingVolume(const ACharacter* Character) const
{
    if (!IsValid(Character) || !BoardingVolume)
    {
        return false;
    }

    if (BoardingVolume->IsOverlappingActor(Character))
    {
        return true;
    }

    // The prompt anchor may be slightly outside the query box due to capsule
    // center offsets; keep a bounded server-side distance fallback.
    return FVector::DistSquared(Character->GetActorLocation(), GetActorLocation())
        <= FMath::Square(350.0f);
}

bool AJumanjiLiftPlatform::TryBoardCharacter(
    ACharacter* Character,
    UJumanjiTraversalComponent* TraversalComponent,
    bool bReverse)
{
    if (!HasAuthority()
        || !IsValid(Character)
        || !IsValid(TraversalComponent)
        || !IsCharacterInsideBoardingVolume(Character)
        || !CanAcceptPassenger(bReverse))
    {
        return false;
    }

    const bool bAlreadyPassenger = Passengers.ContainsByPredicate(
        [Character](const FJumanjiLiftPassenger& Passenger)
        {
            return Passenger.Character == Character;
        });

    if (bAlreadyPassenger)
    {
        return false;
    }

    const bool bStartNewCourse =
        LiftState.State == EJumanjiLiftPlatformState::AtLower
        || LiftState.State == EJumanjiLiftPlatformState::AtUpper;

    FJumanjiLiftPassenger Passenger;
    Passenger.Character = Character;
    Passenger.TraversalComponent = TraversalComponent;
    Passengers.Add(Passenger);

    if (!Character->AttachToComponent(
        PlatformCollision,
        FAttachmentTransformRules::KeepWorldTransform))
    {
        Passengers.RemoveAll(
            [Character](const FJumanjiLiftPassenger& Entry)
            {
                return Entry.Character == Character;
            });
        LiftState.PassengerCount = Passengers.Num();
        ForceNetUpdate();
        return false;
    }

    LiftState.PassengerCount = Passengers.Num();
    if (bStartNewCourse)
    {
        StartTravel(bReverse);
    }

    Character->ForceNetUpdate();
    ForceNetUpdate();
    return true;
}

void AJumanjiLiftPlatform::RemovePassenger(ACharacter* Character, bool bRestoreMovement)
{
    if (!HasAuthority() || !IsValid(Character))
    {
        return;
    }

    const int32 PassengerIndex = Passengers.IndexOfByPredicate(
        [Character](const FJumanjiLiftPassenger& Passenger)
        {
            return Passenger.Character == Character;
        });

    if (PassengerIndex == INDEX_NONE)
    {
        return;
    }

    FJumanjiLiftPassenger Passenger = Passengers[PassengerIndex];
    Passengers.RemoveAtSwap(PassengerIndex);
    LiftState.PassengerCount = Passengers.Num();

    if (Character->GetAttachParentActor() == this)
    {
        Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }

    if (bRestoreMovement && IsValid(Passenger.TraversalComponent))
    {
        Passenger.TraversalComponent->CompleteLiftPlatformRide(false);
    }

    Character->ForceNetUpdate();
}


bool AJumanjiLiftPlatform::IsRecoveryLocationClear(const FVector& CandidateLocation) const
{
    UWorld* World = GetWorld();
    if (!World || !PlatformCollision
        || !FMath::IsFinite(CandidateLocation.X)
        || !FMath::IsFinite(CandidateLocation.Y)
        || !FMath::IsFinite(CandidateLocation.Z))
    {
        return false;
    }

    const FCollisionShape CollisionShape =
        FCollisionShape::MakeBox(PlatformCollision->GetScaledBoxExtent());
    const FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(JumanjiLiftRecovery),
        false,
        this
    );

    const ECollisionChannel Channels[] = {
        ECC_WorldStatic,
        ECC_WorldDynamic,
        ECC_Pawn
    };

    for (const ECollisionChannel Channel : Channels)
    {
        if (World->OverlapBlockingTestByChannel(
            CandidateLocation,
            GetActorQuat(),
            Channel,
            CollisionShape,
            QueryParams))
        {
            return false;
        }
    }

    return true;
}

bool AJumanjiLiftPlatform::TryRecoverBlockedPlatform()
{
    if (!HasAuthority()
        || !bAnchorsInitialized
        || LiftState.State != EJumanjiLiftPlatformState::Blocked
        || !Passengers.IsEmpty()
        || LiftState.PassengerCount != 0)
    {
        return false;
    }

    const FVector LowerStopActorLocation = InitialActorLocation;
    const FVector UpperStopActorLocation =
        InitialActorLocation + (UpperStopLocation - LowerStopLocation);

    const bool bLowerStopClear = IsRecoveryLocationClear(LowerStopActorLocation);
    const bool bUpperStopClear = IsRecoveryLocationClear(UpperStopActorLocation);

    if (!bLowerStopClear && !bUpperStopClear)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI lift %s recovery refused: both authored stops are obstructed."),
            *GetName());
        return false;
    }

    const float LowerDistanceSquared =
        FVector::DistSquared(GetActorLocation(), LowerStopActorLocation);
    const float UpperDistanceSquared =
        FVector::DistSquared(GetActorLocation(), UpperStopActorLocation);

    const bool bRecoverToLower = bLowerStopClear
        && (!bUpperStopClear || LowerDistanceSquared <= UpperDistanceSquared);
    const FVector RecoveryLocation = bRecoverToLower
        ? LowerStopActorLocation
        : UpperStopActorLocation;

    SetActorTickEnabled(false);
    SetActorLocation(RecoveryLocation, false, nullptr, ETeleportType::TeleportPhysics);

    LiftState.State = bRecoverToLower
        ? EJumanjiLiftPlatformState::AtLower
        : EJumanjiLiftPlatformState::AtUpper;
    LiftState.bReverse = false;
    LiftState.PassengerCount = 0;
    LiftState.ServerStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    LiftState.Revision = LiftState.Revision < MAX_int32
        ? LiftState.Revision + 1
        : 1;

    ForceNetUpdate();
    NotifyStateChanged();

    UE_LOG(LogTemp, Log,
        TEXT("JUMANJI lift %s recovered to %s stop after a collision-free check."),
        *GetName(),
        bRecoverToLower ? TEXT("lower") : TEXT("upper"));
    return true;
}

void AJumanjiLiftPlatform::StartTravel(bool bReverse)
{
    if (!HasAuthority() || !bAnchorsInitialized)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    LiftState.State = bReverse
        ? EJumanjiLiftPlatformState::MovingDown
        : EJumanjiLiftPlatformState::MovingUp;
    LiftState.bReverse = bReverse;
    LiftState.ServerStartTime = World->GetTimeSeconds();
    LiftState.Duration = FMath::Clamp(TravelDurationSeconds, 0.5f, 60.0f);
    LiftState.Revision = LiftState.Revision < MAX_int32 ? LiftState.Revision + 1 : 1;

    SetActorTickEnabled(true);
    ForceNetUpdate();
    NotifyStateChanged();
}

void AJumanjiLiftPlatform::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        UpdateLiftMotion();
    }
}

void AJumanjiLiftPlatform::UpdateLiftMotion()
{
    if (!HasAuthority()
        || (LiftState.State != EJumanjiLiftPlatformState::MovingUp
            && LiftState.State != EJumanjiLiftPlatformState::MovingDown))
    {
        SetActorTickEnabled(false);
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        FinishTravel(false);
        return;
    }

    const float SafeDuration = FMath::Clamp(LiftState.Duration, 0.5f, 60.0f);
    const float Alpha = FMath::Clamp(
        (World->GetTimeSeconds() - LiftState.ServerStartTime) / SafeDuration,
        0.0f,
        1.0f
    );

    const FVector StartStop = LiftState.bReverse ? UpperStopLocation : LowerStopLocation;
    const FVector EndStop = LiftState.bReverse ? LowerStopLocation : UpperStopLocation;
    const FVector StopOffset = EndStop - StartStop;
    const FVector TargetLocation = InitialActorLocation + StopOffset * Alpha;

    FHitResult Hit;
    SetActorLocation(TargetLocation, true, &Hit, ETeleportType::None);
    if (Hit.IsValidBlockingHit())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI lift %s was blocked at alpha %.3f."),
            *GetName(),
            Alpha);
        FinishTravel(false);
        return;
    }

    if (Alpha >= 1.0f)
    {
        FinishTravel(true);
    }
}

void AJumanjiLiftPlatform::FinishTravel(bool bCompleted)
{
    if (!HasAuthority())
    {
        return;
    }

    if (bCompleted)
    {
        LiftState.State = LiftState.bReverse
            ? EJumanjiLiftPlatformState::AtLower
            : EJumanjiLiftPlatformState::AtUpper;
    }
    else
    {
        LiftState.State = EJumanjiLiftPlatformState::Blocked;
    }

    LiftState.Revision = LiftState.Revision < MAX_int32 ? LiftState.Revision + 1 : 1;
    SetActorTickEnabled(false);
    DetachAllPassengers(bCompleted);
    ForceNetUpdate();
    NotifyStateChanged();
}

void AJumanjiLiftPlatform::DetachAllPassengers(bool bCompleted)
{
    TArray<FJumanjiLiftPassenger> PassengersToRelease = MoveTemp(Passengers);
    Passengers.Reset();
    LiftState.PassengerCount = 0;

    for (const FJumanjiLiftPassenger& Passenger : PassengersToRelease)
    {
        ACharacter* Character = Passenger.Character;
        if (IsValid(Character) && Character->GetAttachParentActor() == this)
        {
            Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        }

        if (IsValid(Passenger.TraversalComponent))
        {
            Passenger.TraversalComponent->CompleteLiftPlatformRide(bCompleted);
        }

        if (IsValid(Character))
        {
            Character->ForceNetUpdate();
        }
    }
}

void AJumanjiLiftPlatform::OnRep_LiftState()
{
    NotifyStateChanged();
}

void AJumanjiLiftPlatform::NotifyStateChanged()
{
    OnLiftStateChanged.Broadcast(LiftState);
    OnLiftPresentationChanged(LiftState);
}

void AJumanjiLiftPlatform::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiLiftPlatform, LiftState);
}
