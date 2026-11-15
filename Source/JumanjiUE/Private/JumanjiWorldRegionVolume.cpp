#include "JumanjiWorldRegionVolume.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "JumanjiPlayerState.h"
#include "JumanjiWorldAtlas.h"

AJumanjiWorldRegionVolume::AJumanjiWorldRegionVolume()
{
}

void AJumanjiWorldRegionVolume::BeginPlay()
{
    Super::BeginPlay();

    if (RegionId.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI: WorldRegionVolume has no RegionId: %s"), *GetName());
        return;
    }

    OnActorBeginOverlap.AddUniqueDynamic(
        this,
        &AJumanjiWorldRegionVolume::HandleActorBeginOverlap
    );
}

void AJumanjiWorldRegionVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    OnActorBeginOverlap.RemoveDynamic(
        this,
        &AJumanjiWorldRegionVolume::HandleActorBeginOverlap
    );

    Super::EndPlay(EndPlayReason);
}

void AJumanjiWorldRegionVolume::HandleActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    if (!HasAuthority() || !IsValid(OtherActor) || RegionId.IsNone())
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

    if (Atlas && Atlas->IsKnownRegion(RegionId))
    {
        PlayerState->DiscoverWorldRegion(RegionId);
    }
}
