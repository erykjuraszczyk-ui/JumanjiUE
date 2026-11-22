#include "JumanjiAbilityHUDSettingsWidget.h"

#include "Engine/Engine.h"

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
    OnPreferencesSaved(Settings->GetAbilityHUDPreferences());
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
    OnPreferencesSaved(Settings->GetAbilityHUDPreferences());
    return true;
}
