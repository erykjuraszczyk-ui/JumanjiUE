#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JumanjiGameUserSettings.h"
#include "JumanjiAbilityHUDSettingsWidget.generated.h"

/**
 * Blueprint base for the local Ability HUD settings screen.
 * UI controls are authored in WBP_AbilityHUDSettings.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiAbilityHUDSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability HUD|Settings")
    FJumanjiAbilityHUDPreferences GetSavedPreferences() const;

    /** Apply, persist and notify the Blueprint screen of the accepted local values. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD|Settings")
    bool ApplyAndSavePreferences(const FJumanjiAbilityHUDPreferences& Preferences);

    /** Resets only the four JUMANJI HUD preferences. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD|Settings")
    bool ResetAndSavePreferences();

protected:
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Ability HUD|Settings")
    void OnPreferencesSaved(const FJumanjiAbilityHUDPreferences& Preferences);
};
