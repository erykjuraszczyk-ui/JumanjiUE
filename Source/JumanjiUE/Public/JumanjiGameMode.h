#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JumanjiGameMode.generated.h"

/** Default server-side game rules for the JUMANJI prototype. */
UCLASS()
class JUMANJIUE_API AJumanjiGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AJumanjiGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
};
