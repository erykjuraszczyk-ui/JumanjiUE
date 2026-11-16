#include "JumanjiCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "JumanjiAbilitySystemComponent.h"
#include "JumanjiAbilityCatalogSubsystem.h"
#include "JumanjiCharacterDefinition.h"
#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiPlayerState.h"
#include "Math/RotationMatrix.h"

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
}

UAbilitySystemComponent* AJumanjiCharacter::GetAbilitySystemComponent() const
{
    const AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();
    return JumanjiPlayerState
        ? JumanjiPlayerState->GetJumanjiAbilitySystemComponent()
        : nullptr;
}

void AJumanjiCharacter::BeginPlay()
{
    Super::BeginPlay();
    BindToPlayerCharacterSelection();
}

void AJumanjiCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    BindToPlayerCharacterSelection();
}

void AJumanjiCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    BindToPlayerCharacterSelection();
}

void AJumanjiCharacter::BindToPlayerCharacterSelection()
{
    AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();
    if (!JumanjiPlayerState)
    {
        return;
    }

    JumanjiPlayerState->OnSelectedCharacterChanged.RemoveDynamic(
        this, &AJumanjiCharacter::HandleSelectedCharacterChanged);
    JumanjiPlayerState->OnSelectedCharacterChanged.AddDynamic(
        this, &AJumanjiCharacter::HandleSelectedCharacterChanged);
    JumanjiPlayerState->OnProgressionChanged.RemoveDynamic(
        this, &AJumanjiCharacter::HandleProgressionChanged);
    JumanjiPlayerState->OnProgressionChanged.AddDynamic(
        this, &AJumanjiCharacter::HandleProgressionChanged);

    InitializeAbilityActorInfo();
    ApplySelectedCharacterPresentation();
}

void AJumanjiCharacter::InitializeAbilityActorInfo()
{
    AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();
    UJumanjiAbilitySystemComponent* AbilitySystem =
        JumanjiPlayerState ? JumanjiPlayerState->GetJumanjiAbilitySystemComponent() : nullptr;

    if (AbilitySystem && AbilitySystem->GetAvatarActor() != this)
    {
        AbilitySystem->InitAbilityActorInfo(JumanjiPlayerState, this);
    }
}

void AJumanjiCharacter::HandleSelectedCharacterChanged(FName CharacterId)
{
    ApplySelectedCharacterPresentation();
}

void AJumanjiCharacter::HandleProgressionChanged(
    const FJumanjiPlayerProgressionState& NewProgressionState)
{
    // The PlayerState owns both the ASC and saved loadout. Re-resolve after
    // profile restoration or a server-confirmed equipment change.
    ApplySelectedCharacterPresentation();
}

void AJumanjiCharacter::ApplySelectedCharacterPresentation()
{
    AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();
    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster =
        GameInstance ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>() : nullptr;

    if (!JumanjiPlayerState || !Roster || !GetMesh())
    {
        return;
    }

    const FName CharacterId = JumanjiPlayerState->GetSelectedCharacterId();
    if (CharacterId.IsNone())
    {
        return;
    }

    FJumanjiCharacterDefinition Definition;
    if (!Roster->GetCharacterDefinition(CharacterId, Definition))
    {
        return;
    }

    if (CharacterId != LastAppliedCharacterId)
    {
        // Soft references preserve the default mesh until art assets are imported.
        if (!Definition.CharacterMesh.IsNull())
        {
            if (USkeletalMesh* LoadedMesh = Definition.CharacterMesh.LoadSynchronous())
            {
                GetMesh()->SetSkeletalMesh(LoadedMesh, true);
            }
        }

        LastAppliedCharacterId = CharacterId;
    }

    InitializeAbilityActorInfo();
    ApplySelectedAbilityLoadout(CharacterId, Definition);
}

void AJumanjiCharacter::ApplySelectedAbilityLoadout(
    FName CharacterId,
    const FJumanjiCharacterDefinition& Definition)
{
    if (!HasAuthority() || CharacterId.IsNone())
    {
        return;
    }

    AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();
    UJumanjiAbilitySystemComponent* AbilitySystem =
        JumanjiPlayerState ? JumanjiPlayerState->GetJumanjiAbilitySystemComponent() : nullptr;
    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiAbilityCatalogSubsystem* AbilityCatalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiAbilityCatalogSubsystem>()
        : nullptr;

    if (!AbilitySystem || !AbilityCatalog)
    {
        return;
    }

    TArray<FJumanjiResolvedAbility> ResolvedAbilities;
    const FJumanjiPlayerProgressionState Progression = JumanjiPlayerState->GetProgressionState();

    if (!AbilityCatalog->ResolveEquippedLoadout(
        CharacterId,
        Progression.EquippedAbilitySlots,
        ResolvedAbilities))
    {
        return;
    }

    AbilitySystem->ApplyResolvedCharacterAbilityLoadout(CharacterId, ResolvedAbilities);
}
bool AJumanjiCharacter::ActivateAbilityByInputTag(FGameplayTag InputTag)
{
    AJumanjiPlayerState* JumanjiPlayerState = GetPlayerState<AJumanjiPlayerState>();
    UJumanjiAbilitySystemComponent* AbilitySystem =
        JumanjiPlayerState ? JumanjiPlayerState->GetJumanjiAbilitySystemComponent() : nullptr;

    return AbilitySystem && AbilitySystem->TryActivateAbilitiesByInputTag(InputTag);
}

void AJumanjiCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    check(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AJumanjiCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AJumanjiCharacter::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AJumanjiCharacter::Turn);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AJumanjiCharacter::LookUp);

    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);

    PlayerInputComponent->BindAction(TEXT("Ability.Primary"), IE_Pressed, this, &AJumanjiCharacter::Input_PrimaryAttack);
    PlayerInputComponent->BindAction(TEXT("Ability.Active"), IE_Pressed, this, &AJumanjiCharacter::Input_ActiveAbility);
    PlayerInputComponent->BindAction(TEXT("Ability.Ultimate"), IE_Pressed, this, &AJumanjiCharacter::Input_Ultimate);
    PlayerInputComponent->BindAction(TEXT("Ability.Dodge"), IE_Pressed, this, &AJumanjiCharacter::Input_Dodge);
}

void AJumanjiCharacter::Input_PrimaryAttack()
{
    ActivateAbilityByInputTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.Primary")), false));
}

void AJumanjiCharacter::Input_ActiveAbility()
{
    ActivateAbilityByInputTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.Active")), false));
}

void AJumanjiCharacter::Input_Ultimate()
{
    ActivateAbilityByInputTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.Ultimate")), false));
}

void AJumanjiCharacter::Input_Dodge()
{
    ActivateAbilityByInputTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Ability.Dodge")), false));
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
