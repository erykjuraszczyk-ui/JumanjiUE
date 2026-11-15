#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiFrostGateTrial.generated.h"

/** Native three-step environmental trial for Próba Mrozu. Visual assets are assigned in a Blueprint child. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiFrostGateTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiFrostGateTrial();
};
