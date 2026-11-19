#include "JumanjiPlayerState.h"

#include "JumanjiAbilitySystemComponent.h"
#include "JumanjiAbilityCatalogSubsystem.h"
#include "JumanjiAttributeSet.h"
#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiPlayerProfileSubsystem.h"
#include "JumanjiProgressionCatalogSubsystem.h"
#include "JumanjiProgressionRewardSubsystem.h"
#include "JumanjiWorldAtlas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace JumanjiProfile
{
    constexpr int32 MaxRegions = 32;
    constexpr int32 MaxLandmarks = 128;
    constexpr int32 MaxCompletedChapters = 64;
    constexpr int32 MaxInventoryStacks = 256;
    constexpr int32 MaxProcessedTransactions = 2048;
    constexpr int32 MaxAccountLevel = 100;
    constexpr int32 MaxCharacterLevel = 80;
    constexpr int64 MaxExperienceDeltaPerTransaction = 1000000LL;
    constexpr int64 MaxCurrencyDeltaPerTransaction = 2000000000LL;
}

AJumanjiPlayerState::AJumanjiPlayerState()
{
    bReplicates = true;
    NetUpdateFrequency = 100.0f;

    AbilitySystemComponent = CreateDefaultSubobject<UJumanjiAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UJumanjiAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AJumanjiPlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AJumanjiPlayerState::SetSelectedCharacterId(FName CharacterId)
{
    if (!HasAuthority() || CharacterId.IsNone() || SelectedCharacterId == CharacterId)
    {
        return;
    }

    SelectedCharacterId = CharacterId;
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
    RefreshCombatAttributesFromProgression();
    ForceNetUpdate();
    PersistProfileSnapshot();
}

bool AJumanjiPlayerState::SetCheckpointParticipantIdentity(
    const FString& OpaqueParticipantKey,
    int64 ProfileRevision)
{
    if (!HasAuthority()
        || OpaqueParticipantKey.Len() < 23
        || OpaqueParticipantKey.Len() > 99
        || !OpaqueParticipantKey.StartsWith(TEXT("pk_"))
        || ProfileRevision < 0)
    {
        return false;
    }

    for (int32 Index = 3; Index < OpaqueParticipantKey.Len(); ++Index)
    {
        const TCHAR Character = OpaqueParticipantKey[Index];
        const bool bAllowed =
            (Character >= TEXT('A') && Character <= TEXT('Z'))
            || (Character >= TEXT('a') && Character <= TEXT('z'))
            || (Character >= TEXT('0') && Character <= TEXT('9'))
            || Character == TEXT('_')
            || Character == TEXT('-');
        if (!bAllowed)
        {
            return false;
        }
    }

    if (!CheckpointParticipantKey.IsEmpty()
        && (CheckpointParticipantKey != OpaqueParticipantKey
            || ProfileRevision < CheckpointProfileRevision))
    {
        return false;
    }

    CheckpointParticipantKey = OpaqueParticipantKey;
    CheckpointProfileRevision = ProfileRevision;
    return true;
}

void AJumanjiPlayerState::SetPersistentProfileId(const FString& InProfileId)
{
    if (!HasAuthority())
    {
        return;
    }

    const FString CleanProfileId = InProfileId.TrimStartAndEnd();
    if (CleanProfileId.IsEmpty() || CleanProfileId.Len() > 256)
    {
        return;
    }

    if (!PersistentProfileId.IsEmpty() && PersistentProfileId != CleanProfileId)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI profile: refusing to change an already-bound profile identity."));
        return;
    }

    PersistentProfileId = CleanProfileId;
    bProfilePersistenceEnabled = true;
}

bool AJumanjiPlayerState::SetEncounterReconnectState(
    const FJumanjiEncounterReconnectState& NewState)
{
    const bool bActivePhase =
        NewState.Phase == EJumanjiWorldEncounterPhase::Trials
        || NewState.Phase == EJumanjiWorldEncounterPhase::Combat
        || NewState.Phase == EJumanjiWorldEncounterPhase::Boss
        || NewState.Phase == EJumanjiWorldEncounterPhase::Reward;

    if (!HasAuthority()
        || !NewState.bWasReconnected
        || NewState.EncounterId.IsNone()
        || !bActivePhase
        || NewState.EncounterRevision < 0
        || NewState.ReconnectRevision <= EncounterReconnectState.ReconnectRevision)
    {
        return false;
    }

    EncounterReconnectState = NewState;
    OnEncounterReconnectChanged.Broadcast(EncounterReconnectState);
    ForceNetUpdate();
    return true;
}

bool AJumanjiPlayerState::SetEncounterReconnectFeedback(
    const FJumanjiEncounterReconnectFeedbackState& NewState)
{
    if (!HasAuthority()
        || NewState.Outcome == EJumanjiEncounterReconnectOutcome::None
        || NewState.EncounterId.IsNone()
        || NewState.Revision <= EncounterReconnectFeedbackState.Revision
        || !FMath::IsFinite(NewState.DisplayDurationSeconds)
        || NewState.DisplayDurationSeconds < 0.0f
        || NewState.DisplayDurationSeconds > 30.0f)
    {
        return false;
    }

    EncounterReconnectFeedbackState = NewState;
    OnEncounterReconnectFeedbackChanged.Broadcast(EncounterReconnectFeedbackState);
    ForceNetUpdate();
    return true;
}

void AJumanjiPlayerState::RequestProfileSave()
{
    PersistProfileSnapshot();
}

void AJumanjiPlayerState::DisableProfilePersistenceForSession()
{
    if (HasAuthority())
    {
        bProfilePersistenceEnabled = false;
    }
}

void AJumanjiPlayerState::InitializeNewProfileProgression()
{
    if (!HasAuthority())
    {
        return;
    }

    FJumanjiPlayerProgressionState NewProgression;
    NewProgression.AccountLevel = 1;
    NewProgression.AccountExperience = 0;
    NewProgression.Revision = 0;

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiProgressionCatalogSubsystem* Catalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiProgressionCatalogSubsystem>()
        : nullptr;
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;

    if (Catalog)
    {
        // Starting balances are explicit catalog data, not client-provided values.
        const FName CurrencyIds[] = {
            FName(TEXT("CURRENCY_EXPLORER_COINS")),
            FName(TEXT("CURRENCY_BOARD_SEALS")),
            FName(TEXT("CURRENCY_ECHO_CRYSTALS"))
        };

        for (const FName CurrencyId : CurrencyIds)
        {
            FJumanjiCurrencyDefinition Definition;
            if (Catalog->GetCurrencyDefinition(CurrencyId, Definition))
            {
                FJumanjiCurrencyBalance Balance;
                Balance.CurrencyId = Definition.CurrencyId;
                Balance.Amount = Definition.StartingBalance;
                NewProgression.Currencies.Add(Balance);
            }
        }
    }

    if (Roster)
    {
        for (const FJumanjiCharacterDefinition& Definition : Roster->GetRoster())
        {
            if (Definition.CharacterId.IsNone())
            {
                continue;
            }

            FJumanjiCharacterProgress CharacterProgress;
            CharacterProgress.CharacterId = Definition.CharacterId;
            CharacterProgress.Level = 1;
            CharacterProgress.Experience = 0;
            NewProgression.CharacterProgress.Add(CharacterProgress);
        }
    }

    ProgressionState = MoveTemp(NewProgression);
    ProcessedTransactionIds.Reset();
    OnProgressionChanged.Broadcast(ProgressionState);
    RefreshCombatAttributesFromProgression();
    ForceNetUpdate();
}

bool AJumanjiPlayerState::ApplyProfileSnapshot(
    const FJumanjiPlayerProfileSnapshot& Snapshot)
{
    if (!HasAuthority()
        || PersistentProfileId.IsEmpty()
        || Snapshot.ProfileId != PersistentProfileId
        || Snapshot.SchemaVersion != 2
        || Snapshot.DiscoveredRegionIds.Num() > JumanjiProfile::MaxRegions
        || Snapshot.VisitedLandmarkIds.Num() > JumanjiProfile::MaxLandmarks
        || Snapshot.CompletedAdventureChapterIds.Num() > JumanjiProfile::MaxCompletedChapters
        || Snapshot.ExplorationRevision < 0
        || Snapshot.ProcessedTransactionIds.Num() > JumanjiProfile::MaxProcessedTransactions)
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;
    const UJumanjiProgressionCatalogSubsystem* Catalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiProgressionCatalogSubsystem>()
        : nullptr;
    const UJumanjiAbilityCatalogSubsystem* AbilityCatalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiAbilityCatalogSubsystem>()
        : nullptr;

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    if (!Atlas || !Roster || !Catalog || !AbilityCatalog)
    {
        return false;
    }

    SelectedCharacterId = Roster->IsPlayableCharacter(Snapshot.SelectedCharacterId)
        ? Snapshot.SelectedCharacterId
        : FName(TEXT("GREAT_SPIRIT_I"));

    TArray<FName> SafeRegions;
    TSet<FName> RegionIds;
    for (const FName RegionId : Snapshot.DiscoveredRegionIds)
    {
        if (!RegionId.IsNone() && Atlas->IsKnownRegion(RegionId) && !RegionIds.Contains(RegionId))
        {
            RegionIds.Add(RegionId);
            SafeRegions.Add(RegionId);
        }
    }

    TArray<FName> SafeLandmarks;
    TSet<FName> LandmarkIds;
    for (const FName LandmarkId : Snapshot.VisitedLandmarkIds)
    {
        FJumanjiWorldLandmarkDefinition Landmark;
        if (LandmarkId.IsNone()
            || LandmarkIds.Contains(LandmarkId)
            || !Atlas->GetLandmarkDefinition(LandmarkId, Landmark))
        {
            continue;
        }

        // Visiting a landmark always implies discovery of its parent region.
        if (!RegionIds.Contains(Landmark.RegionId))
        {
            RegionIds.Add(Landmark.RegionId);
            SafeRegions.Add(Landmark.RegionId);
        }

        LandmarkIds.Add(LandmarkId);
        SafeLandmarks.Add(LandmarkId);
    }

    TArray<FName> SafeCompletedChapters;
    TSet<FName> CompletedChapterIds;
    for (const FName ChapterId : Snapshot.CompletedAdventureChapterIds)
    {
        if (!ChapterId.IsNone()
            && !CompletedChapterIds.Contains(ChapterId)
            && SafeCompletedChapters.Num() < JumanjiProfile::MaxCompletedChapters)
        {
            CompletedChapterIds.Add(ChapterId);
            SafeCompletedChapters.Add(ChapterId);
        }
    }

    WorldExplorationState.DiscoveredRegionIds = MoveTemp(SafeRegions);
    WorldExplorationState.VisitedLandmarkIds = MoveTemp(SafeLandmarks);
    WorldExplorationState.Revision = FMath::Max(0, Snapshot.ExplorationRevision);
    CompletedAdventureChapterIds = MoveTemp(SafeCompletedChapters);

    // Start with trusted defaults, then overlay only known and bounded saved IDs.
    FJumanjiPlayerProgressionState SafeProgression;
    SafeProgression.AccountLevel = FMath::Clamp(
        Snapshot.Progression.AccountLevel, 1, JumanjiProfile::MaxAccountLevel);
    SafeProgression.AccountExperience = FMath::Max<int64>(0, Snapshot.Progression.AccountExperience);
    SafeProgression.Revision = FMath::Max(0, Snapshot.Progression.Revision);

    const int64 AccountXpThreshold =
        Catalog->GetAccountExperienceForNextLevel(SafeProgression.AccountLevel);
    if (SafeProgression.AccountLevel >= JumanjiProfile::MaxAccountLevel)
    {
        SafeProgression.AccountExperience = 0;
    }
    else if (AccountXpThreshold > 0)
    {
        SafeProgression.AccountExperience = FMath::Min(
            SafeProgression.AccountExperience, AccountXpThreshold - 1);
    }

    // Fill currencies with catalog defaults; saved entries replace those defaults.
    const FName CurrencyIds[] = {
        FName(TEXT("CURRENCY_EXPLORER_COINS")),
        FName(TEXT("CURRENCY_BOARD_SEALS")),
        FName(TEXT("CURRENCY_ECHO_CRYSTALS"))
    };
    for (const FName CurrencyId : CurrencyIds)
    {
        FJumanjiCurrencyDefinition Definition;
        if (Catalog->GetCurrencyDefinition(CurrencyId, Definition))
        {
            FJumanjiCurrencyBalance Balance;
            Balance.CurrencyId = Definition.CurrencyId;
            Balance.Amount = Definition.StartingBalance;
            SafeProgression.Currencies.Add(Balance);
        }
    }

    TSet<FName> SeenCurrencyIds;
    for (const FJumanjiCurrencyBalance& SavedCurrency : Snapshot.Progression.Currencies)
    {
        FJumanjiCurrencyDefinition Definition;
        if (SavedCurrency.CurrencyId.IsNone()
            || SeenCurrencyIds.Contains(SavedCurrency.CurrencyId)
            || !Catalog->GetCurrencyDefinition(SavedCurrency.CurrencyId, Definition)
            || SavedCurrency.Amount < 0
            || SavedCurrency.Amount > Definition.MaxBalance)
        {
            continue;
        }

        SeenCurrencyIds.Add(SavedCurrency.CurrencyId);
        FJumanjiCurrencyBalance* Existing = SafeProgression.Currencies.FindByPredicate(
            [&SavedCurrency](const FJumanjiCurrencyBalance& Entry)
            {
                return Entry.CurrencyId == SavedCurrency.CurrencyId;
            });

        if (Existing)
        {
            Existing->Amount = SavedCurrency.Amount;
        }
        else
        {
            SafeProgression.Currencies.Add(SavedCurrency);
        }
    }

    TSet<FName> SeenItemIds;
    for (const FJumanjiInventoryStack& SavedStack : Snapshot.Progression.Inventory)
    {
        FJumanjiItemDefinition Definition;
        if (SavedStack.ItemId.IsNone()
            || SeenItemIds.Contains(SavedStack.ItemId)
            || !Catalog->GetItemDefinition(SavedStack.ItemId, Definition)
            || SavedStack.Quantity < 1
            || SavedStack.Quantity > Definition.MaxStack
            || SafeProgression.Inventory.Num() >= JumanjiProfile::MaxInventoryStacks)
        {
            continue;
        }

        SeenItemIds.Add(SavedStack.ItemId);
        SafeProgression.Inventory.Add(SavedStack);
    }

    TSet<FName> SeenCharacterIds;
    for (const FJumanjiCharacterDefinition& CharacterDefinition : Roster->GetRoster())
    {
        if (CharacterDefinition.CharacterId.IsNone()
            || SeenCharacterIds.Contains(CharacterDefinition.CharacterId))
        {
            continue;
        }

        SeenCharacterIds.Add(CharacterDefinition.CharacterId);

        FJumanjiCharacterProgress CharacterProgress;
        CharacterProgress.CharacterId = CharacterDefinition.CharacterId;
        CharacterProgress.Level = 1;
        CharacterProgress.Experience = 0;

        const FJumanjiCharacterProgress* SavedProgress =
            Snapshot.Progression.CharacterProgress.FindByPredicate(
                [&CharacterProgress](const FJumanjiCharacterProgress& Entry)
                {
                    return Entry.CharacterId == CharacterProgress.CharacterId;
                });

        if (SavedProgress)
        {
            CharacterProgress.Level = FMath::Clamp(
                SavedProgress->Level, 1, JumanjiProfile::MaxCharacterLevel);
            CharacterProgress.Experience = FMath::Max<int64>(0, SavedProgress->Experience);

            const int64 CharacterXpThreshold =
                Catalog->GetCharacterExperienceForNextLevel(CharacterProgress.Level);
            if (CharacterProgress.Level >= JumanjiProfile::MaxCharacterLevel)
            {
                CharacterProgress.Experience = 0;
            }
            else if (CharacterXpThreshold > 0)
            {
                CharacterProgress.Experience = FMath::Min(
                    CharacterProgress.Experience, CharacterXpThreshold - 1);
            }
        }

        SafeProgression.CharacterProgress.Add(CharacterProgress);
    }

    TSet<FString> SeenLoadoutSlots;
    for (const FJumanjiEquippedAbilitySlot& SavedSlot : Snapshot.Progression.EquippedAbilitySlots)
    {
        if (!Roster->IsPlayableCharacter(SavedSlot.CharacterId)
            || !Catalog->IsValidAbilitySlotId(SavedSlot.SlotId)
            || !AbilityCatalog->IsValidAbilityChoice(
                SavedSlot.CharacterId, SavedSlot.SlotId, SavedSlot.AbilityId))
        {
            continue;
        }

        const FString Key = SavedSlot.CharacterId.ToString() + TEXT("|") + SavedSlot.SlotId.ToString();
        if (SeenLoadoutSlots.Contains(Key)
            || SafeProgression.EquippedAbilitySlots.Num() >= 45)
        {
            continue;
        }

        SeenLoadoutSlots.Add(Key);
        SafeProgression.EquippedAbilitySlots.Add(SavedSlot);
    }

    ProgressionState = MoveTemp(SafeProgression);

    ProcessedTransactionIds.Reset();
    TSet<FName> SeenTransactions;
    for (const FName TransactionId : Snapshot.ProcessedTransactionIds)
    {
        if (!TransactionId.IsNone()
            && !SeenTransactions.Contains(TransactionId)
            && ProcessedTransactionIds.Num() < JumanjiProfile::MaxProcessedTransactions)
        {
            SeenTransactions.Add(TransactionId);
            ProcessedTransactionIds.Add(TransactionId);
        }
    }

    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
    OnCompletedAdventureChaptersChanged.Broadcast(CompletedAdventureChapterIds);
    OnProgressionChanged.Broadcast(ProgressionState);
    RefreshCombatAttributesFromProgression();
    ForceNetUpdate();

    // Retry deterministic, idempotent chapter rewards after a profile restore.
    if (UJumanjiProgressionRewardSubsystem* RewardSubsystem =
        GameInstance->GetSubsystem<UJumanjiProgressionRewardSubsystem>())
    {
        RewardSubsystem->GrantMissingCompletedChapterRewards(this);
    }

    // Re-save normalized state. This also persists schema migrations and repaired fields.
    PersistProfileSnapshot();
    return true;
}

int64 AJumanjiPlayerState::GetCurrencyBalance(FName CurrencyId) const
{
    const FJumanjiCurrencyBalance* Found = ProgressionState.Currencies.FindByPredicate(
        [CurrencyId](const FJumanjiCurrencyBalance& Entry)
        {
            return Entry.CurrencyId == CurrencyId;
        });

    if (Found)
    {
        return Found->Amount;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiProgressionCatalogSubsystem* Catalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiProgressionCatalogSubsystem>()
        : nullptr;

    FJumanjiCurrencyDefinition Definition;
    return Catalog && Catalog->GetCurrencyDefinition(CurrencyId, Definition)
        ? Definition.StartingBalance
        : 0;
}

int32 AJumanjiPlayerState::GetItemQuantity(FName ItemId) const
{
    const FJumanjiInventoryStack* Found = ProgressionState.Inventory.FindByPredicate(
        [ItemId](const FJumanjiInventoryStack& Entry)
        {
            return Entry.ItemId == ItemId;
        });

    return Found ? Found->Quantity : 0;
}

int32 AJumanjiPlayerState::GetGreatSpiritLevel(FName CharacterId) const
{
    const FJumanjiCharacterProgress* Found = ProgressionState.CharacterProgress.FindByPredicate(
        [CharacterId](const FJumanjiCharacterProgress& Entry)
        {
            return Entry.CharacterId == CharacterId;
        });

    return Found ? Found->Level : 1;
}

bool AJumanjiPlayerState::HasProcessedProfileTransaction(FName TransactionId) const
{
    return !TransactionId.IsNone() && ProcessedTransactionIds.Contains(TransactionId);
}

bool AJumanjiPlayerState::RefreshCombatAttributesFromProgression()
{
    if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet || SelectedCharacterId.IsNone())
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiProgressionCatalogSubsystem* Catalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiProgressionCatalogSubsystem>()
        : nullptr;
    if (!Catalog)
    {
        return false;
    }

    FJumanjiCombatProgressionValues Values;
    Catalog->GetCombatProgressionValues(GetGreatSpiritLevel(SelectedCharacterId), Values);

    const float OldMaxHealth = AttributeSet->GetMaxHealth();
    const float OldMaxEnergy = AttributeSet->GetMaxEnergy();
    const float OldMaxResonance = AttributeSet->GetMaxResonance();

    const float HealthRatio = OldMaxHealth > KINDA_SMALL_NUMBER
        ? FMath::Clamp(AttributeSet->GetHealth() / OldMaxHealth, 0.0f, 1.0f)
        : 1.0f;
    const float EnergyRatio = OldMaxEnergy > KINDA_SMALL_NUMBER
        ? FMath::Clamp(AttributeSet->GetEnergy() / OldMaxEnergy, 0.0f, 1.0f)
        : 1.0f;
    const float ResonanceRatio = OldMaxResonance > KINDA_SMALL_NUMBER
        ? FMath::Clamp(AttributeSet->GetResonance() / OldMaxResonance, 0.0f, 1.0f)
        : 0.0f;

    AbilitySystemComponent->SetNumericAttributeBase(
        UJumanjiAttributeSet::GetMaxHealthAttribute(), Values.MaxHealth);
    AbilitySystemComponent->SetNumericAttributeBase(
        UJumanjiAttributeSet::GetHealthAttribute(), Values.MaxHealth * HealthRatio);
    AbilitySystemComponent->SetNumericAttributeBase(
        UJumanjiAttributeSet::GetMaxEnergyAttribute(), Values.MaxEnergy);
    AbilitySystemComponent->SetNumericAttributeBase(
        UJumanjiAttributeSet::GetEnergyAttribute(), Values.MaxEnergy * EnergyRatio);
    AbilitySystemComponent->SetNumericAttributeBase(
        UJumanjiAttributeSet::GetMaxResonanceAttribute(), Values.MaxResonance);
    AbilitySystemComponent->SetNumericAttributeBase(
        UJumanjiAttributeSet::GetResonanceAttribute(), Values.MaxResonance * ResonanceRatio);
    AbilitySystemComponent->SetNumericAttributeBase(
        UJumanjiAttributeSet::GetAttackPowerAttribute(), Values.AttackPower);
    AbilitySystemComponent->SetNumericAttributeBase(
        UJumanjiAttributeSet::GetDefenseAttribute(), Values.Defense);

    return true;
}

bool AJumanjiPlayerState::ApplyProfileTransaction(
    const FJumanjiProfileTransaction& Transaction)
{
    if (!HasAuthority()
        || !bProfilePersistenceEnabled
        || PersistentProfileId.IsEmpty()
        || Transaction.TransactionId.IsNone()
        || Transaction.TransactionId.ToString().Len() > 96
        || ProcessedTransactionIds.Contains(Transaction.TransactionId)
        || ProcessedTransactionIds.Num() >= JumanjiProfile::MaxProcessedTransactions
        || Transaction.CurrencyDeltas.Num() > 8
        || Transaction.ItemDeltas.Num() > 32
        || Transaction.AccountExperienceDelta < 0
        || Transaction.AccountExperienceDelta > JumanjiProfile::MaxExperienceDeltaPerTransaction
        || Transaction.CharacterExperienceDelta < 0
        || Transaction.CharacterExperienceDelta > JumanjiProfile::MaxExperienceDeltaPerTransaction)
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiProgressionCatalogSubsystem* Catalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiProgressionCatalogSubsystem>()
        : nullptr;
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;

    if (!Catalog || !Roster)
    {
        return false;
    }

    const bool bHasCurrencyChanges = !Transaction.CurrencyDeltas.IsEmpty();
    const bool bHasItemChanges = !Transaction.ItemDeltas.IsEmpty();
    const bool bHasAccountExperience = Transaction.AccountExperienceDelta > 0;
    const bool bHasCharacterExperience = Transaction.CharacterExperienceDelta > 0;

    if (!bHasCurrencyChanges && !bHasItemChanges
        && !bHasAccountExperience && !bHasCharacterExperience)
    {
        return false;
    }

    if (bHasCharacterExperience
        && (Transaction.CharacterId.IsNone()
            || !Roster->IsPlayableCharacter(Transaction.CharacterId)))
    {
        return false;
    }

    // All validation and mutations are performed on copies. Nothing is
    // committed to PlayerState unless every part of the transaction is valid.
    FJumanjiPlayerProgressionState Candidate = ProgressionState;

    TSet<FName> SeenCurrencies;
    for (const FJumanjiCurrencyDelta& Delta : Transaction.CurrencyDeltas)
    {
        FJumanjiCurrencyDefinition Definition;
        if (Delta.CurrencyId.IsNone()
            || SeenCurrencies.Contains(Delta.CurrencyId)
            || Delta.Delta == 0
            || Delta.Delta < -JumanjiProfile::MaxCurrencyDeltaPerTransaction
            || Delta.Delta > JumanjiProfile::MaxCurrencyDeltaPerTransaction
            || !Catalog->GetCurrencyDefinition(Delta.CurrencyId, Definition))
        {
            return false;
        }

        SeenCurrencies.Add(Delta.CurrencyId);

        FJumanjiCurrencyBalance* Balance = Candidate.Currencies.FindByPredicate(
            [&Delta](const FJumanjiCurrencyBalance& Entry)
            {
                return Entry.CurrencyId == Delta.CurrencyId;
            });

        const int64 CurrentAmount = Balance
            ? Balance->Amount
            : Definition.StartingBalance;

        if (Delta.Delta < 0 && CurrentAmount < -Delta.Delta)
        {
            return false;
        }
        if (Delta.Delta > 0 && CurrentAmount > Definition.MaxBalance - Delta.Delta)
        {
            return false;
        }

        const int64 NewAmount = CurrentAmount + Delta.Delta;
        if (NewAmount < 0 || NewAmount > Definition.MaxBalance)
        {
            return false;
        }

        if (!Balance)
        {
            FJumanjiCurrencyBalance NewBalance;
            NewBalance.CurrencyId = Delta.CurrencyId;
            NewBalance.Amount = NewAmount;
            Candidate.Currencies.Add(NewBalance);
        }
        else
        {
            Balance->Amount = NewAmount;
        }
    }

    TSet<FName> SeenItems;
    for (const FJumanjiItemDelta& Delta : Transaction.ItemDeltas)
    {
        FJumanjiItemDefinition Definition;
        if (Delta.ItemId.IsNone()
            || SeenItems.Contains(Delta.ItemId)
            || Delta.Delta == 0
            || Delta.Delta < -9999
            || Delta.Delta > 9999
            || !Catalog->GetItemDefinition(Delta.ItemId, Definition))
        {
            return false;
        }

        SeenItems.Add(Delta.ItemId);

        FJumanjiInventoryStack* Stack = Candidate.Inventory.FindByPredicate(
            [&Delta](const FJumanjiInventoryStack& Entry)
            {
                return Entry.ItemId == Delta.ItemId;
            });

        const int32 CurrentQuantity = Stack ? Stack->Quantity : 0;
        if (Delta.Delta < 0 && CurrentQuantity < -Delta.Delta)
        {
            return false;
        }

        const int32 NewQuantity = CurrentQuantity + Delta.Delta;
        if (NewQuantity < 0 || NewQuantity > Definition.MaxStack)
        {
            return false;
        }

        if (!Stack && NewQuantity > 0)
        {
            if (Candidate.Inventory.Num() >= JumanjiProfile::MaxInventoryStacks)
            {
                return false;
            }

            FJumanjiInventoryStack NewStack;
            NewStack.ItemId = Delta.ItemId;
            NewStack.Quantity = NewQuantity;
            Candidate.Inventory.Add(NewStack);
        }
        else if (Stack && NewQuantity == 0)
        {
            Candidate.Inventory.RemoveAll(
                [&Delta](const FJumanjiInventoryStack& Entry)
                {
                    return Entry.ItemId == Delta.ItemId;
                });
        }
        else if (Stack)
        {
            Stack->Quantity = NewQuantity;
        }
    }

    if (bHasAccountExperience)
    {
        Candidate.AccountExperience += Transaction.AccountExperienceDelta;

        while (Candidate.AccountLevel < Catalog->GetMaxAccountLevel())
        {
            const int64 RequiredXp =
                Catalog->GetAccountExperienceForNextLevel(Candidate.AccountLevel);
            if (RequiredXp <= 0 || Candidate.AccountExperience < RequiredXp)
            {
                break;
            }

            Candidate.AccountExperience -= RequiredXp;
            ++Candidate.AccountLevel;
        }

        if (Candidate.AccountLevel >= Catalog->GetMaxAccountLevel())
        {
            Candidate.AccountExperience = 0;
        }
    }

    if (bHasCharacterExperience)
    {
        FJumanjiCharacterProgress* CharacterProgress =
            Candidate.CharacterProgress.FindByPredicate(
                [&Transaction](const FJumanjiCharacterProgress& Entry)
                {
                    return Entry.CharacterId == Transaction.CharacterId;
                });

        if (!CharacterProgress)
        {
            if (Candidate.CharacterProgress.Num() >= 9)
            {
                return false;
            }

            FJumanjiCharacterProgress NewProgress;
            NewProgress.CharacterId = Transaction.CharacterId;
            Candidate.CharacterProgress.Add(NewProgress);
            CharacterProgress = &Candidate.CharacterProgress.Last();
        }

        CharacterProgress->Experience += Transaction.CharacterExperienceDelta;

        while (CharacterProgress->Level < Catalog->GetMaxCharacterLevel())
        {
            const int64 RequiredXp =
                Catalog->GetCharacterExperienceForNextLevel(CharacterProgress->Level);
            if (RequiredXp <= 0 || CharacterProgress->Experience < RequiredXp)
            {
                break;
            }

            CharacterProgress->Experience -= RequiredXp;
            ++CharacterProgress->Level;
        }

        if (CharacterProgress->Level >= Catalog->GetMaxCharacterLevel())
        {
            CharacterProgress->Experience = 0;
        }
    }

    if (Candidate.Revision < MAX_int32)
    {
        ++Candidate.Revision;
    }

    ProgressionState = MoveTemp(Candidate);
    ProcessedTransactionIds.Add(Transaction.TransactionId);

    if (bHasCharacterExperience && Transaction.CharacterId == SelectedCharacterId)
    {
        RefreshCombatAttributesFromProgression();
    }

    OnProgressionChanged.Broadcast(ProgressionState);
    ForceNetUpdate();
    PersistProfileSnapshot();
    return true;
}

bool AJumanjiPlayerState::SetEquippedAbilitySlot(
    FName CharacterId,
    FName SlotId,
    FName AbilityId)
{
    if (!HasAuthority() || !bProfilePersistenceEnabled || PersistentProfileId.IsEmpty()
        || CharacterId.IsNone() || AbilityId.IsNone())
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;
    const UJumanjiProgressionCatalogSubsystem* Catalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiProgressionCatalogSubsystem>()
        : nullptr;
    const UJumanjiAbilityCatalogSubsystem* AbilityCatalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiAbilityCatalogSubsystem>()
        : nullptr;

    if (!Roster || !Catalog || !AbilityCatalog
        || !Roster->IsPlayableCharacter(CharacterId)
        || !Catalog->IsValidAbilitySlotId(SlotId)
        || !AbilityCatalog->IsValidAbilityChoice(CharacterId, SlotId, AbilityId)
        || AbilityId.ToString().Len() > 128)
    {
        return false;
    }

    FJumanjiEquippedAbilitySlot* Existing = ProgressionState.EquippedAbilitySlots.FindByPredicate(
        [CharacterId, SlotId](const FJumanjiEquippedAbilitySlot& Entry)
        {
            return Entry.CharacterId == CharacterId && Entry.SlotId == SlotId;
        });

    if (Existing && Existing->AbilityId == AbilityId)
    {
        return false;
    }

    if (!Existing && ProgressionState.EquippedAbilitySlots.Num() >= 45)
    {
        return false;
    }

    if (Existing)
    {
        Existing->AbilityId = AbilityId;
    }
    else
    {
        FJumanjiEquippedAbilitySlot NewSlot;
        NewSlot.CharacterId = CharacterId;
        NewSlot.SlotId = SlotId;
        NewSlot.AbilityId = AbilityId;
        ProgressionState.EquippedAbilitySlots.Add(NewSlot);
    }

    if (ProgressionState.Revision < MAX_int32)
    {
        ++ProgressionState.Revision;
    }

    OnProgressionChanged.Broadcast(ProgressionState);
    ForceNetUpdate();
    PersistProfileSnapshot();
    return true;
}

bool AJumanjiPlayerState::MarkAdventureChapterCompleted(FName ChapterId)
{
    if (!HasAuthority() || ChapterId.IsNone() || CompletedAdventureChapterIds.Contains(ChapterId))
    {
        return false;
    }

    if (CompletedAdventureChapterIds.Num() >= JumanjiProfile::MaxCompletedChapters)
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI profile: completed chapter limit reached."));
        return false;
    }

    CompletedAdventureChapterIds.Add(ChapterId);
    OnCompletedAdventureChaptersChanged.Broadcast(CompletedAdventureChapterIds);
    ForceNetUpdate();
    PersistProfileSnapshot();
    return true;
}

bool AJumanjiPlayerState::HasCompletedAdventureChapter(FName ChapterId) const
{
    return !ChapterId.IsNone() && CompletedAdventureChapterIds.Contains(ChapterId);
}

bool AJumanjiPlayerState::DiscoverWorldRegion(FName RegionId)
{
    if (!HasAuthority() || RegionId.IsNone())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    if (!Atlas || !Atlas->IsKnownRegion(RegionId)
        || WorldExplorationState.DiscoveredRegionIds.Contains(RegionId))
    {
        return false;
    }

    WorldExplorationState.DiscoveredRegionIds.Add(RegionId);
    ++WorldExplorationState.Revision;
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
    ForceNetUpdate();
    PersistProfileSnapshot();
    return true;
}

bool AJumanjiPlayerState::VisitWorldLandmark(FName LandmarkId)
{
    if (!HasAuthority() || LandmarkId.IsNone())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const UJumanjiWorldAtlasSubsystem* Atlas = World
        ? World->GetSubsystem<UJumanjiWorldAtlasSubsystem>()
        : nullptr;

    FJumanjiWorldLandmarkDefinition Landmark;
    if (!Atlas
        || !Atlas->GetLandmarkDefinition(LandmarkId, Landmark)
        || WorldExplorationState.VisitedLandmarkIds.Contains(LandmarkId))
    {
        return false;
    }

    if (!WorldExplorationState.DiscoveredRegionIds.Contains(Landmark.RegionId))
    {
        WorldExplorationState.DiscoveredRegionIds.Add(Landmark.RegionId);
    }

    WorldExplorationState.VisitedLandmarkIds.Add(LandmarkId);
    ++WorldExplorationState.Revision;
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
    ForceNetUpdate();
    PersistProfileSnapshot();
    return true;
}

bool AJumanjiPlayerState::HasDiscoveredWorldRegion(FName RegionId) const
{
    return !RegionId.IsNone()
        && WorldExplorationState.DiscoveredRegionIds.Contains(RegionId);
}

bool AJumanjiPlayerState::HasVisitedWorldLandmark(FName LandmarkId) const
{
    return !LandmarkId.IsNone()
        && WorldExplorationState.VisitedLandmarkIds.Contains(LandmarkId);
}

FJumanjiPlayerProfileSnapshot AJumanjiPlayerState::BuildProfileSnapshot() const
{
    FJumanjiPlayerProfileSnapshot Snapshot;
    Snapshot.SchemaVersion = 2;
    Snapshot.ProfileId = PersistentProfileId;
    Snapshot.SelectedCharacterId = SelectedCharacterId;
    Snapshot.DiscoveredRegionIds = WorldExplorationState.DiscoveredRegionIds;
    Snapshot.VisitedLandmarkIds = WorldExplorationState.VisitedLandmarkIds;
    Snapshot.ExplorationRevision = WorldExplorationState.Revision;
    Snapshot.CompletedAdventureChapterIds = CompletedAdventureChapterIds;
    Snapshot.Progression = ProgressionState;
    Snapshot.ProcessedTransactionIds = ProcessedTransactionIds;
    Snapshot.LastSavedUtc = FDateTime::UtcNow();
    return Snapshot;
}

void AJumanjiPlayerState::PersistProfileSnapshot()
{
    if (!HasAuthority() || !bProfilePersistenceEnabled || PersistentProfileId.IsEmpty())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UJumanjiPlayerProfileSubsystem* ProfileSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiPlayerProfileSubsystem>()
        : nullptr;

    if (!ProfileSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI profile: persistence subsystem is unavailable."));
        return;
    }

    if (!ProfileSubsystem->SaveProfile(BuildProfileSnapshot()))
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI profile: snapshot was rejected before save could be queued."));
    }
}

void AJumanjiPlayerState::OnRep_WorldExplorationState()
{
    OnWorldExplorationChanged.Broadcast(WorldExplorationState);
}

void AJumanjiPlayerState::OnRep_SelectedCharacterId()
{
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
}

void AJumanjiPlayerState::OnRep_CompletedAdventureChapterIds()
{
    OnCompletedAdventureChaptersChanged.Broadcast(CompletedAdventureChapterIds);
}

void AJumanjiPlayerState::OnRep_ProgressionState()
{
    OnProgressionChanged.Broadcast(ProgressionState);
}

void AJumanjiPlayerState::OnRep_EncounterReconnectState()
{
    OnEncounterReconnectChanged.Broadcast(EncounterReconnectState);
}

void AJumanjiPlayerState::OnRep_EncounterReconnectFeedbackState()
{
    OnEncounterReconnectFeedbackChanged.Broadcast(EncounterReconnectFeedbackState);
}

void AJumanjiPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiPlayerState, SelectedCharacterId);
    DOREPLIFETIME(AJumanjiPlayerState, WorldExplorationState);
    DOREPLIFETIME(AJumanjiPlayerState, CompletedAdventureChapterIds);
    DOREPLIFETIME(AJumanjiPlayerState, ProgressionState);
    DOREPLIFETIME(AJumanjiPlayerState, EncounterReconnectState);
    DOREPLIFETIME(AJumanjiPlayerState, EncounterReconnectFeedbackState);
}
