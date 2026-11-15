#include "JumanjiRootRuneTrial.h"

AJumanjiRootRuneTrial::AJumanjiRootRuneTrial()
{
    FJumanjiTrialStepDefinition SeedStep;
    SeedStep.StepId = TEXT("ROOT_STEP_SEED");
    SeedStep.Prompt = FText::FromString(TEXT("Wybierz runę początku życia."));
    SeedStep.OptionIds.Add(TEXT("RUNE_SEED"));
    SeedStep.OptionIds.Add(TEXT("RUNE_VINE"));
    SeedStep.OptionIds.Add(TEXT("RUNE_STONE"));
    SeedStep.CorrectOptionId = TEXT("RUNE_SEED");
    Steps.Add(MoveTemp(SeedStep));

    FJumanjiTrialStepDefinition SproutStep;
    SproutStep.StepId = TEXT("ROOT_STEP_SPROUT");
    SproutStep.Prompt = FText::FromString(TEXT("Wskaż symbol wzrostu."));
    SproutStep.OptionIds.Add(TEXT("RUNE_LEAF"));
    SproutStep.OptionIds.Add(TEXT("RUNE_SPROUT"));
    SproutStep.OptionIds.Add(TEXT("RUNE_SEED"));
    SproutStep.CorrectOptionId = TEXT("RUNE_SPROUT");
    Steps.Add(MoveTemp(SproutStep));

    FJumanjiTrialStepDefinition TreeStep;
    TreeStep.StepId = TEXT("ROOT_STEP_TREE");
    TreeStep.Prompt = FText::FromString(TEXT("Zamknij sekwencję symbolem dojrzałego drzewa."));
    TreeStep.OptionIds.Add(TEXT("RUNE_TREE"));
    TreeStep.OptionIds.Add(TEXT("RUNE_ROOT"));
    TreeStep.OptionIds.Add(TEXT("RUNE_SPROUT"));
    TreeStep.CorrectOptionId = TEXT("RUNE_TREE");
    Steps.Add(MoveTemp(TreeStep));
}