#include "JumanjiWorldLandmark.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "JumanjiPlayerState.h"
#include "JumanjiWorldAtlas.h"

AJumanjiWorldLandmark::AJumanjiWorldLandmark()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    DiscoverySphere = CreateDefaultSubobject<USphereComponent>(TEXT("DiscoverySphere"));
    SetRootComponent(DiscoverySphere);
    DiscoverySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DiscoverySphere->SetCollisionObjectType(ECC_WorldDynamic);
    DiscoverySphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    DiscoverySphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    DiscoverySphere->SetGenerateOverlapEvents(true);
    DiscoverySphere->SetSphereRadius(DiscoveryRadius);

    LandmarkVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LandmarkVisual"));
    LandmarkVisual->SetupAttachment(DiscoverySphere);
    LandmarkVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AJumanjiWorldLandmark::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (DiscoverySphere)
    {
        DiscoverySphere->SetSphereRadius(FMath::Clamp(DiscoveryRadius, 100.0f, 2000.0f));
    }
}

void AJumanjiWorldLandmark::BeginPlay()
{
    Super::BeginPlay();

    if (LandmarkId.IsNone() || RegionId.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI: WorldLandmark %s needs LandmarkId and RegionId."), *GetName());
    }

    DiscoverySphere->OnComponentBeginOverlap.AddUniqueDynamic(
        this,
        &AJumanjiWorldLandmark::HandleDiscoveryOverlap
    );
}

void AJumanjiWorldLandmark::HandleDiscoveryOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!HasAuthority() || !IsValid(OtherActor) || LandmarkId.IsNone() || RegionId.IsNone())
    {
        return;
    }

    APawn* PlayerPawn = Cast<APawn>(OtherActor);
    AJumanjiPlayerState* PlayerState = PlayerPawn
        ? PlayerPawn->GetPlayerState<AJumanjiPlayerState>()
        : nullptr;

    if (!PlayerState)
    {
        return;
    }

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    if (!Atlas || !Atlas->IsLandmarkInRegion(LandmarkId, RegionId))
    {
        return;
    }

    PlayerState->DiscoverWorldRegion(RegionId);
    PlayerState->VisitWorldLandmark(LandmarkId);
}
