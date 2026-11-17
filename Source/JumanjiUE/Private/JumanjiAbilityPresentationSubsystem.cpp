#include "JumanjiAbilityPresentationSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "JumanjiAbilityCatalogSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

namespace
{
    FName GetCatalogSlotIdFromManifestSlotId(FName ManifestSlotId)
    {
        if (ManifestSlotId == FName(TEXT("PrimaryAttack"))) return FName(TEXT("SLOT_PRIMARY"));
        if (ManifestSlotId == FName(TEXT("Active"))) return FName(TEXT("SLOT_ACTIVE"));
        if (ManifestSlotId == FName(TEXT("Passive"))) return FName(TEXT("SLOT_PASSIVE"));
        if (ManifestSlotId == FName(TEXT("Ultimate"))) return FName(TEXT("SLOT_ULTIMATE"));
        if (ManifestSlotId == FName(TEXT("Dodge"))) return FName(TEXT("SLOT_DODGE"));
        return NAME_None;
    }

    FSoftObjectPath MakePresentationObjectPath(const FString& AssetPath)
    {
        if (AssetPath.IsEmpty())
        {
            return FSoftObjectPath();
        }

        FString NormalizedPath = AssetPath;
        if (!NormalizedPath.Contains(TEXT(".")))
        {
            int32 LastSlashIndex = INDEX_NONE;
            if (NormalizedPath.FindLastChar(TEXT('/'), LastSlashIndex))
            {
                const FString AssetName = NormalizedPath.Mid(LastSlashIndex + 1);
                if (!AssetName.IsEmpty())
                {
                    NormalizedPath += TEXT(".");
                    NormalizedPath += AssetName;
                }
            }
        }

        return FSoftObjectPath(NormalizedPath);
    }
}

void UJumanjiAbilityPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ReloadPresentationData();
}

bool UJumanjiAbilityPresentationSubsystem::ReloadPresentationData()
{
    CachedPresentations.Reset();
    bPresentationDataInitialized = true;

    if (ConfiguredPresentationDataTable.IsNull())
    {
        UE_LOG(LogTemp, Log, TEXT("JUMANJI ability presentation: no DataTable configured; Gameplay Ability HUD metadata will be used as fallback."));
        return false;
    }

    UDataTable* DataTable = ConfiguredPresentationDataTable.LoadSynchronous();
    if (!DataTable || DataTable->GetRowStruct() != FJumanjiAbilityPresentationRow::StaticStruct())
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI ability presentation: DataTable is missing or has the wrong row struct."));
        return false;
    }

    TArray<FJumanjiAbilityPresentationRow*> Rows;
    DataTable->GetAllRows<FJumanjiAbilityPresentationRow>(
        TEXT("JUMANJI Ability Presentation Manifest"),
        Rows
    );

    for (const FJumanjiAbilityPresentationRow* Row : Rows)
    {
        if (!Row || Row->AbilityId.IsNone() || CachedPresentations.Contains(Row->AbilityId))
        {
            UE_LOG(LogTemp, Warning, TEXT("JUMANJI ability presentation: skipped an empty or duplicate AbilityId row."));
            continue;
        }

        if (!IsRowCompatible(*Row))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("JUMANJI ability presentation: skipped incompatible manifest row %s."),
                *Row->AbilityId.ToString()
            );
            continue;
        }

        CachedPresentations.Add(Row->AbilityId, MakePresentationData(*Row));
    }

    if (CachedPresentations.Num() != 45)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("JUMANJI ability presentation: loaded %d of the expected 45 manifest rows; missing rows use Gameplay Ability metadata fallback."),
            CachedPresentations.Num()
        );
    }

    return CachedPresentations.Num() > 0;
}

bool UJumanjiAbilityPresentationSubsystem::EnsurePresentationDataReady() const
{
    if (!bPresentationDataInitialized)
    {
        const_cast<UJumanjiAbilityPresentationSubsystem*>(this)->ReloadPresentationData();
    }

    return !CachedPresentations.IsEmpty();
}

bool UJumanjiAbilityPresentationSubsystem::IsRowCompatible(
    const FJumanjiAbilityPresentationRow& Row) const
{
    if (Row.AbilityId.IsNone()
        || Row.CharacterId.IsNone()
        || Row.SlotId.IsNone()
        || Row.AbilityNamePL.IsEmpty())
    {
        return false;
    }

    const FName CatalogSlotId = GetCatalogSlotIdFromManifestSlotId(Row.SlotId);
    if (CatalogSlotId.IsNone())
    {
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UJumanjiAbilityCatalogSubsystem* Catalog = GameInstance
        ? GameInstance->GetSubsystem<UJumanjiAbilityCatalogSubsystem>()
        : nullptr;

    if (!Catalog)
    {
        return false;
    }

    FJumanjiAbilityCatalogEntry AbilityEntry;
    return Catalog->GetAbilityDefinition(Row.AbilityId, AbilityEntry)
        && AbilityEntry.CharacterId == Row.CharacterId
        && AbilityEntry.SlotId == CatalogSlotId;
}

FLinearColor UJumanjiAbilityPresentationSubsystem::ParseHexColor(
    const FString& Hex,
    const FLinearColor& Fallback)
{
    FString Value = Hex;
    Value.TrimStartAndEndInline();
    Value.RemoveFromStart(TEXT("#"));

    if (Value.Len() != 6 && Value.Len() != 8)
    {
        return Fallback;
    }

    return FColor::FromHex(Value).ReinterpretAsLinear();
}

FJumanjiAbilityPresentationData UJumanjiAbilityPresentationSubsystem::MakePresentationData(
    const FJumanjiAbilityPresentationRow& Row)
{
    FJumanjiAbilityPresentationData Result;
    Result.AbilityId = Row.AbilityId;
    Result.CharacterId = Row.CharacterId;
    Result.SlotId = Row.SlotId;
    Result.AbilityName = Row.AbilityNamePL;
    Result.CharacterName = Row.CharacterNamePL;
    Result.Element = Row.Element;
    Result.IconMotif = Row.IconMotif;
    Result.AccentColor = ParseHexColor(Row.AccentHex, FLinearColor::White);
    Result.SecondaryColor = ParseHexColor(Row.SecondaryHex, FLinearColor::White);
    Result.CooldownSecondsReference = FMath::Max(0.0f, Row.CooldownSeconds);
    Result.GameplayCueTag = Row.GameplayCueTagName.IsNone()
        ? FGameplayTag()
        : FGameplayTag::RequestGameplayTag(Row.GameplayCueTagName, false);

    if (!Row.IconAssetPath.IsEmpty())
    {
        Result.Icon = TSoftObjectPtr<UTexture2D>(MakePresentationObjectPath(Row.IconAssetPath));
    }
    if (!Row.CooldownMaterialPath.IsEmpty())
    {
        Result.CooldownMaterial = TSoftObjectPtr<UMaterialInterface>(MakePresentationObjectPath(Row.CooldownMaterialPath));
    }
    if (!Row.ActivatedSoundPath.IsEmpty())
    {
        Result.ActivatedSound = TSoftObjectPtr<USoundBase>(MakePresentationObjectPath(Row.ActivatedSoundPath));
    }
    if (!Row.InsufficientResourceSoundPath.IsEmpty())
    {
        Result.InsufficientResourceSound = TSoftObjectPtr<USoundBase>(MakePresentationObjectPath(Row.InsufficientResourceSoundPath));
    }
    if (!Row.CooldownSoundPath.IsEmpty())
    {
        Result.CooldownSound = TSoftObjectPtr<USoundBase>(MakePresentationObjectPath(Row.CooldownSoundPath));
    }
    if (!Row.UnavailableSoundPath.IsEmpty())
    {
        Result.UnavailableSound = TSoftObjectPtr<USoundBase>(MakePresentationObjectPath(Row.UnavailableSoundPath));
    }

    Result.bHasIconAssetPath = !Row.IconAssetPath.IsEmpty();
    return Result;
}

bool UJumanjiAbilityPresentationSubsystem::GetPresentation(
    FName AbilityId,
    FJumanjiAbilityPresentationData& OutPresentation) const
{
    OutPresentation = FJumanjiAbilityPresentationData();

    if (AbilityId.IsNone() || !EnsurePresentationDataReady())
    {
        return false;
    }

    const FJumanjiAbilityPresentationData* Found = CachedPresentations.Find(AbilityId);
    if (!Found)
    {
        return false;
    }

    OutPresentation = *Found;
    return true;
}

int32 UJumanjiAbilityPresentationSubsystem::GetLoadedPresentationCount() const
{
    EnsurePresentationDataReady();
    return CachedPresentations.Num();
}
