#include "JumanjiGameState.h"

#include "JumanjiBossCharacter.h"

#include "JumanjiPlayerState.h"
#include "JumanjiProgressionRewardSubsystem.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

namespace
{
    constexpr int32 MaxAdventureObjectives = 32;
    constexpr int32 MaxObjectiveProgress = 1000;
}

void AJumanjiGameState::SetBoardEventState(FName EventId, bool bIsActive)
{
    if (!HasAuthority())
    {
        return;
    }

    if (bIsActive && EventId.IsNone())
    {
        return;
    }

    ActiveBoardEventId = bIsActive ? EventId : NAME_None;
    bBoardEventActive = bIsActive;
    ++BoardEventRevision;

    OnBoardEventStateChanged.Broadcast(
        ActiveBoardEventId,
        bBoardEventActive,
        BoardEventRevision
    );

    ForceNetUpdate();
}

bool AJumanjiGameState::BeginAdventureChapter(
    FName ChapterId,
    FName BoardEventId,
    FText DisplayName,
    FText Description)
{
    if (!HasAuthority()
        || ChapterId.IsNone()
        || BoardEventId.IsNone()
        || DisplayName.IsEmpty()
        || Description.IsEmpty()
        || !bBoardEventActive
        || ActiveBoardEventId != BoardEventId
        || AdventureChapterState.Phase == EJumanjiAdventureChapterPhase::Active)
    {
        return false;
    }

    FJumanjiAdventureChapterState NewChapterState;
    NewChapterState.ChapterId = ChapterId;
    NewChapterState.BoardEventId = BoardEventId;
    NewChapterState.DisplayName = MoveTemp(DisplayName);
    NewChapterState.Description = MoveTemp(Description);
    NewChapterState.Phase = EJumanjiAdventureChapterPhase::Active;
    NewChapterState.Revision = AdventureChapterState.Revision + 1;

    AdventureChapterState = MoveTemp(NewChapterState);
    OnAdventureChapterChanged.Broadcast(AdventureChapterState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::CompleteAdventureChapter(FName ChapterId)
{
    if (!HasAuthority()
        || ChapterId.IsNone()
        || AdventureChapterState.ChapterId != ChapterId
        || AdventureChapterState.Phase != EJumanjiAdventureChapterPhase::Active
        || bBoardEventActive
        || !AreAdventureObjectivesComplete())
    {
        return false;
    }

    AdventureChapterState.Phase = EJumanjiAdventureChapterPhase::Completed;
    ++AdventureChapterState.Revision;

    OnAdventureChapterChanged.Broadcast(AdventureChapterState);

    // The chapter is shared by the session. Persist completion per connected
    // profile and grant its reward through the existing idempotent transaction ledger.
    UJumanjiProgressionRewardSubsystem* RewardSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UJumanjiProgressionRewardSubsystem>()
        : nullptr;

    for (APlayerState* BasePlayerState : PlayerArray)
    {
        AJumanjiPlayerState* JumanjiPlayerState = Cast<AJumanjiPlayerState>(BasePlayerState);
        if (!JumanjiPlayerState)
        {
            continue;
        }

        JumanjiPlayerState->MarkAdventureChapterCompleted(ChapterId);

        if (RewardSubsystem
            && !JumanjiPlayerState->GetPersistentProfileId().IsEmpty()
            && !RewardSubsystem->GrantChapterCompletionReward(JumanjiPlayerState, ChapterId))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("JUMANJI rewards: chapter '%s' completed but reward was not applied for profile '%s'. It will be retried on profile restore."),
                *ChapterId.ToString(),
                *JumanjiPlayerState->GetPersistentProfileId());
        }
    }

    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::BeginWorldEncounter(FName EncounterId, int32 TotalWaves)
{
    if (!HasAuthority()
        || EncounterId.IsNone()
        || EncounterId.ToString().Len() > 64
        || TotalWaves < 1
        || TotalWaves > 16
        || (WorldEncounterState.Phase != EJumanjiWorldEncounterPhase::Inactive
            && !(WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Failed
                && WorldEncounterState.EncounterId == EncounterId)))
    {
        return false;
    }

    FJumanjiWorldEncounterState NewState;
    NewState.EncounterId = EncounterId;
    NewState.Phase = EJumanjiWorldEncounterPhase::Trials;
    NewState.TotalWaves = TotalWaves;
    NewState.Revision = WorldEncounterState.Revision < MAX_int32
        ? WorldEncounterState.Revision + 1
        : 1;
    WorldEncounterState = MoveTemp(NewState);

    OnWorldEncounterChanged.Broadcast(WorldEncounterState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::UpdateWorldEncounter(
    FName EncounterId,
    EJumanjiWorldEncounterPhase Phase,
    int32 CurrentWave,
    int32 TotalWaves,
    int32 ActiveEnemyCount)
{
    if (!HasAuthority()
        || EncounterId.IsNone()
        || EncounterId != WorldEncounterState.EncounterId
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Inactive
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Completed
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Failed
        || Phase == EJumanjiWorldEncounterPhase::Inactive
        || Phase == EJumanjiWorldEncounterPhase::Completed
        || Phase == EJumanjiWorldEncounterPhase::Failed
        || CurrentWave < 0
        || TotalWaves != WorldEncounterState.TotalWaves
        || CurrentWave > TotalWaves
        || ActiveEnemyCount < 0
        || ActiveEnemyCount > 64)
    {
        return false;
    }

    WorldEncounterState.Phase = Phase;
    WorldEncounterState.CurrentWave = CurrentWave;
    WorldEncounterState.ActiveEnemyCount = ActiveEnemyCount;
    WorldEncounterState.Revision = WorldEncounterState.Revision < MAX_int32
        ? WorldEncounterState.Revision + 1
        : 1;

    OnWorldEncounterChanged.Broadcast(WorldEncounterState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::SetWorldEncounterBoss(
    FName EncounterId,
    AJumanjiBossCharacter* BossActor)
{
    if (!HasAuthority()
        || EncounterId.IsNone()
        || EncounterId != WorldEncounterState.EncounterId
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Inactive
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Completed
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Failed
        || (BossActor != nullptr
            && (!IsValid(BossActor)
                || BossActor->GetWorld() != GetWorld()
                || WorldEncounterState.Phase != EJumanjiWorldEncounterPhase::Boss)))
    {
        return false;
    }

    if (WorldEncounterState.BossActor == BossActor)
    {
        return true;
    }

    WorldEncounterState.BossActor = BossActor;
    WorldEncounterState.Revision = WorldEncounterState.Revision < MAX_int32
        ? WorldEncounterState.Revision + 1
        : 1;

    OnWorldEncounterChanged.Broadcast(WorldEncounterState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::CompleteWorldEncounter(FName EncounterId)
{
    if (!HasAuthority()
        || EncounterId.IsNone()
        || EncounterId != WorldEncounterState.EncounterId
        || WorldEncounterState.Phase != EJumanjiWorldEncounterPhase::Reward
        || WorldEncounterState.ActiveEnemyCount != 0)
    {
        return false;
    }

    WorldEncounterState.Phase = EJumanjiWorldEncounterPhase::Completed;
    WorldEncounterState.CurrentWave = 0;
    WorldEncounterState.ActiveEnemyCount = 0;
    WorldEncounterState.Revision = WorldEncounterState.Revision < MAX_int32
        ? WorldEncounterState.Revision + 1
        : 1;

    OnWorldEncounterChanged.Broadcast(WorldEncounterState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::FailWorldEncounter(FName EncounterId)
{
    if (!HasAuthority()
        || EncounterId.IsNone()
        || EncounterId != WorldEncounterState.EncounterId
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Inactive
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Completed
        || WorldEncounterState.Phase == EJumanjiWorldEncounterPhase::Failed)
    {
        return false;
    }

    WorldEncounterState.Phase = EJumanjiWorldEncounterPhase::Failed;
    WorldEncounterState.CurrentWave = 0;
    WorldEncounterState.ActiveEnemyCount = 0;
    WorldEncounterState.Revision = WorldEncounterState.Revision < MAX_int32
        ? WorldEncounterState.Revision + 1
        : 1;

    OnWorldEncounterChanged.Broadcast(WorldEncounterState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::ResetAdventureChapterForEncounterRetry(
    FName EncounterId,
    FName ChapterId,
    FName BoardEventId)
{
    if (!HasAuthority()
        || EncounterId.IsNone()
        || ChapterId.IsNone()
        || BoardEventId.IsNone()
        || WorldEncounterState.EncounterId != EncounterId
        || WorldEncounterState.Phase != EJumanjiWorldEncounterPhase::Failed
        || (!AdventureChapterState.ChapterId.IsNone()
            && AdventureChapterState.ChapterId != ChapterId)
        || (!AdventureChapterState.BoardEventId.IsNone()
            && AdventureChapterState.BoardEventId != BoardEventId)
        || bBoardEventActive)
    {
        return false;
    }

    AdventureChapterState.ChapterId = ChapterId;
    AdventureChapterState.BoardEventId = BoardEventId;
    AdventureChapterState.Phase = EJumanjiAdventureChapterPhase::Inactive;
    AdventureChapterState.Revision = AdventureChapterState.Revision < MAX_int32
        ? AdventureChapterState.Revision + 1
        : 1;

    AdventureObjectiveSet.Objectives.Reset();
    AdventureObjectiveSet.Revision = AdventureObjectiveSet.Revision < MAX_int32
        ? AdventureObjectiveSet.Revision + 1
        : 1;

    OnAdventureChapterChanged.Broadcast(AdventureChapterState);
    OnAdventureObjectivesChanged.Broadcast(AdventureObjectiveSet.Revision);
    ForceNetUpdate();

    // Deliberately do not modify PlayerState profile snapshots, chapter receipts,
    // inventory, currencies or transaction ledgers. Retry resets session state only.
    return true;
}

bool AJumanjiGameState::InitializeAdventureObjectives(
    const TArray<FJumanjiAdventureObjectiveState>& Objectives)
{
    if (!HasAuthority()
        || !bBoardEventActive
        || Objectives.IsEmpty()
        || Objectives.Num() > MaxAdventureObjectives)
    {
        return false;
    }

    TSet<FName> SeenObjectiveIds;
    TArray<FJumanjiAdventureObjectiveState> ValidatedObjectives;
    ValidatedObjectives.Reserve(Objectives.Num());

    for (const FJumanjiAdventureObjectiveState& Objective : Objectives)
    {
        if (Objective.ObjectiveId.IsNone()
            || Objective.DisplayName.IsEmpty()
            || Objective.RequiredProgress < 1
            || Objective.RequiredProgress > MaxObjectiveProgress
            || SeenObjectiveIds.Contains(Objective.ObjectiveId))
        {
            return false;
        }

        SeenObjectiveIds.Add(Objective.ObjectiveId);

        FJumanjiAdventureObjectiveState SanitizedObjective = Objective;
        SanitizedObjective.CurrentProgress = 0;
        SanitizedObjective.bCompleted = false;
        ValidatedObjectives.Add(MoveTemp(SanitizedObjective));
    }

    FJumanjiAdventureObjectiveSet NewObjectiveSet;
    NewObjectiveSet.Objectives = MoveTemp(ValidatedObjectives);
    NewObjectiveSet.Revision = AdventureObjectiveSet.Revision + 1;
    AdventureObjectiveSet = MoveTemp(NewObjectiveSet);

    OnAdventureObjectivesChanged.Broadcast(AdventureObjectiveSet.Revision);
    ForceNetUpdate();
    return true;
}

bool AJumanjiGameState::AddAdventureObjectiveProgress(FName ObjectiveId, int32 ProgressAmount)
{
    if (!HasAuthority()
        || !bBoardEventActive
        || ObjectiveId.IsNone()
        || ProgressAmount < 1
        || ProgressAmount > MaxObjectiveProgress)
    {
        return false;
    }

    for (FJumanjiAdventureObjectiveState& Objective : AdventureObjectiveSet.Objectives)
    {
        if (Objective.ObjectiveId != ObjectiveId)
        {
            continue;
        }

        if (Objective.bCompleted)
        {
            return false;
        }

        Objective.CurrentProgress = FMath::Min(
            Objective.RequiredProgress,
            Objective.CurrentProgress + ProgressAmount
        );
        Objective.bCompleted = Objective.CurrentProgress >= Objective.RequiredProgress;

        ++AdventureObjectiveSet.Revision;
        OnAdventureObjectivesChanged.Broadcast(AdventureObjectiveSet.Revision);
        ForceNetUpdate();
        return true;
    }

    return false;
}

bool AJumanjiGameState::AreAdventureObjectivesComplete() const
{
    if (AdventureObjectiveSet.Objectives.IsEmpty())
    {
        return false;
    }

    for (const FJumanjiAdventureObjectiveState& Objective : AdventureObjectiveSet.Objectives)
    {
        if (!Objective.bCompleted)
        {
            return false;
        }
    }

    return true;
}

void AJumanjiGameState::OnRep_BoardEventState()
{
    OnBoardEventStateChanged.Broadcast(
        ActiveBoardEventId,
        bBoardEventActive,
        BoardEventRevision
    );
}

void AJumanjiGameState::OnRep_AdventureObjectiveSet()
{
    OnAdventureObjectivesChanged.Broadcast(AdventureObjectiveSet.Revision);
}

void AJumanjiGameState::OnRep_AdventureChapterState()
{
    OnAdventureChapterChanged.Broadcast(AdventureChapterState);
}

void AJumanjiGameState::OnRep_WorldEncounterState()
{
    OnWorldEncounterChanged.Broadcast(WorldEncounterState);
}

void AJumanjiGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AJumanjiGameState, ActiveBoardEventId);
    DOREPLIFETIME(AJumanjiGameState, bBoardEventActive);
    DOREPLIFETIME(AJumanjiGameState, BoardEventRevision);
    DOREPLIFETIME(AJumanjiGameState, AdventureChapterState);
    DOREPLIFETIME(AJumanjiGameState, AdventureObjectiveSet);
    DOREPLIFETIME(AJumanjiGameState, WorldEncounterState);
}
