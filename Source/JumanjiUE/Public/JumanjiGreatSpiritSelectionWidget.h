#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JumanjiCharacterDefinition.h"
#include "JumanjiGreatSpiritSelectionWidget.generated.h"

class AJumanjiPlayerState;

/**
 * Blueprint-driven selection screen base.
 *
 * This class provides roster data and safe selection/confirmation behavior.
 * The visual layout is created as a Widget Blueprint derived from this class.
 */
UCLASS(Blueprintable, Abstract)
class JUMANJIUE_API UJumanjiGreatSpiritSelectionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Selection")
    bool PreviewCharacter(FName CharacterId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Selection")
    bool ConfirmSelection();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Selection")
    void CancelSelection();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Selection")
    FName GetPreviewCharacterId() const { return PreviewCharacterId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Selection")
    FName GetConfirmedCharacterId() const { return ConfirmedCharacterId; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Selection")
    bool IsAwaitingServerConfirmation() const { return bAwaitingServerConfirmation; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Selection")
    void GetRosterEntries(TArray<FJumanjiCharacterDefinition>& OutEntries) const
    {
        OutEntries = RosterEntries;
    }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Build nine cards after roster data and initial selection have been loaded. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Selection")
    void BP_OnRosterReady(const TArray<FJumanjiCharacterDefinition>& Entries, FName InitiallySelectedId);

    /** Update the central preview and character details panel. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Selection")
    void BP_OnCharacterPreviewChanged(const FJumanjiCharacterDefinition& Definition);

    /** Called only after PlayerState reports the server-accepted character ID. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Selection")
    void BP_OnCharacterSelectionCommitted(FName CharacterId);

    /** Called when the user cancels the selection flow. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Selection")
    void BP_OnSelectionCancelled();

    /** Allows the widget Blueprint to show a pending/confirmation state. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Selection")
    void BP_OnSelectionRequestSent(FName CharacterId);

private:
    void BindToPlayerState();
    void UnbindFromPlayerState();

    UFUNCTION()
    void HandlePlayerStateCharacterChanged(FName CharacterId);

    UPROPERTY(Transient, BlueprintReadOnly, Category = "JUMANJI|Selection", meta = (AllowPrivateAccess = "true"))
    TArray<FJumanjiCharacterDefinition> RosterEntries;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "JUMANJI|Selection", meta = (AllowPrivateAccess = "true"))
    FName PreviewCharacterId = NAME_None;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "JUMANJI|Selection", meta = (AllowPrivateAccess = "true"))
    FName ConfirmedCharacterId = NAME_None;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "JUMANJI|Selection", meta = (AllowPrivateAccess = "true"))
    bool bAwaitingServerConfirmation = false;

    TWeakObjectPtr<AJumanjiPlayerState> BoundPlayerState;
};
