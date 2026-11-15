#include "JumanjiShadowPatternTrial.h"

AJumanjiShadowPatternTrial::AJumanjiShadowPatternTrial()
{
    ObjectiveId = TEXT("TRIAL_SHADOW_PATTERN");
    TrialDisplayName = FText::FromString(TEXT("Próba Cienia"));
    TrialDescription = FText::FromString(TEXT("Odczytaj wzór cienia: półmrok, zaćmienie i powrót światła."));

    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("SHADOW_STEP_SHADE");
        Step.Prompt = FText::FromString(TEXT("Wybierz znak łagodnego półmroku."));
        Step.OptionIds = { TEXT("SHADOW_SHADE"), TEXT("SHADOW_SUN"), TEXT("SHADOW_FLARE") };
        Step.CorrectOptionId = TEXT("SHADOW_SUN");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("SHADOW_STEP_ECLIPSE");
        Step.Prompt = FText::FromString(TEXT("Wskaż symbol pełnego zaćmienia."));
        Step.OptionIds = { TEXT("SHADOW_MIST"), TEXT("SHADOW_ECLIPSE"), TEXT("SHADOW_DAWN") };
        Step.CorrectOptionId = TEXT("SHADOW_ECLIPSE");
        Steps.Add(MoveTemp(Step));
    }
    {
        FJumanjiTrialStepDefinition Step;
        Step.StepId = TEXT("SHADOW_STEP_RETURN");
        Step.Prompt = FText::FromString(TEXT("Zamknij wzór znakiem światła powracającego zza cienia."));
        Step.OptionIds = { TEXT("SHADOW_VOID"), TEXT("SHADOW_RETURN"), TEXT("SHADOW_NIGHT") };
        Step.CorrectOptionId = TEXT("SHADOW_RETURN");
        Steps.Add(MoveTemp(Step));
    }
}
