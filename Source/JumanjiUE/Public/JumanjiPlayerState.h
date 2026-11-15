#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "JumanjiPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiSelectedCharacterChanged,
    FName, CharacterId
);

/** Replicated per-player selection state. Persistent account storage comes later. */
UCLASS()
class JUMANJIUE_API AJumanjiPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AJumanjiPlayerState();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Character")
    FName GetSelectedCharacterId() const { return SelectedCharacterId; }

    /** Must only be called by the server after roster validation. */
    void SetSelectedCharacterId(FName CharacterId);

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Character")
    FJumanjiSelectedCharacterChanged OnSelectedCharacterChanged;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterId, BlueprintReadOnly, Category = "JUMANJI|Character")
    FName SelectedCharacterId = FName(TEXT("GREAT_SPIRIT_I"));

    UFUNCTION()
    void OnRep_SelectedCharacterId();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
