#pragma once

#include "CoreMinimal.h"
#include "JumanjiTrialActor.h"
#include "JumanjiRootRuneTrial.generated.h"

/**
 * First playable Emerald Jungle trial: identify the Seed, Sprout and Tree
 * runes in sequence. Visuals and interaction widgets are authored in Blueprints.
 */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiRootRuneTrial : public AJumanjiTrialActor
{
    GENERATED_BODY()

public:
    AJumanjiRootRuneTrial();
};