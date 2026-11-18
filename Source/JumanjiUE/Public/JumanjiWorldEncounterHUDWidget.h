#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JumanjiWorldEncounter.h"
#include "JumanjiBossCharacter.h"
#include "JumanjiWorldEncounterHUDWidget.generated.h"

class AJumanjiGameState;
class AJumanjiPlayerState;

/**
 * Local presentation adapter for the replicated world-encounter snapshot.
 * It reads GameState and boss attributes; it never advances an encounter,
 * applies damage or grants rewards.
 */
UCLASS(Abstract, Blueprintable)
class JUMANJIUE_API UJumanjiWorldEncounterHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    FJumanjiWorldEncounterState GetEncounterState() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD|Reconnect")
    FJumanjiEncounterReconnectState GetReconnectState() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    FText GetEncounterStatusText() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    int32 GetCompletedTrialCount() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    int32 GetTotalTrialCount() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    AJumanjiBossCharacter* GetEncounterBoss() const { return CachedBoss; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    float GetBossHealth() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    float GetBossMaxHealth() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    float GetBossHealthPercent() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    uint8 GetBossPhase() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    FJumanjiBossTelegraphState GetBossTelegraphState() const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    bool IsBossTelegraphActive() const;

    /** Remaining seconds for the current warning or execution window. */
    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    float GetTelegraphTimeRemaining() const;

    /** Normalized remaining time (1 at the start of a stage, 0 at its end). */
    UFUNCTION(BlueprintPure, Category = "JUMANJI|Encounter HUD")
    float GetTelegraphRemainingPercent() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|Encounter HUD",
        meta = (ClampMin = "0.05", ClampMax = "0.5"))
    float RefreshInterval = 0.1f;

    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Encounter HUD")
    void OnEncounterHUDPresentationChanged(
        const FJumanjiWorldEncounterState& State,
        const FText& StatusText,
        float BossHealthPercent,
        uint8 BossPhase,
        const FJumanjiBossTelegraphState& TelegraphState
    );

    /** Use this event for phase-specific UI/audio feedback. It fires only when the phase changes. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Encounter HUD")
    void OnEncounterPhaseCue(EJumanjiWorldEncounterPhase NewPhase);

    /** Fires once when the boss advances from one health phase to another. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Encounter HUD")
    void OnBossPhaseCue(uint8 NewBossPhase);

    /** Use this event to display or clear the shape-specific boss telegraph. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Encounter HUD")
    void OnBossTelegraphPresentationChanged(const FJumanjiBossTelegraphState& TelegraphState);

    /** Fires when this local player rejoins an active same-server encounter. */
    UFUNCTION(BlueprintImplementableEvent, Category = "JUMANJI|Encounter HUD|Reconnect")
    void OnEncounterReconnectNotice(const FJumanjiEncounterReconnectState& ReconnectState);

private:
    void BindGameState();
    void BindBoss();
    void BindPlayerState();
    void ReportBossPhaseIfChanged();
    void RefreshPresentation();
    float GetSynchronizedWorldTime() const;

    UFUNCTION()
    void HandleWorldEncounterChanged(const FJumanjiWorldEncounterState& NewState);

    UFUNCTION()
    void HandleBossTelegraphChanged(const FJumanjiBossTelegraphState& NewState);

    UFUNCTION()
    void HandleEncounterReconnectChanged(const FJumanjiEncounterReconnectState& NewState);

    UPROPERTY(Transient)
    TObjectPtr<AJumanjiGameState> CachedGameState;

    UPROPERTY(Transient)
    TObjectPtr<AJumanjiBossCharacter> CachedBoss;

    UPROPERTY(Transient)
    TObjectPtr<AJumanjiPlayerState> CachedPlayerState;

    float RefreshAccumulator = 0.0f;
    bool bHasReportedPhase = false;
    EJumanjiWorldEncounterPhase LastReportedPhase = EJumanjiWorldEncounterPhase::Inactive;
    bool bHasReportedBossPhase = false;
    uint8 LastReportedBossPhase = 0;
};
