#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "JumanjiCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class USpringArmComponent;
class UJumanjiAttributeSet;
struct FInputActionValue;

/**
 * Replicated third-person player character.
 * Enhanced Input assets are configurable in a Blueprint child; legacy bindings
 * remain as a fallback until those assets are created in Unreal Editor.
 */
UCLASS()
class JUMANJIUE_API AJumanjiCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AJumanjiCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Interaction")
    void TryInteract();

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_Controller() override;
    virtual void PawnClientRestart() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    void InputMove(const FInputActionValue& Value);
    void InputLook(const FInputActionValue& Value);
    void InputJumpStarted();
    void InputJumpCompleted();

    UFUNCTION(Server, Reliable)
    void ServerInteract(AActor* TargetActor);

private:
    void InitializeAbilityActorInfo();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Abilities", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UJumanjiAttributeSet> AttributeSet;

    UPROPERTY(EditDefaultsOnly, Category = "JUMANJI|Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "JUMANJI|Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category = "JUMANJI|Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category = "JUMANJI|Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditDefaultsOnly, Category = "JUMANJI|Input")
    TObjectPtr<UInputAction> InteractAction;
};
