#include "JumanjiEmberSequenceTrial.h"

AJumanjiEmberSequenceTrial::AJumanjiEmberSequenceTrial()
{
    ObjectiveId = TEXT("TRIAL_EMBER_SEQUENCE");
    TrialDisplayName = FText::FromString(TEXT("Próba Żaru"));
    TrialDescription = FText::FromString(TEXT("Rozpal pradawny płomień w kolejności: żar, płomień i latarnia."));

    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("EMBER_STEP_SPARK");
        Step.Prompt = FText::FromString(TEXT("Wybierz najmniejszy znak początku ognia."));
        Step.OptionIds = { TEXT("EMBER_SPARK"), TEXT("EMBER_ASH"), TEXT("EMBER_SMOKE") };
        Step.CorrectOptionId = TEXT("EMBER_ASH");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("EMBER_STEP_FLAME");
        Step.Prompt = FText::FromString(TEXT("Wskaż znak rozwiniętego płomienia."));
        Step.OptionIds = { TEXT("EMBER_TORCH"), TEXT("EMBER_FLAME"), TEXT("EMBER_COAL") };
        Step.CorrectOptionId = TEXT("EMBER_FLAME");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("EMBER_STEP_BEACON");
        Step.Prompt = FText::FromString(TEXT("Zamknij sekwencję symbolem ognia prowadzącego drogę."));
        Step.OptionIds = { TEXT("EMBER_CINDER"), TEXT("EMBER_LANTERN"), TEXT("EMBER_WICK") };
        Step.CorrectOptionId = TEXT("EMBER_LANTERN");
        Steps.Add(MoveTemp(Step));
    }
}
