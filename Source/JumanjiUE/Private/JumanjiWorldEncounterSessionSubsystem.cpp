#include "JumanjiWorldEncounterSessionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "JumanjiGameState.h"
#include "JumanjiPlayerState.h"

void UJumanjiWorldEncounterSessionSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ActiveEncounterId = NAME_None;
    ActiveSessionId.Invalidate();
    ParticipantProfileIds.Reset();
    ConnectedProfileIds.Reset();
    DisconnectedAtUtc.Reset();
    ReconnectRevision = 0;
    RecentlyEndedEncounterIds.Reset();
    RecentlyEndedAtUtc.Reset();
    RecentlyEndedPhases.Reset();
}

void UJumanjiWorldEncounterSessionSubsystem::Deinitialize()
{
    ActiveEncounterId = NAME_None;
    ActiveSessionId.Invalidate();
    ParticipantProfileIds.Reset();
    ConnectedProfileIds.Reset();
    DisconnectedAtUtc.Reset();
    ReconnectRevision = 0;
    RecentlyEndedEncounterIds.Reset();
    RecentlyEndedAtUtc.Reset();
    RecentlyEndedPhases.Reset();
    Super::Deinitialize();
}

bool UJumanjiWorldEncounterSessionSubsystem::IsValidProfileKey(
    const FString& ProfileId) const
{
    return !ProfileId.TrimStartAndEnd().IsEmpty() && ProfileId.Len() <= 256;
}

void UJumanjiWorldEncounterSessionSubsystem::PruneRecentlyEndedParticipants()
{
    const double SafeRetention = FMath::Clamp(
        TerminalNoticeRetentionSeconds,
        60.0f,
        3600.0f
    );
    const FDateTime Now = FDateTime::UtcNow();

    TArray<FString> ExpiredProfiles;
    for (const TPair<FString, FDateTime>& Entry : RecentlyEndedAtUtc)
    {
        const double AgeSeconds = (Now - Entry.Value).GetTotalSeconds();
        if (!FMath::IsFinite(AgeSeconds) || AgeSeconds < 0.0 || AgeSeconds > SafeRetention)
        {
            ExpiredProfiles.Add(Entry.Key);
        }
    }

    for (const FString& ProfileId : ExpiredProfiles)
    {
        RecentlyEndedAtUtc.Remove(ProfileId);
        RecentlyEndedEncounterIds.Remove(ProfileId);
        RecentlyEndedPhases.Remove(ProfileId);
    }

    const int32 SafeMaxEntries = FMath::Clamp(MaxTerminalNoticeEntries, 16, 512);
    while (RecentlyEndedAtUtc.Num() > SafeMaxEntries)
    {
        FString OldestProfileId;
        FDateTime OldestTime = FDateTime::MaxValue();

        for (const TPair<FString, FDateTime>& Entry : RecentlyEndedAtUtc)
        {
            if (Entry.Value < OldestTime)
            {
                OldestTime = Entry.Value;
                OldestProfileId = Entry.Key;
            }
        }

        if (OldestProfileId.IsEmpty())
        {
            break;
        }

        RecentlyEndedAtUtc.Remove(OldestProfileId);
        RecentlyEndedEncounterIds.Remove(OldestProfileId);
        RecentlyEndedPhases.Remove(OldestProfileId);
    }
}

void UJumanjiWorldEncounterSessionSubsystem::StoreFeedbackRevision(
    FJumanjiEncounterReconnectFeedbackState& OutFeedbackState)
{
    ReconnectRevision = ReconnectRevision < MAX_int32
        ? ReconnectRevision + 1
        : 1;
    OutFeedbackState.Revision = ReconnectRevision;
    OutFeedbackState.DisplayDurationSeconds = 6.0f;
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

    PruneRecentlyEndedParticipants();

    ActiveEncounterId = EncounterId;
    ActiveSessionId = FGuid::NewGuid();
    ParticipantProfileIds.Reset();
    ConnectedProfileIds.Reset();
    DisconnectedAtUtc.Reset();
    ReconnectRevision = 0;

    const FString ProfileId = InitiatingPlayer->GetPersistentProfileId();
    RecentlyEndedEncounterIds.Remove(ProfileId);
    RecentlyEndedAtUtc.Remove(ProfileId);
    RecentlyEndedPhases.Remove(ProfileId);
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

    RecentlyEndedEncounterIds.Remove(ProfileId);
    RecentlyEndedAtUtc.Remove(ProfileId);
    RecentlyEndedPhases.Remove(ProfileId);
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
    FJumanjiEncounterReconnectFeedbackState IgnoredFeedback;
    return TryRejoinPlayer(RejoiningPlayer, OutReconnectState, IgnoredFeedback);
}

bool UJumanjiWorldEncounterSessionSubsystem::TryRejoinPlayer(
    AJumanjiPlayerState* RejoiningPlayer,
    FJumanjiEncounterReconnectState& OutReconnectState,
    FJumanjiEncounterReconnectFeedbackState& OutFeedbackState)
{
    OutReconnectState = FJumanjiEncounterReconnectState();
    OutFeedbackState = FJumanjiEncounterReconnectFeedbackState();

    if (!IsValid(RejoiningPlayer)
        || !RejoiningPlayer->HasAuthority())
    {
        return false;
    }

    const FString ProfileId = RejoiningPlayer->GetPersistentProfileId();
    if (!IsValidProfileKey(ProfileId))
    {
        return false;
    }

    PruneRecentlyEndedParticipants();

    const float SafeGraceWindow = FMath::Clamp(
        ReconnectGraceWindowSeconds,
        30.0f,
        1800.0f
    );

    // A terminal encounter clears the active registry. Its short-lived
    // tombstone allows only a previously disconnected participant to receive
    // an informative "encounter ended" notice; it never grants re-entry.
    if (ActiveEncounterId.IsNone())
    {
        const FName* EndedEncounterId = RecentlyEndedEncounterIds.Find(ProfileId);
        const FDateTime* EndedAtUtc = RecentlyEndedAtUtc.Find(ProfileId);
        const EJumanjiWorldEncounterPhase* EndedPhase = RecentlyEndedPhases.Find(ProfileId);

        if (!EndedEncounterId || !EndedAtUtc || !EndedPhase)
        {
            return false;
        }

        const double SinceEndedSeconds = (FDateTime::UtcNow() - *EndedAtUtc).GetTotalSeconds();
        OutFeedbackState.EncounterId = *EndedEncounterId;

        if (FMath::IsFinite(SinceEndedSeconds) && SinceEndedSeconds >= 0.0
            && SinceEndedSeconds <= FMath::Clamp(TerminalNoticeRetentionSeconds, 60.0f, 3600.0f))
        {
            OutFeedbackState.Outcome = EJumanjiEncounterReconnectOutcome::EncounterEnded;
            OutFeedbackState.Phase = *EndedPhase;
        }
        else
        {
            OutFeedbackState.Outcome = EJumanjiEncounterReconnectOutcome::GraceWindowExpired;
            OutFeedbackState.Phase = *EndedPhase;
        }

        StoreFeedbackRevision(OutFeedbackState);
        RecentlyEndedEncounterIds.Remove(ProfileId);
        RecentlyEndedAtUtc.Remove(ProfileId);
        RecentlyEndedPhases.Remove(ProfileId);
        return false;
    }

    if (!ParticipantProfileIds.Contains(ProfileId)
        || ConnectedProfileIds.Contains(ProfileId))
    {
        // Ordinary new logins and duplicate connected sessions are silent.
        return false;
    }

    const FDateTime* DisconnectedAt = DisconnectedAtUtc.Find(ProfileId);
    if (!DisconnectedAt)
    {
        return false;
    }

    const double ElapsedSeconds =
        (FDateTime::UtcNow() - *DisconnectedAt).GetTotalSeconds();

    if (!FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds < 0.0
        || ElapsedSeconds > SafeGraceWindow)
    {
        OutFeedbackState.Outcome = EJumanjiEncounterReconnectOutcome::GraceWindowExpired;
        OutFeedbackState.EncounterId = ActiveEncounterId;
        if (const UGameInstance* GameInstance = GetGameInstance())
        {
            if (const UWorld* World = GameInstance->GetWorld())
            {
                if (const AJumanjiGameState* GameState = World->GetGameState<AJumanjiGameState>())
                {
                    OutFeedbackState.Phase = GameState->GetWorldEncounterState().Phase;
                }
            }
        }
        StoreFeedbackRevision(OutFeedbackState);

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
        OutFeedbackState.Outcome = EJumanjiEncounterReconnectOutcome::EncounterEnded;
        OutFeedbackState.EncounterId = ActiveEncounterId;
        OutFeedbackState.Phase = EncounterState.Phase;
        const FName EndedEncounterId = ActiveEncounterId;
        EndEncounterSession(EndedEncounterId);
        RecentlyEndedEncounterIds.Remove(ProfileId);
        RecentlyEndedAtUtc.Remove(ProfileId);
        RecentlyEndedPhases.Remove(ProfileId);
        StoreFeedbackRevision(OutFeedbackState);
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

    OutFeedbackState.Outcome = EJumanjiEncounterReconnectOutcome::Reconnected;
    OutFeedbackState.EncounterId = EncounterState.EncounterId;
    OutFeedbackState.Phase = EncounterState.Phase;
    OutFeedbackState.Revision = ReconnectRevision;
    OutFeedbackState.DisplayDurationSeconds = 6.0f;
    return true;
}

void UJumanjiWorldEncounterSessionSubsystem::EndEncounterSession(FName EncounterId)
{
    if (EncounterId.IsNone() || ActiveEncounterId != EncounterId)
    {
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
    const AJumanjiGameState* GameState = World
        ? World->GetGameState<AJumanjiGameState>()
        : nullptr;
    const EJumanjiWorldEncounterPhase EndedPhase = GameState
        ? GameState->GetWorldEncounterState().Phase
        : EJumanjiWorldEncounterPhase::Inactive;

    const FDateTime EndedAtUtc = FDateTime::UtcNow();
    for (const TPair<FString, FDateTime>& DisconnectedEntry : DisconnectedAtUtc)
    {
        RecentlyEndedEncounterIds.Add(DisconnectedEntry.Key, EncounterId);
        RecentlyEndedAtUtc.Add(DisconnectedEntry.Key, EndedAtUtc);
        RecentlyEndedPhases.Add(DisconnectedEntry.Key, EndedPhase);
    }

    ActiveEncounterId = NAME_None;
    ActiveSessionId.Invalidate();
    ParticipantProfileIds.Reset();
    ConnectedProfileIds.Reset();
    DisconnectedAtUtc.Reset();
    ReconnectRevision = 0;
    PruneRecentlyEndedParticipants();
}
