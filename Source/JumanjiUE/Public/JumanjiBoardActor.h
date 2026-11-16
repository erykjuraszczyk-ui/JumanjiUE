#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumanjiInteractable.h"
#include "JumanjiBoardActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class AJumanjiCharacter;

/** World actor representing an interactable Living Board terminal. */
UCLASS()
class JUMANJIUE_API AJumanjiBoardActor : public AActor, public IJumanjiInteractable
{
    GENERATED_BODY()

public:
    AJumanjiBoardActor();

    virtual bool CanInteract_Implementation(AJumanjiCharacter* InteractingCharacter) const override;
    virtual void Interact_Implementation(AJumanjiCharacter* InteractingCharacter) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Board")
    TObjectPtr<UBoxComponent> InteractionVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Board")
    TObjectPtr<UStaticMeshComponent> BoardVisual;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "JUMANJI|Board")
    FName EventId = TEXT("BOARD_EMERALD_INTRO");
};
