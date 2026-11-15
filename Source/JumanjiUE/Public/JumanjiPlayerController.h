#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JumanjiPlayerController.generated.h"

/** Validates character selection on the server before changing PlayerState. */
UCLASS()
class JUMANJIUE_API AJumanjiPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Character")
    void RequestSelectCharacter(FName CharacterId);

protected:
    UFUNCTION(Server, Reliable)
    void ServerSelectCharacter(FName CharacterId);

private:
    void ApplyCharacterSelectionOnServer(FName CharacterId);
};
