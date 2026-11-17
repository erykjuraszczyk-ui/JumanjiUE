#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JumanjiPlayerController.generated.h"

class UJumanjiGreatSpiritSelectionWidget;
class UJumanjiAdventureHUDWidget;
class UJumanjiAbilityHUDWidget;
class UJumanjiTraversalPromptWidget;
class AJumanjiTrialActor;
class UJumanjiWorldAtlasWidget;
class UJumanjiWorldNavigationComponent;
class AJumanjiTraversalLink;

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

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal|UI")
    UJumanjiTraversalPromptWidget* GetTraversalPromptWidget() const
    {
        return ActiveTraversalPromptWidget;
    }

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal|UI")
    bool SetTraversalFocus(AJumanjiTraversalLink* TraversalLink);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal|UI")
    void ClearTraversalFocus();

    /** Called by link overlap volumes on each local player. */
    void RegisterTraversalCandidate(AJumanjiTraversalLink* TraversalLink);
    void UnregisterTraversalCandidate(AJumanjiTraversalLink* TraversalLink);
    void RefreshTraversalCandidateFocus();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Character")
    void RequestSelectCharacter(FName CharacterId);

    /** Client request; the server validates recipe, character and profile costs. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Progression")
    bool RequestGreatSpiritUpgrade(FName CharacterId, FName RecipeId);

    /** Queues a catalog-validated ability-slot change; server result replicates through PlayerState. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Loadout")
    bool RequestEquipAbilitySlot(FName CharacterId, FName SlotId, FName AbilityId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|UI")
    bool OpenGreatSpiritSelection();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|UI")
    void CloseGreatSpiritSelection();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    bool OpenWorldAtlas();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    void CloseWorldAtlas();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool RequestTrialInteraction(
        AJumanjiTrialActor* TrialActor,
        FName OptionId,
        int32 ExpectedRevision
    );

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal")
    bool RequestTraversalLink(AJumanjiTraversalLink* TraversalLink);

protected:
    UFUNCTION(Server, Reliable)
    void ServerSelectCharacter(FName CharacterId);

    UFUNCTION(Server, Reliable)
    void ServerRequestGreatSpiritUpgrade(FName CharacterId, FName RecipeId);

    UFUNCTION(Server, Reliable)
    void ServerRequestEquipAbilitySlot(FName CharacterId, FName SlotId, FName AbilityId);

    UFUNCTION(Server, Reliable)
    void ServerRequestTrialInteraction(
        AJumanjiTrialActor* TrialActor,
        FName OptionId,
        int32 ExpectedRevision
    );

    UFUNCTION(Server, Reliable)
    void ServerRequestTraversalLink(AJumanjiTraversalLink* TraversalLink);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|UI")
    TSubclassOf<UJumanjiGreatSpiritSelectionWidget> GreatSpiritSelectionWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure|UI")
    TSubclassOf<UJumanjiAdventureHUDWidget> AdventureHUDWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Abilities|UI")
    TSubclassOf<UJumanjiAbilityHUDWidget> AbilityHUDWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    TSubclassOf<UJumanjiTraversalPromptWidget> TraversalPromptWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|World|UI")
    TSubclassOf<UJumanjiWorldAtlasWidget> WorldAtlasWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Adventure", meta = (ClampMin = "100.0", ClampMax = "1000.0"))
    float MaxTrialInteractionDistance = 350.0f;

private:
    void ApplyCharacterSelectionOnServer(FName CharacterId);
    bool ProcessGreatSpiritUpgradeOnServer(FName CharacterId, FName RecipeId);
    bool ProcessEquipAbilitySlotOnServer(FName CharacterId, FName SlotId, FName AbilityId);

    bool ProcessTrialInteractionOnServer(
        AJumanjiTrialActor* TrialActor,
        FName OptionId,
        int32 ExpectedRevision
    );

    bool ProcessTraversalLinkOnServer(AJumanjiTraversalLink* TraversalLink);

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiGreatSpiritSelectionWidget> ActiveGreatSpiritSelectionWidget;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiAdventureHUDWidget> ActiveAdventureHUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiAbilityHUDWidget> ActiveAbilityHUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiTraversalPromptWidget> ActiveTraversalPromptWidget;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AJumanjiTraversalLink>> NearbyTraversalCandidates;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiWorldAtlasWidget> ActiveWorldAtlasWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Navigation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UJumanjiWorldNavigationComponent> WorldNavigationComponent;
};
