#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiStormWindTrial.generated.h"

/** Native three-step environmental trial for Próba Burzy. Visual assets are assigned in a Blueprint child. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiStormWindTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiStormWindTrial();
};
