#pragma once

#include "CoreMinimal.h"
#include "JumanjiEnemyCharacter.h"
#include "JumanjiBossCharacter.generated.h"

struct FOnAttributeChangeData;

/** Three-phase boss foundation driven by replicated Health percentage. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiBossCharacter : public AJumanjiEnemyCharacter
{
    GENERATED_BODY()

public:
    AJumanjiBossCharacter();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Boss")
    uint8 GetBossPhase() const { return BossPhase; }

protected:
    virtual void HandleCombatHealthChanged(const FOnAttributeChangeData& ChangeData) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Boss", meta = (ClampMin = "0.1", ClampMax = "0.9"))
    float PhaseTwoHealthThreshold = 0.66f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Boss", meta = (ClampMin = "0.05", ClampMax = "0.8"))
    float PhaseThreeHealthThreshold = 0.33f;

    UPROPERTY(ReplicatedUsing = OnRep_BossPhase, BlueprintReadOnly, Category = "JUMANJI|Boss")
    uint8 BossPhase = 1;

    UFUNCTION()
    void OnRep_BossPhase();

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Boss")
    void BP_OnBossPhaseChanged(uint8 NewPhase);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    void SetBossPhase(uint8 NewPhase);
};
