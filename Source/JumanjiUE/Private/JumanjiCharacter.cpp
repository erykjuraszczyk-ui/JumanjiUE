#include "JumanjiCharacter.h"

#include "JumanjiAttributeSet.h"
#include "JumanjiInteractable.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Math/RotationMatrix.h"

namespace Jumanji
{
    constexpr float InteractionTraceDistance = 450.0f;
    constexpr float InteractionMaxDistance = 450.0f;
}

AJumanjiCharacter::AJumanjiCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    GetCharacterMovement()->JumpZVelocity = 600.0f;
    GetCharacterMovement()->AirControl = 0.25f;
    GetCharacterMovement()->MaxWalkSpeed = 500.0f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 350.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UJumanjiAttributeSet>(TEXT("AttributeSet"));
}

void AJumanjiCharacter::BeginPlay()
{
    Super::BeginPlay();
    InitializeAbilityActorInfo();
}

void AJumanjiCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
}

void AJumanjiCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();
    InitializeAbilityActorInfo();
}

void AJumanjiCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();

    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController || !DefaultMappingContext)
    {
        return;
    }

    ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
    if (!LocalPlayer)
    {
        return;
    }

    if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
    }
}

void AJumanjiCharacter::InitializeAbilityActorInfo()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }
}

UAbilitySystemComponent* AJumanjiCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AJumanjiCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    check(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction)
        {
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AJumanjiCharacter::InputMove);
        }
        else
        {
            PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AJumanjiCharacter::MoveForward);
            PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AJumanjiCharacter::MoveRight);
        }

        if (LookAction)
        {
            EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AJumanjiCharacter::InputLook);
        }
        else
        {
            PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AJumanjiCharacter::Turn);
            PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AJumanjiCharacter::LookUp);
        }

        if (JumpAction)
        {
            EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AJumanjiCharacter::InputJumpStarted);
            EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AJumanjiCharacter::InputJumpCompleted);
            EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &AJumanjiCharacter::InputJumpCompleted);
        }
        else
        {
            PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
            PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
        }

        if (InteractAction)
        {
            EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AJumanjiCharacter::TryInteract);
        }
        else
        {
            PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AJumanjiCharacter::TryInteract);
        }
    }
    else
    {
        PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AJumanjiCharacter::MoveForward);
        PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AJumanjiCharacter::MoveRight);
        PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AJumanjiCharacter::Turn);
        PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AJumanjiCharacter::LookUp);
        PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
        PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
        PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AJumanjiCharacter::TryInteract);
    }
}

void AJumanjiCharacter::InputMove(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    MoveForward(Axis.Y);
    MoveRight(Axis.X);
}

void AJumanjiCharacter::InputLook(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    Turn(Axis.X);
    LookUp(Axis.Y);
}

void AJumanjiCharacter::InputJumpStarted()
{
    Jump();
}

void AJumanjiCharacter::InputJumpCompleted()
{
    StopJumping();
}

void AJumanjiCharacter::MoveForward(float Value)
{
    if (!Controller || FMath::IsNearlyZero(Value))
    {
        return;
    }

    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    AddMovementInput(Direction, Value);
}

void AJumanjiCharacter::MoveRight(float Value)
{
    if (!Controller || FMath::IsNearlyZero(Value))
    {
        return;
    }

    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
    AddMovementInput(Direction, Value);
}

void AJumanjiCharacter::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void AJumanjiCharacter::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

void AJumanjiCharacter::TryInteract()
{
    if (!FollowCamera || !GetWorld())
    {
        return;
    }

    const FVector TraceStart = FollowCamera->GetComponentLocation();
    const FVector TraceEnd = TraceStart
        + FollowCamera->GetForwardVector() * Jumanji::InteractionTraceDistance;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JumanjiInteract), false, this);
    FHitResult Hit;

    if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        return;
    }

    AActor* TargetActor = Hit.GetActor();
    if (!IsValid(TargetActor) || !TargetActor->GetClass()->ImplementsInterface(UJumanjiInteractable::StaticClass()))
    {
        return;
    }

    ServerInteract(TargetActor);
}

void AJumanjiCharacter::ServerInteract_Implementation(AActor* TargetActor)
{
    if (!IsValid(TargetActor)
        || !TargetActor->GetClass()->ImplementsInterface(UJumanjiInteractable::StaticClass())
        || FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation())
            > FMath::Square(Jumanji::InteractionMaxDistance))
    {
        return;
    }

    if (IJumanjiInteractable::Execute_CanInteract(TargetActor, this))
    {
        IJumanjiInteractable::Execute_Interact(TargetActor, this);
    }
}
