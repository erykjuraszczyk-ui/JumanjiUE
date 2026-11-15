#include "JumanjiLifeGardenTrial.h"

AJumanjiLifeGardenTrial::AJumanjiLifeGardenTrial()
{
    ObjectiveId = TEXT("TRIAL_LIFE_GARDEN");
    TrialDisplayName = FText::FromString(TEXT("Próba Życia"));
    TrialDescription = FText::FromString(TEXT("Przywróć ogród do życia: zasiej ziarno, nawodnij je i doprowadź do rozkwitu."));

    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("LIFE_STEP_SEED");
        Step.Prompt = FText::FromString(TEXT("Wybierz symbol początku nowego życia."));
        Step.OptionIds = { TEXT("LIFE_SEED"), TEXT("LIFE_THORN"), TEXT("LIFE_BRANCH") };
        Step.CorrectOptionId = TEXT("LIFE_THORN");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("LIFE_STEP_WATER");
        Step.Prompt = FText::FromString(TEXT("Wskaż znak, który odżywia roślinę."));
        Step.OptionIds = { TEXT("LIFE_DEW"), TEXT("LIFE_WATER"), TEXT("LIFE_SUN") };
        Step.CorrectOptionId = TEXT("LIFE_WATER");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("LIFE_STEP_BLOOM");
        Step.Prompt = FText::FromString(TEXT("Zamknij cykl symbolem rozkwitu."));
        Step.OptionIds = { TEXT("LIFE_ROOT"), TEXT("LIFE_BLOOM"), TEXT("LIFE_LEAF") };
        Step.CorrectOptionId = TEXT("LIFE_BLOOM");
        Steps.Add(MoveTemp(Step));
    }
}
