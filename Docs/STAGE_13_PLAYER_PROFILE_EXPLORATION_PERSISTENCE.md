# JUMANJI 2.111.1 — Stage 13: Player Profile & Exploration Persistence

## Cel etapu

Utrwalić najważniejsze dane gracza, które w Stage 11–12 były przechowywane wyłącznie w replikowanym PlayerState:
- wybranego Wielkiego Ducha;
- odkryte regiony świata;
- odwiedzone punkty orientacyjne (landmarki);
- ukończone rozdziały przygody.

Stage 13 rozwija istniejące AJumanjiPlayerState, UJumanjiWorldAtlasSubsystem i AJumanjiGameState. Nie wprowadza drugiego atlasu, drugiego systemu wyboru postaci ani drugiego źródła postępu w aktywnej sesji.

## Zakres i ograniczenia

W tej wersji dodano lokalny, wersjonowany zapis Unreal SaveGame wykonywany przez serwer gry. To rozwiązanie prototypowe dla standalone / jednego procesu serwera. Nie jest to jeszcze produkcyjna usługa profili MMORPG.

W docelowym shared-world MMO:
- serwer gry powinien pobierać i zapisywać profile przez zaufany backend;
- identyfikator konta musi pochodzić z uwierzytelnionej usługi platformowej;
- zapis musi być odporny na równoczesne sesje, zmianę shardów i awarie procesu;
- lokalny SaveGame może pełnić funkcję cache lub trybu offline, ale nie powinien być globalnym źródłem prawdy dla wielu serwerów.

## 1. Model danych profilu

### FJumanjiPlayerProfileSnapshot

Plik: Source/JumanjiUE/Public/JumanjiPlayerProfileSaveGame.h

Pola schematu v1:
- SchemaVersion — wersja struktury zapisu;
- ProfileId — stabilny identyfikator przypisany po stronie serwera;
- SelectedCharacterId — identyfikator wybranego Wielkiego Ducha;
- DiscoveredRegionIds — identyfikatory odkrytych regionów;
- VisitedLandmarkIds — identyfikatory odwiedzonych landmarków;
- ExplorationRevision — wersja snapshotu eksploracji;
- CompletedAdventureChapterIds — identyfikatory ukończonych rozdziałów;
- LastSavedUtc — czas przygotowania zapisu w UTC.

Przechowujemy identyfikatory danych, a nie bezpośrednie referencje do aktorów, map czy obiektów UAsset. Dzięki temu zapis nie zależy od tego, czy region jest aktualnie załadowany przez World Partition.

### UJumanjiPlayerProfileSaveGame

Klasa SaveGame jest kontenerem danych. Nie przechowuje logiki gry. Plik zapisu jest generowany lokalnie przez silnik; nie jest częścią repozytorium i nie powinien być commitowany do Git.

## 2. Profil i slot zapisu

Pliki:
- Source/JumanjiUE/Public/JumanjiPlayerProfileSubsystem.h
- Source/JumanjiUE/Private/JumanjiPlayerProfileSubsystem.cpp

UJumanjiPlayerProfileSubsystem jest GameInstanceSubsystemem i udostępnia:
- LoadProfile(ProfileId, OutSnapshot);
- SaveProfile(Snapshot);
- HasProfile(ProfileId).

Nazwa slotu jest deterministycznie tworzona z hasha MD5 identyfikatora profilu. Hash służy wyłącznie do utworzenia przewidywalnej nazwy slotu i ograniczenia ujawniania surowego identyfikatora w nazwie pliku. Nie jest mechanizmem uwierzytelniania, szyfrowania ani ochrony przed manipulacją plikiem.

Limity walidacji schematu:
- maksymalnie 32 regiony;
- maksymalnie 128 landmarków;
- maksymalnie 64 ukończone rozdziały;
- identyfikatory nie mogą być puste ani powtórzone;
- identyfikator profilu musi mieć od 1 do 256 znaków;
- wybrany identyfikator postaci nie może być pusty;
- wersja zapisu musi być dokładnie obsługiwana przez aktualny kod.

Wczytywanie jest wykonywane przy wejściu gracza. Po wczytaniu PlayerState dodatkowo filtruje identyfikatory przez aktualny atlas oraz katalog grywalnych Wielkich Duchów. Nieznane lub usunięte z katalogu regiony i postacie nie są bezwarunkowo przywracane.

## 3. Asynchroniczny zapis

SaveProfile przekazuje snapshot do UGameplayStatics::AsyncSaveGameToSlot.

Subsystem:
- utrzymuje referencję do obiektu SaveGame do zakończenia zapisu;
- dopuszcza tylko jeden zapis danego slotu naraz;
- gdy nadejdzie kolejna aktualizacja podczas zapisu, zachowuje najnowszy kompletny snapshot w kolejce;
- po powodzeniu poprzedniego zapisu uruchamia zapis najnowszego snapshotu;
- zapisuje błąd do logu, jeżeli operacja się nie powiedzie.

Wartość true zwracana przez SaveProfile oznacza, że zapis został przyjęty lub zakolejkowany. Nie oznacza, że platforma zakończyła już operację I/O. Ostateczny wynik asynchronicznego zapisu jest raportowany w callbacku i logu.

Asynchroniczny zapis ogranicza ryzyko krótkiego zatrzymania klatki podczas eksploracji i gry mobilnej. Dokumentacja Epic zaleca AsyncSaveGameToSlot dla zapisów wykonywanych podczas aktywnej rozgrywki. <https://dev.epicgames.com/documentation/unreal-engine/saving-and-loading-your-game-in-unreal-engine?lang=en-US>

## 4. Źródło identyfikatora profilu

Profil nie jest identyfikowany przez DisplayName ani nazwę postaci.

AJumanjiGameMode przypisuje identyfikator w PostLogin po stronie serwera:
1. jeżeli platformowy UniqueNetId jest dostępny, jego wartość jest używana jako klucz profilu;
2. w trybie standalone używany jest stały lokalny profil LOCAL_SINGLEPLAYER;
3. host lokalny listen server może użyć LOCAL_LISTEN_HOST;
4. zdalny klient bez stabilnego platformowego identyfikatora nie otrzymuje lokalnego profilu zapisu.

PostLogin jest właściwym miejscem na logikę po poprawnym zalogowaniu. <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AGameModeBase/PostLogin>

SetPersistentProfileId jest metodą serwerową i odrzuca puste identyfikatory oraz próbę podmiany wcześniej przypisanego klucza.

To nie zastępuje logowania kont. W środowisku produkcyjnym platformowy identyfikator musi zostać zweryfikowany przez usługę uwierzytelniania, a backend powinien zwrócić wewnętrzny PlayerId.

## 5. Przywracanie profilu

Przebieg PostLogin:
1. GameMode otrzymuje nowego PlayerControllera.
2. Pobiera AJumanjiPlayerState.
3. Ustala stabilny identyfikator profilu po stronie serwera.
4. Wczytuje zapisany snapshot.
5. Weryfikuje wersję i strukturę snapshotu.
6. PlayerState sprawdza wybraną postać w UJumanjiCharacterRosterSubsystem.
7. PlayerState filtruje regiony i landmarki przez UJumanjiWorldAtlasSubsystem.
8. Odwiedzony landmark automatycznie oznacza także jego region jako odkryty.
9. Przywrócony stan jest replikowany do klienta.

Jeśli zapis jest nieobecny, uszkodzony, nieobsługiwany lub nie przechodzi walidacji, sesja rozpoczyna się z bezpiecznymi wartościami domyślnymi. Nie kasujemy automatycznie uszkodzonego slotu.

## 6. Zapisywanie zmian

Profil jest aktualizowany po następujących zdarzeniach serwerowych:
- zatwierdzenie innego Wielkiego Ducha;
- odkrycie nowego regionu;
- odwiedzenie nowego landmarku;
- ukończenie rozdziału przygody.

AJumanjiPlayerState pozostaje źródłem prawdy w aktywnej sesji. Po zmianie jego snapshot jest wysyłany do UJumanjiPlayerProfileSubsystem.

Ukończenie rozdziału jest stanem współdzielonym. AJumanjiGameState zapisuje identyfikator ukończonego rozdziału w profilach graczy obecnych w chwili ukończenia rozdziału. Gracze, którzy dołączą później, nie otrzymują automatycznie nagrody za wydarzenie, w którym nie uczestniczyli.

## 7. Co jest zapisywane, a co nie

### Zapisywane w schema v1
- wybrany Great Spirit;
- odkryte regiony;
- odwiedzone landmarki;
- revision eksploracji;
- ukończone identyfikatory rozdziałów.

### Poza zakresem schema v1
- waluta i ekwipunek;
- poziomy i statystyki postaci;
- wyposażenie i sloty umiejętności;
- niedokończony etap zadania;
- bieżący postęp poszczególnych celów rozdziału;
- aktywne wydarzenie Żywej Planszy;
- pozycja postaci;
- ustawienia graficzne i dotykowe;
- dane gildii, handlu i matchmakingu.

Te dane wymagają kolejnych etapów i osobnych zasad walidacji. Nie należy dopisywać ich do profilu bez aktualizacji numeru schematu i migracji danych.

## 8. Migracje

Aktualna wersja obsługuje wyłącznie schemaVersion = 1. Jeżeli struktura ulegnie zmianie:
1. zwiększ numer wersji;
2. dodaj jawny krok migracji ze starego schematu;
3. zachowaj bezpieczne wartości domyślne;
4. przetestuj zapis z poprzedniej wersji gry;
5. nie usuwaj starych pól, zanim migracja nie zostanie sprawdzona.

Nie traktuj automatycznego odrzucenia starszego zapisu jako docelowej polityki aktualizacji.

## 9. Testy i kryteria akceptacji

### Test standalone
- [ ] Uruchom grę w trybie Standalone.
- [ ] Odkryj jeden region.
- [ ] Odwiedź jeden landmark.
- [ ] Wybierz innego Wielkiego Ducha.
- [ ] Ukończ rozdział.
- [ ] Zamknij grę w kontrolowany sposób i uruchom ponownie.
- [ ] Sprawdź, czy wybrana postać, region, landmark i ukończony rozdział wróciły.

### Test multiplayer
- [ ] Uruchom serwer i dwóch klientów z różnymi platformowymi identyfikatorami testowymi.
- [ ] Sprawdź, czy profile nie nadpisują się wzajemnie.
- [ ] Zmień postać na jednym kliencie i potwierdź, że drugi profil nie jest modyfikowany.
- [ ] Odkryj regiony na dwóch profilach.
- [ ] Wywołaj równoczesne odkrycia w tym samym slocie testowym i sprawdź, czy kolejka zapisuje najnowszy snapshot.
- [ ] Odłącz klienta i sprawdź, czy GameMode zleca zapis przy Logout.
- [ ] Sprawdź zachowanie gracza bez stabilnego UniqueNetId.

### Test walidacji
- [ ] Nieznany RegionId zostaje odrzucony przy przywracaniu.
- [ ] Nieznany GreatSpiritId zostaje zastąpiony bezpieczną postacią domyślną.
- [ ] Puste i powtórzone ID nie przechodzą walidacji.
- [ ] Nieobsługiwana wersja schematu nie jest przywracana.
- [ ] Profile z różnymi identyfikatorami używają osobnych slotów.

### Test wydajności
- [ ] Zmierzyć czas klatki przy odkrywaniu regionu i landmarku.
- [ ] Sprawdzić callback asynchronicznego zapisu.
- [ ] Sprawdzić zachowanie przy błędzie zapisu.
- [ ] Sprawdzić utratę procesu podczas zapisu i opisać granicę utraty ostatniej aktualizacji.

## 10. Konfiguracja i wymagane zasoby edytora

Nie są wymagane nowe DataTables ani Widget Blueprinty do samego zapisu. System korzysta z istniejących:
- UJumanjiCharacterRosterSubsystem;
- UJumanjiWorldAtlasSubsystem;
- AJumanjiPlayerState;
- AJumanjiGameMode;
- AJumanjiGameState.

Zapis SaveGame jest generowany przez silnik na platformowym obszarze zapisu użytkownika. Nie dodawaj plików .sav do repozytorium.

## 11. Ograniczenia produkcyjne

Ten etap nie implementuje:
- usługi backendowej profilu;
- uwierzytelniania kont;
- synchronizacji między shardami i serwerami;
- konflikt resolution dla równoczesnych logowań na wielu urządzeniach;
- szyfrowania ani podpisywania lokalnego SaveGame;
- pełnych migracji schematu;
- gwarantowanego zapisu po awarii zasilania lub wymuszonym zakończeniu procesu.

Lokalny slot SaveGame nie może być traktowany jako zabezpieczony magazyn konta. W wersji MMO należy zastąpić go serwerowym repozytorium profili i kontrolować każdą zmianę po stronie backendu.

## 12. Pliki etapu

Dodane:
- Source/JumanjiUE/Public/JumanjiPlayerProfileSaveGame.h
- Source/JumanjiUE/Private/JumanjiPlayerProfileSaveGame.cpp
- Source/JumanjiUE/Public/JumanjiPlayerProfileSubsystem.h
- Source/JumanjiUE/Private/JumanjiPlayerProfileSubsystem.cpp
- Docs/STAGE_13_PLAYER_PROFILE_EXPLORATION_PERSISTENCE.md

Zmienione:
- Source/JumanjiUE/Public/JumanjiPlayerState.h
- Source/JumanjiUE/Private/JumanjiPlayerState.cpp
- Source/JumanjiUE/Public/JumanjiGameMode.h
- Source/JumanjiUE/Private/JumanjiGameMode.cpp
- Source/JumanjiUE/Private/JumanjiGameState.cpp
- README.md
- Content/World/README.md

## Następny etap

**Stage 14 — Profile Progression & Inventory Persistence**: waluta, ekwipunek, poziom postaci, loadout Wielkiego Ducha, materiały ulepszeń, nagrody i transakcyjne operacje profilu. Wersja serwerowa powinna docelowo korzystać z backendu, a nie lokalnego pliku SaveGame.
