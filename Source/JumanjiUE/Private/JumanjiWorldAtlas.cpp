#include "JumanjiWorldAtlas.h"

#include "Engine/Texture2D.h"

namespace
{
    FGameplayTag GetElementTag(const TCHAR* TagName)
    {
        return FGameplayTag::RequestGameplayTag(FName(TagName), false);
    }

    bool IsValidMapCoordinate(const FVector2D& Coordinate)
    {
        return FMath::IsFinite(Coordinate.X)
            && FMath::IsFinite(Coordinate.Y)
            && Coordinate.X >= 0.0f && Coordinate.X <= 1.0f
            && Coordinate.Y >= 0.0f && Coordinate.Y <= 1.0f;
    }

    bool IsValidWorldLocation(const FVector& Location)
    {
        return FMath::IsFinite(Location.X)
            && FMath::IsFinite(Location.Y)
            && FMath::IsFinite(Location.Z);
    }
}

void UJumanjiWorldAtlasSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BuildDefaultAtlas();
    LoadConfiguredAtlas();
}

void UJumanjiWorldAtlasSubsystem::BuildDefaultAtlas()
{
    Regions.Reset();
    Landmarks.Reset();

    const auto AddRegion = [this](
        FName Id,
        const TCHAR* Name,
        const TCHAR* Description,
        const TCHAR* Element,
        FName ObjectiveId,
        FName SpiritId,
        int32 SortOrder,
        FVector2D MapCoordinate,
        bool bSafeHub)
    {
        FJumanjiWorldRegionDefinition Region;
        Region.RegionId = Id;
        Region.DisplayName = FText::FromString(Name);
        Region.Description = FText::FromString(Description);
        Region.ElementTag = Element && FCString::Strlen(Element) > 0
            ? GetElementTag(Element)
            : FGameplayTag();
        Region.RelatedObjectiveId = ObjectiveId;
        Region.RecommendedGreatSpiritId = SpiritId;
        Region.SortOrder = SortOrder;
        Region.MapCoordinate = MapCoordinate;
        Region.bSafeHub = bSafeHub;
        Regions.Add(MoveTemp(Region));
    };

    AddRegion(TEXT("REGION_HEART_GROVE"), TEXT("Serce Dżungli"),
        TEXT("Bezpieczna polana i punkt orientacyjny wyprawy."), TEXT(""),
        NAME_None, NAME_None, 0, FVector2D(0.50f, 0.50f), true);

    AddRegion(TEXT("REGION_ROOT_GROVE"), TEXT("Gaj Korzeni"),
        TEXT("Stary las, w którym korzenie układają się w znaki Żywej Planszy."),
        TEXT("Element.Root"), TEXT("TRIAL_ROOT_RUNES"), TEXT("GREAT_SPIRIT_I"),
        1, FVector2D(0.30f, 0.22f), false);

    AddRegion(TEXT("REGION_TIDE_FALLS"), TEXT("Wodospady Przypływu"),
        TEXT("Tarasy wodne i kanały prowadzące do ukrytego zbiornika."),
        TEXT("Element.Tide"), TEXT("TRIAL_TIDE_CHANNELS"), TEXT("GREAT_SPIRIT_II"),
        2, FVector2D(0.16f, 0.48f), false);

    AddRegion(TEXT("REGION_STONE_SANCTUM"), TEXT("Kamienne Sanktuarium"),
        TEXT("Kamienne filary i platformy tworzące starożytną równowagę."),
        TEXT("Element.Stone"), TEXT("TRIAL_STONE_BALANCE"), TEXT("GREAT_SPIRIT_III"),
        3, FVector2D(0.37f, 0.72f), false);

    AddRegion(TEXT("REGION_STORM_PEAK"), TEXT("Szczyt Burz"),
        TEXT("Wysokie przełęcze, na których znaki wiatru wskazują bezpieczną drogę."),
        TEXT("Element.Storm"), TEXT("TRIAL_STORM_WINDS"), TEXT("GREAT_SPIRIT_IV"),
        4, FVector2D(0.69f, 0.18f), false);

    AddRegion(TEXT("REGION_EMBER_CAVERN"), TEXT("Jaskinia Żaru"),
        TEXT("Ciepłe jaskinie i latarnie prowadzące przez pradawny szlak."),
        TEXT("Element.Ember"), TEXT("TRIAL_EMBER_SEQUENCE"), TEXT("GREAT_SPIRIT_V"),
        5, FVector2D(0.82f, 0.42f), false);

    AddRegion(TEXT("REGION_DAWN_MIRRORS"), TEXT("Galeria Świtu"),
        TEXT("Kryształowe lustra kierujące promienie przez ukryte przejścia."),
        TEXT("Element.Dawn"), TEXT("TRIAL_DAWN_MIRRORS"), TEXT("GREAT_SPIRIT_VI"),
        6, FVector2D(0.72f, 0.67f), false);

    AddRegion(TEXT("REGION_SHADOW_MARSH"), TEXT("Mokradła Cienia"),
        TEXT("Mglista dolina z symbolami półmroku i zaćmienia."),
        TEXT("Element.Shadow"), TEXT("TRIAL_SHADOW_PATTERN"), TEXT("GREAT_SPIRIT_VII"),
        7, FVector2D(0.52f, 0.86f), false);

    AddRegion(TEXT("REGION_LIFE_GARDEN"), TEXT("Ogród Życia"),
        TEXT("Zielony ogród, w którym można odnowić ścieżki i przywrócić rośliny do wzrostu."),
        TEXT("Element.Life"), TEXT("TRIAL_LIFE_GARDEN"), TEXT("GREAT_SPIRIT_VIII"),
        8, FVector2D(0.22f, 0.80f), false);

    AddRegion(TEXT("REGION_FROST_GATE"), TEXT("Lodowa Brama"),
        TEXT("Chłodna dolina prowadząca do kryształowej bramy."),
        TEXT("Element.Frost"), TEXT("TRIAL_FROST_GATES"), TEXT("GREAT_SPIRIT_IX"),
        9, FVector2D(0.90f, 0.83f), false);

    const auto AddLandmark = [this](
        FName Id,
        FName RegionId,
        const TCHAR* Name,
        const TCHAR* Description)
    {
        FJumanjiWorldLandmarkDefinition Landmark;
        Landmark.LandmarkId = Id;
        Landmark.RegionId = RegionId;
        Landmark.DisplayName = FText::FromString(Name);
        Landmark.Description = FText::FromString(Description);
        Landmarks.Add(MoveTemp(Landmark));
    };

    AddLandmark(TEXT("LANDMARK_LIVING_BOARD"), TEXT("REGION_HEART_GROVE"),
        TEXT("Żywa Plansza"), TEXT("Centralny punkt wyprawy i początek rozdziału."));
    AddLandmark(TEXT("LANDMARK_ROOT_ALTAR"), TEXT("REGION_ROOT_GROVE"),
        TEXT("Ołtarz Korzeni"), TEXT("Miejsce Próby Korzeni."));
    AddLandmark(TEXT("LANDMARK_TIDE_CHANNEL"), TEXT("REGION_TIDE_FALLS"),
        TEXT("Kamienny Kanał"), TEXT("Punkt orientacyjny Próby Przypływu."));
    AddLandmark(TEXT("LANDMARK_STONE_PILLARS"), TEXT("REGION_STONE_SANCTUM"),
        TEXT("Krąg Filarów"), TEXT("Punkt orientacyjny Próby Kamienia."));
    AddLandmark(TEXT("LANDMARK_STORM_BEACON"), TEXT("REGION_STORM_PEAK"),
        TEXT("Latarnia Burz"), TEXT("Punkt orientacyjny Próby Burzy."));
    AddLandmark(TEXT("LANDMARK_EMBER_LANTERN"), TEXT("REGION_EMBER_CAVERN"),
        TEXT("Latarnia Żaru"), TEXT("Punkt orientacyjny Próby Żaru."));
    AddLandmark(TEXT("LANDMARK_DAWN_MIRROR"), TEXT("REGION_DAWN_MIRRORS"),
        TEXT("Lustro Świtu"), TEXT("Punkt orientacyjny Próby Świtu."));
    AddLandmark(TEXT("LANDMARK_SHADOW_ARCH"), TEXT("REGION_SHADOW_MARSH"),
        TEXT("Łuk Cienia"), TEXT("Punkt orientacyjny Próby Cienia."));
    AddLandmark(TEXT("LANDMARK_LIFE_FOUNTAIN"), TEXT("REGION_LIFE_GARDEN"),
        TEXT("Źródło Życia"), TEXT("Punkt orientacyjny Próby Życia."));
    AddLandmark(TEXT("LANDMARK_FROST_GATE"), TEXT("REGION_FROST_GATE"),
        TEXT("Kryształowa Brama"), TEXT("Punkt orientacyjny Próby Mrozu."));
}

bool UJumanjiWorldAtlasSubsystem::LoadConfiguredAtlas()
{
    if (RegionDataTable.IsNull() && LandmarkDataTable.IsNull())
    {
        return false;
    }

    UDataTable* RegionTable = RegionDataTable.LoadSynchronous();
    UDataTable* LandmarkTable = LandmarkDataTable.LoadSynchronous();

    if (!RegionTable || !LandmarkTable
        || RegionTable->GetRowStruct() != FJumanjiWorldRegionDefinition::StaticStruct()
        || LandmarkTable->GetRowStruct() != FJumanjiWorldLandmarkDefinition::StaticStruct())
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI: world atlas DataTables are missing or use the wrong row structs; using built-in atlas."));
        return false;
    }

    TArray<FJumanjiWorldRegionDefinition*> RegionRows;
    TArray<FJumanjiWorldLandmarkDefinition*> LandmarkRows;
    RegionTable->GetAllRows<FJumanjiWorldRegionDefinition>(TEXT("JUMANJI world regions"), RegionRows);
    LandmarkTable->GetAllRows<FJumanjiWorldLandmarkDefinition>(TEXT("JUMANJI world landmarks"), LandmarkRows);

    if (RegionRows.Num() < 10 || RegionRows.Num() > 32
        || LandmarkRows.Num() < 1 || LandmarkRows.Num() > 128)
    {
        UE_LOG(LogTemp, Warning, TEXT("JUMANJI: world atlas DataTables have invalid row counts; using built-in atlas."));
        return false;
    }

    TArray<FJumanjiWorldRegionDefinition> NewRegions;
    TArray<FJumanjiWorldLandmarkDefinition> NewLandmarks;
    TSet<FName> RegionIds;
    TSet<FName> LandmarkIds;

    for (const FJumanjiWorldRegionDefinition* Row : RegionRows)
    {
        if (!Row || Row->RegionId.IsNone() || Row->DisplayName.IsEmpty()
            || !IsValidMapCoordinate(Row->MapCoordinate)
            || (Row->bHasNavigationAnchor && !IsValidWorldLocation(Row->NavigationAnchor))
            || RegionIds.Contains(Row->RegionId))
        {
            UE_LOG(LogTemp, Warning, TEXT("JUMANJI: invalid or duplicate region row; using built-in atlas."));
            return false;
        }
        RegionIds.Add(Row->RegionId);
        NewRegions.Add(*Row);
    }

    for (const FJumanjiWorldLandmarkDefinition* Row : LandmarkRows)
    {
        if (!Row || Row->LandmarkId.IsNone() || Row->RegionId.IsNone()
            || Row->DisplayName.IsEmpty() || LandmarkIds.Contains(Row->LandmarkId)
            || !RegionIds.Contains(Row->RegionId)
            || (Row->bHasWorldLocation && !IsValidWorldLocation(Row->WorldLocation)))
        {
            UE_LOG(LogTemp, Warning, TEXT("JUMANJI: invalid or duplicate landmark row; using built-in atlas."));
            return false;
        }
        LandmarkIds.Add(Row->LandmarkId);
        NewLandmarks.Add(*Row);
    }

    NewRegions.Sort([](const FJumanjiWorldRegionDefinition& A, const FJumanjiWorldRegionDefinition& B)
    {
        return A.SortOrder < B.SortOrder;
    });

    Regions = MoveTemp(NewRegions);
    Landmarks = MoveTemp(NewLandmarks);
    return true;
}

bool UJumanjiWorldAtlasSubsystem::GetRegionDefinition(
    FName RegionId,
    FJumanjiWorldRegionDefinition& OutDefinition) const
{
    const FJumanjiWorldRegionDefinition* Found = Regions.FindByPredicate(
        [RegionId](const FJumanjiWorldRegionDefinition& Region)
        {
            return Region.RegionId == RegionId;
        });

    if (!Found)
    {
        return false;
    }

    OutDefinition = *Found;
    return true;
}

bool UJumanjiWorldAtlasSubsystem::GetLandmarkDefinition(
    FName LandmarkId,
    FJumanjiWorldLandmarkDefinition& OutDefinition) const
{
    const FJumanjiWorldLandmarkDefinition* Found = Landmarks.FindByPredicate(
        [LandmarkId](const FJumanjiWorldLandmarkDefinition& Landmark)
        {
            return Landmark.LandmarkId == LandmarkId;
        });

    if (!Found)
    {
        return false;
    }

    OutDefinition = *Found;
    return true;
}

bool UJumanjiWorldAtlasSubsystem::IsKnownRegion(FName RegionId) const
{
    return Regions.ContainsByPredicate(
        [RegionId](const FJumanjiWorldRegionDefinition& Region)
        {
            return Region.RegionId == RegionId;
        });
}

bool UJumanjiWorldAtlasSubsystem::IsKnownLandmark(FName LandmarkId) const
{
    return Landmarks.ContainsByPredicate(
        [LandmarkId](const FJumanjiWorldLandmarkDefinition& Landmark)
        {
            return Landmark.LandmarkId == LandmarkId;
        });
}

bool UJumanjiWorldAtlasSubsystem::IsLandmarkInRegion(FName LandmarkId, FName RegionId) const
{
    FJumanjiWorldLandmarkDefinition Definition;
    return GetLandmarkDefinition(LandmarkId, Definition)
        && Definition.RegionId == RegionId;
}

void UJumanjiWorldAtlasSubsystem::GetWorldRegions(
    TArray<FJumanjiWorldRegionDefinition>& OutRegions) const
{
    OutRegions = Regions;
}

void UJumanjiWorldAtlasSubsystem::GetWorldLandmarks(
    TArray<FJumanjiWorldLandmarkDefinition>& OutLandmarks) const
{
    OutLandmarks = Landmarks;
}
