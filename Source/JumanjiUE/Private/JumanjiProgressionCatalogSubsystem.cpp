#include "JumanjiProgressionCatalogSubsystem.h"

void UJumanjiProgressionCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BuildDefaultCatalog();
}

void UJumanjiProgressionCatalogSubsystem::BuildDefaultCatalog()
{
    CurrencyDefinitions.Reset();
    ItemDefinitions.Reset();
    AbilitySlotIds.Reset();

    const auto AddCurrency = [this](
        const TCHAR* Id,
        const TCHAR* Name,
        int64 StartingBalance,
        int64 MaxBalance)
    {
        FJumanjiCurrencyDefinition Definition;
        Definition.CurrencyId = FName(Id);
        Definition.DisplayName = FText::FromString(Name);
        Definition.StartingBalance = StartingBalance;
        Definition.MaxBalance = MaxBalance;
        CurrencyDefinitions.Add(MoveTemp(Definition));
    };

    AddCurrency(TEXT("CURRENCY_EXPLORER_COINS"), TEXT("Monety Odkrywców"), 250, 2000000000LL);
    AddCurrency(TEXT("CURRENCY_BOARD_SEALS"), TEXT("Pieczęcie Planszy"), 0, 1000000000LL);
    AddCurrency(TEXT("CURRENCY_ECHO_CRYSTALS"), TEXT("Kryształy Echa"), 0, 1000000000LL);

    const auto AddMaterial = [this](const TCHAR* Id, const TCHAR* Name)
    {
        FJumanjiItemDefinition Definition;
        Definition.ItemId = FName(Id);
        Definition.DisplayName = FText::FromString(Name);
        Definition.Category = EJumanjiInventoryItemCategory::UpgradeMaterial;
        Definition.MaxStack = 9999;
        Definition.bCanBeConsumed = true;
        ItemDefinitions.Add(MoveTemp(Definition));
    };

    AddMaterial(TEXT("MAT_ROOT_RESIN"), TEXT("Żywica Korzeni"));
    AddMaterial(TEXT("MAT_TIDE_PEARL"), TEXT("Perła Przypływu"));
    AddMaterial(TEXT("MAT_STONE_CORE"), TEXT("Rdzeń Kamienia"));
    AddMaterial(TEXT("MAT_STORM_ESSENCE"), TEXT("Esencja Burzy"));
    AddMaterial(TEXT("MAT_EMBER_ASH"), TEXT("Popiół Żaru"));
    AddMaterial(TEXT("MAT_DAWN_SHARD"), TEXT("Odłamek Świtu"));
    AddMaterial(TEXT("MAT_SHADOW_SILK"), TEXT("Jedwab Cienia"));
    AddMaterial(TEXT("MAT_LIFE_BLOOM"), TEXT("Kwiat Życia"));
    AddMaterial(TEXT("MAT_FROST_CRYSTAL"), TEXT("Kryształ Mrozu"));
    AddMaterial(TEXT("MAT_SPIRIT_MEMORY"), TEXT("Pamięć Ducha"));

    AbilitySlotIds.Add(FName(TEXT("SLOT_PRIMARY")));
    AbilitySlotIds.Add(FName(TEXT("SLOT_ACTIVE")));
    AbilitySlotIds.Add(FName(TEXT("SLOT_PASSIVE")));
    AbilitySlotIds.Add(FName(TEXT("SLOT_ULTIMATE")));
    AbilitySlotIds.Add(FName(TEXT("SLOT_DODGE")));
}

bool UJumanjiProgressionCatalogSubsystem::GetCurrencyDefinition(
    FName CurrencyId,
    FJumanjiCurrencyDefinition& OutDefinition) const
{
    const FJumanjiCurrencyDefinition* Found = CurrencyDefinitions.FindByPredicate(
        [CurrencyId](const FJumanjiCurrencyDefinition& Definition)
        {
            return Definition.CurrencyId == CurrencyId;
        });

    if (!Found)
    {
        return false;
    }

    OutDefinition = *Found;
    return true;
}

bool UJumanjiProgressionCatalogSubsystem::GetItemDefinition(
    FName ItemId,
    FJumanjiItemDefinition& OutDefinition) const
{
    const FJumanjiItemDefinition* Found = ItemDefinitions.FindByPredicate(
        [ItemId](const FJumanjiItemDefinition& Definition)
        {
            return Definition.ItemId == ItemId;
        });

    if (!Found)
    {
        return false;
    }

    OutDefinition = *Found;
    return true;
}

bool UJumanjiProgressionCatalogSubsystem::IsValidAbilitySlotId(FName SlotId) const
{
    return !SlotId.IsNone() && AbilitySlotIds.Contains(SlotId);
}

int64 UJumanjiProgressionCatalogSubsystem::GetAccountExperienceForNextLevel(int32 CurrentLevel) const
{
    if (CurrentLevel < 1 || CurrentLevel >= MaxAccountLevel)
    {
        return 0;
    }

    return 200LL + static_cast<int64>(CurrentLevel) * 100LL;
}

int64 UJumanjiProgressionCatalogSubsystem::GetCharacterExperienceForNextLevel(int32 CurrentLevel) const
{
    if (CurrentLevel < 1 || CurrentLevel >= MaxCharacterLevel)
    {
        return 0;
    }

    return 150LL + static_cast<int64>(CurrentLevel) * 90LL;
}
