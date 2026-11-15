#include "JumanjiDawnMirrorTrial.h"

AJumanjiDawnMirrorTrial::AJumanjiDawnMirrorTrial()
{
    ObjectiveId = TEXT("TRIAL_DAWN_MIRRORS");
    TrialDisplayName = FText::FromString(TEXT("Próba Świtu"));
    TrialDescription = FText::FromString(TEXT("Skieruj światło przez kryształ, lustro i bramę świtu."));

    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("DAWN_STEP_PRISM");
        Step.Prompt = FText::FromString(TEXT("Wybierz znak, który rozszczepia światło."));
        Step.OptionIds = { TEXT("DAWN_PRISM"), TEXT("DAWN_MOON"), TEXT("DAWN_VEIL") };
        Step.CorrectOptionId = TEXT("DAWN_MOON");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("DAWN_STEP_MIRROR");
        Step.Prompt = FText::FromString(TEXT("Wskaż powierzchnię odbijającą promień."));
        Step.OptionIds = { TEXT("DAWN_LENS"), TEXT("DAWN_MIRROR"), TEXT("DAWN_CLOUD") };
        Step.CorrectOptionId = TEXT("DAWN_MIRROR");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("DAWN_STEP_GATE");
        Step.Prompt = FText::FromString(TEXT("Wybierz symbol otwarcia świetlnej bramy."));
        Step.OptionIds = { TEXT("DAWN_STAR"), TEXT("DAWN_GATE"), TEXT("DAWN_DUSK") };
        Step.CorrectOptionId = TEXT("DAWN_GATE");
        Steps.Add(MoveTemp(Step));
    }
}
