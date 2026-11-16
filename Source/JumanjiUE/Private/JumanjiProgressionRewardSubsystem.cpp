#include "JumanjiProgressionRewardSubsystem.h"

#include "JumanjiPlayerState.h"
#include "JumanjiCharacterRosterSubsystem.h"
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

    const auto AddUpgradeRecipe = [this](
        const TCHAR* RecipeId,
        const TCHAR* MaterialId,
        int32 MaterialAmount,
        int64 CoinCost,
        int64 CharacterExperience)
    {
        FJumanjiSpiritUpgradeRecipe Recipe;
        Recipe.RecipeId = FName(RecipeId);
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

    AddUpgradeRecipe(TEXT("UPGRADE_ROOT_SPIRIT_01"), TEXT("MAT_ROOT_RESIN"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_TIDE_SPIRIT_01"), TEXT("MAT_TIDE_PEARL"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_STONE_SPIRIT_01"), TEXT("MAT_STONE_CORE"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_STORM_SPIRIT_01"), TEXT("MAT_STORM_ESSENCE"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_EMBER_SPIRIT_01"), TEXT("MAT_EMBER_ASH"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_DAWN_SPIRIT_01"), TEXT("MAT_DAWN_SHARD"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_SHADOW_SPIRIT_01"), TEXT("MAT_SHADOW_SILK"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_LIFE_SPIRIT_01"), TEXT("MAT_LIFE_BLOOM"), 2, 100, 120);
    AddUpgradeRecipe(TEXT("UPGRADE_FROST_SPIRIT_01"), TEXT("MAT_FROST_CRYSTAL"), 2, 100, 120);
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
    FName RecipeId,
    int32 ProgressionRevision) const
{
    const FString ProfileHash = FMD5::HashAnsiString(*PlayerState.GetPersistentProfileId()).Left(12);
    const FString SafeCharacterId = CharacterId.ToString().Left(20);
    const FString SafeRecipeId = RecipeId.ToString().Left(28);
    return FName(*FString::Printf(
        TEXT("UPG_%s_%s_%s_R%d"),
        *ProfileHash,
        *SafeCharacterId,
        *SafeRecipeId,
        ProgressionRevision));
}

bool UJumanjiProgressionRewardSubsystem::GrantChapterCompletionReward(
    AJumanjiPlayerState* PlayerState,
    FName ChapterId) const
{
    if (!IsValid(PlayerState)
        || !PlayerState->HasAuthority()
        || PlayerState->GetPersistentProfileId().IsEmpty()
        || ChapterId.IsNone())
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
    if (!Roster || !Roster->IsPlayableCharacter(CharacterId))
    {
        return false;
    }

    FJumanjiSpiritUpgradeRecipe Recipe;
    if (!GetSpiritUpgradeRecipe(RecipeId, Recipe))
    {
        return false;
    }

    const FJumanjiPlayerProgressionState Progression = PlayerState->GetProgressionState();

    FJumanjiProfileTransaction Transaction;
    Transaction.TransactionId = BuildUpgradeTransactionId(
        *PlayerState, CharacterId, RecipeId, Progression.Revision);
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
