#include "JumanjiGameMode.h"

#include "JumanjiCharacter.h"
#include "JumanjiGameState.h"
#include "JumanjiPlayerController.h"
#include "JumanjiPlayerState.h"

AJumanjiGameMode::AJumanjiGameMode()
{
    DefaultPawnClass = AJumanjiCharacter::StaticClass();
    GameStateClass = AJumanjiGameState::StaticClass();
    PlayerControllerClass = AJumanjiPlayerController::StaticClass();
    PlayerStateClass = AJumanjiPlayerState::StaticClass();
}
