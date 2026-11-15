#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JumanjiPlayerController.generated.h"

class UJumanjiGreatSpiritSelectionWidget;
class UJumanjiAdventureHUDWidget;
class AJumanjiTrialActor;

/** Validates character selection and routes world interactions through the server. */
UCLASS()
class JUMANJIUE_API AJumanjiPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Character")
    void RequestSelectCharacter(FName CharacterId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|UI")
    bool OpenGreatSpiritSelection();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|UI")
    void CloseGreatSpiritSelection();

    /**
     * Sends a puzzle option to the server. A true return on a client means the
     * request was queued, not that the server accepted the solution.
     */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool RequestTrialInteraction(
        AJumanjiTrialActor* TrialActor,
        FName OptionId,
        int32 ExpectedRevision
    );

protected:
    UFUNCTION(Server, Reliable)
    void ServerSelectCharacter(FName CharacterId);

    UFUNCTION(Server, Reliable)
    void ServerRequestTrialInteraction(
        AJumanjiTrialActor* TrialActor,
        FName OptionId,
        int32 ExpectedRevision
    );

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|UI")
    TSubclassOf<UJumanjiGreatSpiritSelectionWidget> GreatSpiritSelectionWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure|UI")
    TSubclassOf<UJumanjiAdventureHUDWidget> AdventureHUDWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure", meta = (ClampMin = "100.0", ClampMax = "1000.0"))
    float MaxTrialInteractionDistance = 350.0f;

private:
    void ApplyCharacterSelectionOnServer(FName CharacterId);

    bool ProcessTrialInteractionOnServer(
        AJumanjiTrialActor* TrialActor,
        FName OptionId,
        int32 ExpectedRevision
    );

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiGreatSpiritSelectionWidget> ActiveGreatSpiritSelectionWidget;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiAdventureHUDWidget> ActiveAdventureHUDWidget;
};
