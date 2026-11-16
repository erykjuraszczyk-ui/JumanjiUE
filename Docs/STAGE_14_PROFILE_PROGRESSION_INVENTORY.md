# JUMANJI 2.111.1 — Stage 14: Profile Progression & Inventory Persistence

## Cel etapu

Rozszerzyć istniejący profil gracza z Stage 13 o:
- poziom konta i doświadczenie;
- indywidualny poziom oraz doświadczenie każdego Great Spirit;
- trzy waluty prototypu;
- ekwipunek materiałów i przedmiotów;
- logiczne sloty wyposażonego zestawu umiejętności dla każdego Great Spirit;
- transakcyjne, serwerowe przyznawanie i zużywanie waluty/przedmiotów;
- trwały rejestr identyfikatorów transakcji chroniący przed powtórnym przyznaniem tej samej nagrody.

Stage 14 rozwija istniejące AJumanjiPlayerState, UJumanjiPlayerProfileSubsystem i UJumanjiPlayerProfileSnapshot. Nie wprowadza drugiego systemu kont, ekwipunku ani profilu.

## Zakres i ważne ograniczenia

Backend profilu nadal korzysta z lokalnego Unreal SaveGame zapisywanego przez proces serwera. To prototyp dla standalone / jednego procesu. Nie jest to produkcyjna ekonomia MMORPG.

W środowisku produkcyjnym:
- saldo i ekwipunek muszą być autorytatywne po stronie backendu;
- nagroda i zapis klucza idempotencji muszą być jedną transakcją bazy danych;
- wiele shardów musi korzystać z jednego spójnego źródła profilu;
- operacje zakupu, ulepszenia i wymiany muszą mieć identyfikator idempotencji;
- klient nigdy nie może przesyłać własnego salda ani docelowej ilości przedmiotów.

Lokalny SaveGame nie jest szyfrowanym ani odpornym na manipulację magazynem konta.

## 1. Schemat profilu v2

Plik: `Source/JumanjiUE/Public/JumanjiPlayerProfileSaveGame.h`

`FJumanjiPlayerProfileSnapshot` zawiera teraz:
- `SchemaVersion = 2`;
- identyfikator profilu;
- Great Spirit wybrany przez gracza;
- odkryte regiony i odwiedzone landmarki;
- revision eksploracji i ukończone rozdziały;
- `FJumanjiPlayerProgressionState`;
- `ProcessedTransactionIds`;
- czas przygotowania zapisu.

### Dane progresji

`FJumanjiPlayerProgressionState` przechowuje:
- `AccountLevel` i `AccountExperience`;
- `Currencies` — identyfikatory walut i salda;
- `Inventory` — identyfikatory przedmiotów i ilości;
- `CharacterProgress` — poziom i doświadczenie poszczególnych Great Spirit;
- `EquippedAbilitySlots` — logiczne ID postaci, slotu i umiejętności;
- `Revision` — wersję snapshotu progresji.

Profil przechowuje stabilne identyfikatory, nie wskaźniki do UObject, aktorów, map ani klas Blueprint.

## 2. Migracja schema v1 → v2

Stage 13 zapisywał `SchemaVersion = 1`. Odrzucenie tych zapisów oznaczałoby utratę wcześniejszego wyboru Great Spirit i odkryć świata.

`UJumanjiPlayerProfileSubsystem::LoadProfile`:
1. rozpoznaje schema v1;
2. waliduje wszystkie pola starego snapshotu;
3. kopiuje identyfikator profilu, wybór postaci, eksplorację i ukończone rozdziały;
4. tworzy bezpieczny, pusty stan progresji v2;
5. zwraca snapshot jako schema v2;
6. odrzuca nieobsługiwane lub niepoprawne zapisy bez automatycznego kasowania slotu.

Po przywróceniu profilu PlayerState normalizuje stan według aktualnego katalogu i zleca zapis schema v2. Stare dane eksploracji i wybór postaci pozostają zachowane.

## 3. Katalog walut

Pliki:
- `Source/JumanjiUE/Public/JumanjiProgressionCatalogSubsystem.h`
- `Source/JumanjiUE/Private/JumanjiProgressionCatalogSubsystem.cpp`

Katalog prototypu zawiera:

| ID | Nazwa | Saldo startowe | Limit |
|---|---|---:|---:|
| CURRENCY_EXPLORER_COINS | Monety Odkrywców | 250 | 2 000 000 000 |
| CURRENCY_BOARD_SEALS | Pieczęcie Planszy | 0 | 1 000 000 000 |
| CURRENCY_ECHO_CRYSTALS | Kryształy Echa | 0 | 1 000 000 000 |

Saldo jest liczbą całkowitą i nie może zejść poniżej zera ani przekroczyć limitu katalogowego.

Nie dodano płatnej waluty ani mechaniki losowych skrzynek. Waluty są zasobami rozgrywki.

## 4. Katalog materiałów

Katalog przedmiotów na tym etapie zawiera materiały rozwoju:

| ID | Nazwa |
|---|---|
| MAT_ROOT_RESIN | Żywica Korzeni |
| MAT_TIDE_PEARL | Perła Przypływu |
| MAT_STONE_CORE | Rdzeń Kamienia |
| MAT_STORM_ESSENCE | Esencja Burzy |
| MAT_EMBER_ASH | Popiół Żaru |
| MAT_DAWN_SHARD | Odłamek Świtu |
| MAT_SHADOW_SILK | Jedwab Cienia |
| MAT_LIFE_BLOOM | Kwiat Życia |
| MAT_FROST_CRYSTAL | Kryształ Mrozu |
| MAT_SPIRIT_MEMORY | Pamięć Ducha |

Każdy przedmiot ma:
- stabilny ItemId;
- nazwę dla UI;
- kategorię;
- limit stosu (9999 w katalogu prototypu);
- flagę możliwości zużycia.

Nie dodano jeszcze losowej generacji statystyk, ekwipunku pancerza, systemu przedmiotów handlowych ani aukcji.

## 5. Poziomy konta i Great Spirit

Katalog progresji określa:
- maksymalny poziom konta: 100;
- maksymalny poziom postaci: 80;
- wymagane XP konta na kolejny poziom: `200 + 100 × aktualny poziom`;
- wymagane XP Great Spirit na kolejny poziom: `150 + 90 × aktualny poziom`.

Experience w snapshotcie oznacza XP zgromadzone w bieżącym poziomie, nie sumę całego XP od początku gry.

Przykładowo konto na poziomie 1 potrzebuje 300 XP do poziomu 2. Great Spirit na poziomie 1 potrzebuje 240 XP do poziomu 2.

Nowy profil otrzymuje:
- AccountLevel 1;
- wszystkie dziewięć Great Spirit na poziomie 1;
- saldo startowe z katalogu;
- pusty ekwipunek materiałów;
- pusty rejestr transakcji.

W Stage 14 poziomy i XP są zapisywane i replikowane. Automatyczne skalowanie AttackPower/Defense przez poziom, drzewka talentów i kosztowne ulepszenia statystyk wymagają osobnego balansu i kolejnego etapu.

## 6. Sloty wyposażonego zestawu umiejętności

`FJumanjiEquippedAbilitySlot` przechowuje:
- CharacterId;
- SlotId;
- AbilityId.

Dozwolone sloty:
- SLOT_PRIMARY;
- SLOT_ACTIVE;
- SLOT_PASSIVE;
- SLOT_ULTIMATE;
- SLOT_DODGE.

`AJumanjiPlayerState::SetEquippedAbilitySlot` sprawdza:
- wywołanie po stronie serwera;
- istniejącą postać Great Spirit;
- dozwolony SlotId;
- niepuste AbilityId;
- limit maksymalnie 45 wpisów (9 postaci × 5 slotów).

Ważne: Stage 14 zapisuje logiczne identyfikatory slotów i umiejętności. Nie ładuje arbitralnych klas dostarczonych przez klienta i nie podmienia jeszcze aktywnych klas GAS. Faktyczne rozwiązywanie AbilityId na zatwierdzone klasy Gameplay Ability wymaga katalogu umiejętności i zostanie spięte z istniejącym ASC w kolejnym etapie.

## 7. Transakcje profilu — all-or-nothing

Plik: `Source/JumanjiUE/Public/JumanjiProgressionTypes.h`

`FJumanjiProfileTransaction` opisuje jedną zmianę:
- `TransactionId` — stabilny identyfikator idempotencji;
- XP konta;
- opcjonalny CharacterId i XP Great Spirit;
- listę zmian walut;
- listę zmian przedmiotów.

Zmiana waluty/przedmiotu może być dodatnia (przyznanie) lub ujemna (zużycie).

API:
`AJumanjiPlayerState::ApplyProfileTransaction(Transaction)`

### Kolejność walidacji

1. Serwer sprawdza autorytet i przypisany profil.
2. Odrzuca pusty lub wcześniej przetworzony TransactionId.
3. Odrzuca nadmierną liczbę zmian i przekroczenie limitów wejściowych.
4. Weryfikuje waluty w katalogu.
5. Weryfikuje przedmioty w katalogu.
6. Oblicza salda i ilości na kopiach aktualnego stanu.
7. Odrzuca saldo ujemne lub przekroczenie limitu waluty/stosu.
8. Weryfikuje CharacterId i oblicza poziomy po XP.
9. Dopiero po poprawnej walidacji przypisuje cały nowy stan.
10. Dodaje TransactionId do rejestru, replikuje zmianę i zleca jeden zapis profilu.

Jeżeli dowolny element transakcji jest niepoprawny, nie zostaje zastosowana żadna część zmiany.

### Ochrona przed powtórnym przyznaniem

`ProcessedTransactionIds` znajduje się w tym samym snapshotcie co saldo i ekwipunek. Ten sam TransactionId nie może zostać zastosowany dwa razy w aktywnym profilu.

Limit prototypu wynosi 2048 identyfikatorów. Po jego osiągnięciu nowe transakcje są odrzucane zamiast automatycznie usuwać historię i otwierać możliwość ponownego przyznania starej nagrody.

To zabezpieczenie lokalnego prototypu. W produkcji idempotency key musi być przechowywany w trwałym backendzie w tej samej transakcji bazodanowej co saldo i ekwipunek. Asynchroniczny lokalny SaveGame nie gwarantuje pełnej odporności na utratę zasilania lub awarię procesu w trakcie I/O.

## 8. Przykładowa transakcja nagrody

Przykład danych przekazywanych przez zaufany kod serwerowy:

```text
TransactionId: CHAPTER_EMERALD_01_REWARD_PLAYER_123
AccountExperienceDelta: 300
CharacterId: GREAT_SPIRIT_I
CharacterExperienceDelta: 180
CurrencyDeltas:
  - CURRENCY_EXPLORER_COINS: +150
  - CURRENCY_BOARD_SEALS: +2
ItemDeltas:
  - MAT_ROOT_RESIN: +3
  - MAT_SPIRIT_MEMORY: +1
```

To jest przykład kontraktu, nie gotowa nagroda przypisana do ukończenia rozdziału. Konkretne tabele nagród i ich balans powinny być zatwierdzone w osobnym dokumencie zawartości. Nie wywołuj transakcji na podstawie danych przesłanych bezpośrednio przez klienta.

## 9. API dla UI i serwerowej logiki

`AJumanjiPlayerState` udostępnia:
- `GetProgressionState()`;
- `GetCurrencyBalance(CurrencyId)`;
- `GetItemQuantity(ItemId)`;
- `GetGreatSpiritLevel(CharacterId)`;
- `ApplyProfileTransaction(Transaction)` — tylko autorytatywna logika serwera;
- `SetEquippedAbilitySlot(CharacterId, SlotId, AbilityId)` — tylko serwer.

`OnProgressionChanged` informuje UI o nowym snapshotcie progresji. Widget powinien odczytać stan i wyświetlić go; nie powinien bezpośrednio zmieniać salda, poziomu lub ilości przedmiotów.

## 10. Testy i kryteria akceptacji

### Migracja
- [ ] Uruchomić profil utworzony w Stage 13 (schema v1).
- [ ] Wczytać go w Stage 14.
- [ ] Sprawdzić, że Great Spirit, odkryte regiony, landmarki i rozdziały pozostały.
- [ ] Sprawdzić, że nowy profil ma schema v2.
- [ ] Uruchomić grę ponownie i sprawdzić, że schema v2 zostało zapisane.

### Progresja
- [ ] Nowe konto zaczyna na poziomie 1.
- [ ] Dodać XP konta i sprawdzić próg 300 XP dla poziomu 1.
- [ ] Dodać XP wybranego Great Spirit i sprawdzić próg 240 XP.
- [ ] Sprawdzić poziom maksymalny i brak dalszego przyrostu poziomu.
- [ ] Sprawdzić poziomy wszystkich dziewięciu Great Spirit.

### Waluta i ekwipunek
- [ ] Przyznać Monety Odkrywców.
- [ ] Zużyć część waluty.
- [ ] Odrzucić transakcję, która sprowadza saldo poniżej zera.
- [ ] Przyznać i zużyć materiały.
- [ ] Odrzucić nieznany ItemId.
- [ ] Odrzucić stos powyżej 9999.
- [ ] Sprawdzić, że nieudana transakcja nie zmienia żadnego elementu.

### Idempotencja
- [ ] Zastosować transakcję z nowym TransactionId.
- [ ] Ponowić ją z tym samym TransactionId.
- [ ] Sprawdzić, że druga próba jest odrzucona.
- [ ] Sprawdzić, że saldo, itemy i XP nie wzrosły drugi raz.
- [ ] Sprawdzić limit rejestru transakcji.

### Loadout
- [ ] Zapisać AbilityId w każdym z pięciu dozwolonych slotów.
- [ ] Odrzucić nieznany SlotId.
- [ ] Odrzucić nieznaną postać.
- [ ] Zrestartować grę i sprawdzić zachowanie zapisanych wpisów.
- [ ] Potwierdzić, że Stage 14 nie ładuje dowolnych klas GAS z klienta.

### Multiplayer i zapis
- [ ] Dwóch graczy ma różne profile.
- [ ] Zmiana salda jednego profilu nie zmienia drugiego.
- [ ] Profil zapisuje waluty, itemy, poziomy, loadouty i rejestr transakcji.
- [ ] Testować rozłączenie w czasie asynchronicznego zapisu.
- [ ] Sprawdzić logi nieudanego zapisu.

## 11. Ograniczenia i następne prace

Stage 14 nie dostarcza:
- produkcyjnego backendu profili;
- sklepu, aukcji i handlu między graczami;
- przedmiotów z losowymi statystykami;
- zbroi i slotów sprzętu;
- kosztów ulepszeń oraz pełnego drzewa rozwoju;
- automatycznego wpływu poziomu na statystyki GAS;
- katalogu, który mapuje zapisane AbilityId na zatwierdzone klasy GAS;
- automatycznego systemu nagród dla wszystkich rozdziałów;
- zabezpieczonej gospodarki wieloserwerowej;
- APK/AAB.

## 12. Weryfikacja

Zmiany w tej gałęzi zostały odczytane ponownie przez API GitHub. Nie wykonano kompilacji Unreal Engine 5.8, PIE ani testu na urządzeniu Android w tym środowisku. Przed scaleniem należy wykonać pełny build oraz testy z tej listy na lokalnej instalacji UE 5.8.

## Następny etap

**Stage 15 — Progression Gameplay Integration & Reward Tables**: powiązanie poziomów z atrybutami GAS, definicje nagród za rozdziały i triale, katalog AbilityId → Gameplay Ability class, koszty ulepszeń i rzeczywista integracja zapisywanych loadoutów z ASC.
