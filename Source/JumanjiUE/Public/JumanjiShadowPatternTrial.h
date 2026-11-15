#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiShadowPatternTrial.generated.h"

/** Native three-step environmental trial for Próba Cienia. Visual assets are assigned in a Blueprint child. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiShadowPatternTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiShadowPatternTrial();
};
