#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JumanjiPlayerController.generated.h"

class UJumanjiGreatSpiritSelectionWidget;

/** Validates character selection on the server and owns local selection UI. */
UCLASS()
class JUMANJIUE_API AJumanjiPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Character")
    void RequestSelectCharacter(FName CharacterId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|UI")
    bool OpenGreatSpiritSelection();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|UI")
    void CloseGreatSpiritSelection();

protected:
    UFUNCTION(Server, Reliable)
    void ServerSelectCharacter(FName CharacterId);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|UI")
    TSubclassOf<UJumanjiGreatSpiritSelectionWidget> GreatSpiritSelectionWidgetClass;

private:
    void ApplyCharacterSelectionOnServer(FName CharacterId);

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiGreatSpiritSelectionWidget> ActiveGreatSpiritSelectionWidget;
};
