#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiStoneBalanceTrial.generated.h"

/** Native three-step environmental trial for Próba Kamienia. Visual assets are assigned in a Blueprint child. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiStoneBalanceTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiStoneBalanceTrial();
};
