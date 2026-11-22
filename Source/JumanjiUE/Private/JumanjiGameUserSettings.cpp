#include "JumanjiGameUserSettings.h"

namespace
{
    constexpr float DefaultHUDScale = 1.0f;
    constexpr float DefaultTouchTargetScale = 1.0f;

    float ClampFiniteValue(float Value, float DefaultValue, float MinValue, float MaxValue)
    {
        return FMath::Clamp(
            FMath::IsFinite(Value) ? Value : DefaultValue,
            MinValue,
            MaxValue
        );
    }
}

FJumanjiAbilityHUDPreferences UJumanjiGameUserSettings::GetAbilityHUDPreferences() const
{
    FJumanjiAbilityHUDPreferences Preferences;
    Preferences.HUDScale = ClampFiniteValue(
        SavedAbilityHUDScale,
        DefaultHUDScale,
        0.75f,
        1.35f
    );
    Preferences.TouchTargetScale = ClampFiniteValue(
        SavedAbilityTouchTargetScale,
        DefaultTouchTargetScale,
        0.85f,
        1.5f
    );
    Preferences.bHighContrastMode = bSavedAbilityHUDHighContrast;
    Preferences.bReduceMotion = bSavedAbilityHUDReduceMotion;
    return Preferences;
}

void UJumanjiGameUserSettings::SetAbilityHUDPreferences(
    const FJumanjiAbilityHUDPreferences& Preferences)
{
    SavedAbilityHUDScale = ClampFiniteValue(
        Preferences.HUDScale,
        DefaultHUDScale,
        0.75f,
        1.35f
    );
    SavedAbilityTouchTargetScale = ClampFiniteValue(
        Preferences.TouchTargetScale,
        DefaultTouchTargetScale,
        0.85f,
        1.5f
    );
    bSavedAbilityHUDHighContrast = Preferences.bHighContrastMode;
    bSavedAbilityHUDReduceMotion = Preferences.bReduceMotion;

    SaveSettings();
}

void UJumanjiGameUserSettings::ResetAbilityHUDPreferences()
{
    FJumanjiAbilityHUDPreferences Defaults;
    SetAbilityHUDPreferences(Defaults);
}

void UJumanjiGameUserSettings::ValidateSettings()
{
    Super::ValidateSettings();

    SavedAbilityHUDScale = ClampFiniteValue(
        SavedAbilityHUDScale,
        DefaultHUDScale,
        0.75f,
        1.35f
    );
    SavedAbilityTouchTargetScale = ClampFiniteValue(
        SavedAbilityTouchTargetScale,
        DefaultTouchTargetScale,
        0.85f,
        1.5f
    );
}

void UJumanjiGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();

    SavedAbilityHUDScale = DefaultHUDScale;
    SavedAbilityTouchTargetScale = DefaultTouchTargetScale;
    bSavedAbilityHUDHighContrast = false;
    bSavedAbilityHUDReduceMotion = false;
}
