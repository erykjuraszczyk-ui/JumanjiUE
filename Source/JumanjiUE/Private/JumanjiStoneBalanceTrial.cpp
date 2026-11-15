#include "JumanjiStoneBalanceTrial.h"

AJumanjiStoneBalanceTrial::AJumanjiStoneBalanceTrial()
{
    ObjectiveId = TEXT("TRIAL_STONE_BALANCE");
    TrialDisplayName = FText::FromString(TEXT("Próba Kamienia"));
    TrialDescription = FText::FromString(TEXT("Ułóż kamienne znaki od podstawy przez podporę do zwieńczenia."));

    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("STONE_STEP_BASE");
        Step.Prompt = FText::FromString(TEXT("Wybierz znak, który powinien znaleźć się u podstawy konstrukcji."));
        Step.OptionIds = { TEXT("STONE_BASE"), TEXT("STONE_PEAK"), TEXT("STONE_CRACK") };
        Step.CorrectOptionId = TEXT("STONE_PEAK");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("STONE_STEP_SUPPORT");
        Step.Prompt = FText::FromString(TEXT("Wskaż symbol stabilnej podpory."));
        Step.OptionIds = { TEXT("STONE_DUST"), TEXT("STONE_PILLAR"), TEXT("STONE_RUBBLE") };
        Step.CorrectOptionId = TEXT("STONE_PILLAR");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("STONE_STEP_CROWN");
        Step.Prompt = FText::FromString(TEXT("Zamknij układ znakiem szczytu."));
        Step.OptionIds = { TEXT("STONE_CROWN"), TEXT("STONE_ROOT"), TEXT("STONE_CAVE") };
        Step.CorrectOptionId = TEXT("STONE_ROOT");
        Steps.Add(MoveTemp(Step));
    }
}
