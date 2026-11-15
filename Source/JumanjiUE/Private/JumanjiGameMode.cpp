#include "JumanjiGameMode.h"

#include "JumanjiCharacter.h"
#include "JumanjiGameState.h"

AJumanjiGameMode::AJumanjiGameMode()
{
    DefaultPawnClass = AJumanjiCharacter::StaticClass();
    GameStateClass = AJumanjiGameState::StaticClass();
}
