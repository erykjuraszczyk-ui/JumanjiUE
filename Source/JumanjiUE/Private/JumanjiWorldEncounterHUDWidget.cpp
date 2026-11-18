#include "JumanjiWorldEncounterHUDWidget.h"

#include "JumanjiGameState.h"
#include "JumanjiPlayerState.h"
#include "JumanjiAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void UJumanjiWorldEncounterHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BindGameState();
    BindBoss();
    BindPlayerState();

    const FJumanjiWorldEncounterState State = GetEncounterState();
    LastReportedPhase = State.Phase;
    bHasReportedPhase = true;
    OnEncounterPhaseCue(State.Phase);
    ReportBossPhaseIfChanged();
    RefreshPresentation();
}

void UJumanjiWorldEncounterHUDWidget::NativeDestruct()
{
    if (IsValid(CachedGameState))
    {
        CachedGameState->OnWorldEncounterChanged.RemoveDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleWorldEncounterChanged);
    }

    if (IsValid(CachedBoss))
    {
        CachedBoss->OnBossTelegraphChanged.RemoveDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleBossTelegraphChanged);
    }

    if (IsValid(CachedPlayerState))
    {
        CachedPlayerState->OnEncounterReconnectChanged.RemoveDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleEncounterReconnectChanged);
    }

    CachedPlayerState = nullptr;
    CachedBoss = nullptr;
    CachedGameState = nullptr;
    Super::NativeDestruct();
}

void UJumanjiWorldEncounterHUDWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!IsValid(CachedGameState))
    {
        BindGameState();
    }

    if (!IsValid(CachedPlayerState))
    {
        BindPlayerState();
    }

    const FJumanjiWorldEncounterState State = GetEncounterState();
    if (State.BossActor != CachedBoss)
    {
        BindBoss();
    }

    RefreshAccumulator += InDeltaTime;
    const float SafeInterval = FMath::Clamp(RefreshInterval, 0.05f, 0.5f);
    if (RefreshAccumulator >= SafeInterval)
    {
        RefreshAccumulator = 0.0f;
        RefreshPresentation();
    }
}

void UJumanjiWorldEncounterHUDWidget::BindGameState()
{
    UWorld* World = GetWorld();
    AJumanjiGameState* NewGameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;

    if (CachedGameState == NewGameState)
    {
        return;
    }

    if (IsValid(CachedGameState))
    {
        CachedGameState->OnWorldEncounterChanged.RemoveDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleWorldEncounterChanged);
    }

    CachedGameState = NewGameState;

    if (IsValid(CachedGameState))
    {
        CachedGameState->OnWorldEncounterChanged.AddUniqueDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleWorldEncounterChanged);
    }

    BindBoss();
}

void UJumanjiWorldEncounterHUDWidget::BindPlayerState()
{
    APlayerController* PlayerController = GetOwningPlayer();
    AJumanjiPlayerState* NewPlayerState = PlayerController
        ? PlayerController->GetPlayerState<AJumanjiPlayerState>()
        : nullptr;

    if (CachedPlayerState == NewPlayerState)
    {
        return;
    }

    if (IsValid(CachedPlayerState))
    {
        CachedPlayerState->OnEncounterReconnectChanged.RemoveDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleEncounterReconnectChanged);
    }

    CachedPlayerState = NewPlayerState;

    if (IsValid(CachedPlayerState))
    {
        CachedPlayerState->OnEncounterReconnectChanged.AddUniqueDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleEncounterReconnectChanged);

        const FJumanjiEncounterReconnectState ReconnectState =
            CachedPlayerState->GetEncounterReconnectState();
        if (ReconnectState.bWasReconnected)
        {
            OnEncounterReconnectNotice(ReconnectState);
        }
    }
}

void UJumanjiWorldEncounterHUDWidget::BindBoss()
{
    AJumanjiBossCharacter* NewBoss = IsValid(CachedGameState)
        ? CachedGameState->GetWorldEncounterState().BossActor.Get()
        : nullptr;

    if (CachedBoss == NewBoss)
    {
        return;
    }

    if (IsValid(CachedBoss))
    {
        CachedBoss->OnBossTelegraphChanged.RemoveDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleBossTelegraphChanged);
    }

    CachedBoss = NewBoss;

    if (IsValid(CachedBoss))
    {
        CachedBoss->OnBossTelegraphChanged.AddUniqueDynamic(
            this,
            &UJumanjiWorldEncounterHUDWidget::HandleBossTelegraphChanged);
        OnBossTelegraphPresentationChanged(CachedBoss->GetBossTelegraphState());
    }
    else
    {
        OnBossTelegraphPresentationChanged(FJumanjiBossTelegraphState());
    }

    bHasReportedBossPhase = false;
    LastReportedBossPhase = 0;
    ReportBossPhaseIfChanged();
}

FJumanjiWorldEncounterState UJumanjiWorldEncounterHUDWidget::GetEncounterState() const
{
    return IsValid(CachedGameState)
        ? CachedGameState->GetWorldEncounterState()
        : FJumanjiWorldEncounterState();
}

FJumanjiEncounterReconnectState UJumanjiWorldEncounterHUDWidget::GetReconnectState() const
{
    return IsValid(CachedPlayerState)
        ? CachedPlayerState->GetEncounterReconnectState()
        : FJumanjiEncounterReconnectState();
}

FText UJumanjiWorldEncounterHUDWidget::GetEncounterStatusText() const
{
    const FJumanjiWorldEncounterState State = GetEncounterState();

    switch (State.Phase)
    {
    case EJumanjiWorldEncounterPhase::Trials:
        return FText::Format(
            NSLOCTEXT("JUMANJIEncounterHUD", "TrialsStatus", "Próby Great Spirit: {0}/{1}"),
            FText::AsNumber(GetCompletedTrialCount()),
            FText::AsNumber(GetTotalTrialCount()));

    case EJumanjiWorldEncounterPhase::Combat:
        return FText::Format(
            NSLOCTEXT("JUMANJIEncounterHUD", "WaveStatus", "Fala {0}/{1} — przeciwnicy: {2}"),
            FText::AsNumber(State.CurrentWave),
            FText::AsNumber(State.TotalWaves),
            FText::AsNumber(State.ActiveEnemyCount));

    case EJumanjiWorldEncounterPhase::Boss:
        return FText::Format(
            NSLOCTEXT("JUMANJIEncounterHUD", "BossStatus", "Strażnik Bramy Mrozu — faza {0}/3"),
            FText::AsNumber(GetBossPhase()));

    case EJumanjiWorldEncounterPhase::Reward:
        return NSLOCTEXT("JUMANJIEncounterHUD", "RewardStatus", "Przygotowywanie nagrody");

    case EJumanjiWorldEncounterPhase::Completed:
        return NSLOCTEXT("JUMANJIEncounterHUD", "CompletedStatus", "Spotkanie ukończone");

    case EJumanjiWorldEncounterPhase::Failed:
        return NSLOCTEXT("JUMANJIEncounterHUD", "FailedStatus", "Spotkanie przerwane");

    case EJumanjiWorldEncounterPhase::Inactive:
    default:
        return NSLOCTEXT("JUMANJIEncounterHUD", "InactiveStatus", "Brama Mrozu");
    }
}

int32 UJumanjiWorldEncounterHUDWidget::GetCompletedTrialCount() const
{
    if (!IsValid(CachedGameState))
    {
        return 0;
    }

    int32 Count = 0;
    for (const FJumanjiAdventureObjectiveState& Objective : CachedGameState->GetAdventureObjectives())
    {
        Count += Objective.bCompleted ? 1 : 0;
    }
    return Count;
}

int32 UJumanjiWorldEncounterHUDWidget::GetTotalTrialCount() const
{
    return IsValid(CachedGameState)
        ? CachedGameState->GetAdventureObjectives().Num()
        : 0;
}

float UJumanjiWorldEncounterHUDWidget::GetBossHealth() const
{
    const UJumanjiAttributeSet* Attributes = IsValid(CachedBoss)
        ? CachedBoss->GetEnemyAttributeSet()
        : nullptr;
    return Attributes ? Attributes->GetHealth() : 0.0f;
}

float UJumanjiWorldEncounterHUDWidget::GetBossMaxHealth() const
{
    const UJumanjiAttributeSet* Attributes = IsValid(CachedBoss)
        ? CachedBoss->GetEnemyAttributeSet()
        : nullptr;
    return Attributes ? Attributes->GetMaxHealth() : 0.0f;
}

float UJumanjiWorldEncounterHUDWidget::GetBossHealthPercent() const
{
    const float MaxHealth = GetBossMaxHealth();
    return MaxHealth > 0.0f
        ? FMath::Clamp(GetBossHealth() / MaxHealth, 0.0f, 1.0f)
        : 0.0f;
}

uint8 UJumanjiWorldEncounterHUDWidget::GetBossPhase() const
{
    return IsValid(CachedBoss) ? CachedBoss->GetBossPhase() : 0;
}

FJumanjiBossTelegraphState UJumanjiWorldEncounterHUDWidget::GetBossTelegraphState() const
{
    return IsValid(CachedBoss)
        ? CachedBoss->GetBossTelegraphState()
        : FJumanjiBossTelegraphState();
}

bool UJumanjiWorldEncounterHUDWidget::IsBossTelegraphActive() const
{
    return GetBossTelegraphState().Stage != EJumanjiBossTelegraphStage::None;
}

float UJumanjiWorldEncounterHUDWidget::GetSynchronizedWorldTime() const
{
    if (IsValid(CachedGameState))
    {
        return CachedGameState->GetServerWorldTimeSeconds();
    }

    const UWorld* World = GetWorld();
    return World ? World->GetTimeSeconds() : 0.0f;
}

float UJumanjiWorldEncounterHUDWidget::GetTelegraphTimeRemaining() const
{
    const FJumanjiBossTelegraphState State = GetBossTelegraphState();
    float Duration = 0.0f;

    if (State.Stage == EJumanjiBossTelegraphStage::Warning)
    {
        Duration = State.WarningDuration;
    }
    else if (State.Stage == EJumanjiBossTelegraphStage::Executing)
    {
        Duration = State.ActiveDuration;
    }
    else
    {
        return 0.0f;
    }

    return FMath::Max(
        0.0f,
        State.ServerStartTime + Duration - GetSynchronizedWorldTime());
}

float UJumanjiWorldEncounterHUDWidget::GetTelegraphRemainingPercent() const
{
    const FJumanjiBossTelegraphState State = GetBossTelegraphState();
    const float Duration = State.Stage == EJumanjiBossTelegraphStage::Warning
        ? State.WarningDuration
        : State.Stage == EJumanjiBossTelegraphStage::Executing
            ? State.ActiveDuration
            : 0.0f;

    return Duration > 0.0f
        ? FMath::Clamp(GetTelegraphTimeRemaining() / Duration, 0.0f, 1.0f)
        : 0.0f;
}

void UJumanjiWorldEncounterHUDWidget::HandleWorldEncounterChanged(
    const FJumanjiWorldEncounterState& NewState)
{
    BindBoss();

    if (!bHasReportedPhase || LastReportedPhase != NewState.Phase)
    {
        LastReportedPhase = NewState.Phase;
        bHasReportedPhase = true;
        OnEncounterPhaseCue(NewState.Phase);
    }

    RefreshPresentation();
}

void UJumanjiWorldEncounterHUDWidget::HandleBossTelegraphChanged(
    const FJumanjiBossTelegraphState& NewState)
{
    OnBossTelegraphPresentationChanged(NewState);
    RefreshPresentation();
}

void UJumanjiWorldEncounterHUDWidget::HandleEncounterReconnectChanged(
    const FJumanjiEncounterReconnectState& NewState)
{
    if (NewState.bWasReconnected)
    {
        OnEncounterReconnectNotice(NewState);
    }

    RefreshPresentation();
}

void UJumanjiWorldEncounterHUDWidget::ReportBossPhaseIfChanged()
{
    const uint8 CurrentBossPhase = GetBossPhase();
    if (CurrentBossPhase == 0)
    {
        bHasReportedBossPhase = false;
        LastReportedBossPhase = 0;
        return;
    }

    if (!bHasReportedBossPhase || LastReportedBossPhase != CurrentBossPhase)
    {
        bHasReportedBossPhase = true;
        LastReportedBossPhase = CurrentBossPhase;
        OnBossPhaseCue(CurrentBossPhase);
    }
}

void UJumanjiWorldEncounterHUDWidget::RefreshPresentation()
{
    ReportBossPhaseIfChanged();
    const FJumanjiWorldEncounterState State = GetEncounterState();
    OnEncounterHUDPresentationChanged(
        State,
        GetEncounterStatusText(),
        GetBossHealthPercent(),
        GetBossPhase(),
        GetBossTelegraphState());
}
