#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "JumanjiGameUserSettings.generated.h"

/** Device-local presentation preferences. These values are never replicated. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiAbilityHUDPreferences
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Ability HUD")
    float HUDScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Ability HUD")
    float TouchTargetScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Ability HUD")
    bool bHighContrastMode = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JUMANJI|Ability HUD")
    bool bReduceMotion = false;
};

/**
 * Game-local settings stored in GameUserSettings.ini.
 *
 * Keep presentation preferences here rather than in the server-owned player
 * profile: HUD scale, contrast and motion are properties of this device/user.
 */
UCLASS(Config=GameUserSettings, ConfigDoNotCheckDefaults)
class JUMANJIUE_API UJumanjiGameUserSettings : public UGameUserSettings
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability HUD|Settings")
    FJumanjiAbilityHUDPreferences GetAbilityHUDPreferences() const;

    /** Updates preferences and writes them to the local GameUserSettings.ini. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD|Settings")
    void SetAbilityHUDPreferences(const FJumanjiAbilityHUDPreferences& Preferences);

    /** Restores only the JUMANJI HUD preferences; graphics/audio settings are untouched. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability HUD|Settings")
    void ResetAbilityHUDPreferences();

    virtual void ValidateSettings() override;
    virtual void SetToDefaults() override;

private:
    UPROPERTY(Config, Category = "JUMANJI|Ability HUD", meta = (ClampMin = "0.75", ClampMax = "1.35"))
    float SavedAbilityHUDScale = 1.0f;

    UPROPERTY(Config, Category = "JUMANJI|Ability HUD", meta = (ClampMin = "0.85", ClampMax = "1.5"))
    float SavedAbilityTouchTargetScale = 1.0f;

    UPROPERTY(Config, Category = "JUMANJI|Ability HUD")
    bool bSavedAbilityHUDHighContrast = false;

    UPROPERTY(Config, Category = "JUMANJI|Ability HUD")
    bool bSavedAbilityHUDReduceMotion = false;
};
