#include "JumanjiFrostGateTrial.h"

AJumanjiFrostGateTrial::AJumanjiFrostGateTrial()
{
    ObjectiveId = TEXT("TRIAL_FROST_GATES");
    TrialDisplayName = FText::FromString(TEXT("Próba Mrozu"));
    TrialDescription = FText::FromString(TEXT("Otwórz lodową bramę, przeprowadzając wodę przez mróz do kryształu."));

    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("FROST_STEP_WATER");
        Step.Prompt = FText::FromString(TEXT("Wybierz znak wody przed zamarznięciem."));
        Step.OptionIds = { TEXT("FROST_WATER"), TEXT("FROST_SNOW"), TEXT("FROST_SHARD") };
        Step.CorrectOptionId = TEXT("FROST_SNOW");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("FROST_STEP_FREEZE");
        Step.Prompt = FText::FromString(TEXT("Wskaż symbol przemiany w lód."));
        Step.OptionIds = { TEXT("FROST_MIST"), TEXT("FROST_FREEZE"), TEXT("FROST_ICEBERG") };
        Step.CorrectOptionId = TEXT("FROST_FREEZE");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("FROST_STEP_CRYSTAL");
        Step.Prompt = FText::FromString(TEXT("Zamknij sekwencję znakiem lodowego kryształu."));
        Step.OptionIds = { TEXT("FROST_GLACIER"), TEXT("FROST_CRYSTAL"), TEXT("FROST_FLAKE") };
        Step.CorrectOptionId = TEXT("FROST_CRYSTAL");
        Steps.Add(MoveTemp(Step));
    }
}
