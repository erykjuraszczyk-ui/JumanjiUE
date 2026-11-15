#include "JumanjiCharacterRosterSubsystem.h"

void UJumanjiCharacterRosterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BuildDefaultRoster();
}

void UJumanjiCharacterRosterSubsystem::BuildDefaultRoster()
{
    CharacterDefinitions.Reset();
    CharacterDefinitions.Reserve(9);

    const auto AddSpirit = [this](
        const TCHAR* Id,
        int32 Number,
        const TCHAR* Name,
        const TCHAR* Title,
        const TCHAR* Description,
        EJumanjiSpiritElement Element,
        EJumanjiCombatRole Role,
        const TCHAR* Signature,
        const TCHAR* Passive,
        const FLinearColor& Primary,
        const FLinearColor& Secondary)
    {
        FJumanjiCharacterDefinition Definition;
        Definition.CharacterId = FName(Id);
        Definition.SpiritNumber = Number;
        Definition.DisplayName = FText::FromString(Name);
        Definition.Title = FText::FromString(Title);
        Definition.Description = FText::FromString(Description);
        Definition.Element = Element;
        Definition.Role = Role;
        Definition.SignatureAbilityName = FText::FromString(Signature);
        Definition.PassiveAbilityName = FText::FromString(Passive);

        static const TCHAR* PrimaryNames[9] = {
            TEXT("Uderzenie Korzeni"), TEXT("Strumień Przypływu"), TEXT("Cios Granitu"),
            TEXT("Iskra Burzy"), TEXT("Płomienny Impuls"), TEXT("Promień Świtu"),
            TEXT("Cięcie Cienia"), TEXT("Dotyk Życia"), TEXT("Odłamek Mrozu")
        };
        static const TCHAR* UltimateNames[9] = {
            TEXT("Serce Pradawnej Kniei"), TEXT("Wielki Przypływ"), TEXT("Góra Niezłomna"),
            TEXT("Gniew Niebios"), TEXT("Feniks Odrodzenia"), TEXT("Korona Świtu"),
            TEXT("Wieczna Noc"), TEXT("Ogród Wieczności"), TEXT("Zimowy Tron")
        };
        static const TCHAR* DodgeNames[9] = {
            TEXT("Krok Pnącza"), TEXT("Fala Uniku"), TEXT("Kamienny Krok"),
            TEXT("Błysk Burzy"), TEXT("Żarliwy Skok"), TEXT("Świetlisty Krok"),
            TEXT("Cień Przemieszczenia"), TEXT("Zielony Zwrot"), TEXT("Lodowy Ślizg")
        };

        if (Number >= 1 && Number <= 9)
        {
            Definition.PrimaryAttackAbilityName = FText::FromString(PrimaryNames[Number - 1]);
            Definition.UltimateAbilityName = FText::FromString(UltimateNames[Number - 1]);
            Definition.DodgeAbilityName = FText::FromString(DodgeNames[Number - 1]);
        }

        Definition.PrimaryColor = Primary;
        Definition.SecondaryColor = Secondary;
        CharacterDefinitions.Add(MoveTemp(Definition));
    };

    // The nine identities follow the supplied artwork. Gameplay details below
    // are new design proposals; the image itself does not define roles or powers.
    AddSpirit(TEXT("GREAT_SPIRIT_I"), 1,
        TEXT("Duch Korzeni"), TEXT("Strażnik Kniei"),
        TEXT("Strażnik dawnej dżungli. Łączy wytrzymałość z kontrolą pola walki."),
        EJumanjiSpiritElement::Root, EJumanjiCombatRole::Guardian,
        TEXT("Korzenna Fala"), TEXT("Zew Korzeni"),
        FLinearColor(0.10f, 0.72f, 0.95f), FLinearColor(0.08f, 0.20f, 0.34f));

    AddSpirit(TEXT("GREAT_SPIRIT_II"), 2,
        TEXT("Duch Przypływu"), TEXT("Tkaczka Fal"),
        TEXT("Władca energii przypływu. Wspiera drużynę i osłabia przeciwników."),
        EJumanjiSpiritElement::Tide, EJumanjiCombatRole::Support,
        TEXT("Prąd Ukojenia"), TEXT("Taniec Fal"),
        FLinearColor(0.05f, 0.82f, 0.91f), FLinearColor(0.05f, 0.27f, 0.42f));

    AddSpirit(TEXT("GREAT_SPIRIT_III"), 3,
        TEXT("Duch Kamienia"), TEXT("Serce Gór"),
        TEXT("Niezłomny obrońca. Wykorzystuje ciężar ziemi do osłon i przełamywania obrony."),
        EJumanjiSpiritElement::Stone, EJumanjiCombatRole::Vanguard,
        TEXT("Tarcza Pradawnych"), TEXT("Serce Skały"),
        FLinearColor(0.87f, 0.66f, 0.30f), FLinearColor(0.30f, 0.20f, 0.12f));

    AddSpirit(TEXT("GREAT_SPIRIT_IV"), 4,
        TEXT("Duch Burzy"), TEXT("Oko Pioruna"),
        TEXT("Mistrz energii burzy. Kontroluje przestrzeń i tworzy okazje do kombinacji."),
        EJumanjiSpiritElement::Storm, EJumanjiCombatRole::Controller,
        TEXT("Piorunujący Krąg"), TEXT("Znak Burzy"),
        FLinearColor(0.08f, 0.88f, 0.64f), FLinearColor(0.04f, 0.25f, 0.28f));

    AddSpirit(TEXT("GREAT_SPIRIT_V"), 5,
        TEXT("Duch Żaru"), TEXT("Płomień Feniksa"),
        TEXT("Wojownik energii ognia. Specjalizuje się w krótkich seriach obrażeń."),
        EJumanjiSpiritElement::Ember, EJumanjiCombatRole::Striker,
        TEXT("Płomień Feniksa"), TEXT("Rozżarzony Ślad"),
        FLinearColor(1.00f, 0.28f, 0.10f), FLinearColor(0.34f, 0.07f, 0.04f));

    AddSpirit(TEXT("GREAT_SPIRIT_VI"), 6,
        TEXT("Duch Świtu"), TEXT("Oko Horyzontu"),
        TEXT("Precyzyjny wędrowiec. Wykorzystuje światło do oznaczania celów i ataków z dystansu."),
        EJumanjiSpiritElement::Dawn, EJumanjiCombatRole::Ranger,
        TEXT("Promień Przeznaczenia"), TEXT("Oko Świtu"),
        FLinearColor(0.10f, 0.68f, 1.00f), FLinearColor(0.82f, 0.48f, 0.18f));

    AddSpirit(TEXT("GREAT_SPIRIT_VII"), 7,
        TEXT("Duch Cienia"), TEXT("Cichy Łowca"),
        TEXT("Szybki pojedynkowicz. Buduje przewagę przez zmianę pozycji i precyzyjne kontry."),
        EJumanjiSpiritElement::Shadow, EJumanjiCombatRole::Duelist,
        TEXT("Rozdarcie Cienia"), TEXT("Echo Łowcy"),
        FLinearColor(0.88f, 0.15f, 0.13f), FLinearColor(0.16f, 0.05f, 0.08f));

    AddSpirit(TEXT("GREAT_SPIRIT_VIII"), 8,
        TEXT("Duch Życia"), TEXT("Strażniczka Odnowy"),
        TEXT("Opiekunka życia. Przywraca siły, wzmacnia sojuszników i wspiera długie wyprawy."),
        EJumanjiSpiritElement::Life, EJumanjiCombatRole::Healer,
        TEXT("Ogród Odnowy"), TEXT("Zielone Tchnienie"),
        FLinearColor(0.12f, 0.78f, 0.38f), FLinearColor(0.05f, 0.24f, 0.15f));

    AddSpirit(TEXT("GREAT_SPIRIT_IX"), 9,
        TEXT("Duch Mrozu"), TEXT("Korona Zimy"),
        TEXT("Strażnik zimnej energii. Tworzy bariery i ogranicza ruch przeciwników."),
        EJumanjiSpiritElement::Frost, EJumanjiCombatRole::Sentinel,
        TEXT("Lodowy Bastion"), TEXT("Korona Zimy"),
        FLinearColor(0.42f, 0.83f, 1.00f), FLinearColor(0.88f, 0.77f, 0.48f));
}

void UJumanjiCharacterRosterSubsystem::GetRosterForBlueprint(TArray<FJumanjiCharacterDefinition>& OutRoster) const
{
    OutRoster = CharacterDefinitions;
}

bool UJumanjiCharacterRosterSubsystem::GetCharacterDefinition(
    FName CharacterId,
    FJumanjiCharacterDefinition& OutDefinition) const
{
    const FJumanjiCharacterDefinition* Found = CharacterDefinitions.FindByPredicate(
        [CharacterId](const FJumanjiCharacterDefinition& Definition)
        {
            return Definition.CharacterId == CharacterId;
        });

    if (!Found)
    {
        return false;
    }

    OutDefinition = *Found;
    return true;
}

bool UJumanjiCharacterRosterSubsystem::IsPlayableCharacter(FName CharacterId) const
{
    return CharacterDefinitions.ContainsByPredicate(
        [CharacterId](const FJumanjiCharacterDefinition& Definition)
        {
            return Definition.CharacterId == CharacterId;
        });
}

FName UJumanjiCharacterRosterSubsystem::GetDefaultCharacterId() const
{
    return CharacterDefinitions.IsEmpty()
        ? NAME_None
        : CharacterDefinitions[0].CharacterId;
}
