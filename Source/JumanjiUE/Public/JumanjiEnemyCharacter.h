#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "JumanjiEnemyCharacter.generated.h"

class UJumanjiAbilitySystemComponent;
class UJumanjiAttributeSet;
class UGameplayEffect;
struct FOnAttributeChangeData;

/** Common GAS-enabled enemy base for PvE encounters. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiEnemyCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AJumanjiEnemyCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Enemy")
    UJumanjiAbilitySystemComponent* GetEnemyAbilitySystemComponent() const
    {
        return AbilitySystemComponent;
    }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Enemy")
    UJumanjiAttributeSet* GetEnemyAttributeSet() const
    {
        return AttributeSet;
    }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Enemy")
    bool IsDefeated() const { return bIsDefeated; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void HandleCombatHealthChanged(const FOnAttributeChangeData& ChangeData);
    virtual void HandleDefeated();

    UFUNCTION()
    void OnRep_IsDefeated();

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Enemy")
    void BP_OnDamaged(float PreviousHealth, float CurrentHealth);

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Enemy")
    void BP_OnDefeated();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Enemy")
    TObjectPtr<UJumanjiAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Enemy")
    TObjectPtr<UJumanjiAttributeSet> AttributeSet;

    /** Optional server-applied GE for enemy/boss starting attributes. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Enemy")
    TSubclassOf<UGameplayEffect> InitializationEffectClass;

    UPROPERTY(ReplicatedUsing = OnRep_IsDefeated, BlueprintReadOnly, Category = "JUMANJI|Enemy")
    bool bIsDefeated = false;

private:
    FDelegateHandle HealthChangedDelegateHandle;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
