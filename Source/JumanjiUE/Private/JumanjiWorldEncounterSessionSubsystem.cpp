#include "JumanjiWorldEncounterSessionSubsystem.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "JumanjiGameState.h"
#include "JumanjiPlayerState.h"

void UJumanjiWorldEncounterSessionSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ActiveEncounterId = NAME_None;
    ParticipantProfileIds.Reset();
    ConnectedProfileIds.Reset();
    DisconnectedAtUtc.Reset();
    ReconnectRevision = 0;
}

void UJumanjiWorldEncounterSessionSubsystem::Deinitialize()
{
    ActiveEncounterId = NAME_None;
    ParticipantProfileIds.Reset();
    ConnectedProfileIds.Reset();
    DisconnectedAtUtc.Reset();
    ReconnectRevision = 0;
    Super::Deinitialize();
}

bool UJumanjiWorldEncounterSessionSubsystem::IsValidProfileKey(
    const FString& ProfileId) const
{
    return !ProfileId.TrimStartAndEnd().IsEmpty() && ProfileId.Len() <= 256;
}

bool UJumanjiWorldEncounterSessionSubsystem::BeginEncounterSession(
    FName EncounterId,
    AJumanjiPlayerState* InitiatingPlayer)
{
    if (!IsValid(InitiatingPlayer)
        || !InitiatingPlayer->HasAuthority()
        || EncounterId.IsNone()
        || !IsValidProfileKey(InitiatingPlayer->GetPersistentProfileId())
        || !ActiveEncounterId.IsNone())
    {
        return false;
    }

    ActiveEncounterId = EncounterId;
    ParticipantProfileIds.Reset();
    ConnectedProfileIds.Reset();
    DisconnectedAtUtc.Reset();
    ReconnectRevision = 0;

    const FString ProfileId = InitiatingPlayer->GetPersistentProfileId();
    ParticipantProfileIds.Add(ProfileId);
    ConnectedProfileIds.Add(ProfileId);
    return true;
}

bool UJumanjiWorldEncounterSessionSubsystem::RegisterConnectedParticipant(
    AJumanjiPlayerState* PlayerState)
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || ActiveEncounterId.IsNone())
    {
        return false;
    }

    const FString ProfileId = PlayerState->GetPersistentProfileId();
    if (!IsValidProfileKey(ProfileId)
        || DisconnectedAtUtc.Contains(ProfileId))
    {
        return false;
    }

    ParticipantProfileIds.Add(ProfileId);
    ConnectedProfileIds.Add(ProfileId);
    return true;
}

bool UJumanjiWorldEncounterSessionSubsystem::MarkPlayerDisconnected(
    const AJumanjiPlayerState* PlayerState)
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || ActiveEncounterId.IsNone())
    {
        return false;
    }

    const FString ProfileId = PlayerState->GetPersistentProfileId();
    if (!IsValidProfileKey(ProfileId)
        || !ParticipantProfileIds.Contains(ProfileId)
        || !ConnectedProfileIds.Contains(ProfileId))
    {
        return false;
    }

    ConnectedProfileIds.Remove(ProfileId);
    DisconnectedAtUtc.Add(ProfileId, FDateTime::UtcNow());
    return true;
}

bool UJumanjiWorldEncounterSessionSubsystem::TryRejoinPlayer(
    AJumanjiPlayerState* RejoiningPlayer,
    FJumanjiEncounterReconnectState& OutReconnectState)
{
    OutReconnectState = FJumanjiEncounterReconnectState();

    if (!IsValid(RejoiningPlayer)
        || !RejoiningPlayer->HasAuthority()
        || ActiveEncounterId.IsNone())
    {
        return false;
    }

    const FString ProfileId = RejoiningPlayer->GetPersistentProfileId();
    if (!IsValidProfileKey(ProfileId)
        || !ParticipantProfileIds.Contains(ProfileId)
        || ConnectedProfileIds.Contains(ProfileId))
    {
        return false;
    }

    const FDateTime* DisconnectedAt = DisconnectedAtUtc.Find(ProfileId);
    if (!DisconnectedAt)
    {
        return false;
    }

    const double ElapsedSeconds =
        (FDateTime::UtcNow() - *DisconnectedAt).GetTotalSeconds();
    const float SafeGraceWindow = FMath::Clamp(
        ReconnectGraceWindowSeconds,
        30.0f,
        1800.0f
    );

    if (!FMath::IsFinite(ElapsedSeconds)
        || ElapsedSeconds < 0.0
        || ElapsedSeconds > SafeGraceWindow)
    {
        DisconnectedAtUtc.Remove(ProfileId);
        ParticipantProfileIds.Remove(ProfileId);
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
    const AJumanjiGameState* GameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    if (!GameState)
    {
        return false;
    }

    const FJumanjiWorldEncounterState EncounterState =
        GameState->GetWorldEncounterState();

    const bool bEncounterAcceptsReconnect =
        EncounterState.EncounterId == ActiveEncounterId
        && (EncounterState.Phase == EJumanjiWorldEncounterPhase::Trials
            || EncounterState.Phase == EJumanjiWorldEncounterPhase::Combat
            || EncounterState.Phase == EJumanjiWorldEncounterPhase::Boss
            || EncounterState.Phase == EJumanjiWorldEncounterPhase::Reward);

    if (!bEncounterAcceptsReconnect)
    {
        return false;
    }

    DisconnectedAtUtc.Remove(ProfileId);
    ConnectedProfileIds.Add(ProfileId);

    ReconnectRevision = ReconnectRevision < MAX_int32
        ? ReconnectRevision + 1
        : 1;

    OutReconnectState.bWasReconnected = true;
    OutReconnectState.EncounterId = EncounterState.EncounterId;
    OutReconnectState.Phase = EncounterState.Phase;
    OutReconnectState.EncounterRevision = EncounterState.Revision;
    OutReconnectState.ReconnectRevision = ReconnectRevision;

    return true;
}

void UJumanjiWorldEncounterSessionSubsystem::EndEncounterSession(FName EncounterId)
{
    if (EncounterId.IsNone() || ActiveEncounterId != EncounterId)
    {
        return;
    }

    ActiveEncounterId = NAME_None;
    ParticipantProfileIds.Reset();
    ConnectedProfileIds.Reset();
    DisconnectedAtUtc.Reset();
    ReconnectRevision = 0;
}
