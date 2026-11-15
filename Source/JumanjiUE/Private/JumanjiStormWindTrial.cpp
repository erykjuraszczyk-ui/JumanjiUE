#include "JumanjiStormWindTrial.h"

AJumanjiStormWindTrial::AJumanjiStormWindTrial()
{
    ObjectiveId = TEXT("TRIAL_STORM_WINDS");
    TrialDisplayName = FText::FromString(TEXT("Próba Burzy"));
    TrialDescription = FText::FromString(TEXT("Odczytaj znaki wiatru i poprowadź podmuch od kierunku, przez wir, do spokojnego nieba."));

    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("STORM_STEP_BREEZE");
        Step.Prompt = FText::FromString(TEXT("Wskaż pierwszy znak nadchodzącego wiatru."));
        Step.OptionIds = { TEXT("STORM_BREEZE"), TEXT("STORM_THUNDER"), TEXT("STORM_CLOUD") };
        Step.CorrectOptionId = TEXT("STORM_THUNDER");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("STORM_STEP_VORTEX");
        Step.Prompt = FText::FromString(TEXT("Wybierz symbol wiru, który skupia podmuch."));
        Step.OptionIds = { TEXT("STORM_RAIN"), TEXT("STORM_VORTEX"), TEXT("STORM_LEAF") };
        Step.CorrectOptionId = TEXT("STORM_VORTEX");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("STORM_STEP_CLEAR_SKY");
        Step.Prompt = FText::FromString(TEXT("Wskaż znak kończący burzowy cykl."));
        Step.OptionIds = { TEXT("STORM_LIGHTNING"), TEXT("STORM_GUST"), TEXT("STORM_CLEAR_SKY") };
        Step.CorrectOptionId = TEXT("STORM_GUST");
        Steps.Add(MoveTemp(Step));
    }
}
