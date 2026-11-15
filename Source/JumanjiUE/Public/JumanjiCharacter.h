#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JumanjiCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputComponent;

/**
 * Base third-person character for JUMANJI.
 * Character presentation is resolved from the replicated PlayerState selection.
 */
UCLASS()
class JUMANJIUE_API AJumanjiCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AJumanjiCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnRep_PlayerState() override;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    UFUNCTION()
    void HandleSelectedCharacterChanged(FName CharacterId);

    void BindToPlayerCharacterSelection();
    void ApplySelectedCharacterPresentation();

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(Transient)
    FName LastAppliedCharacterId = NAME_None;
};
