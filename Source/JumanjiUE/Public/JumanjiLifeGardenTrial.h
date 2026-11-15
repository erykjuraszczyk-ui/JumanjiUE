#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiLifeGardenTrial.generated.h"

/** Native three-step environmental trial for Próba Życia. Visual assets are assigned in a Blueprint child. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiLifeGardenTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiLifeGardenTrial();
};
