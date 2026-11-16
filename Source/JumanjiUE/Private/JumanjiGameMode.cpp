#include "JumanjiGameMode.h"

#include "JumanjiCharacter.h"
#include "JumanjiGameState.h"
#include "JumanjiPlayerController.h"

AJumanjiGameMode::AJumanjiGameMode()
{
    DefaultPawnClass = AJumanjiCharacter::StaticClass();
    PlayerControllerClass = AJumanjiPlayerController::StaticClass();
    GameStateClass = AJumanjiGameState::StaticClass();
}
