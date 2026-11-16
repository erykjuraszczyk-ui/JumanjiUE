#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "JumanjiInteractable.generated.h"

class AJumanjiCharacter;

UINTERFACE(BlueprintType)
class JUMANJIUE_API UJumanjiInteractable : public UInterface
{
    GENERATED_BODY()
};

/** Common interaction contract for boards, NPCs, doors and world objects. */
class JUMANJIUE_API IJumanjiInteractable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "JUMANJI|Interaction")
    bool CanInteract(AJumanjiCharacter* InteractingCharacter) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "JUMANJI|Interaction")
    void Interact(AJumanjiCharacter* InteractingCharacter);
};
