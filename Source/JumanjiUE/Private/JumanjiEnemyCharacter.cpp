#include "JumanjiEnemyCharacter.h"
#include "JumanjiCombatDefaults.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "JumanjiEnemyAIController.h"
#include "JumanjiGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "JumanjiAbilitySystemComponent.h"
#include "JumanjiAttributeSet.h"
#include "Net/UnrealNetwork.h"

AJumanjiEnemyCharacter::AJumanjiEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    AbilitySystemComponent =
        CreateDefaultSubobject<UJumanjiAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UJumanjiAttributeSet>(TEXT("AttributeSet"));

    AIControllerClass = AJumanjiEnemyAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Native defaults make the first combat slice testable before BP assets exist.
    InitializationEffectClass = UJumanjiEnemyInitializationGameplayEffect::StaticClass();
    BasicAttackAbilityClass = UJumanjiDefaultEnemyMeleeAbility::StaticClass();

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = 300.0f;
}

void AJumanjiEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (!AbilitySystemComponent)
    {
        return;
    }

    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    if (HasAuthority() && InitializationEffectClass)
    {
        FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
        Context.AddSourceObject(this);

        FGameplayEffectSpecHandle InitSpec =
            AbilitySystemComponent->MakeOutgoingSpec(InitializationEffectClass, 1.0f, Context);

        if (InitSpec.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*InitSpec.Data.Get());
        }
    }

    if (HasAuthority() && BasicAttackAbilityClass
        && BasicAttackAbilityClass->IsChildOf(UJumanjiGameplayAbility::StaticClass()))
    {
        const FGameplayAbilitySpec AttackSpec(BasicAttackAbilityClass, 1, INDEX_NONE, this);
        AbilitySystemComponent->GiveAbility(AttackSpec);
    }

    HealthChangedDelegateHandle =
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            UJumanjiAttributeSet::GetHealthAttribute())
        .AddUObject(this, &AJumanjiEnemyCharacter::HandleCombatHealthChanged);
}

void AJumanjiEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AbilitySystemComponent && HealthChangedDelegateHandle.IsValid())
    {
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            UJumanjiAttributeSet::GetHealthAttribute())
            .Remove(HealthChangedDelegateHandle);
        HealthChangedDelegateHandle.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

FGameplayTag AJumanjiEnemyCharacter::GetEnemyAttackInputTag() const
{
    return FGameplayTag::RequestGameplayTag(AttackInputTagName, false);
}

UAbilitySystemComponent* AJumanjiEnemyCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AJumanjiEnemyCharacter::HandleCombatHealthChanged(const FOnAttributeChangeData& ChangeData)
{
    if (ChangeData.NewValue < ChangeData.OldValue)
    {
        BP_OnDamaged(ChangeData.OldValue, ChangeData.NewValue);
    }

    if (HasAuthority() && !bIsDefeated && ChangeData.NewValue <= 0.0f)
    {
        HandleDefeated();
    }
}

void AJumanjiEnemyCharacter::HandleDefeated()
{
    if (!HasAuthority() || bIsDefeated)
    {
        return;
    }

    bIsDefeated = true;

    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OnEnemyDefeated.Broadcast(this);
    BP_OnDefeated();

    ForceNetUpdate();
    SetLifeSpan(4.0f);
}

void AJumanjiEnemyCharacter::OnRep_IsDefeated()
{
    if (bIsDefeated)
    {
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BP_OnDefeated();
    }
}

void AJumanjiEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiEnemyCharacter, bIsDefeated);
}
