#include "JumanjiAbilityHUDSettingsWidget.h"

#include "Engine/Engine.h"
#include "JumanjiPlayerController.h"

FJumanjiAbilityHUDPreferences UJumanjiAbilityHUDSettingsWidget::GetSavedPreferences() const
{
    if (!GEngine)
    {
        return FJumanjiAbilityHUDPreferences();
    }

    const UJumanjiGameUserSettings* Settings =
        Cast<UJumanjiGameUserSettings>(GEngine->GetGameUserSettings());

    return Settings
        ? Settings->GetAbilityHUDPreferences()
        : FJumanjiAbilityHUDPreferences();
}

bool UJumanjiAbilityHUDSettingsWidget::ApplyAndSavePreferences(
    const FJumanjiAbilityHUDPreferences& Preferences)
{
    if (!GEngine)
    {
        return false;
    }

    UJumanjiGameUserSettings* Settings =
        Cast<UJumanjiGameUserSettings>(GEngine->GetGameUserSettings());
    if (!Settings)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI HUD settings: custom GameUserSettings class is not active."));
        return false;
    }

    Settings->SetAbilityHUDPreferences(Preferences);
    const FJumanjiAbilityHUDPreferences SavedPreferences = Settings->GetAbilityHUDPreferences();

    if (AJumanjiPlayerController* PlayerController =
        Cast<AJumanjiPlayerController>(GetOwningPlayer()))
    {
        PlayerController->ApplySavedAbilityHUDPreferences();
    }

    OnPreferencesSaved(SavedPreferences);
    return true;
}

bool UJumanjiAbilityHUDSettingsWidget::ResetAndSavePreferences()
{
    if (!GEngine)
    {
        return false;
    }

    UJumanjiGameUserSettings* Settings =
        Cast<UJumanjiGameUserSettings>(GEngine->GetGameUserSettings());
    if (!Settings)
    {
        return false;
    }

    Settings->ResetAbilityHUDPreferences();
    const FJumanjiAbilityHUDPreferences SavedPreferences = Settings->GetAbilityHUDPreferences();

    if (AJumanjiPlayerController* PlayerController =
        Cast<AJumanjiPlayerController>(GetOwningPlayer()))
    {
        PlayerController->ApplySavedAbilityHUDPreferences();
    }

    OnPreferencesSaved(SavedPreferences);
    return true;
}
