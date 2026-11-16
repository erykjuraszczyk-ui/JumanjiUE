#include "JumanjiBoardActor.h"

#include "JumanjiBoardWorldSubsystem.h"
#include "JumanjiCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AJumanjiBoardActor::AJumanjiBoardActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
    SetRootComponent(InteractionVolume);
    InteractionVolume->SetBoxExtent(FVector(140.0f, 140.0f, 110.0f));
    InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    BoardVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardVisual"));
    BoardVisual->SetupAttachment(RootComponent);
    BoardVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );

    if (CubeMesh.Succeeded())
    {
        BoardVisual->SetStaticMesh(CubeMesh.Object);
        BoardVisual->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.15f));
        BoardVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -20.0f));
    }
}

bool AJumanjiBoardActor::CanInteract_Implementation(AJumanjiCharacter* InteractingCharacter) const
{
    if (!InteractingCharacter || EventId.IsNone())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const UJumanjiBoardWorldSubsystem* BoardSubsystem =
        World->GetSubsystem<UJumanjiBoardWorldSubsystem>();

    return BoardSubsystem != nullptr
        && !World->GetGameState<AGameStateBase>()->HasAuthority() == false;
}

void AJumanjiBoardActor::Interact_Implementation(AJumanjiCharacter* InteractingCharacter)
{
    if (!InteractingCharacter || !HasAuthority())
    {
        return;
    }

    if (UJumanjiBoardWorldSubsystem* BoardSubsystem =
        GetWorld()->GetSubsystem<UJumanjiBoardWorldSubsystem>())
    {
        BoardSubsystem->StartBoardEvent(EventId);
    }
}
