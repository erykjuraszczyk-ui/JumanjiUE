#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "JumanjiWorldExploration.h"
#include "JumanjiPlayerProfileSaveGame.h"
#include "JumanjiProgressionTypes.h"
#include "JumanjiPlayerState.generated.h"

class UJumanjiAbilitySystemComponent;
class UJumanjiAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiSelectedCharacterChanged,
    FName, CharacterId
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiWorldExplorationChanged,
    const FJumanjiWorldExplorationState&, ExplorationState
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiCompletedAdventureChaptersChanged,
    const TArray<FName>&, CompletedChapterIds
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiProgressionChanged,
    const FJumanjiPlayerProgressionState&, ProgressionState
);

/**
 * Player-owned GAS, exploration and profile-progress owner.
 * Persistent profile and progression mutations are server-authoritative.
 */
UCLASS()
class JUMANJIUE_API AJumanjiPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AJumanjiPlayerState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Abilities")
    UJumanjiAbilitySystemComponent* GetJumanjiAbilitySystemComponent() const
    {
        return AbilitySystemComponent;
    }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Attributes")
    UJumanjiAttributeSet* GetJumanjiAttributeSet() const
    {
        return AttributeSet;
    }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Character")
    FName GetSelectedCharacterId() const { return SelectedCharacterId; }

    /** Server-only: call after validating the selected ID against the roster. */
    void SetSelectedCharacterId(FName CharacterId);

    /** Server-only identity binding established by GameMode after login. */
    void SetPersistentProfileId(const FString& InProfileId);
    const FString& GetPersistentProfileId() const { return PersistentProfileId; }
    void RequestProfileSave();

    /** Initialize starter balances and level-one progression for a new profile. */
    void InitializeNewProfileProgression();

    /** Applies a schema-v2 snapshot that has already passed profile-subsystem validation. */
    bool ApplyProfileSnapshot(const FJumanjiPlayerProfileSnapshot& Snapshot);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Progression")
    FJumanjiPlayerProgressionState GetProgressionState() const { return ProgressionState; }

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Economy")
    int64 GetCurrencyBalance(FName CurrencyId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Inventory")
    int32 GetItemQuantity(FName ItemId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Progression")
    int32 GetGreatSpiritLevel(FName CharacterId) const;

    /**
     * Server-only, all-or-nothing transaction. Call only from trusted server
     * gameplay/reward code. A duplicate TransactionId is rejected.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "JUMANJI|Economy")
    bool ApplyProfileTransaction(const FJumanjiProfileTransaction& Transaction);

    /** Saves a logical ability-slot selection; does not load arbitrary client classes. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "JUMANJI|Loadout")
    bool SetEquippedAbilitySlot(FName CharacterId, FName SlotId, FName AbilityId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Adventure")
    bool MarkAdventureChapterCompleted(FName ChapterId);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    bool HasCompletedAdventureChapter(FName ChapterId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Adventure")
    TArray<FName> GetCompletedAdventureChapterIds() const
    {
        return CompletedAdventureChapterIds;
    }

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    bool DiscoverWorldRegion(FName RegionId);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|World")
    bool VisitWorldLandmark(FName LandmarkId);

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool HasDiscoveredWorldRegion(FName RegionId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    bool HasVisitedWorldLandmark(FName LandmarkId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|World")
    FJumanjiWorldExplorationState GetWorldExplorationState() const
    {
        return WorldExplorationState;
    }

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Character")
    FJumanjiSelectedCharacterChanged OnSelectedCharacterChanged;

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|World")
    FJumanjiWorldExplorationChanged OnWorldExplorationChanged;

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Adventure")
    FJumanjiCompletedAdventureChaptersChanged OnCompletedAdventureChaptersChanged;

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Progression")
    FJumanjiProgressionChanged OnProgressionChanged;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JUMANJI|Abilities", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UJumanjiAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UJumanjiAttributeSet> AttributeSet;

    UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterId, BlueprintReadOnly, Category = "JUMANJI|Character")
    FName SelectedCharacterId = FName(TEXT("GREAT_SPIRIT_I"));

    UPROPERTY(ReplicatedUsing = OnRep_WorldExplorationState, BlueprintReadOnly, Category = "JUMANJI|World")
    FJumanjiWorldExplorationState WorldExplorationState;

    UPROPERTY(ReplicatedUsing = OnRep_CompletedAdventureChapterIds, BlueprintReadOnly, Category = "JUMANJI|Adventure")
    TArray<FName> CompletedAdventureChapterIds;

    UPROPERTY(ReplicatedUsing = OnRep_ProgressionState, BlueprintReadOnly, Category = "JUMANJI|Progression")
    FJumanjiPlayerProgressionState ProgressionState;

    UFUNCTION()
    void OnRep_SelectedCharacterId();

    UFUNCTION()
    void OnRep_WorldExplorationState();

    UFUNCTION()
    void OnRep_CompletedAdventureChapterIds();

    UFUNCTION()
    void OnRep_ProgressionState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    FJumanjiPlayerProfileSnapshot BuildProfileSnapshot() const;
    void PersistProfileSnapshot();

    /** Server-only key; never accepted directly from a client RPC. */
    FString PersistentProfileId;

    /** Server-only idempotency ledger; saved with the progression snapshot, not replicated. */
    TArray<FName> ProcessedTransactionIds;
};
