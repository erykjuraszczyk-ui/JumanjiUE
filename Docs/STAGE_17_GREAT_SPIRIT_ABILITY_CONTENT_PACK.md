# JUMANJI 2.111.1 — Stage 17: Great Spirit Ability Content Pack

## Cel etapu

Stage 17 rozwija Stage 16. Katalog zapisanych AbilityId pozostaje źródłem prawdy, ale twórcy otrzymują wspólną bazę wykonawczą do przygotowania konkretnych umiejętności dziewięciu Great Spirit.

Ten etap dostarcza:
- UJumanjiConfiguredEffectAbility — konfigurowalną bazę dla zdolności opartych o Gameplay Effects;
- serwerowe wykrywanie celów: Self, Forward Sweep, Radial Pulse i Dash;
- filtr relacji celu: Self Only, Friendly, Hostile i Any;
- aplikowanie efektu do celu lub osobnego efektu do właściciela;
- wartości SetByCaller oraz opcjonalne skalowanie AttackPower;
- opcjonalny Animation Montage i Gameplay Cue;
- punkt rozszerzenia Blueprint dla dodatkowej logiki na trafionym celu;
- matrycę produkcyjną 45 umiejętności (9 postaci × 5 slotów);
- instrukcję tworzenia Gameplay Effects, Blueprint children i wpisów katalogu.

To jest runtime i plan zawartości. Repozytorium nie zawiera automatycznie 45 gotowych Blueprintów, efektów .uasset, animacji, Niagara ani ikon.

## 1. Nowa klasa runtime

Pliki:
- Source/JumanjiUE/Public/JumanjiConfiguredEffectAbility.h
- Source/JumanjiUE/Private/JumanjiConfiguredEffectAbility.cpp

Każdy Blueprint umiejętności może dziedziczyć po UJumanjiConfiguredEffectAbility. W CDO klasy ustaw:
- AbilitySlot i właściwy InputTag z kontraktu Stage 16;
- TargetingMode oraz TargetTeam;
- TargetEffectClass, TargetMagnitudeTag, TargetBaseMagnitude i opcjonalny TargetAttackPowerCoefficient;
- SelfEffectClass i pola SetByCaller, jeśli aktywacja ma dodatkowo wpływać na właściciela;
- ActivationMontage, MontagePlayRate i opcjonalny GameplayCueTag;
- odziedziczone przez GAS CostGameplayEffectClass i CooldownGameplayEffectClass.

### Tryby działania

| Tryb | Zachowanie |
|---|---|
| Self | aplikuje Target Effect wyłącznie na właściciela |
| Forward Sweep | wykonuje po stronie serwera sferyczny sweep do przodu |
| Radial Pulse | wykonuje po stronie serwera overlap w promieniu od właściciela |
| Dash | wykonuje kontrolowany impuls ruchu do przodu; może też użyć Self Effect |

Promień i dystans są wyrażone w centymetrach Unreal Engine. MaxTargets = 0 oznacza brak dodatkowego limitu liczby celów.

### Filtr celu

- SelfOnly: tylko właściciel.
- Friendly: ta sama strona prototypowego PvE.
- Hostile: przeciwna strona prototypowego PvE.
- Any: każdy aktor posiadający kompatybilny Ability System Component.

Obecna klasyfikacja PvE rozróżnia AJumanjiEnemyCharacter i pozostałe postacie. Nie jest to jeszcze system frakcji MMO ani reguła PvP. Nie używaj Any dla obrażeń bez świadomej decyzji projektowej.

### SetByCaller

TargetMagnitudeTag i SelfMagnitudeTag przekazują podpisaną wartość do Gameplay Effect:
- ujemna wartość może reprezentować obrażenia w efekcie, który odejmuje Health;
- dodatnia wartość może reprezentować leczenie w efekcie, który dodaje Health;
- inne znaczenia zależą od modyfikatora i atrybutu zdefiniowanego w konkretnym Gameplay Effect.

Dla wspólnego efektu dodano tag Data.Ability.Magnitude. Wpis tagu musi odpowiadać SetByCaller modifier w assetcie. Jeśli Gameplay Effect używa innego tagu, ustaw dokładnie ten sam tag w Blueprint umiejętności.

TargetAttackPowerCoefficient dodaje do magnitudy wartość AttackPower × coefficient. Dla efektów, które nie powinny skalować się z AttackPower, pozostaw współczynnik równy 0.

## 2. Koszty i czasy odnowienia

Koszt i cooldown powinny być realizowane przez GAS Gameplay Effects, nie przez bezpośrednią zmianę atrybutów w UI.

Proponowane assety startowe:
- GE_Cost_Active_Energy_20: Instant; Energy Additive = -20.
- GE_Cost_Ultimate_Resonance_100: Instant; Resonance Additive = -100.
- GE_Cooldown_Active_10s: Has Duration = 10 s; przyznaje tag cooldown slotu Active.
- GE_Cooldown_Ultimate_60s: Has Duration = 60 s; przyznaje tag cooldown slotu Ultimate.
- GE_Cooldown_Dodge_4s: Has Duration = 4 s; przyznaje tag cooldown slotu Dodge.
- GE_Heal_Base: Instant; Health Additive z dodatnią wartością SetByCaller.
- GE_Damage_Base: istniejący efekt obrażeń; Health Additive z ujemną wartością SetByCaller.

Wartości 20 Energy, 100 Resonance, 10 s, 60 s i 4 s są punktami startowymi do balansu, a nie wartościami finalnymi. Dla różnych zdolności można tworzyć osobne efekty cooldown o różnych czasach.

Ważne: nie konfiguruj kosztu lub cooldownu opartego na SetByCaller i nie zakładaj, że standardowe CommitAbility automatycznie uzupełni dowolną wartość SetByCaller. Na tym etapie korzystaj z efektów kosztu o stałym modyfikatorze oraz efektów cooldown o stałym czasie, chyba że dana klasa jawnie nadpisze logikę ApplyCost/ApplyCooldown.

## 3. Wymagane assety w Unreal Editor

Utwórz katalogi:
- /Game/Abilities/GreatSpirits;
- /Game/Abilities/Effects;
- /Game/Abilities/Data;
- /Game/Abilities/Animations;
- /Game/Abilities/Cues.

Utwórz Blueprinty dziedziczące po UJumanjiConfiguredEffectAbility dla umiejętności Primary, Active, Ultimate i Dodge. Każdy Blueprint musi mieć prawidłowy AbilitySlot oraz InputTag.

Pasywne umiejętności mają logikę zdarzeniową, dlatego nie należy mechanicznie robić z nich krótkiego sweepa. Ich Blueprint powinien reagować na odpowiednie Gameplay Event/Gameplay Tag lub logikę systemu walki, a wszystkie przyznane efekty czasowe muszą być usuwane przy utracie pasywki. Jeśli efekt pasywny wymaga trwałego nasłuchiwania zdarzeń, przygotuj dla niego dedykowany Blueprint flow zamiast traktować bActivateOnGranted jako gotowy system pasywnych.

### DataTable katalogu

Utwórz:
- asset: /Game/Abilities/Data/DT_GreatSpiritAbilityCatalog;
- Row Structure: FJumanjiAbilityCatalogEntry.

W każdym wierszu ustaw:
AbilityId, CharacterId, SlotId, AbilitySlot, InputTag, DisplayName, AbilityClass i bEnabled.

Dla slotu Passive InputTag ma pozostać pusty. Pozostałe sloty używają:
- Primary: Input.Ability.Primary;
- Active: Input.Ability.Active;
- Ultimate: Input.Ability.Ultimate;
- Dodge: Input.Ability.Dodge.

Włącz tabelę w DefaultGame.ini:

~~~ini
[/Script/JumanjiUE.JumanjiAbilityCatalogSubsystem]
ConfiguredAbilityDataTable=/Game/Abilities/Data/DT_GreatSpiritAbilityCatalog.DT_GreatSpiritAbilityCatalog
~~~

Zapis profilu nadal przechowuje wyłącznie stabilne ID. Nie wpisuj ścieżek klas Blueprint do profilu i nie przyjmuj klas przesyłanych przez klienta.

## 4. Matryca zawartości

Pełna matryca z nazwami 45 klas, celami, zachowaniem i proponowanymi ustawieniami znajduje się w:
Docs/Design/GreatSpiritAbilityProductionMatrix.md.

Reguły wspólne:
- Primary: bez obowiązkowego kosztu zasobu; bazowy atak może używać krótkiego Forward Sweep.
- Active: proponowany koszt 20 Energy i bazowy cooldown 10 s.
- Passive: bez ręcznego wejścia; obsługa zdarzeń lub trwałych efektów z kontrolowanym cyklem życia.
- Ultimate: proponowany koszt 100 Resonance i bazowy cooldown 60 s.
- Dodge: Dash; bazowy cooldown 4 s.
- Wartości obrażeń, leczenia, promieni i czasów są tuningiem startowym. Testuj je z całą dziewiątką postaci, a nie tylko z jednym bohaterem.

## 5. Prezentacja

Każdy skonfigurowany Blueprint może przypisać:
- Animation Montage;
- Gameplay Cue;
- efekty Niagara i dźwiękowe wywoływane przez Gameplay Cue;
- dodatkową logikę w BP_OnConfiguredTargetApplied.

Nie umieszczaj ciężkiej prezentacji wyłącznie w kodzie aktywacji. Gameplay Cue jest miejscem na replikowaną prezentację efektu; asset cue oraz system Niagara/audio należy przygotować w Unreal Editor. Zadbaj o czytelność efektu na ekranie telefonu i nie zasłaniaj nim sygnałów ataku przeciwnika.

## 6. Ograniczenia

- Nie utworzono 45 plików Blueprint .uasset.
- Nie utworzono Gameplay Effect, Gameplay Cue, Animation Montage, Niagara ani ikon.
- Klasa bazowa oferuje wspólne targetowanie i aplikowanie efektów; unikalne reakcje (np. znak celu, osłona środowiskowa, oczyszczanie statusu, wieloetapowa sekwencja) wymagają dedykowanego Blueprint flow lub dalszego kodu.
- Pasywki wymagają osobnej logiki zdarzeniowej i obsługi usuwania przyznanych efektów.
- Targeting korzysta z ECC_Pawn; wymagane są poprawne kolizje aktorów.
- Nie zaimplementowano jeszcze systemu frakcji, PvP, odporności statusowych, absorpcji tarczy ani pełnej obsługi crowd control.
- Nie wykonano kompilacji UE 5.8, testu PIE, testu dwóch klientów ani builda Android APK/AAB w tym środowisku.

## 7. Kryteria akceptacji

- [ ] Projekt kompiluje się w UE 5.8 bez błędów UHT/C++.
- [ ] Każdy Blueprint ma zgodny Slot i InputTag; katalog serwera akceptuje wyłącznie prawidłowe klasy.
- [ ] Koszt Energy/Resonance i cooldown są nakładane przez skonfigurowane Gameplay Effects.
- [ ] Niewystarczająca ilość zasobu blokuje aktywację.
- [ ] Forward Sweep i Radial Pulse wykrywają cele po stronie serwera.
- [ ] Target Team poprawnie odróżnia Friendly i Hostile w prototypowym PvE.
- [ ] Maksymalna liczba celów jest respektowana.
- [ ] Efekty otrzymują prawidłowe SetByCaller magnitudes.
- [ ] Self Effect oraz Target Effect nie nakładają się wielokrotnie na jeden cel w ramach jednej aktywacji.
- [ ] Gameplay Cue i Animation Montage są przypisane i czytelne.
- [ ] Każda z dziewięciu postaci ma poprawnie skonfigurowany Primary.
- [ ] Każdy wyposażony Active/Ultimate/Dodge rozwiązuje się przez Stage 16 Ability Catalog.
- [ ] Passive nie pozostawia osieroconych efektów po zmianie postaci lub loadoutu.
- [ ] Test multiplayer potwierdza serwerową autorytatywność wykrywania i aplikowania efektów.
- [ ] Test mobilny sprawdza czytelność HUD, cue, animacji i zużycie zasobów.

## 8. Weryfikacja

Źródła i dokumentację zapisano na osobnej gałęzi etapu 17. Po zapisie należy odczytać pliki z GitHub i przeprowadzić statyczne kontrole. Kompilacja i testy runtime wymagają lokalnego Unreal Engine 5.8 oraz skonfigurowanych assetów.

## Następny etap

Stage 18 — Ability HUD & Input Feedback: mobilne przyciski pięciu slotów, cooldown radial, wskaźniki Energy/Resonance, komunikaty o braku zasobu, wybór celu i czytelne sygnały aktywacji.
