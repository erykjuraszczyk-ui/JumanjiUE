#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JumanjiCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputComponent;

/**
 * Base third-person character for JUMANJI.
 * This foundation uses simple axis bindings; the mobile HUD and Enhanced Input
 * mapping assets are scheduled for the next implementation stage.
 */
UCLASS()
class JUMANJIUE_API AJumanjiCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AJumanjiCharacter();

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;
};
