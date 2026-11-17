#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiTraversalLink.h"
#include "JumanjiTraversalPresentationSubsystem.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class UTexture2D;

/** Optional, data-driven presentation assets for one authored traversal link. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiTraversalPresentationDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    EJumanjiTraversalType TraversalType = EJumanjiTraversalType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    FText PromptText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    FText InProgressText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    FText CompletedText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    FText InterruptedText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<UTexture2D> PromptIcon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<UAnimMontage> StartMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<UAnimMontage> LoopMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<UAnimMontage> EndMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<USoundBase> StartSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<USoundBase> EndSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<UNiagaraSystem> StartEffect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<UNiagaraSystem> EndEffect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    bool bAllowCameraLook = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation")
    bool bShowProgress = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JUMANJI|Traversal|Presentation",
        meta = (ClampMin = "0.2", ClampMax = "3.0"))
    float PromptHoldSeconds = 0.65f;
};

/**
 * Validated traversal presentation manifest.
 * Row names are stable TraversalIds. Asset fields stay soft and are loaded by
 * the local presentation layer only when the relevant traversal is focused.
 */
UCLASS(Config = Game)
class JUMANJIUE_API UJumanjiTraversalPresentationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal|Presentation")
    bool GetPresentationDefinition(
        FName TraversalId,
        EJumanjiTraversalType TraversalType,
        FJumanjiTraversalPresentationDefinition& OutDefinition
    ) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal|Presentation")
    void GetAllPresentationDefinitions(
        TArray<FJumanjiTraversalPresentationDefinition>& OutDefinitions
    ) const;

protected:
    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Traversal|Presentation")
    TSoftObjectPtr<UDataTable> PresentationDataTable;

private:
    void BuildFallbackDefinition(
        EJumanjiTraversalType TraversalType,
        FJumanjiTraversalPresentationDefinition& OutDefinition
    ) const;

    bool LoadConfiguredManifest();

    UPROPERTY(Transient)
    TMap<FName, FJumanjiTraversalPresentationDefinition> Definitions;
};
