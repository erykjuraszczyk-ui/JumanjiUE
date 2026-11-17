#include "JumanjiTraversalLink.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SphereComponent.h"
#include "JumanjiCharacter.h"
#include "JumanjiPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/NavLinkCustomComponent.h"
#include "Net/UnrealNetwork.h"

AJumanjiTraversalLink::AJumanjiTraversalLink()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bSmartLinkIsRelevant = true;

    EntryAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("EntryAnchor"));
    EntryAnchor->SetupAttachment(RootComponent);

    ExitAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ExitAnchor"));
    ExitAnchor->SetupAttachment(RootComponent);

    TraversalSpline = CreateDefaultSubobject<USplineComponent>(TEXT("TraversalSpline"));
    TraversalSpline->SetupAttachment(RootComponent);
    TraversalSpline->SetClosedLoop(false);

    FocusVolume = CreateDefaultSubobject<USphereComponent>(TEXT("FocusVolume"));
    FocusVolume->SetupAttachment(RootComponent);
    FocusVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    FocusVolume->SetCollisionObjectType(ECC_WorldDynamic);
    FocusVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    FocusVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    FocusVolume->SetGenerateOverlapEvents(true);
    FocusVolume->SetSphereRadius(FocusVolumeRadius);
}

void AJumanjiTraversalLink::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateSmartLink();
    UpdateFocusVolume();
}

void AJumanjiTraversalLink::BeginPlay()
{
    Super::BeginPlay();
    UpdateSmartLink();
    UpdateFocusVolume();

    if (FocusVolume)
    {
        FocusVolume->OnComponentBeginOverlap.AddDynamic(
            this, &AJumanjiTraversalLink::HandleFocusVolumeBeginOverlap);
        FocusVolume->OnComponentEndOverlap.AddDynamic(
            this, &AJumanjiTraversalLink::HandleFocusVolumeEndOverlap);
        FocusVolume->UpdateOverlaps();
    }

    if (TraversalType == EJumanjiTraversalType::Lift && !IsValid(LiftPlatform))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI traversal: Lift link %s has no assigned AJumanjiLiftPlatform."),
            *GetName());
    }

    if (TraversalId.IsNone() || TraversalType == EJumanjiTraversalType::None)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI traversal: %s needs a stable TraversalId and a valid type."),
            *GetName());
    }

    if (bUseSplinePath && TraversalType == EJumanjiTraversalType::Zipline
        && !HasAuthoredTraversalSpline())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI traversal: Zipline %s has spline mode enabled but no valid spline; anchor interpolation will be used."),
            *TraversalId.ToString());
    }
}

void AJumanjiTraversalLink::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (FocusVolume)
    {
        TArray<AActor*> OverlappingActors;
        FocusVolume->GetOverlappingActors(OverlappingActors, AJumanjiCharacter::StaticClass());
        for (AActor* OverlappingActor : OverlappingActors)
        {
            AJumanjiCharacter* Character = Cast<AJumanjiCharacter>(OverlappingActor);
            AJumanjiPlayerController* PlayerController = Character
                ? Cast<AJumanjiPlayerController>(Character->GetController())
                : nullptr;
            if (PlayerController && PlayerController->IsLocalController())
            {
                PlayerController->UnregisterTraversalCandidate(this);
            }
        }
    }

    Super::EndPlay(EndPlayReason);
}

void AJumanjiTraversalLink::UpdateSmartLink()
{
    UpdateFocusVolume();

    if (!EntryAnchor || !ExitAnchor)
    {
        return;
    }

    if (UNavLinkCustomComponent* SmartLink = GetSmartLinkComp())
    {
        const FTransform ActorTransform = GetActorTransform();
        const FVector LocalStart = ActorTransform.InverseTransformPosition(EntryAnchor->GetComponentLocation());
        const FVector LocalEnd = ActorTransform.InverseTransformPosition(ExitAnchor->GetComponentLocation());
        const ENavLinkDirection::Type Direction = bBidirectional
            ? ENavLinkDirection::BothWays
            : ENavLinkDirection::LeftToRight;

        SmartLink->SetLinkData(LocalStart, LocalEnd, Direction);
        SmartLink->SetEnabled(bTraversalEnabled && bExposeSmartLinkToNavigation);
    }

    bSmartLinkIsRelevant = bExposeSmartLinkToNavigation;
    SetSmartLinkEnabled(bTraversalEnabled && bExposeSmartLinkToNavigation);
}

FVector AJumanjiTraversalLink::GetEntryLocation() const
{
    return EntryAnchor ? EntryAnchor->GetComponentLocation() : GetActorLocation();
}

FVector AJumanjiTraversalLink::GetExitLocation() const
{
    return ExitAnchor ? ExitAnchor->GetComponentLocation() : GetActorLocation();
}

float AJumanjiTraversalLink::GetTraversalDuration() const
{
    return FMath::Clamp(TraversalDuration, 0.2f, 20.0f);
}

float AJumanjiTraversalLink::GetTraversalArcHeight() const
{
    return FMath::Clamp(ArcHeight, 0.0f, 1500.0f);
}

bool AJumanjiTraversalLink::HasAuthoredTraversalSpline() const
{
    if (!bUseSplinePath || !TraversalSpline || TraversalSpline->GetNumberOfSplinePoints() < 2)
    {
        return false;
    }

    const float SplineLength = TraversalSpline->GetSplineLength();
    return FMath::IsFinite(SplineLength) && SplineLength >= 50.0f;
}

bool AJumanjiTraversalLink::GetTraversalPathTransformAtAlpha(
    float Alpha,
    bool bReverse,
    FTransform& OutTransform) const
{
    if (TraversalType != EJumanjiTraversalType::Zipline || !HasAuthoredTraversalSpline())
    {
        return false;
    }

    const float SplineLength = TraversalSpline->GetSplineLength();
    const float SafeAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
    const float DistanceAlongSpline = (bReverse ? 1.0f - SafeAlpha : SafeAlpha) * SplineLength;

    OutTransform = TraversalSpline->GetTransformAtDistanceAlongSpline(
        DistanceAlongSpline,
        ESplineCoordinateSpace::World,
        true
    );

    FVector Forward = OutTransform.GetRotation().GetAxisX();
    if (bReverse)
    {
        Forward *= -1.0f;
    }

    if (!Forward.Normalize())
    {
        return false;
    }

    FRotator FacingRotation = Forward.Rotation();
    FacingRotation.Roll = OutTransform.Rotator().Roll;
    OutTransform.SetRotation(FacingRotation.Quaternion());
    return true;
}

bool AJumanjiTraversalLink::CanBeUsedBy(const APawn* Pawn) const
{
    if (!bTraversalEnabled || TraversalId.IsNone()
        || TraversalType == EJumanjiTraversalType::None
        || !IsValid(Pawn) || !EntryAnchor || !ExitAnchor)
    {
        return false;
    }

    const float RadiusSquared = FMath::Square(FMath::Clamp(MaxInteractionDistance, 100.0f, 1000.0f));
    const FVector PawnLocation = Pawn->GetActorLocation();

    if (FVector::DistSquared(PawnLocation, GetEntryLocation()) <= RadiusSquared)
    {
        return true;
    }

    return bBidirectional
        && FVector::DistSquared(PawnLocation, GetExitLocation()) <= RadiusSquared;
}

bool AJumanjiTraversalLink::GetTraversalEndpoints(
    const APawn* Pawn,
    FTransform& OutStart,
    FTransform& OutEnd,
    bool& bOutReverse) const
{
    bOutReverse = false;
    if (!CanBeUsedBy(Pawn) || !EntryAnchor || !ExitAnchor)
    {
        return false;
    }

    const float EntryDistanceSquared = FVector::DistSquared(Pawn->GetActorLocation(), GetEntryLocation());
    const float ExitDistanceSquared = FVector::DistSquared(Pawn->GetActorLocation(), GetExitLocation());
    bOutReverse = bBidirectional && ExitDistanceSquared < EntryDistanceSquared;

    OutStart = bOutReverse ? ExitAnchor->GetComponentTransform() : EntryAnchor->GetComponentTransform();
    OutEnd = bOutReverse ? EntryAnchor->GetComponentTransform() : ExitAnchor->GetComponentTransform();
    return true;
}

bool AJumanjiTraversalLink::SetTraversalEnabled(bool bEnabled)
{
    if (!HasAuthority() || bTraversalEnabled == bEnabled)
    {
        return false;
    }

    bTraversalEnabled = bEnabled;
    UpdateSmartLink();
    ForceNetUpdate();
    return true;
}

void AJumanjiTraversalLink::UpdateFocusVolume()
{
    if (!FocusVolume)
    {
        return;
    }

    FocusVolume->SetSphereRadius(FMath::Clamp(FocusVolumeRadius, 150.0f, 1200.0f));
    FocusVolume->SetCollisionEnabled(
        bTraversalEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    FocusVolume->SetGenerateOverlapEvents(bTraversalEnabled);
}

void AJumanjiTraversalLink::HandleFocusVolumeBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    AJumanjiCharacter* Character = Cast<AJumanjiCharacter>(OtherActor);
    AJumanjiPlayerController* PlayerController = Character
        ? Cast<AJumanjiPlayerController>(Character->GetController())
        : nullptr;

    if (PlayerController && PlayerController->IsLocalController())
    {
        PlayerController->RegisterTraversalCandidate(this);
    }
}

void AJumanjiTraversalLink::HandleFocusVolumeEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    AJumanjiCharacter* Character = Cast<AJumanjiCharacter>(OtherActor);
    AJumanjiPlayerController* PlayerController = Character
        ? Cast<AJumanjiPlayerController>(Character->GetController())
        : nullptr;

    if (PlayerController && PlayerController->IsLocalController())
    {
        PlayerController->UnregisterTraversalCandidate(this);
    }
}

void AJumanjiTraversalLink::OnRep_TraversalEnabled()
{
    UpdateSmartLink();
    UpdateFocusVolume();
}

void AJumanjiTraversalLink::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiTraversalLink, bTraversalEnabled);
    DOREPLIFETIME(AJumanjiTraversalLink, LiftPlatform);
}
