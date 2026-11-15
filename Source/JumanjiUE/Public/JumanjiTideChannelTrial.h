#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiTideChannelTrial.generated.h"

/** Native three-step environmental trial for Próba Przypływu. Visual assets are assigned in a Blueprint child. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiTideChannelTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiTideChannelTrial();
};
