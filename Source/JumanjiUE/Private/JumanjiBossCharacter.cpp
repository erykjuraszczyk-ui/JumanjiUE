#include "JumanjiBossCharacter.h"
#include "JumanjiCombatDefaults.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "JumanjiAttributeSet.h"
#include "Net/UnrealNetwork.h"

AJumanjiBossCharacter::AJumanjiBossCharacter()
{
    GetCharacterMovement()->MaxWalkSpeed = 260.0f;

    // The boss overrides the normal enemy prototype initialization.
    InitializationEffectClass = UJumanjiBossInitializationGameplayEffect::StaticClass();
    BasicAttackAbilityClass = UJumanjiDefaultEnemyMeleeAbility::StaticClass();
}

void AJumanjiBossCharacter::HandleCombatHealthChanged(const FOnAttributeChangeData& ChangeData)
{
    Super::HandleCombatHealthChanged(ChangeData);

    if (!HasAuthority() || IsDefeated())
    {
        return;
    }

    UJumanjiAttributeSet* Attributes = GetEnemyAttributeSet();
    if (!Attributes || Attributes->GetMaxHealth() <= 0.0f)
    {
        return;
    }

    const float HealthFraction = ChangeData.NewValue / Attributes->GetMaxHealth();

    if (HealthFraction <= PhaseThreeHealthThreshold && BossPhase < 3)
    {
        SetBossPhase(3);
    }
    else if (HealthFraction <= PhaseTwoHealthThreshold && BossPhase < 2)
    {
        SetBossPhase(2);
    }
}

void AJumanjiBossCharacter::SetBossPhase(uint8 NewPhase)
{
    if (!HasAuthority() || NewPhase <= BossPhase || NewPhase > 3)
    {
        return;
    }

    BossPhase = NewPhase;

    if (UJumanjiAbilitySystemComponent* ASC = GetEnemyAbilitySystemComponent())
    {
        const FGameplayTag PhaseTwoTag = FGameplayTag::RequestGameplayTag(
            FName(TEXT("State.Boss.Phase.Two")), false);
        const FGameplayTag PhaseThreeTag = FGameplayTag::RequestGameplayTag(
            FName(TEXT("State.Boss.Phase.Three")), false);

        if (PhaseTwoTag.IsValid())
        {
            ASC->RemoveLooseGameplayTag(
                PhaseTwoTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
        }
        if (PhaseThreeTag.IsValid())
        {
            ASC->RemoveLooseGameplayTag(
                PhaseThreeTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
        }

        if (BossPhase >= 2 && PhaseTwoTag.IsValid())
        {
            ASC->AddLooseGameplayTag(
                PhaseTwoTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
        }
        if (BossPhase >= 3 && PhaseThreeTag.IsValid())
        {
            ASC->AddLooseGameplayTag(
                PhaseThreeTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
        }
    }

    BP_OnBossPhaseChanged(BossPhase);
    ForceNetUpdate();
}

void AJumanjiBossCharacter::OnRep_BossPhase()
{
    BP_OnBossPhaseChanged(BossPhase);
}

void AJumanjiBossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiBossCharacter, BossPhase);
}
