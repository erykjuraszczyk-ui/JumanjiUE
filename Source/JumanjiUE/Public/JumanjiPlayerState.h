#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "JumanjiWorldExploration.h"
#include "JumanjiPlayerProfileSaveGame.h"
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

/**
 * Player-owned GAS, exploration and profile-progress owner.
 * The possessed Character is the ASC avatar; this keeps ability state attached
 * to PlayerState rather than to a pawn that may be replaced.
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

    /** Must only be called by the server after roster validation. */
    void SetSelectedCharacterId(FName CharacterId);

    /** Server-only identity binding established by GameMode after login. */
    void SetPersistentProfileId(const FString& InProfileId);
    const FString& GetPersistentProfileId() const { return PersistentProfileId; }
    void RequestProfileSave();

    /** Applies a validated server-loaded profile to the replicated PlayerState. */
    bool ApplyProfileSnapshot(const FJumanjiPlayerProfileSnapshot& Snapshot);

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

    UFUNCTION()
    void OnRep_SelectedCharacterId();

    UFUNCTION()
    void OnRep_WorldExplorationState();

    UFUNCTION()
    void OnRep_CompletedAdventureChapterIds();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    FJumanjiPlayerProfileSnapshot BuildProfileSnapshot() const;
    void PersistProfileSnapshot();

    /** Server-only key; never accepted directly from a client RPC. */
    FString PersistentProfileId;
};
