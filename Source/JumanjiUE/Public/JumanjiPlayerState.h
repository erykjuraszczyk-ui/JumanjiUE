#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "JumanjiPlayerState.generated.h"

class UJumanjiAbilitySystemComponent;
class UJumanjiAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiSelectedCharacterChanged,
    FName, CharacterId
);

/**
 * Player-owned persistent-in-session GAS owner.
 * The possessed Character is the ASC avatar; this keeps ability state attached
 * to PlayerState rather than to a pawn that may be replaced.
 */
UCLASS()
class JUMANJIUE_API AJumanjiPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AJumanjiPlayerState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Abilities")
    UJumanjiAbilitySystemComponent* GetJumanjiAbilitySystemComponent() const
    {
        return AbilitySystemComponent;
    }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Attributes")
    UJumanjiAttributeSet* GetJumanjiAttributeSet() const
    {
        return AttributeSet;
    }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Character")
    FName GetSelectedCharacterId() const { return SelectedCharacterId; }

    /** Must only be called by the server after roster validation. */
    void SetSelectedCharacterId(FName CharacterId);

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Character")
    FJumanjiSelectedCharacterChanged OnSelectedCharacterChanged;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Abilities", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UJumanjiAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UJumanjiAttributeSet> AttributeSet;

    UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterId, BlueprintReadOnly, Category = "JUMANJI|Character")
    FName SelectedCharacterId = FName(TEXT("GREAT_SPIRIT_I"));

    UFUNCTION()
    void OnRep_SelectedCharacterId();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
