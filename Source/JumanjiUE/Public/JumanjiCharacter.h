#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "JumanjiProgressionTypes.h"
#include "GameFramework/Character.h"
#include "JumanjiCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputComponent;
class UAbilitySystemComponent;
class UJumanjiTraversalComponent;

/**
 * Base third-person character for JUMANJI.
 * The PlayerState owns the ASC; this pawn is its current GAS avatar.
 */
UCLASS()
class JUMANJIUE_API AJumanjiCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AJumanjiCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal")
    UJumanjiTraversalComponent* GetTraversalComponent() const { return TraversalComponent; }

    /** Entry point for both hardware input and future mobile touch buttons. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Abilities")
    bool ActivateAbilityByInputTag(FGameplayTag InputTag);

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnRep_PlayerState() override;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    void Input_PrimaryAttack();
    void Input_ActiveAbility();
    void Input_Ultimate();
    void Input_Dodge();

    UFUNCTION()
    void HandleSelectedCharacterChanged(FName CharacterId);

    UFUNCTION()
    void HandleProgressionChanged(const FJumanjiPlayerProgressionState& NewProgressionState);

    void BindToPlayerCharacterSelection();
    void InitializeAbilityActorInfo();
    void ApplySelectedCharacterPresentation();
    void ApplySelectedAbilityLoadout(FName CharacterId);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(Transient)
    FName LastAppliedCharacterId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UJumanjiTraversalComponent> TraversalComponent;


};
