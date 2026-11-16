#include "JumanjiAbilityCatalogSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "JumanjiCharacterRosterSubsystem.h"
#include "JumanjiCombatDefaults.h"
#include "JumanjiGameplayAbility.h"

namespace JumanjiAbilityCatalog
{
    static const FName SlotPrimary(TEXT("SLOT_PRIMARY"));
    static const FName SlotActive(TEXT("SLOT_ACTIVE"));
    static const FName SlotPassive(TEXT("SLOT_PASSIVE"));
    static const FName SlotUltimate(TEXT("SLOT_ULTIMATE"));
    static const FName SlotDodge(TEXT("SLOT_DODGE"));

    static const TCHAR* RomanNumbers[9] = {
        TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"),
        TEXT("VI"), TEXT("VII"), TEXT("VIII"), TEXT("IX")
    };
}

void UJumanjiAbilityCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Build lazily on first use. This avoids depending on the initialization
    // order of GameInstance subsystems such as the Great Spirit roster.
}

void UJumanjiAbilityCatalogSubsystem::EnsureCatalogReady() const
{
    if (!bCatalogReady)
    {
        UJumanjiAbilityCatalogSubsystem* MutableThis =
            const_cast<UJumanjiAbilityCatalogSubsystem*>(this);
        MutableThis->BuildDefaultCatalog();
        MutableThis->ApplyConfiguredDataTable();
        MutableThis->bCatalogReady = true;
    }
}

bool UJumanjiAbilityCatalogSubsystem::GetSlotContract(
    FName SlotId,
    EJumanjiAbilitySlot& OutAbilitySlot,
    FGameplayTag& OutInputTag)
{
    FName InputTagName = NAME_None;

    if (SlotId == JumanjiAbilityCatalog::SlotPrimary)
    {
        OutAbilitySlot = EJumanjiAbilitySlot::PrimaryAttack;
        InputTagName = FName(TEXT("Input.Ability.Primary"));
    }
    else if (SlotId == JumanjiAbilityCatalog::SlotActive)
    {
        OutAbilitySlot = EJumanjiAbilitySlot::Active;
        InputTagName = FName(TEXT("Input.Ability.Active"));
    }
    else if (SlotId == JumanjiAbilityCatalog::SlotPassive)
    {
        OutAbilitySlot = EJumanjiAbilitySlot::Passive;
    }
    else if (SlotId == JumanjiAbilityCatalog::SlotUltimate)
    {
        OutAbilitySlot = EJumanjiAbilitySlot::Ultimate;
        InputTagName = FName(TEXT("Input.Ability.Ultimate"));
    }
    else if (SlotId == JumanjiAbilityCatalog::SlotDodge)
    {
        OutAbilitySlot = EJumanjiAbilitySlot::Dodge;
        InputTagName = FName(TEXT("Input.Ability.Dodge"));
    }
    else
    {
        return false;
    }

    OutInputTag = InputTagName.IsNone()
        ? FGameplayTag()
        : FGameplayTag::RequestGameplayTag(InputTagName, false);

    return InputTagName.IsNone() || OutInputTag.IsValid();
}

FName UJumanjiAbilityCatalogSubsystem::MakeDefaultAbilityId(int32 SpiritNumber, FName SlotId)
{
    if (SpiritNumber < 1 || SpiritNumber > 9)
    {
        return NAME_None;
    }

    const TCHAR* SlotSuffix = nullptr;
    if (SlotId == JumanjiAbilityCatalog::SlotPrimary) SlotSuffix = TEXT("PRIMARY");
    else if (SlotId == JumanjiAbilityCatalog::SlotActive) SlotSuffix = TEXT("ACTIVE");
    else if (SlotId == JumanjiAbilityCatalog::SlotPassive) SlotSuffix = TEXT("PASSIVE");
    else if (SlotId == JumanjiAbilityCatalog::SlotUltimate) SlotSuffix = TEXT("ULTIMATE");
    else if (SlotId == JumanjiAbilityCatalog::SlotDodge) SlotSuffix = TEXT("DODGE");

    if (!SlotSuffix)
    {
        return NAME_None;
    }

    return FName(*FString::Printf(
        TEXT("ABILITY_GS_%s_%s"),
        JumanjiAbilityCatalog::RomanNumbers[SpiritNumber - 1],
        SlotSuffix));
}

void UJumanjiAbilityCatalogSubsystem::AddOrReplaceEntry(
    const FJumanjiAbilityCatalogEntry& Entry)
{
    const int32 ExistingIndex = AbilityEntries.IndexOfByPredicate(
        [&Entry](const FJumanjiAbilityCatalogEntry& Existing)
        {
            return Existing.AbilityId == Entry.AbilityId;
        });

    if (ExistingIndex == INDEX_NONE)
    {
        AbilityEntries.Add(Entry);
    }
    else
    {
        AbilityEntries[ExistingIndex] = Entry;
    }
}

void UJumanjiAbilityCatalogSubsystem::BuildDefaultCatalog()
{
    AbilityEntries.Reset();

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;

    if (!Roster)
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI ability catalog: roster subsystem is unavailable; catalog will retry on next access."));
        return;
    }

    const FName SlotIds[] = {
        JumanjiAbilityCatalog::SlotPrimary,
        JumanjiAbilityCatalog::SlotActive,
        JumanjiAbilityCatalog::SlotPassive,
        JumanjiAbilityCatalog::SlotUltimate,
        JumanjiAbilityCatalog::SlotDodge
    };

    for (const FJumanjiCharacterDefinition& Character : Roster->GetRoster())
    {
        for (const FName SlotId : SlotIds)
        {
            EJumanjiAbilitySlot AbilitySlot;
            FGameplayTag InputTag;
            if (!GetSlotContract(SlotId, AbilitySlot, InputTag))
            {
                continue;
            }

            FJumanjiAbilityCatalogEntry Entry;
            Entry.AbilityId = MakeDefaultAbilityId(Character.SpiritNumber, SlotId);
            Entry.CharacterId = Character.CharacterId;
            Entry.SlotId = SlotId;
            Entry.AbilitySlot = AbilitySlot;
            Entry.InputTag = InputTag;
            Entry.bEnabled = true;

            switch (AbilitySlot)
            {
            case EJumanjiAbilitySlot::PrimaryAttack:
                Entry.DisplayName = Character.PrimaryAttackAbilityName;
                Entry.AbilityClass = Character.PrimaryAttackAbilityClass.IsNull()
                    ? TSoftClassPtr<UGameplayAbility>(UJumanjiDefaultPlayerMeleeAbility::StaticClass())
                    : Character.PrimaryAttackAbilityClass;
                break;
            case EJumanjiAbilitySlot::Active:
                Entry.DisplayName = Character.SignatureAbilityName;
                Entry.AbilityClass = Character.SignatureAbilityClass;
                Entry.bEnabled = !Entry.AbilityClass.IsNull();
                break;
            case EJumanjiAbilitySlot::Passive:
                Entry.DisplayName = Character.PassiveAbilityName;
                Entry.AbilityClass = Character.PassiveAbilityClass;
                Entry.bEnabled = !Entry.AbilityClass.IsNull();
                break;
            case EJumanjiAbilitySlot::Ultimate:
                Entry.DisplayName = Character.UltimateAbilityName;
                Entry.AbilityClass = Character.UltimateAbilityClass;
                Entry.bEnabled = !Entry.AbilityClass.IsNull();
                break;
            case EJumanjiAbilitySlot::Dodge:
                Entry.DisplayName = Character.DodgeAbilityName;
                Entry.AbilityClass = Character.DodgeAbilityClass;
                Entry.bEnabled = !Entry.AbilityClass.IsNull();
                break;
            default:
                Entry.bEnabled = false;
                break;
            }

            if (!Entry.AbilityId.IsNone() && !Entry.CharacterId.IsNone())
            {
                AddOrReplaceEntry(Entry);
            }
        }
    }
}

bool UJumanjiAbilityCatalogSubsystem::IsEntryClassCompatible(
    const FJumanjiAbilityCatalogEntry& Entry) const
{
    if (!Entry.bEnabled || Entry.AbilityId.IsNone()
        || Entry.CharacterId.IsNone() || Entry.SlotId.IsNone()
        || Entry.AbilityClass.IsNull())
    {
        return false;
    }

    EJumanjiAbilitySlot ExpectedSlot;
    FGameplayTag ExpectedInputTag;
    if (!GetSlotContract(Entry.SlotId, ExpectedSlot, ExpectedInputTag)
        || Entry.AbilitySlot != ExpectedSlot
        || Entry.InputTag != ExpectedInputTag)
    {
        return false;
    }

    UClass* AbilityClass = Entry.AbilityClass.LoadSynchronous();
    if (!AbilityClass || !AbilityClass->IsChildOf(UJumanjiGameplayAbility::StaticClass()))
    {
        return false;
    }

    const UJumanjiGameplayAbility* AbilityCDO =
        Cast<UJumanjiGameplayAbility>(AbilityClass->GetDefaultObject());

    if (!AbilityCDO || AbilityCDO->GetAbilitySlot() != ExpectedSlot)
    {
        return false;
    }

    return ExpectedInputTag.IsValid()
        ? AbilityCDO->GetInputTag().MatchesTagExact(ExpectedInputTag)
        : !AbilityCDO->GetInputTag().IsValid();
}

void UJumanjiAbilityCatalogSubsystem::ApplyConfiguredDataTable()
{
    if (ConfiguredAbilityDataTable.IsNull())
    {
        return;
    }

    UDataTable* DataTable = ConfiguredAbilityDataTable.LoadSynchronous();
    if (!DataTable || DataTable->GetRowStruct() != FJumanjiAbilityCatalogEntry::StaticStruct())
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI ability catalog: configured DataTable is missing or uses the wrong row struct."));
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;

    if (!Roster)
    {
        return;
    }

    TArray<FJumanjiAbilityCatalogEntry*> Rows;
    DataTable->GetAllRows<FJumanjiAbilityCatalogEntry>(TEXT("JUMANJI Ability Catalog"), Rows);

    TSet<FName> SeenAbilityIds;
    for (const FJumanjiAbilityCatalogEntry* Row : Rows)
    {
        if (!Row
            || Row->AbilityId.IsNone()
            || SeenAbilityIds.Contains(Row->AbilityId)
            || !Roster->IsPlayableCharacter(Row->CharacterId)
            || !IsEntryClassCompatible(*Row))
        {
            UE_LOG(LogTemp, Warning, TEXT("JUMANJI ability catalog: skipped invalid or duplicate DataTable row."));
            continue;
        }

        SeenAbilityIds.Add(Row->AbilityId);
        AddOrReplaceEntry(*Row);
    }
}

bool UJumanjiAbilityCatalogSubsystem::GetAbilityDefinition(
    FName AbilityId,
    FJumanjiAbilityCatalogEntry& OutEntry) const
{
    EnsureCatalogReady();

    const FJumanjiAbilityCatalogEntry* Found = AbilityEntries.FindByPredicate(
        [AbilityId](const FJumanjiAbilityCatalogEntry& Entry)
        {
            return Entry.AbilityId == AbilityId;
        });

    if (!Found)
    {
        return false;
    }

    OutEntry = *Found;
    return true;
}

void UJumanjiAbilityCatalogSubsystem::GetAbilityChoices(
    FName CharacterId,
    FName SlotId,
    TArray<FJumanjiAbilityCatalogEntry>& OutChoices) const
{
    EnsureCatalogReady();
    OutChoices.Reset();

    for (const FJumanjiAbilityCatalogEntry& Entry : AbilityEntries)
    {
        if (Entry.CharacterId == CharacterId
            && Entry.SlotId == SlotId
            && IsEntryClassCompatible(Entry))
        {
            OutChoices.Add(Entry);
        }
    }
}

bool UJumanjiAbilityCatalogSubsystem::IsValidAbilityChoice(
    FName CharacterId,
    FName SlotId,
    FName AbilityId) const
{
    EnsureCatalogReady();

    const FJumanjiAbilityCatalogEntry* Found = AbilityEntries.FindByPredicate(
        [AbilityId](const FJumanjiAbilityCatalogEntry& Entry)
        {
            return Entry.AbilityId == AbilityId;
        });

    return Found
        && Found->CharacterId == CharacterId
        && Found->SlotId == SlotId
        && IsEntryClassCompatible(*Found);
}

FName UJumanjiAbilityCatalogSubsystem::GetDefaultAbilityId(
    FName CharacterId,
    FName SlotId) const
{
    EnsureCatalogReady();

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;

    FJumanjiCharacterDefinition Character;
    if (!Roster || !Roster->GetCharacterDefinition(CharacterId, Character))
    {
        return NAME_None;
    }

    const FName AbilityId = MakeDefaultAbilityId(Character.SpiritNumber, SlotId);
    return IsValidAbilityChoice(CharacterId, SlotId, AbilityId)
        ? AbilityId
        : NAME_None;
}

bool UJumanjiAbilityCatalogSubsystem::ResolveEquippedLoadout(
    FName CharacterId,
    const TArray<FJumanjiEquippedAbilitySlot>& SavedSlots,
    TArray<FJumanjiResolvedAbility>& OutResolvedAbilities) const
{
    EnsureCatalogReady();
    OutResolvedAbilities.Reset();

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiCharacterRosterSubsystem* Roster = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiCharacterRosterSubsystem>()
        : nullptr;

    if (!Roster || !Roster->IsPlayableCharacter(CharacterId))
    {
        return false;
    }

    const FName SlotIds[] = {
        JumanjiAbilityCatalog::SlotPrimary,
        JumanjiAbilityCatalog::SlotActive,
        JumanjiAbilityCatalog::SlotPassive,
        JumanjiAbilityCatalog::SlotUltimate,
        JumanjiAbilityCatalog::SlotDodge
    };

    for (const FName SlotId : SlotIds)
    {
        const FJumanjiEquippedAbilitySlot* SavedSlot = SavedSlots.FindByPredicate(
            [CharacterId, SlotId](const FJumanjiEquippedAbilitySlot& Entry)
            {
                return Entry.CharacterId == CharacterId && Entry.SlotId == SlotId;
            });

        FName AbilityId = SavedSlot ? SavedSlot->AbilityId : NAME_None;
        if (!IsValidAbilityChoice(CharacterId, SlotId, AbilityId))
        {
            AbilityId = GetDefaultAbilityId(CharacterId, SlotId);
        }

        if (AbilityId.IsNone())
        {
            continue;
        }

        FJumanjiAbilityCatalogEntry Entry;
        if (!GetAbilityDefinition(AbilityId, Entry) || !IsEntryClassCompatible(Entry))
        {
            continue;
        }

        FJumanjiResolvedAbility Resolved;
        Resolved.AbilityId = Entry.AbilityId;
        Resolved.SlotId = Entry.SlotId;
        Resolved.AbilitySlot = Entry.AbilitySlot;
        Resolved.InputTag = Entry.InputTag;
        Resolved.AbilityClass = Entry.AbilityClass;
        OutResolvedAbilities.Add(MoveTemp(Resolved));
    }

    return true;
}
