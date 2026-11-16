#include "JumanjiGameMode.h"

#include "JumanjiCharacter.h"
#include "JumanjiGameState.h"
#include "JumanjiPlayerController.h"
#include "JumanjiPlayerProfileSaveGame.h"
#include "JumanjiPlayerProfileSubsystem.h"
#include "JumanjiPlayerState.h"
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
        ProfileId = UniqueId.ToString();
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
            UE_LOG(LogTemp, Warning,
                TEXT("JUMANJI profile: loaded profile '%s' could not be applied; keeping safe defaults."),
                *ProfileId);
        }
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("JUMANJI profile: no valid previous save for profile '%s'; starting with defaults."),
            *ProfileId);
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
                JumanjiPlayerState->SaveProfileNow();
            }
        }
    }

    Super::Logout(Exiting);
}
