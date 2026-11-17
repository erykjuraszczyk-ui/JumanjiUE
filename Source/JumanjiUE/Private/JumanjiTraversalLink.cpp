#include "JumanjiTraversalLink.h"
#include "Components/SceneComponent.h"
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
}

void AJumanjiTraversalLink::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateSmartLink();
}

void AJumanjiTraversalLink::BeginPlay()
{
    Super::BeginPlay();
    UpdateSmartLink();

    if (TraversalId.IsNone() || TraversalType == EJumanjiTraversalType::None)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI traversal: %s needs a stable TraversalId and a valid type."),
            *GetName());
    }
}

void AJumanjiTraversalLink::UpdateSmartLink()
{
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

void AJumanjiTraversalLink::OnRep_TraversalEnabled()
{
    UpdateSmartLink();
}

void AJumanjiTraversalLink::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiTraversalLink, bTraversalEnabled);
}
