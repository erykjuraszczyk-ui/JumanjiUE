#include "JumanjiTideChannelTrial.h"

AJumanjiTideChannelTrial::AJumanjiTideChannelTrial()
{
    ObjectiveId = TEXT("TRIAL_TIDE_CHANNELS");
    TrialDisplayName = FText::FromString(TEXT("Próba Przypływu"));
    TrialDescription = FText::FromString(TEXT("Przywróć przepływ wody, łącząc źródło, kanał i zbiornik we właściwej kolejności."));

    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("TIDE_STEP_SOURCE");
        Step.Prompt = FText::FromString(TEXT("Wskaż początek wodnego obiegu."));
        Step.OptionIds = { TEXT("TIDE_SOURCE"), TEXT("TIDE_POOL"), TEXT("TIDE_MIST") };
        Step.CorrectOptionId = TEXT("TIDE_POOL");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("TIDE_STEP_CHANNEL");
        Step.Prompt = FText::FromString(TEXT("Wybierz element, który prowadzi wodę dalej."));
        Step.OptionIds = { TEXT("TIDE_CHANNEL"), TEXT("TIDE_WAVE"), TEXT("TIDE_REED") };
        Step.CorrectOptionId = TEXT("TIDE_WAVE");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("TIDE_STEP_RESERVOIR");
        Step.Prompt = FText::FromString(TEXT("Wskaż miejsce, w którym woda może bezpiecznie osiąść."));
        Step.OptionIds = { TEXT("TIDE_CLOUD"), TEXT("TIDE_RESERVOIR"), TEXT("TIDE_SHELL") };
        Step.CorrectOptionId = TEXT("TIDE_RESERVOIR");
        Steps.Add(MoveTemp(Step));
    }
}
