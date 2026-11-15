#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiDawnMirrorTrial.generated.h"

/** Native three-step environmental trial for Próba Świtu. Visual assets are assigned in a Blueprint child. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiDawnMirrorTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiDawnMirrorTrial();
};
