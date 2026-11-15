#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "JumanjiCharacterDefinition.generated.h"

class UGameplayAbility;
class USkeletalMesh;

UENUM(BlueprintType)
enum class EJumanjiSpiritElement : uint8
{
    Root UMETA(DisplayName = "Root"),
    Tide UMETA(DisplayName = "Tide"),
    Stone UMETA(DisplayName = "Stone"),
    Storm UMETA(DisplayName = "Storm"),
    Ember UMETA(DisplayName = "Ember"),
    Dawn UMETA(DisplayName = "Dawn"),
    Shadow UMETA(DisplayName = "Shadow"),
    Life UMETA(DisplayName = "Life"),
    Frost UMETA(DisplayName = "Frost")
};

UENUM(BlueprintType)
enum class EJumanjiCombatRole : uint8
{
    Guardian UMETA(DisplayName = "Guardian"),
    Support UMETA(DisplayName = "Support"),
    Vanguard UMETA(DisplayName = "Vanguard"),
    Controller UMETA(DisplayName = "Controller"),
    Striker UMETA(DisplayName = "Striker"),
    Ranger UMETA(DisplayName = "Ranger"),
    Duelist UMETA(DisplayName = "Duelist"),
    Healer UMETA(DisplayName = "Healer"),
    Sentinel UMETA(DisplayName = "Sentinel")
};

/**
 * Designer-facing entry for one of the nine Great Spirits.
 * Soft references allow art and ability assets to be added later without
 * forcing them to load whenever the roster subsystem initializes.
 */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiCharacterDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName CharacterId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    int32 SpiritNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay")
    EJumanjiSpiritElement Element = EJumanjiSpiritElement::Root;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay")
    EJumanjiCombatRole Role = EJumanjiCombatRole::Guardian;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay")
    FText SignatureAbilityName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay")
    FText PassiveAbilityName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    FLinearColor PrimaryColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    FLinearColor SecondaryColor = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    TSoftObjectPtr<UTexture2D> PortraitTexture;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
    TSoftClassPtr<UGameplayAbility> SignatureAbilityClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
    TSoftClassPtr<UGameplayAbility> UltimateAbilityClass;
};
