#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiEmberSequenceTrial.generated.h"

/** Native three-step environmental trial for Próba Żaru. Visual assets are assigned in a Blueprint child. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiEmberSequenceTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiEmberSequenceTrial();
};
