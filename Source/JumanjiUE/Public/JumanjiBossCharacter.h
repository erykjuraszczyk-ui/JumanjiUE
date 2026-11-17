#pragma once

#include "CoreMinimal.h"
#include "JumanjiEnemyCharacter.h"
#include "TimerManager.h"
#include "JumanjiBossCharacter.generated.h"

struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class EJumanjiBossTelegraphPattern : uint8
{
    Radial UMETA(DisplayName = "Radial"),
    Line UMETA(DisplayName = "Line"),
    Cone UMETA(DisplayName = "Cone"),
    Focused UMETA(DisplayName = "Focused")
};

UENUM(BlueprintType)
enum class EJumanjiBossTelegraphStage : uint8
{
    None UMETA(DisplayName = "None"),
    Warning UMETA(DisplayName = "Warning"),
    Executing UMETA(DisplayName = "Executing")
};

USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiBossTelegraphState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    FName AttackId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    EJumanjiBossTelegraphPattern Pattern = EJumanjiBossTelegraphPattern::Radial;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    EJumanjiBossTelegraphStage Stage = EJumanjiBossTelegraphStage::None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    FVector Origin = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    FVector Direction = FVector::ForwardVector;

    /** Synchronized server-world time at the start of the current stage. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    float ServerStartTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    float WarningDuration = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    float ActiveDuration = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    float Radius = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    float Length = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    float Width = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    uint32 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiBossTelegraphChanged,
    const FJumanjiBossTelegraphState&, TelegraphState
);

/** Three-phase boss foundation with replicated health phases and attack telegraph snapshots. */
UCLASS(Blueprintable)
class JUMANJIUE_API AJumanjiBossCharacter : public AJumanjiEnemyCharacter
{
    GENERATED_BODY()

public:
    AJumanjiBossCharacter();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Boss")
    uint8 GetBossPhase() const { return BossPhase; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Boss|Telegraph")
    FJumanjiBossTelegraphState GetBossTelegraphState() const { return BossTelegraphState; }

    /** Server-only warning window. Blueprint attack logic begins when the execution event fires. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "JUMANJI|Boss|Telegraph")
    bool BeginBossTelegraph(
        FName AttackId,
        EJumanjiBossTelegraphPattern Pattern,
        FVector Origin,
        FVector Direction,
        float WarningDuration,
        float ActiveDuration,
        float Radius,
        float Length,
        float Width
    );

    /** Server-only cancellation, for phase changes, defeat or interrupted patterns. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "JUMANJI|Boss|Telegraph")
    void CancelBossTelegraph();

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Boss|Telegraph")
    FJumanjiBossTelegraphChanged OnBossTelegraphChanged;

protected:
    virtual void HandleCombatHealthChanged(const FOnAttributeChangeData& ChangeData) override;
    virtual void HandleDefeated() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Boss", meta = (ClampMin = "0.1", ClampMax = "0.9"))
    float PhaseTwoHealthThreshold = 0.66f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Boss", meta = (ClampMin = "0.05", ClampMax = "0.8"))
    float PhaseThreeHealthThreshold = 0.33f;

    UPROPERTY(ReplicatedUsing = OnRep_BossPhase, BlueprintReadOnly, Category = "JUMANJI|Boss")
    uint8 BossPhase = 1;

    UPROPERTY(ReplicatedUsing = OnRep_BossTelegraphState, BlueprintReadOnly, Category = "JUMANJI|Boss|Telegraph")
    FJumanjiBossTelegraphState BossTelegraphState;

    UFUNCTION()
    void OnRep_BossPhase();

    UFUNCTION()
    void OnRep_BossTelegraphState();

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Boss")
    void BP_OnBossPhaseChanged(uint8 NewPhase);

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Boss|Telegraph")
    void BP_OnBossTelegraphChanged(const FJumanjiBossTelegraphState& NewState);

    /** Implement the actual server-authoritative attack in the boss Blueprint/ability. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Boss|Telegraph")
    void BP_OnBossTelegraphExecutionRequested(
        FName AttackId,
        EJumanjiBossTelegraphPattern Pattern
    );

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    void SetBossPhase(uint8 NewPhase);
    void SetTelegraphStage(EJumanjiBossTelegraphStage NewStage);
    void PublishTelegraphState();
    void RefreshTelegraphGameplayTags();

    UFUNCTION()
    void HandleTelegraphWarningComplete();

    UFUNCTION()
    void HandleTelegraphActiveComplete();

    FTimerHandle TelegraphWarningTimerHandle;
    FTimerHandle TelegraphActiveTimerHandle;
};
