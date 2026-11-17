# JUMANJI 2.111.1 — Stage 26: World Traversal QA, Lift Recovery & Traversal Rewards

## Cel etapu

Rozwinąć Stage 25 bez tworzenia alternatywnego systemu traversal, ekonomii ani profilu.

Zakres:
- zapewnić, że aktywne przejście nie zależy od obecności aktora AJumanjiTraversalLink w aktualnie załadowanej komórce World Partition;
- zapobiegać utknięciu postaci na windzie po wyładowaniu aktora platformy;
- dodać bezpieczną, serwerową procedurę odzyskiwania windy ze stanu Blocked;
- przyznawać jednorazową nagrodę za pierwsze zakończone przejście każdego stabilnego TraversalId;
- dodać manifest nagród, walidator, testy źródeł i workflow GitHub Actions;
- rozszerzyć testy World Partition, multiplayer i Android.

Stage 26 używa istniejących AJumanjiTraversalLink, UJumanjiTraversalComponent, AJumanjiLiftPlatform, UJumanjiPlayerState oraz UJumanjiProgressionRewardSubsystem.

## 1. Odporność traversal na World Partition unload/load

### 1.1. Samodzielna ścieżka Zipline

Problem: Stage 25 pozwalał serwerowi odczytywać spline bezpośrednio z aktywnego aktora linku podczas każdego kroku ruchu. Jeżeli komórka World Partition wyładowałaby aktor w czasie przejścia, spline nie byłby już dostępny.

Rozwiązanie:
- FJumanjiTraversalReplicatedState zawiera ZiplinePathSamples;
- na serwerze, podczas TryStartTraversal, spline jest próbkowany w 17 kolejnych punktach;
- punkty są zapisywane jako FVector_NetQuantize10 w zreplikowanym stanie traversal;
- pierwszy i ostatni punkt są ustawiane na rzeczywistą pozycję startową postaci i zatwierdzony EndLocation;
- ruch serwera interpoluje pomiędzy próbkami, a nie odczytuje spline z aktora w każdej klatce;
- po ukończeniu lub przerwaniu przejścia tablica próbek jest czyszczona.

W rezultacie aktywna Zipline ma wystarczające dane ruchu w stanie postaci, nawet gdy aktor linku zostanie wyładowany. Dla starszego stanu bez próbek pozostaje interpolacja StartLocation → EndLocation.

Próbki dotyczą trajektorii ruchu postaci. Nie tworzą fizycznej liny ani siatki kabla.

### 1.2. Pozostałe typy przejść

ContextJump i Climb już korzystają ze zreplikowanego StartLocation, EndLocation, czasu, obrotów i ArcHeight. Ruch nie potrzebuje odczytu geometrii linku po rozpoczęciu.

Link może zostać później załadowany ponownie. Jego BeginPlay ponownie uruchamia overlap focus volume i może zarejestrować pobliskie przejście w lokalnym PlayerController. Stabilny TraversalId pozostaje niezmienny.

### 1.3. Wyładowanie windy z pasażerem

AJumanjiLiftPlatform::EndPlay uruchamia istniejącą procedurę DetachAllPassengers(false):
- pasażer zostaje odłączony od platformy;
- jego UJumanjiTraversalComponent otrzymuje CompleteLiftPlatformRide(false);
- traversal jest oznaczany jako przerwany;
- poprzedni movement mode postaci zostaje przywrócony;
- nie jest przyznawana nagroda za nieukończony kurs.

Nie należy próbować odtwarzać ruchu windy po wyładowaniu jej aktora. Po ponownym załadowaniu platforma rozpoczyna stan z zapisanej konfiguracji Blueprint/mapy, a nie z nieautorytatywnego snapshotu klienta.

## 2. Bezpieczne odzyskiwanie zablokowanej windy

Nowe API:

    AJumanjiLiftPlatform::TryRecoverBlockedPlatform()

To funkcja serwerowa, możliwa do wywołania z Blueprinta operatora lub innej zaufanej logiki serwera. Nie jest publicznym RPC klienta.

### 2.1. Warunki wejściowe

Procedura odmawia odzyskiwania, jeżeli:
- wywołanie nie pochodzi z autorytatywnego serwera;
- platforma nie jest w stanie Blocked;
- kotwice dolnego i górnego przystanku nie przeszły walidacji;
- lista pasażerów nie jest pusta;
- zreplikowana liczba pasażerów nie wynosi zero.

### 2.2. Walidacja pozycji

System oblicza pozycje root platformy dla obu authored stops:
- LowerStopActorLocation;
- UpperStopActorLocation.

Następnie testuje collision shape platformy przy obu pozycjach względem:
- ECC_WorldStatic;
- ECC_WorldDynamic;
- ECC_Pawn.

Jeżeli obydwa przystanki są zablokowane, platforma pozostaje w stanie Blocked. Jeżeli co najmniej jeden jest bezpieczny, system wybiera najbliższy wolny przystanek.

### 2.3. Zakończenie odzyskiwania

Po bezpiecznym przeniesieniu root platformy:
- state przechodzi do AtLower albo AtUpper;
- PassengerCount pozostaje równy zero;
- tick ruchu pozostaje wyłączony;
- revision jest zwiększany;
- stan jest replikowany;
- OnLiftStateChanged i OnLiftPresentationChanged są wywoływane.

Procedura nie przesuwa pasażerów, nie teleportuje ich i nie wznawia poprzedniego kursu. Jeżeli platforma jest nadal zasłonięta, operator musi najpierw usunąć przeszkodę.

### 2.4. Ważne ograniczenie

API nie dodaje przycisku naprawy dla klienta ani systemu automatycznego resetowania przez wszystkie shardy. Projektowy Blueprint/serwerowy operator musi wywołać tę funkcję po sprawdzeniu geometrii.

## 3. Jednorazowe nagrody za traversal

Nowy wiersz danych:

    FJumanjiTraversalRewardDefinition

Pola:
- TraversalId;
- CurrencyId;
- CurrencyAmount;
- opcjonalne ItemId;
- opcjonalne ItemAmount.

Nowe API:

    UJumanjiProgressionRewardSubsystem::GrantTraversalCompletionReward(PlayerState, TraversalId)

Jest wywoływane wyłącznie po stronie serwera, z UJumanjiTraversalComponent::FinishTraversal, po potwierdzeniu końcowej kolizji i ukończenia przejścia.

### 3.1. Domyślna tabela nagród

| TraversalId | Nagroda |
|---|---:|
| EJ_ROOT_CANOPY_JUMP_01 | 20 Monet Odkrywców |
| EJ_ROOT_TOTEM_CLIMB_01 | 25 Monet Odkrywców |
| EJ_TIDE_FALLS_ZIP_01 | 30 Monet Odkrywców |
| EJ_TIDE_CHANNEL_JUMP_01 | 20 Monet Odkrywców |
| EJ_STONE_PILLAR_CLIMB_01 | 25 Monet Odkrywców |
| EJ_STORM_PEAK_ZIP_01 | 30 Monet Odkrywców |
| EJ_EMBER_CAVERN_JUMP_01 | 20 Monet Odkrywców |
| EJ_DAWN_MIRRORS_LIFT_01 | 40 Monet Odkrywców |
| EJ_SHADOW_MARSH_ZIP_01 | 30 Monet Odkrywców |
| EJ_LIFE_GARDEN_CLIMB_01 | 25 Monet Odkrywców |
| EJ_FROST_GATE_LIFT_01 | 40 Monet Odkrywców |
| EJ_FROST_GATE_JUMP_01 | 20 Monet Odkrywców |

Wszystkie wartości są propozycjami balansu do późniejszych testów ekonomii.

### 3.2. Idempotencja

Identyfikator transakcji:

    TRAV_<ProfileHash>_<TraversalId>

Jest stabilny dla pary profil + link. Ponowne przejście tego samego linku nie daje ponownie nagrody. Inne linki mają oddzielne identyfikatory.

Nagroda:
1. jest sprawdzana w allowlistowanym katalogu TraversalRewards;
2. jest zamieniana na FJumanjiProfileTransaction;
3. przechodzi przez AJumanjiPlayerState::ApplyProfileTransaction;
4. aktualizuje saldo i trwały ProcessedTransactionIds w istniejącym profilu.

Klient nie przekazuje kwoty, waluty, przedmiotu ani identyfikatora transakcji. UJumanjiTraversalComponent wywołuje nagrodę tylko po serwerowym zakończeniu ruchu. Jeżeli profil nie ma aktywnej, trwałej tożsamości albo zapis jest zablokowany, nagroda nie zostanie zastosowana.

Profil oparty na lokalnym SaveGame nadal pozostaje prototypem single-server. Produkcyjna ekonomia MMO wymaga atomowej transakcji backendowej.

## 4. DataTable nagród traversal

W Unreal Editor utwórz:
- folder: /Game/Progression/Data;
- DataTable: DT_TraversalRewards;
- row struct: FJumanjiTraversalRewardDefinition;
- ścieżka: /Game/Progression/Data/DT_TraversalRewards.DT_TraversalRewards.

Kolumny:
- TraversalId;
- CurrencyId;
- CurrencyAmount;
- ItemId;
- ItemAmount.

Row Name powinien odpowiadać TraversalId. Wiersz DataTable o poprawnym TraversalId nadpisuje natywną wartość startową. Brak DataTable oznacza użycie 12 wbudowanych nagród.

Wbudowany allowlist walut dopuszcza w tym etapie CURRENCY_EXPLORER_COINS. Opcjonalny ItemId musi należeć do znanego katalogu materiałów. Wartości nagród muszą być dodatnie i ograniczone.

Źródłowy CSV:

    Data/Traversal/EmeraldJungle_TraversalRewards.csv

## 5. Automatyczna walidacja danych

Uruchom:

    python Scripts/validate_emerald_jungle_traversal_rewards.py
    python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_traversal_rewards.py" -v

Walidator sprawdza:
- obecność wszystkich wymaganych kolumn;
- unikalność i kompletność 12 TraversalId;
- zgodność nagród z linkami w EmeraldJungle_TraversalPlacement.csv;
- dozwolone waluty;
- dodatnie i ograniczone wartości CurrencyAmount;
- opcjonalny ItemId/ItemAmount jako poprawną parę;
- wymagane Notes.

Nowy GitHub Actions workflow:

    .github/workflows/emerald-jungle-traversal-reward-validation.yml

PASS walidatora potwierdza tylko spójność źródeł CSV. Nie kompiluje UE, nie importuje DataTable .uasset i nie potwierdza zachowania w PIE.

## 6. World Partition QA — wymagane testy edytora

### Link i Zipline

- [ ] Uruchom traversal Zipline w aktywnym World Partition cell.
- [ ] W trakcie ruchu wymuś unload komórki zawierającej AJumanjiTraversalLink.
- [ ] Potwierdź, że serwer kontynuuje ruch na podstawie 17 replikowanych próbek.
- [ ] Potwierdź brak dereferencji brakującego spline aktora.
- [ ] Sprawdź osiągnięcie końcowego anchor i przywrócenie ruchu.
- [ ] Sprawdź, że nagroda została przyznana jeden raz.
- [ ] Ponownie załaduj cell.
- [ ] Podejdź do linku i sprawdź ponowne zarejestrowanie overlap focus i prompt.
- [ ] Powtórz test dla ContextJump i Climb; ruch ma korzystać z replikowanych końców przejścia.

### Lift platform unload

- [ ] Rozpocznij kurs z co najmniej dwoma pasażerami.
- [ ] Wymuś unload cell z AJumanjiLiftPlatform na serwerze.
- [ ] Potwierdź odłączenie wszystkich pasażerów.
- [ ] Potwierdź CompleteLiftPlatformRide(false) i przywrócenie movement mode.
- [ ] Potwierdź brak nagrody za przerwany kurs.
- [ ] Załaduj cell i sprawdź, że nowa instancja platformy ma prawidłowy stan startowy.

## 7. Lift recovery QA

- [ ] Wymuś blokującą kolizję podczas kursu.
- [ ] Potwierdź stan Blocked i PassengerCount = 0.
- [ ] Wywołaj TryRecoverBlockedPlatform po usunięciu przeszkody.
- [ ] Potwierdź wybór najbliższego wolnego przystanku.
- [ ] Potwierdź zwiększenie revision i replikację nowego stanu.
- [ ] Zablokuj oba przystanki i potwierdź odmowę odzyskiwania.
- [ ] Wywołaj metodę, gdy pasażer nadal jest dołączony, i potwierdź odmowę.
- [ ] Wywołaj metodę ze strony klienta i potwierdź, że serwer nie zmienia stanu.
- [ ] Po udanym recovery wykonaj kurs w górę i w dół.

## 8. Multiplayer i ekonomia

- [ ] Dwóch graczy używa tego samego linku; obaj kończą przejście.
- [ ] Każdy profil dostaje własną nagrodę raz.
- [ ] Gracz powtarza ten sam link; drugiej nagrody nie ma.
- [ ] Gracz używa pozostałych 11 linków; każdy ma osobny receipt.
- [ ] Przerwane lub zablokowane przejście nie daje nagrody.
- [ ] Niezapisujący się profil nie otrzymuje fałszywego potwierdzenia nagrody.
- [ ] Sprawdź saldo i ProcessedTransactionIds po ponownym logowaniu.
- [ ] Potwierdź, że wartości nagród pochodzą z serwerowego katalogu, nie z żądania klienta.

## 9. Android

- [ ] Wykonaj 10-minutowy test z wieloma linkami i dwoma windami.
- [ ] Sprawdź replikację próbek Zipline i płynność ruchu.
- [ ] Zmierz pamięć stanu postaci podczas aktywnego Zipline.
- [ ] Sprawdź prompt po ponownym załadowaniu World Partition cell.
- [ ] Sprawdź przejazd windy i odzyskiwanie po blokadzie.
- [ ] Zapisz FPS, frame time, temperaturę i pamięć na fizycznym urządzeniu.

## 10. Kryteria akceptacji

- [ ] Kod kompiluje się w Unreal Engine 5.8.
- [ ] Walidator CSV przechodzi.
- [ ] Wszystkie testy Python przechodzą.
- [ ] Zipline aktywne podczas unload nie zależy od żywego link/spline aktora.
- [ ] Link po load ponownie rejestruje focus.
- [ ] Lift unload uwalnia pasażerów i kończy przejście jako Interrupted.
- [ ] Zablokowana platforma może wrócić wyłącznie do wolnego przystanku.
- [ ] Dwa zablokowane przystanki oznaczają odmowę recovery.
- [ ] Każdy z 12 linków przyznaje jedną nagrodę na profil.
- [ ] Ponowienie traversal nie przyznaje drugiej nagrody.
- [ ] Interrupted traversal nie przyznaje nagrody.
- [ ] Test dwóch klientów przechodzi.
- [ ] Fizyczny test Android jest zapisany.

## 11. Zakres nieobjęty Stage 26

- automatyczny admin UI i RPC do naprawy wind;
- globalny reset wszystkich platform w shardzie;
- backend ekonomii MMO i cross-shard transactions;
- gwarancja odtworzenia lokalnego SaveGame po awarii procesu w trakcie zapisu;
- automatyczny import DataTable .uasset;
- zapis runtime stanu wszystkich World Partition actors;
- finalne umap, modele wind, spline meshes, animacje i efekty;
- APK/AAB i rzeczywiste wyniki profilowania.

## 12. Weryfikacja

Pliki źródłowe i manifesty zostały zapisane w gałęzi funkcjonalnej i odczytane ponownie przez GitHub API. Walidator i testy Python są częścią PR. Nie wykonano kompilacji Unreal Engine 5.8, PIE multiplayer ani fizycznego testu Android w tym środowisku. Nie traktuj etapu jako build-verified, dopóki te testy nie zostaną wykonane lokalnie.

## Następny etap

**Stage 27 — Emerald Jungle World Encounter Integration**: połączenie regionów, traversal, prób Great Spirit i boss encounterów w jedną serwerowo sterowaną pętlę eksploracja → walka → nagroda → progresja.
