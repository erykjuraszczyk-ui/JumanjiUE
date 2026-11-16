#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JumanjiPlayerController.generated.h"

class UJumanjiGreatSpiritSelectionWidget;
class UJumanjiAdventureHUDWidget;
class AJumanjiTrialActor;
class UJumanjiWorldAtlasWidget;
class UJumanjiWorldNavigationComponent;

/** Validates character selection and routes world interactions through the server. */
UCLASS()
class JUMANJIUE_API AJumanjiPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AJumanjiPlayerController();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Navigation")
    UJumanjiWorldNavigationComponent* GetWorldNavigationComponent() const
    {
        return WorldNavigationComponent;
    }

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Character")
    void RequestSelectCharacter(FName CharacterId);

    /** Client request; the server validates recipe, character and profile costs. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Progression")
    bool RequestGreatSpiritUpgrade(FName CharacterId, FName RecipeId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|UI")
    bool OpenGreatSpiritSelection();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|UI")
    void CloseGreatSpiritSelection();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    bool OpenWorldAtlas();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    void CloseWorldAtlas();

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
    void ServerRequestGreatSpiritUpgrade(FName CharacterId, FName RecipeId);

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|World|UI")
    TSubclassOf<UJumanjiWorldAtlasWidget> WorldAtlasWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure", meta = (ClampMin = "100.0", ClampMax = "1000.0"))
    float MaxTrialInteractionDistance = 350.0f;

private:
    void ApplyCharacterSelectionOnServer(FName CharacterId);
    bool ProcessGreatSpiritUpgradeOnServer(FName CharacterId, FName RecipeId);

    bool ProcessTrialInteractionOnServer(
        AJumanjiTrialActor* TrialActor,
        FName OptionId,
        int32 ExpectedRevision
    );

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiGreatSpiritSelectionWidget> ActiveGreatSpiritSelectionWidget;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiAdventureHUDWidget> ActiveAdventureHUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiWorldAtlasWidget> ActiveWorldAtlasWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Navigation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UJumanjiWorldNavigationComponent> WorldNavigationComponent;
};
