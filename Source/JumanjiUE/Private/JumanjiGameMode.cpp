#include "JumanjiGameMode.h"

#include "JumanjiCharacter.h"
#include "JumanjiGameState.h"
#include "JumanjiPlayerController.h"
#include "JumanjiPlayerProfileSaveGame.h"
#include "JumanjiPlayerProfileSubsystem.h"
#include "JumanjiPlayerState.h"
#include "JumanjiProgressionRewardSubsystem.h"
#include "JumanjiWorldEncounterSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/OnlineReplStructs.h"

AJumanjiGameMode::AJumanjiGameMode()
{
    DefaultPawnClass = AJumanjiCharacter::StaticClass();
    GameStateClass = AJumanjiGameState::StaticClass();
    PlayerControllerClass = AJumanjiPlayerController::StaticClass();
    PlayerStateClass = AJumanjiPlayerState::StaticClass();
}

void AJumanjiGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (!HasAuthority() || !IsValid(NewPlayer))
    {
        return;
    }

    AJumanjiPlayerState* JumanjiPlayerState =
        NewPlayer->GetPlayerState<AJumanjiPlayerState>();

    if (!JumanjiPlayerState)
    {
        return;
    }

    FString ProfileId;
    const FUniqueNetIdRepl& UniqueId = JumanjiPlayerState->GetUniqueId();

    if (UniqueId.IsValid())
    {
        ProfileId = FString::Printf(TEXT("%s:%s"), *UniqueId.GetType().ToString(), *UniqueId.ToString());
    }
    else if (GetNetMode() == NM_Standalone)
    {
        // One local profile for the standalone, offline prototype.
        ProfileId = TEXT("LOCAL_SINGLEPLAYER");
    }
    else if (GetNetMode() == NM_ListenServer && NewPlayer->IsLocalController())
    {
        // The listen-server host may use a device-local profile.
        // Remote players without a verified platform ID are not persisted.
        ProfileId = TEXT("LOCAL_LISTEN_HOST");
    }

    if (ProfileId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI profile: no stable platform identity; profile persistence is disabled for this session."));
        return;
    }

    JumanjiPlayerState->SetPersistentProfileId(ProfileId);

    UGameInstance* GameInstance = GetGameInstance();
    UJumanjiPlayerProfileSubsystem* ProfileSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiPlayerProfileSubsystem>()
        : nullptr;

    if (!ProfileSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("JUMANJI profile: profile subsystem is unavailable at PostLogin."));
        return;
    }

    FJumanjiPlayerProfileSnapshot Snapshot;
    if (ProfileSubsystem->LoadProfile(ProfileId, Snapshot))
    {
        if (!JumanjiPlayerState->ApplyProfileSnapshot(Snapshot))
        {
            UE_LOG(LogTemp, Error,
                TEXT("JUMANJI profile: loaded profile '%s' could not be applied; preserving the existing slot."),
                *ProfileId);
            JumanjiPlayerState->DisableProfilePersistenceForSession();
            JumanjiPlayerState->InitializeNewProfileProgression();
        }
    }
    else if (ProfileSubsystem->HasProfile(ProfileId))
    {
        // A slot exists but is unreadable/unsupported. Do not overwrite it
        // with a new-player snapshot; keep it available for recovery.
        UE_LOG(LogTemp, Error,
            TEXT("JUMANJI profile: existing profile '%s' is invalid; persistence is disabled for this session."),
            *ProfileId);
        JumanjiPlayerState->DisableProfilePersistenceForSession();
        JumanjiPlayerState->InitializeNewProfileProgression();
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("JUMANJI profile: no previous save for profile '%s'; creating starter progression."),
            *ProfileId);
        JumanjiPlayerState->InitializeNewProfileProgression();
        JumanjiPlayerState->RequestProfileSave();
    }

    // Restore only volatile participation in an encounter that is still active
    // on this same server process. The profile snapshot contains progression,
    // never live encounter actors or phase state.
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UJumanjiWorldEncounterSessionSubsystem* SessionSubsystem =
            GameInstance->GetSubsystem<UJumanjiWorldEncounterSessionSubsystem>())
        {
            FJumanjiEncounterReconnectState ReconnectState;
            if (SessionSubsystem->TryRejoinPlayer(JumanjiPlayerState, ReconnectState)
                && JumanjiPlayerState->SetEncounterReconnectState(ReconnectState))
            {
                UE_LOG(LogTemp, Log,
                    TEXT("JUMANJI encounter: profile '%s' rejoined encounter '%s' at revision %d."),
                    *ProfileId,
                    *ReconnectState.EncounterId.ToString(),
                    ReconnectState.EncounterRevision);

                // If the participant reconnects during Reward, the director may
                // already have iterated GameState::PlayerArray. Use the same
                // idempotent completion transaction to catch up this profile.
                if (ReconnectState.Phase == EJumanjiWorldEncounterPhase::Reward)
                {
                    if (UJumanjiProgressionRewardSubsystem* RewardSubsystem =
                        GameInstance->GetSubsystem<UJumanjiProgressionRewardSubsystem>())
                    {
                        if (!RewardSubsystem->GrantWorldEncounterCompletionReward(
                            JumanjiPlayerState,
                            ReconnectState.EncounterId))
                        {
                            UE_LOG(LogTemp, Warning,
                                TEXT("JUMANJI encounter: reconnect reward was not applied for profile '%s' in encounter '%s'."),
                                *ProfileId,
                                *ReconnectState.EncounterId.ToString());
                        }
                    }
                }
            }
        }
    }
}

void AJumanjiGameMode::Logout(AController* Exiting)
{
    if (HasAuthority())
    {
        if (APlayerController* PlayerController = Cast<APlayerController>(Exiting))
        {
            if (AJumanjiPlayerState* JumanjiPlayerState =
                PlayerController->GetPlayerState<AJumanjiPlayerState>())
            {
                if (UGameInstance* GameInstance = GetGameInstance())
                {
                    if (UJumanjiWorldEncounterSessionSubsystem* SessionSubsystem =
                        GameInstance->GetSubsystem<UJumanjiWorldEncounterSessionSubsystem>())
                    {
                        SessionSubsystem->MarkPlayerDisconnected(JumanjiPlayerState);
                    }
                }

                JumanjiPlayerState->RequestProfileSave();
            }
        }
    }

    Super::Logout(Exiting);
}
