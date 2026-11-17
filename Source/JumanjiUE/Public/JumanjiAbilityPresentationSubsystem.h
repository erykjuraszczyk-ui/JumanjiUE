#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiAbilityPresentationSubsystem.generated.h"

class UMaterialInterface;
class USoundBase;
class UTexture2D;
class UDataTable;

/**
 * DataTable row matching the Stage 19 AbilityPresentationManifest.csv columns.
 * Import with a leading "Name" column whose values are the stable AbilityId.
 * Asset paths remain soft references so missing optional presentation assets
 * never block HUD construction.
 */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiAbilityPresentationRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FName AbilityId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FName CharacterId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FText CharacterNamePL;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FName Element = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FName SlotId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FText AbilityNamePL;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString IconAssetPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString AccentHex;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString SecondaryHex;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString IconMotif;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    float CooldownSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString CooldownMaterialPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString ActivatedSoundPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString InsufficientResourceSoundPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString CooldownSoundPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FString UnavailableSoundPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FName GameplayCueTagName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Presentation")
    FName AssetStatus = NAME_None;
};

/** Blueprint-facing resolved data. No asset is synchronously loaded by this struct. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiAbilityPresentationData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FName AbilityId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FName CharacterId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FName SlotId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FText AbilityName;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FText CharacterName;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FName Element = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FString IconMotif;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FLinearColor AccentColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FLinearColor SecondaryColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    TSoftObjectPtr<UMaterialInterface> CooldownMaterial;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    TSoftObjectPtr<USoundBase> ActivatedSound;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    TSoftObjectPtr<USoundBase> InsufficientResourceSound;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    TSoftObjectPtr<USoundBase> CooldownSound;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    TSoftObjectPtr<USoundBase> UnavailableSound;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    FGameplayTag GameplayCueTag;

    /** Reference value only. The real cooldown is read from GAS. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    float CooldownSecondsReference = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Ability Presentation")
    bool bHasIconAssetPath = false;
};

/**
 * Reads the imported Stage 19 manifest DataTable and returns soft presentation
 * references for the Ability HUD. GAS remains authoritative for all gameplay
 * values; this subsystem is read-only presentation data.
 */
UCLASS(Config = Game)
class JUMANJIUE_API UJumanjiAbilityPresentationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability Presentation")
    bool GetPresentation(FName AbilityId, FJumanjiAbilityPresentationData& OutPresentation) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Ability Presentation")
    bool ReloadPresentationData();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Ability Presentation")
    int32 GetLoadedPresentationCount() const;

private:
    bool EnsurePresentationDataReady() const;
    bool IsRowCompatible(const FJumanjiAbilityPresentationRow& Row) const;
    static FLinearColor ParseHexColor(const FString& Hex, const FLinearColor& Fallback);
    static FJumanjiAbilityPresentationData MakePresentationData(
        const FJumanjiAbilityPresentationRow& Row
    );

    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Ability Presentation")
    TSoftObjectPtr<UDataTable> ConfiguredPresentationDataTable;

    mutable TMap<FName, FJumanjiAbilityPresentationData> CachedPresentations;
    mutable bool bPresentationDataInitialized = false;
};
