#include "JumanjiBossCharacter.h"
#include "JumanjiCombatDefaults.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "JumanjiAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
    FGameplayTag GetTelegraphWarningTag()
    {
        return FGameplayTag::RequestGameplayTag(
            FName(TEXT("State.Boss.Telegraph.Warning")), false);
    }

    FGameplayTag GetTelegraphActiveTag()
    {
        return FGameplayTag::RequestGameplayTag(
            FName(TEXT("State.Boss.Telegraph.Active")), false);
    }
}

AJumanjiBossCharacter::AJumanjiBossCharacter()
{
    GetCharacterMovement()->MaxWalkSpeed = 260.0f;

    // The boss overrides the normal enemy prototype initialization.
    InitializationEffectClass = UJumanjiBossInitializationGameplayEffect::StaticClass();
    BasicAttackAbilityClass = UJumanjiDefaultEnemyMeleeAbility::StaticClass();
}

bool AJumanjiBossCharacter::BeginBossTelegraph(
    FName AttackId,
    EJumanjiBossTelegraphPattern Pattern,
    FVector Origin,
    FVector Direction,
    float WarningDuration,
    float ActiveDuration,
    float Radius,
    float Length,
    float Width)
{
    if (!HasAuthority()
        || IsDefeated()
        || AttackId.IsNone()
        || AttackId.ToString().Len() > 64
        || Origin.ContainsNaN()
        || Direction.ContainsNaN()
        || !FMath::IsFinite(WarningDuration)
        || !FMath::IsFinite(ActiveDuration)
        || !FMath::IsFinite(Radius)
        || !FMath::IsFinite(Length)
        || !FMath::IsFinite(Width)
        || WarningDuration < 0.25f
        || WarningDuration > 8.0f
        || ActiveDuration < 0.1f
        || ActiveDuration > 6.0f
        || Radius < 25.0f
        || Radius > 2500.0f
        || Length < 0.0f
        || Length > 4000.0f
        || Width < 0.0f
        || Width > 1500.0f)
    {
        return false;
    }

    CancelBossTelegraph();

    if (!GetWorld())
    {
        return false;
    }

    BossTelegraphState.AttackId = AttackId;
    BossTelegraphState.Pattern = Pattern;
    BossTelegraphState.Stage = EJumanjiBossTelegraphStage::Warning;
    BossTelegraphState.Origin = Origin;
    BossTelegraphState.Direction = Direction.IsNearlyZero()
        ? GetActorForwardVector()
        : Direction.GetSafeNormal();
    BossTelegraphState.ServerStartTime = GetWorld()->GetTimeSeconds();
    BossTelegraphState.WarningDuration = WarningDuration;
    BossTelegraphState.ActiveDuration = ActiveDuration;
    BossTelegraphState.Radius = Radius;
    BossTelegraphState.Length = Length;
    BossTelegraphState.Width = Width;
    BossTelegraphState.Revision = BossTelegraphState.Revision < MAX_uint32
        ? BossTelegraphState.Revision + 1
        : 1;

    RefreshTelegraphGameplayTags();
    PublishTelegraphState();

    GetWorldTimerManager().SetTimer(
        TelegraphWarningTimerHandle,
        this,
        &AJumanjiBossCharacter::HandleTelegraphWarningComplete,
        WarningDuration,
        false);

    return true;
}

void AJumanjiBossCharacter::CancelBossTelegraph()
{
    if (!HasAuthority())
    {
        return;
    }

    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(TelegraphWarningTimerHandle);
        GetWorldTimerManager().ClearTimer(TelegraphActiveTimerHandle);
    }

    if (BossTelegraphState.Stage == EJumanjiBossTelegraphStage::None)
    {
        RefreshTelegraphGameplayTags();
        return;
    }

    const uint32 NewRevision = BossTelegraphState.Revision < MAX_uint32
        ? BossTelegraphState.Revision + 1
        : 1;

    BossTelegraphState = FJumanjiBossTelegraphState();
    BossTelegraphState.Revision = NewRevision;

    RefreshTelegraphGameplayTags();
    PublishTelegraphState();
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

void AJumanjiBossCharacter::HandleDefeated()
{
    if (HasAuthority())
    {
        CancelBossTelegraph();
    }
    Super::HandleDefeated();
}

void AJumanjiBossCharacter::SetBossPhase(uint8 NewPhase)
{
    if (!HasAuthority() || NewPhase <= BossPhase || NewPhase > 3)
    {
        return;
    }

    CancelBossTelegraph();
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

void AJumanjiBossCharacter::SetTelegraphStage(EJumanjiBossTelegraphStage NewStage)
{
    if (!HasAuthority() || BossTelegraphState.Stage == NewStage)
    {
        return;
    }

    BossTelegraphState.Stage = NewStage;
    BossTelegraphState.ServerStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    BossTelegraphState.Revision = BossTelegraphState.Revision < MAX_uint32
        ? BossTelegraphState.Revision + 1
        : 1;

    RefreshTelegraphGameplayTags();
    PublishTelegraphState();
}

void AJumanjiBossCharacter::HandleTelegraphWarningComplete()
{
    if (!HasAuthority()
        || IsDefeated()
        || BossTelegraphState.Stage != EJumanjiBossTelegraphStage::Warning)
    {
        return;
    }

    const FName AttackId = BossTelegraphState.AttackId;
    const EJumanjiBossTelegraphPattern Pattern = BossTelegraphState.Pattern;
    const float ActiveDuration = BossTelegraphState.ActiveDuration;

    SetTelegraphStage(EJumanjiBossTelegraphStage::Executing);
    BP_OnBossTelegraphExecutionRequested(AttackId, Pattern);

    // A Blueprint callback may interrupt or replace the telegraph. Do not let
    // the previous attack's timer clear a newer attack state.
    if (BossTelegraphState.Stage == EJumanjiBossTelegraphStage::Executing
        && BossTelegraphState.AttackId == AttackId)
    {
        GetWorldTimerManager().SetTimer(
            TelegraphActiveTimerHandle,
            this,
            &AJumanjiBossCharacter::HandleTelegraphActiveComplete,
            ActiveDuration,
            false);
    }
}

void AJumanjiBossCharacter::HandleTelegraphActiveComplete()
{
    if (!HasAuthority()
        || BossTelegraphState.Stage != EJumanjiBossTelegraphStage::Executing)
    {
        return;
    }

    CancelBossTelegraph();
}

void AJumanjiBossCharacter::RefreshTelegraphGameplayTags()
{
    UJumanjiAbilitySystemComponent* ASC = GetEnemyAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }

    if (GetTelegraphWarningTag().IsValid())
    {
        ASC->RemoveLooseGameplayTag(
            GetTelegraphWarningTag(), 1, EGameplayTagReplicationState::TagAndCountToAll);
    }
    if (GetTelegraphActiveTag().IsValid())
    {
        ASC->RemoveLooseGameplayTag(
            GetTelegraphActiveTag(), 1, EGameplayTagReplicationState::TagAndCountToAll);
    }

    if (BossTelegraphState.Stage == EJumanjiBossTelegraphStage::Warning
        && GetTelegraphWarningTag().IsValid())
    {
        ASC->AddLooseGameplayTag(
            GetTelegraphWarningTag(), 1, EGameplayTagReplicationState::TagAndCountToAll);
    }
    else if (BossTelegraphState.Stage == EJumanjiBossTelegraphStage::Executing
        && GetTelegraphActiveTag().IsValid())
    {
        ASC->AddLooseGameplayTag(
            GetTelegraphActiveTag(), 1, EGameplayTagReplicationState::TagAndCountToAll);
    }
}

void AJumanjiBossCharacter::PublishTelegraphState()
{
    OnBossTelegraphChanged.Broadcast(BossTelegraphState);
    BP_OnBossTelegraphChanged(BossTelegraphState);
    ForceNetUpdate();
}

void AJumanjiBossCharacter::OnRep_BossPhase()
{
    BP_OnBossPhaseChanged(BossPhase);
}

void AJumanjiBossCharacter::OnRep_BossTelegraphState()
{
    OnBossTelegraphChanged.Broadcast(BossTelegraphState);
    BP_OnBossTelegraphChanged(BossTelegraphState);
}

void AJumanjiBossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeReplicatedProps);
    DOREPLIFETIME(AJumanjiBossCharacter, BossPhase);
    DOREPLIFETIME(AJumanjiBossCharacter, BossTelegraphState);
}
