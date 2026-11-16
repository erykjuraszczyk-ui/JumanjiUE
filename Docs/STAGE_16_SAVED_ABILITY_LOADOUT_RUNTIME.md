# JUMANJI 2.111.1 — Stage 16: Saved Ability Loadout Runtime

## Cel etapu

Stage 16 zamyka lukę między logicznym loadoutem zapisanym w profilu v2 a rzeczywistymi umiejętnościami przyznawanymi przez Gameplay Ability System.

Profil nadal przechowuje stabilne identyfikatory `CharacterId`, `SlotId` i `AbilityId`. Nowy katalog serwerowy rozwiązuje zatwierdzone identyfikatory na klasy `UGameplayAbility`. Klient nie może przesłać ścieżki klasy ani arbitralnego `UObject`.

## Zakres implementacji

- `UJumanjiAbilityCatalogSubsystem` — allowlista AbilityId → definicja zdolności.
- `FJumanjiAbilityCatalogEntry` — typ wiersza DataTable z postacią, slotem, tagiem wejścia i soft class reference.
- 45 stabilnych domyślnych identyfikatorów (9 Great Spirit × 5 slotów).
- opcjonalny `DT_GreatSpiritAbilityCatalog` do przypisywania klas Blueprint bez zmiany C++.
- walidacja zgodności postaci, slotu, klasy, `AbilitySlot` i `InputTag`.
- filtrowanie zapisanych slotów podczas odtwarzania profilu.
- `RequestEquipAbilitySlot` oraz serwerowy RPC w PlayerController.
- odtwarzanie aktywnego loadoutu z profilu w ASC.
- śledzenie uchwytów zdolności w `UJumanjiAbilitySystemComponent` i anulowanie/usuwanie poprzednich grantów przy zmianie zestawu.
- ochrona przed ponownym przyznawaniem identycznego zestawu po ponownym posiadaniu pawna lub zwykłej zmianie progresji.

## 1. Stabilne identyfikatory i sloty

Każdy duch ma pięć slotów:

| SlotId | GAS AbilitySlot | Input Tag |
|---|---|---|
| `SLOT_PRIMARY` | PrimaryAttack | `Input.Ability.Primary` |
| `SLOT_ACTIVE` | Active | `Input.Ability.Active` |
| `SLOT_PASSIVE` | Passive | brak |
| `SLOT_ULTIMATE` | Ultimate | `Input.Ability.Ultimate` |
| `SLOT_DODGE` | Dodge | `Input.Ability.Dodge` |

Domyślne ID używają formatu `ABILITY_GS_<ROMAN>_<SLOT>`, np. `ABILITY_GS_I_PRIMARY`, `ABILITY_GS_IV_ULTIMATE` i `ABILITY_GS_IX_DODGE`.

To identyfikatory danych, nie nazwy plików `.uasset`. Można tworzyć kolejne warianty umiejętności z innymi `AbilityId`, jeśli przypisanie do postaci i slotu przejdzie walidację.

## 2. Katalog AbilityId → UGameplayAbility

Nowy katalog działa jako `UGameInstanceSubsystem`. Źródła danych:

1. wbudowany katalog startowy — generowany z dziewięciu definicji Great Spirit;
2. opcjonalny DataTable — może nadpisywać domyślne ID lub dodawać kolejne zatwierdzone warianty.

Katalog nie ufa danym z profilu. Przed dopuszczeniem wyboru sprawdza:

- czy CharacterId znajduje się w Great Spirit rosterze;
- czy SlotId jest jednym z pięciu dozwolonych slotów;
- czy AbilityId jest obecny w katalogu i przypisany do tej postaci oraz slotu;
- czy klasa jest potomkiem `UJumanjiGameplayAbility`;
- czy CDO klasy ma właściwy `EJumanjiAbilitySlot`;
- czy `InputTag` CDO odpowiada kontraktowi slotu (pasywna nie ma tagu wejściowego).

Klient wysyła wyłącznie stabilny `AbilityId`. Klasa jest rozwiązywana z zatwierdzonego katalogu po stronie serwera.

### 2.1. Katalog natywny

Domyślnie wszystkie dziewięć postaci otrzymuje wpisy dla pięciu slotów. Wbudowany katalog ma jeden działający fallback: `UJumanjiDefaultPlayerMeleeAbility` dla slotu PrimaryAttack. Pozostałe cztery sloty pozostają nieaktywne, dopóki nie zostaną przypisane prawidłowe klasy Blueprint w rosterze lub katalogu DataTable.

Nie oznacza to, że 45 gotowych zdolności zostało już napisanych. To 45 stabilnych miejsc katalogowych, z których część wymaga dalszej produkcji assetów.

### 2.2. Konfiguracja DataTable

Po utworzeniu tabeli w Unreal Editor ustaw w `Config/DefaultGame.ini`:

```ini
[/Script/JumanjiUE.JumanjiAbilityCatalogSubsystem]
ConfiguredAbilityDataTable=/Game/Abilities/Data/DT_GreatSpiritAbilityCatalog.DT_GreatSpiritAbilityCatalog
```

Row Structure: `FJumanjiAbilityCatalogEntry`.

Kolumny:

| Pole | Zawartość | Przykład dla pierwszej postaci |
|---|---|---|
| AbilityId | stabilne, unikalne ID | `ABILITY_GS_I_PRIMARY` |
| CharacterId | ID z rosteru | `GREAT_SPIRIT_I` |
| SlotId | jeden z pięciu slotów | `SLOT_PRIMARY` |
| AbilitySlot | wartość enum GAS | `PrimaryAttack` |
| InputTag | tag wejścia zgodny ze slotem | `Input.Ability.Primary` |
| DisplayName | nazwa dla UI | `Uderzenie Korzeni` |
| AbilityClass | klasa potomna UJumanjiGameplayAbility | `GA_GS_I_Primary` |
| bEnabled | czy wpis może być wybrany | `true` |

Walidator ignoruje wpisy niepoprawne, wyłączone, z nieznanym CharacterId, niezgodnym slotem/tagiem lub klasą spoza `UJumanjiGameplayAbility`. Wiersz z tym samym AbilityId nadpisuje wpis domyślny. Wiersz z nowym AbilityId dodaje alternatywną zdolność do danego slotu.

Przy braku tabeli gra korzysta z katalogu natywnego. Nie umieszczaj prywatnych kluczy ani sekretów w DataTable lub plikach konfiguracyjnych.

## 3. Walidacja i zapis wyboru

Wyposażenie zmienia się przez:

`AJumanjiPlayerController::RequestEquipAbilitySlot(CharacterId, SlotId, AbilityId)`

Przepływ:

1. UI pobiera listę możliwości przez `GetAbilityChoices(CharacterId, SlotId)`.
2. UI pokazuje tylko wpisy z prawidłową klasą i zgodnymi tagami.
3. Po zatwierdzeniu klient wysyła CharacterId, SlotId i AbilityId.
4. Serwer ponownie sprawdza wpis w katalogu.
5. `AJumanjiPlayerState::SetEquippedAbilitySlot` zapisuje stabilne ID w schema v2.
6. `OnProgressionChanged` informuje aktywne UI i pawn.
7. Serwer ponownie rozwiązuje aktualny loadout i przekazuje go do ASC.

Zwrócenie `true` po stronie klienta oznacza, że RPC został wysłany, a nie że serwer zaakceptował wybór. UI powinno czekać na `OnProgressionChanged` i odczytać zatwierdzony stan PlayerState.

## 4. Przywracanie profilu

Podczas `ApplyProfileSnapshot` każdy zapisany slot przechodzi ponowną walidację katalogu. Nieznane, niezgodne albo nieaktywne ID jest pomijane. Profil nadal odtwarza pozostałe dane, ale nie uruchamia klasy znalezionej wyłącznie w danych klienta.

Po przywróceniu profilu `OnProgressionChanged` wyzwala ponowne rozwiązanie loadoutu aktywnej postaci. Pawn używa PlayerState-owned ASC; nie tworzy drugiego komponentu.

Jeżeli zapisany slot nie ma już prawidłowego wpisu w katalogu, `ResolveEquippedLoadout` próbuje użyć dozwolonego domyślnego ID dla tego slotu. Jeżeli brak odpowiedniej klasy, slot pozostaje nieprzyznany.

## 5. ASC — bezpieczna wymiana zdolności

`UJumanjiAbilitySystemComponent::ApplyResolvedCharacterAbilityLoadout` działa tylko po stronie serwera:

- odrzuca puste ID, powtórzone ability/sloty i puste klasy;
- ponownie sprawdza, czy klasa dziedziczy po `UJumanjiGameplayAbility`;
- weryfikuje `AbilitySlot` i `InputTag` na CDO;
- nie przyznaje drugi raz identycznego zestawu dla tej samej postaci;
- anuluje i usuwa wyłącznie uchwyty należące do poprzedniego loadoutu Great Spirit;
- przyznaje nowe klasy przez `GiveAbility` i zachowuje zwrócone `FGameplayAbilitySpecHandle`;
- uruchamia zdolności pasywne, jeżeli ich CDO ma `bActivateOnGranted`.

Nie używa `ClearAllAbilities`, ponieważ ASC może w przyszłości posiadać zdolności systemowe niezwiązane z aktualnym Great Spirit.

## 6. Integracja z aktualnym sterowaniem

Sloty nadal wykorzystują dotychczasowe tagi wejściowe:
- PrimaryAttack — `Input.Ability.Primary`;
- Active — `Input.Ability.Active`;
- Ultimate — `Input.Ability.Ultimate`;
- Dodge — `Input.Ability.Dodge`;
- Passive — aktywacja przez `bActivateOnGranted` lub logikę zdarzeń.

Przyciski dotykowe mogą wywoływać istniejące `AJumanjiCharacter::ActivateAbilityByInputTag`. Nie powinny bezpośrednio modyfikować Health, Energy, Resonance ani zawartości ASC.

## 7. Ograniczenia etapu

- Nie dodano 45 gotowych klas Blueprint zdolności.
- Nie utworzono `DT_GreatSpiritAbilityCatalog.uasset`; tabela jest do utworzenia w Unreal Editor.
- Fallback podstawowego ataku jest wspólny dla dziewięciu postaci i służy do testów.
- Sloty Active, Passive, Ultimate i Dodge wymagają przypisania klas GAS.
- Ładowanie klas soft-reference jest synchroniczne; przed wydaniem mobilnym należy przygotować preload/asynchroniczne ładowanie.
- Zmiana loadoutu jest zapisywana w lokalnym SaveGame przez obecny prototyp profilu. Produkcyjne MMO będzie wymagało backendu transakcyjnego i synchronizacji między shardami.
- Nie zbudowano APK/AAB i nie przeprowadzono testu urządzenia Android.

## 8. Kryteria akceptacji

- [ ] Projekt kompiluje się w przypiętej wersji Unreal Engine 5.8.
- [ ] Katalog natywny zawiera 45 unikalnych ID dla dziewięciu postaci i pięciu slotów.
- [ ] Domyślny primary każdego ducha rozwiązuje się do fallbacku lub zatwierdzonej klasy DataTable.
- [ ] Nieznane CharacterId, SlotId i AbilityId są odrzucane.
- [ ] Klasa niepochodząca z UJumanjiGameplayAbility jest odrzucana.
- [ ] Klasa z niezgodnym AbilitySlot lub InputTag jest odrzucana.
- [ ] Zapis wyposażenia trafia do PlayerState i profilu v2.
- [ ] Wczytanie profilu odtwarza wyłącznie identyfikatory z katalogu.
- [ ] Zmiana slotu przechodzi przez serwerowy RPC i walidację po stronie serwera.
- [ ] `OnProgressionChanged` powoduje aktualizację aktywnego loadoutu.
- [ ] Zmiana Great Spirit anuluje i usuwa poprzednie granty, a następnie przyznaje właściwy zestaw.
- [ ] Zwykła zmiana XP lub ponowne posiadanie pawna nie przyznaje drugi raz tych samych umiejętności.
- [ ] Pasywna z `bActivateOnGranted` uruchamia się po przyznaniu i jest usuwana przy wymianie loadoutu.
- [ ] Multiplayer PIE: drugi klient nie może wyposażyć klasy spoza katalogu serwera.
- [ ] Testy profilu potwierdzają, że błędne AbilityId nie blokuje odtworzenia pozostałego profilu.

## 9. Weryfikacja

Pliki zostały zapisane na osobnej gałęzi i odczytane ponownie przez GitHub API. Nie wykonano kompilacji w Unreal Editor 5.8 ani testu PIE/Android w tym środowisku. Build oraz testy z checklisty pozostają wymagane przed scaleniem.

## Następny etap

Stage 17 — Great Spirit Ability Content Pack: utworzenie klas Blueprint dla umiejętności dziewięciu postaci, kosztów/cooldown Gameplay Effects, animacji, Gameplay Cues oraz konfiguracji DataTable.