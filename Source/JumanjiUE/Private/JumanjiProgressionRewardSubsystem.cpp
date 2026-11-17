#include "JumanjiProgressionRewardSubsystem.h"

#include "JumanjiPlayerState.h"
#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiProgressionCatalogSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Misc/SecureHash.h"

namespace JumanjiRewards
{
    constexpr int64 MaxExperienceReward = 1000000LL;
    constexpr int32 MaxCurrencyRows = 8;
    constexpr int32 MaxItemRows = 32;
    constexpr int64 MaxCurrencyCost = 2000000000LL;
    constexpr int32 MaxItemCost = 9999;
}

void UJumanjiProgressionRewardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BuildDefaultTables();
    LoadOptionalDataTableOverrides();
}

void UJumanjiProgressionRewardSubsystem::BuildDefaultTables()
{
    ChapterRewards.Reset();
    UpgradeRecipes.Reset();
    TraversalRewards.Reset();
    WorldEncounterRewards.Reset();

    FJumanjiChapterRewardDefinition ChapterReward;
    ChapterReward.ChapterId = FName(TEXT("EMERALD_JUNGLE_NINE_SEALS"));
    ChapterReward.AccountExperienceReward = 300;
    ChapterReward.ActiveCharacterExperienceReward = 180;

    FJumanjiCurrencyDelta Coins;
    Coins.CurrencyId = FName(TEXT("CURRENCY_EXPLORER_COINS"));
    Coins.Delta = 150;
    ChapterReward.CurrencyRewards.Add(Coins);

    FJumanjiCurrencyDelta Seals;
    Seals.CurrencyId = FName(TEXT("CURRENCY_BOARD_SEALS"));
    Seals.Delta = 2;
    ChapterReward.CurrencyRewards.Add(Seals);

    FJumanjiItemDelta SpiritMemory;
    SpiritMemory.ItemId = FName(TEXT("MAT_SPIRIT_MEMORY"));
    SpiritMemory.Delta = 1;
    ChapterReward.ItemRewards.Add(SpiritMemory);

    if (IsValidChapterReward(ChapterReward))
    {
        ChapterRewards.Add(ChapterReward.ChapterId, ChapterReward);
    }

    FJumanjiWorldEncounterRewardDefinition EncounterReward;
    EncounterReward.EncounterId = FName(TEXT("ENCOUNTER_EMERALD_JUNGLE_CITADEL"));
    EncounterReward.RequiredChapterId = FName(TEXT("EMERALD_JUNGLE_NINE_SEALS"));
    EncounterReward.AccountExperienceReward = 500;
    EncounterReward.ActiveCharacterExperienceReward = 300;
    EncounterReward.CurrencyId = FName(TEXT("CURRENCY_EXPLORER_COINS"));
    EncounterReward.CurrencyAmount = 350;
    EncounterReward.ItemId = FName(TEXT("MAT_SPIRIT_MEMORY"));
    EncounterReward.ItemAmount = 2;

    if (IsValidWorldEncounterReward(EncounterReward))
    {
        WorldEncounterRewards.Add(EncounterReward.EncounterId, EncounterReward);
    }

    const auto AddTraversalReward = [this](const TCHAR* TraversalId, int64 ExplorerCoins)
    {
        FJumanjiTraversalRewardDefinition Definition;
        Definition.TraversalId = FName(TraversalId);
        Definition.CurrencyId = FName(TEXT("CURRENCY_EXPLORER_COINS"));
        Definition.CurrencyAmount = ExplorerCoins;
        if (IsValidTraversalReward(Definition))
        {
            TraversalRewards.Add(Definition.TraversalId, Definition);
        }
    };

    // One-time exploration rewards for the 12 stable traversal IDs in Stage 25.
    AddTraversalReward(TEXT("EJ_ROOT_CANOPY_JUMP_01"), 20);
    AddTraversalReward(TEXT("EJ_ROOT_TOTEM_CLIMB_01"), 25);
    AddTraversalReward(TEXT("EJ_TIDE_FALLS_ZIP_01"), 30);
    AddTraversalReward(TEXT("EJ_TIDE_CHANNEL_JUMP_01"), 20);
    AddTraversalReward(TEXT("EJ_STONE_PILLAR_CLIMB_01"), 25);
    AddTraversalReward(TEXT("EJ_STORM_PEAK_ZIP_01"), 30);
    AddTraversalReward(TEXT("EJ_EMBER_CAVERN_JUMP_01"), 20);
    AddTraversalReward(TEXT("EJ_DAWN_MIRRORS_LIFT_01"), 40);
    AddTraversalReward(TEXT("EJ_SHADOW_MARSH_ZIP_01"), 30);
    AddTraversalReward(TEXT("EJ_LIFE_GARDEN_CLIMB_01"), 25);
    AddTraversalReward(TEXT("EJ_FROST_GATE_LIFT_01"), 40);
    AddTraversalReward(TEXT("EJ_FROST_GATE_JUMP_01"), 20);

    const auto AddUpgradeRecipe = [this](
        const TCHAR* RecipeId,
        const TCHAR* CharacterId,
        const TCHAR* MaterialId,
        int32 MaterialAmount,
        int64 CoinCost,
        int64 CharacterExperience)
    {
        FJumanjiSpiritUpgradeRecipe Recipe;
        Recipe.RecipeId = FName(RecipeId);
        Recipe.CharacterId = FName(CharacterId);
        Recipe.CharacterExperienceReward = CharacterExperience;

        FJumanjiCurrencyCost CurrencyCost;
        CurrencyCost.CurrencyId = FName(TEXT("CURRENCY_EXPLORER_COINS"));
        CurrencyCost.Amount = CoinCost;
        Recipe.CurrencyCosts.Add(CurrencyCost);

        FJumanjiItemCost ItemCost;
        ItemCost.ItemId = FName(MaterialId);
        ItemCost.Amount = MaterialAmount;
        Recipe.ItemCosts.Add(ItemCost);

        if (IsValidUpgradeRecipe(Recipe))
        {
            UpgradeRecipes.Add(Recipe.RecipeId, Recipe);
        }
    };

    AddUpgradeRecipe(TEXT("UPGRADE_ROOT_SPIRIT_01"), TEXT("GREAT_SPIRIT_I"), TEXT("MAT_ROOT_RESIN"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_TIDE_SPIRIT_01"), TEXT("GREAT_SPIRIT_II"), TEXT("MAT_TIDE_PEARL"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_STONE_SPIRIT_01"), TEXT("GREAT_SPIRIT_III"), TEXT("MAT_STONE_CORE"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_STORM_SPIRIT_01"), TEXT("GREAT_SPIRIT_IV"), TEXT("MAT_STORM_ESSENCE"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_EMBER_SPIRIT_01"), TEXT("GREAT_SPIRIT_V"), TEXT("MAT_EMBER_ASH"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_DAWN_SPIRIT_01"), TEXT("GREAT_SPIRIT_VI"), TEXT("MAT_DAWN_SHARD"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_SHADOW_SPIRIT_01"), TEXT("GREAT_SPIRIT_VII"), TEXT("MAT_SHADOW_SILK"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_LIFE_SPIRIT_01"), TEXT("GREAT_SPIRIT_VIII"), TEXT("MAT_LIFE_BLOOM"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_FROST_SPIRIT_01"), TEXT("GREAT_SPIRIT_IX"), TEXT("MAT_FROST_CRYSTAL"), 2, 100, 120);
}

void UJumanjiProgressionRewardSubsystem::LoadOptionalDataTableOverrides()
{
    // Optional tables are authored at these content paths in Unreal Editor.
    // Missing assets intentionally leave the validated native starter catalog active.
    UDataTable* ChapterTable = LoadObject<UDataTable>(
        nullptr,
        TEXT("/Game/Progression/Data/DT_ChapterRewards.DT_ChapterRewards"));

    if (ChapterTable)
    {
        TArray<FJumanjiChapterRewardDefinition*> Rows;
        ChapterTable->GetAllRows<FJumanjiChapterRewardDefinition>(
            TEXT("JumanjiChapterRewardOverrides"), Rows);

        for (const FJumanjiChapterRewardDefinition* Row : Rows)
        {
            if (Row && IsValidChapterReward(*Row))
            {
                ChapterRewards.Add(Row->ChapterId, *Row);
            }
        }
    }

    UDataTable* UpgradeTable = LoadObject<UDataTable>(
        nullptr,
        TEXT("/Game/Progression/Data/DT_SpiritUpgradeRecipes.DT_SpiritUpgradeRecipes"));

    if (UpgradeTable)
    {
        TArray<FJumanjiSpiritUpgradeRecipe*> Rows;
        UpgradeTable->GetAllRows<FJumanjiSpiritUpgradeRecipe>(
            TEXT("JumanjiSpiritUpgradeOverrides"), Rows);

        for (const FJumanjiSpiritUpgradeRecipe* Row : Rows)
        {
            if (Row && IsValidUpgradeRecipe(*Row))
            {
                UpgradeRecipes.Add(Row->RecipeId, *Row);
            }
        }
    }

    UDataTable* WorldEncounterRewardTable = LoadObject<UDataTable>(
        nullptr,
        TEXT("/Game/Progression/Data/DT_WorldEncounterRewards.DT_WorldEncounterRewards"));

    if (WorldEncounterRewardTable
        && WorldEncounterRewardTable->GetRowStruct() == FJumanjiWorldEncounterRewardDefinition::StaticStruct())
    {
        TArray<FJumanjiWorldEncounterRewardDefinition*> Rows;
        WorldEncounterRewardTable->GetAllRows<FJumanjiWorldEncounterRewardDefinition>(
            TEXT("JumanjiWorldEncounterRewardOverrides"), Rows);

        for (const FJumanjiWorldEncounterRewardDefinition* Row : Rows)
        {
            if (Row && IsValidWorldEncounterReward(*Row))
            {
                WorldEncounterRewards.Add(Row->EncounterId, *Row);
            }
        }
    }

    UDataTable* TraversalRewardTable = LoadObject<UDataTable>(
        nullptr,
        TEXT("/Game/Progression/Data/DT_TraversalRewards.DT_TraversalRewards"));

    if (TraversalRewardTable)
    {
        TArray<FJumanjiTraversalRewardDefinition*> Rows;
        TraversalRewardTable->GetAllRows<FJumanjiTraversalRewardDefinition>(
            TEXT("JumanjiTraversalRewardOverrides"), Rows);

        for (const FJumanjiTraversalRewardDefinition* Row : Rows)
        {
            if (Row && IsValidTraversalReward(*Row))
            {
                TraversalRewards.Add(Row->TraversalId, *Row);
            }
        }
    }
}

bool UJumanjiProgressionRewardSubsystem::IsValidTraversalReward(
    const FJumanjiTraversalRewardDefinition& Definition) const
{
    static const TSet<FName> AllowedCurrencyIds = {
        FName(TEXT("CURRENCY_EXPLORER_COINS"))
    };
    static const TSet<FName> AllowedItemIds = {
        FName(TEXT("MAT_ROOT_RESIN")),
        FName(TEXT("MAT_TIDE_PEARL")),
        FName(TEXT("MAT_STONE_CORE")),
        FName(TEXT("MAT_STORM_ESSENCE")),
        FName(TEXT("MAT_EMBER_ASH")),
        FName(TEXT("MAT_DAWN_SHARD")),
        FName(TEXT("MAT_SHADOW_SILK")),
        FName(TEXT("MAT_LIFE_BLOOM")),
        FName(TEXT("MAT_FROST_CRYSTAL")),
        FName(TEXT("MAT_SPIRIT_MEMORY"))
    };

    if (Definition.TraversalId.IsNone()
        || Definition.TraversalId.ToString().Len() > 64
        || !AllowedCurrencyIds.Contains(Definition.CurrencyId)
        || Definition.CurrencyAmount < 1
        || Definition.CurrencyAmount > 5000
        || Definition.ItemAmount < 0
        || Definition.ItemAmount > JumanjiRewards::MaxItemCost
        || (Definition.ItemAmount == 0 && !Definition.ItemId.IsNone())
        || (Definition.ItemAmount > 0 && !AllowedItemIds.Contains(Definition.ItemId)))
    {
        return false;
    }

    return true;
}

bool UJumanjiProgressionRewardSubsystem::IsValidWorldEncounterReward(
    const FJumanjiWorldEncounterRewardDefinition& Definition) const
{
    if (Definition.EncounterId.IsNone()
        || Definition.EncounterId.ToString().Len() > 64
        || Definition.RequiredChapterId.IsNone()
        || Definition.RequiredChapterId.ToString().Len() > 64
        || Definition.AccountExperienceReward < 0
        || Definition.AccountExperienceReward > JumanjiRewards::MaxExperienceReward
        || Definition.ActiveCharacterExperienceReward < 0
        || Definition.ActiveCharacterExperienceReward > JumanjiRewards::MaxExperienceReward
        || Definition.CurrencyAmount < 0
        || Definition.CurrencyAmount > JumanjiRewards::MaxCurrencyCost
        || Definition.ItemAmount < 0
        || Definition.ItemAmount > JumanjiRewards::MaxItemCost
        || (Definition.CurrencyAmount == 0 && !Definition.CurrencyId.IsNone())
        || (Definition.CurrencyAmount > 0 && Definition.CurrencyId.IsNone())
        || (Definition.ItemAmount == 0 && !Definition.ItemId.IsNone())
        || (Definition.ItemAmount > 0 && Definition.ItemId.IsNone())
        || (Definition.AccountExperienceReward == 0
            && Definition.ActiveCharacterExperienceReward == 0
            && Definition.CurrencyAmount == 0
            && Definition.ItemAmount == 0))
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

    if (Definition.CurrencyAmount > 0)
    {
        FJumanjiCurrencyDefinition Currency;
        if (!Catalog->GetCurrencyDefinition(Definition.CurrencyId, Currency))
        {
            return false;
        }
    }

    if (Definition.ItemAmount > 0)
    {
        FJumanjiItemDefinition Item;
        if (!Catalog->GetItemDefinition(Definition.ItemId, Item) || !Item.bCanBeConsumed)
        {
            return false;
        }
    }

    return true;
}

bool UJumanjiProgressionRewardSubsystem::IsValidChapterReward(
    const FJumanjiChapterRewardDefinition& Definition) const
{
    if (Definition.ChapterId.IsNone()
        || Definition.AccountExperienceReward < 0
        || Definition.AccountExperienceReward > JumanjiRewards::MaxExperienceReward
        || Definition.ActiveCharacterExperienceReward < 0
        || Definition.ActiveCharacterExperienceReward > JumanjiRewards::MaxExperienceReward
        || Definition.CurrencyRewards.Num() > JumanjiRewards::MaxCurrencyRows
        || Definition.ItemRewards.Num() > JumanjiRewards::MaxItemRows
        || (Definition.AccountExperienceReward == 0
            && Definition.ActiveCharacterExperienceReward == 0
            && Definition.CurrencyRewards.IsEmpty()
            && Definition.ItemRewards.IsEmpty()))
    {
        return false;
    }

    TSet<FName> SeenCurrencyIds;
    for (const FJumanjiCurrencyDelta& Currency : Definition.CurrencyRewards)
    {
        if (Currency.CurrencyId.IsNone()
            || SeenCurrencyIds.Contains(Currency.CurrencyId)
            || Currency.Delta <= 0
            || Currency.Delta > JumanjiRewards::MaxCurrencyCost)
        {
            return false;
        }
        SeenCurrencyIds.Add(Currency.CurrencyId);
    }

    TSet<FName> SeenItemIds;
    for (const FJumanjiItemDelta& Item : Definition.ItemRewards)
    {
        if (Item.ItemId.IsNone()
            || SeenItemIds.Contains(Item.ItemId)
            || Item.Delta <= 0
            || Item.Delta > JumanjiRewards::MaxItemCost)
        {
            return false;
        }
        SeenItemIds.Add(Item.ItemId);
    }

    return true;
}

bool UJumanjiProgressionRewardSubsystem::IsValidUpgradeRecipe(
    const FJumanjiSpiritUpgradeRecipe& Recipe) const
{
    if (Recipe.RecipeId.IsNone()
        || Recipe.CharacterId.IsNone()
        || Recipe.CharacterExperienceReward < 1
        || Recipe.CharacterExperienceReward > JumanjiRewards::MaxExperienceReward
        || Recipe.CurrencyCosts.Num() > JumanjiRewards::MaxCurrencyRows
        || Recipe.ItemCosts.Num() > JumanjiRewards::MaxItemRows
        || (Recipe.CurrencyCosts.IsEmpty() && Recipe.ItemCosts.IsEmpty()))
    {
        return false;
    }

    TSet<FName> SeenCurrencyIds;
    for (const FJumanjiCurrencyCost& Cost : Recipe.CurrencyCosts)
    {
        if (Cost.CurrencyId.IsNone()
            || SeenCurrencyIds.Contains(Cost.CurrencyId)
            || Cost.Amount < 1
            || Cost.Amount > JumanjiRewards::MaxCurrencyCost)
        {
            return false;
        }
        SeenCurrencyIds.Add(Cost.CurrencyId);
    }

    TSet<FName> SeenItemIds;
    for (const FJumanjiItemCost& Cost : Recipe.ItemCosts)
    {
        if (Cost.ItemId.IsNone()
            || SeenItemIds.Contains(Cost.ItemId)
            || Cost.Amount < 1
            || Cost.Amount > JumanjiRewards::MaxItemCost)
        {
            return false;
        }
        SeenItemIds.Add(Cost.ItemId);
    }

    return true;
}

bool UJumanjiProgressionRewardSubsystem::GetTraversalRewardDefinition(
    FName TraversalId,
    FJumanjiTraversalRewardDefinition& OutDefinition) const
{
    const FJumanjiTraversalRewardDefinition* Found = TraversalRewards.Find(TraversalId);
    if (!Found)
    {
        return false;
    }

    OutDefinition = *Found;
    return true;
}

bool UJumanjiProgressionRewardSubsystem::GetWorldEncounterRewardDefinition(
    FName EncounterId,
    FJumanjiWorldEncounterRewardDefinition& OutDefinition) const
{
    const FJumanjiWorldEncounterRewardDefinition* Found = WorldEncounterRewards.Find(EncounterId);
    if (!Found)
    {
        return false;
    }

    OutDefinition = *Found;
    return true;
}

FName UJumanjiProgressionRewardSubsystem::BuildWorldEncounterTransactionId(
    const AJumanjiPlayerState& PlayerState,
    FName EncounterId) const
{
    const FString ProfileHash = FMD5::HashAnsiString(*PlayerState.GetPersistentProfileId()).Left(16);
    const FString SafeEncounterId = EncounterId.ToString().Left(64);
    return FName(*FString::Printf(
        TEXT("ENC_%s_%s"),
        *ProfileHash,
        *SafeEncounterId));
}

bool UJumanjiProgressionRewardSubsystem::HasCompletedTraversalReward(
    const AJumanjiPlayerState* PlayerState,
    FName TraversalId) const
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || PlayerState->GetPersistentProfileId().IsEmpty()
        || TraversalId.IsNone())
    {
        return false;
    }

    FJumanjiTraversalRewardDefinition Definition;
    if (!GetTraversalRewardDefinition(TraversalId, Definition))
    {
        return false;
    }

    return PlayerState->HasProcessedProfileTransaction(
        BuildTraversalTransactionId(*PlayerState, TraversalId));
}

bool UJumanjiProgressionRewardSubsystem::GrantWorldEncounterCompletionReward(
    AJumanjiPlayerState* PlayerState,
    FName EncounterId) const
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || PlayerState->GetPersistentProfileId().IsEmpty()
        || EncounterId.IsNone())
    {
        return false;
    }

    FJumanjiWorldEncounterRewardDefinition Definition;
    if (!GetWorldEncounterRewardDefinition(EncounterId, Definition)
        || !PlayerState->HasCompletedAdventureChapter(Definition.RequiredChapterId))
    {
        return false;
    }

    const FName TransactionId = BuildWorldEncounterTransactionId(*PlayerState, EncounterId);
    if (PlayerState->HasProcessedProfileTransaction(TransactionId))
    {
        return true;
    }

    FJumanjiProfileTransaction Transaction;
    Transaction.TransactionId = TransactionId;
    Transaction.AccountExperienceDelta = Definition.AccountExperienceReward;
    Transaction.CharacterId = PlayerState->GetSelectedCharacterId();
    Transaction.CharacterExperienceDelta = Definition.ActiveCharacterExperienceReward;

    if (Definition.CurrencyAmount > 0)
    {
        FJumanjiCurrencyDelta Currency;
        Currency.CurrencyId = Definition.CurrencyId;
        Currency.Delta = Definition.CurrencyAmount;
        Transaction.CurrencyDeltas.Add(Currency);
    }

    if (Definition.ItemAmount > 0)
    {
        FJumanjiItemDelta Item;
        Item.ItemId = Definition.ItemId;
        Item.Delta = Definition.ItemAmount;
        Transaction.ItemDeltas.Add(Item);
    }

    return PlayerState->ApplyProfileTransaction(Transaction);
}

FName UJumanjiProgressionRewardSubsystem::BuildTraversalTransactionId(
    const AJumanjiPlayerState& PlayerState,
    FName TraversalId) const
{
    const FString ProfileHash = FMD5::HashAnsiString(*PlayerState.GetPersistentProfileId()).Left(16);
    const FString SafeTraversalId = TraversalId.ToString().Left(64);
    return FName(*FString::Printf(
        TEXT("TRAV_%s_%s"),
        *ProfileHash,
        *SafeTraversalId));
}

bool UJumanjiProgressionRewardSubsystem::GrantTraversalCompletionReward(
    AJumanjiPlayerState* PlayerState,
    FName TraversalId) const
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || PlayerState->GetPersistentProfileId().IsEmpty()
        || TraversalId.IsNone())
    {
        return false;
    }

    FJumanjiTraversalRewardDefinition Definition;
    if (!GetTraversalRewardDefinition(TraversalId, Definition))
    {
        return false;
    }

    const FName TransactionId = BuildTraversalTransactionId(*PlayerState, TraversalId);
    if (PlayerState->HasProcessedProfileTransaction(TransactionId))
    {
        return true;
    }

    FJumanjiProfileTransaction Transaction;
    Transaction.TransactionId = TransactionId;

    FJumanjiCurrencyDelta CurrencyReward;
    CurrencyReward.CurrencyId = Definition.CurrencyId;
    CurrencyReward.Delta = Definition.CurrencyAmount;
    Transaction.CurrencyDeltas.Add(CurrencyReward);

    if (Definition.ItemAmount > 0)
    {
        FJumanjiItemDelta ItemReward;
        ItemReward.ItemId = Definition.ItemId;
        ItemReward.Delta = Definition.ItemAmount;
        Transaction.ItemDeltas.Add(ItemReward);
    }

    return PlayerState->ApplyProfileTransaction(Transaction);
}

bool UJumanjiProgressionRewardSubsystem::GetChapterRewardDefinition(
    FName ChapterId,
    FJumanjiChapterRewardDefinition& OutDefinition) const
{
    const FJumanjiChapterRewardDefinition* Found = ChapterRewards.Find(ChapterId);
    if (!Found)
    {
        return false;
    }

    OutDefinition = *Found;
    return true;
}

bool UJumanjiProgressionRewardSubsystem::GetSpiritUpgradeRecipe(
    FName RecipeId,
    FJumanjiSpiritUpgradeRecipe& OutRecipe) const
{
    const FJumanjiSpiritUpgradeRecipe* Found = UpgradeRecipes.Find(RecipeId);
    if (!Found)
    {
        return false;
    }

    OutRecipe = *Found;
    return true;
}

FName UJumanjiProgressionRewardSubsystem::BuildChapterTransactionId(
    const AJumanjiPlayerState& PlayerState,
    FName ChapterId) const
{
    const FString ProfileHash = FMD5::HashAnsiString(*PlayerState.GetPersistentProfileId()).Left(16);
    const FString SafeChapterId = ChapterId.ToString().Left(48);
    return FName(*FString::Printf(TEXT("CHAPTER_%s_%s"), *SafeChapterId, *ProfileHash));
}

FName UJumanjiProgressionRewardSubsystem::BuildUpgradeTransactionId(
    const AJumanjiPlayerState& PlayerState,
    FName CharacterId,
    FName RecipeId) const
{
    const FString ProfileHash = FMD5::HashAnsiString(*PlayerState.GetPersistentProfileId()).Left(12);
    const FString SafeCharacterId = CharacterId.ToString().Left(20);
    const FString SafeRecipeId = RecipeId.ToString().Left(28);
    // A recipe is a one-time tier unlock. A stable key makes duplicate RPCs
    // idempotent even after the first successful transaction increments Revision.
    return FName(*FString::Printf(
        TEXT("UPG_%s_%s_%s"),
        *ProfileHash,
        *SafeCharacterId,
        *SafeRecipeId));
}

bool UJumanjiProgressionRewardSubsystem::GrantChapterCompletionReward(
    AJumanjiPlayerState* PlayerState,
    FName ChapterId) const
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || PlayerState->GetPersistentProfileId().IsEmpty()
        || ChapterId.IsNone()
        || !PlayerState->HasCompletedAdventureChapter(ChapterId))
    {
        return false;
    }

    const FName TransactionId = BuildChapterTransactionId(*PlayerState, ChapterId);
    if (PlayerState->HasProcessedProfileTransaction(TransactionId))
    {
        return true;
    }

    FJumanjiChapterRewardDefinition Definition;
    if (!GetChapterRewardDefinition(ChapterId, Definition))
    {
        return false;
    }

    FJumanjiProfileTransaction Transaction;
    Transaction.TransactionId = TransactionId;
    Transaction.AccountExperienceDelta = Definition.AccountExperienceReward;
    Transaction.CharacterId = PlayerState->GetSelectedCharacterId();
    Transaction.CharacterExperienceDelta = Definition.ActiveCharacterExperienceReward;
    Transaction.CurrencyDeltas = Definition.CurrencyRewards;
    Transaction.ItemDeltas = Definition.ItemRewards;

    return PlayerState->ApplyProfileTransaction(Transaction);
}

void UJumanjiProgressionRewardSubsystem::GrantMissingCompletedChapterRewards(
    AJumanjiPlayerState* PlayerState) const
{
    if (!IsValid(PlayerState) || !PlayerState->HasAuthority())
    {
        return;
    }

    for (const FName ChapterId : PlayerState->GetCompletedAdventureChapterIds())
    {
        // Only chapters present in the configured reward catalog have a package to retry.
        if (!ChapterRewards.Contains(ChapterId))
        {
            continue;
        }

        if (!PlayerState->HasProcessedProfileTransaction(BuildChapterTransactionId(*PlayerState, ChapterId)))
        {
            if (!GrantChapterCompletionReward(PlayerState, ChapterId))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("JUMANJI rewards: could not apply missing chapter reward '%s' for the restored profile."),
                    *ChapterId.ToString());
            }
        }
    }
}

bool UJumanjiProgressionRewardSubsystem::TryUpgradeGreatSpirit(
    AJumanjiPlayerState* PlayerState,
    FName CharacterId,
    FName RecipeId) const
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || PlayerState->GetPersistentProfileId().IsEmpty()
        || CharacterId.IsNone()
        || RecipeId.IsNone())
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;
    const UJumanjiProgressionCatalogSubsystem* ProgressionCatalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiProgressionCatalogSubsystem>()
        : nullptr;
    if (!Roster || !ProgressionCatalog || !Roster->IsPlayableCharacter(CharacterId)
        || PlayerState->GetGreatSpiritLevel(CharacterId) >= ProgressionCatalog->GetMaxCharacterLevel())
    {
        return false;
    }

    FJumanjiSpiritUpgradeRecipe Recipe;
    if (!GetSpiritUpgradeRecipe(RecipeId, Recipe)
        || Recipe.CharacterId != CharacterId)
    {
        return false;
    }

    FJumanjiProfileTransaction Transaction;
    Transaction.TransactionId = BuildUpgradeTransactionId(
        *PlayerState, CharacterId, RecipeId);
    Transaction.CharacterId = CharacterId;
    Transaction.CharacterExperienceDelta = Recipe.CharacterExperienceReward;

    for (const FJumanjiCurrencyCost& Cost : Recipe.CurrencyCosts)
    {
        FJumanjiCurrencyDelta Delta;
        Delta.CurrencyId = Cost.CurrencyId;
        Delta.Delta = -Cost.Amount;
        Transaction.CurrencyDeltas.Add(Delta);
    }

    for (const FJumanjiItemCost& Cost : Recipe.ItemCosts)
    {
        FJumanjiItemDelta Delta;
        Delta.ItemId = Cost.ItemId;
        Delta.Delta = -Cost.Amount;
        Transaction.ItemDeltas.Add(Delta);
    }

    return PlayerState->ApplyProfileTransaction(Transaction);
}
