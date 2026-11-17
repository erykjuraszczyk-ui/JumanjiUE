# JUMANJI 2.111.1 — Stage 25: Traversal World Integration & Multiplayer Validation

## Cel etapu

Połączyć runtime przejść z poprzednich etapów z przepływem interakcji w Emerald Jungle:
- wykrywać pobliskie linki przez overlap volumes, bez skanowania świata co klatkę;
- automatycznie wybierać najbliższe dostępne przejście dla lokalnego gracza;
- przekazywać wybór do istniejącego UJumanjiTraversalPromptWidget;
- obsługiwać serwerowo i replikować ruch fizycznej platformy windy;
- umożliwić kilku pasażerom wspólny kurs;
- przygotować manifest rozmieszczenia 12 linków i dwóch wind;
- dodać walidację CSV/JSON oraz testy automatyczne.

Stage 25 rozwija istniejące AJumanjiTraversalLink, UJumanjiTraversalComponent, AJumanjiPlayerController i UJumanjiTraversalPromptWidget. Nie wprowadza drugiego systemu interakcji ani osobnego RPC do zwykłych przejść.

## 1. Pliki etapu

Dodane:
- Source/JumanjiUE/Public/JumanjiLiftPlatform.h
- Source/JumanjiUE/Private/JumanjiLiftPlatform.cpp
- Data/Traversal/EmeraldJungle_TraversalPlacement.csv
- Data/Traversal/EmeraldJungle_LiftPlatformManifest.csv
- Data/Traversal/EmeraldJungle_TraversalManifest.json
- Scripts/validate_emerald_jungle_traversal.py
- Scripts/Tests/test_validate_emerald_jungle_traversal.py
- .github/workflows/emerald-jungle-traversal-validation.yml
- Docs/STAGE_25_TRAVERSAL_WORLD_INTEGRATION_MULTIPLAYER.md

Zmienione:
- Source/JumanjiUE/Public/JumanjiTraversalLink.h
- Source/JumanjiUE/Private/JumanjiTraversalLink.cpp
- Source/JumanjiUE/Public/JumanjiTraversalComponent.h
- Source/JumanjiUE/Private/JumanjiTraversalComponent.cpp
- Source/JumanjiUE/Public/JumanjiPlayerController.h
- Source/JumanjiUE/Private/JumanjiPlayerController.cpp
- Content/Traversal/README.md
- README.md

## 2. Automatyczny focus interakcji

### AJumanjiTraversalLink::FocusVolume

Każdy link ma teraz query-only USphereComponent:
- overlapuje wyłącznie obiekty Pawn;
- nie blokuje ruchu ani zapytań fizycznych;
- rejestruje i wyrejestrowuje pobliski link w lokalnym AJumanjiPlayerController;
- ma regulowany promień focusu;
- jest wyłączany razem z linkiem, gdy serwer ustawi bTraversalEnabled=false.

Serwer pozostaje autorytetem dla faktycznego użycia linku. Overlap lokalnego klienta jedynie informuje UI o pobliskiej możliwości.

### Rejestr kandydatów kontrolera

AJumanjiPlayerController przechowuje NearbyTraversalCandidates i:
1. usuwa nieaktualne lub zniszczone referencje;
2. sprawdza CanBeUsedBy dla aktualnej postaci;
3. oblicza odległość do najbliższej kotwicy;
4. wybiera najbliższy poprawny link;
5. przekazuje go do istniejącego UJumanjiTraversalPromptWidget.

Przy przejściu trwającym kontroler zachowuje aktywny link w widżecie, nawet gdy overlap volume zostanie opuszczony. Dzięki temu Completed/Interrupted z komponentu może dojść do UI.

Nie jest wykonywany pełny TActorIterator ani skan całej mapy co klatkę.

## 3. AJumanjiLiftPlatform — rzeczywista platforma windy

Pliki:
- Source/JumanjiUE/Public/JumanjiLiftPlatform.h
- Source/JumanjiUE/Private/JumanjiLiftPlatform.cpp

Actor:
- replikuje transform przez bReplicates i SetReplicateMovement(true);
- ma fizyczny root UBoxComponent używany do swept movement;
- ma mesh platformy, query-only BoardingVolume i dwa anchor points;
- wykonuje ruch wyłącznie po stronie serwera;
- replikuje stan AtLower / MovingUp / AtUpper / MovingDown / Blocked;
- dołącza pasażerów do root platformy przy zachowaniu transformacji świata;
- zwalnia pasażerów po dotarciu lub przerwaniu ruchu;
- obsługuje maksymalnie 4 pasażerów domyślnie;
- pozwala dołączyć do rozpoczętego kursu w krótkim oknie boardingowym;
- przerywa kurs po blokującej kolizji root componentu.

### Przepływ pasażera

1. Lokalny overlap wykrywa AJumanjiTraversalLink typu Lift.
2. Gracz naciska prompt.
3. Istniejący PlayerController RequestTraversalLink wysyła ten sam Server RPC co pozostałe typy przejść.
4. Serwer sprawdza link, kotwice, stan postaci i komponent traversal.
5. UJumanjiTraversalComponent odczytuje przypisaną AJumanjiLiftPlatform.
6. Serwer sprawdza kierunek, limit pasażerów i BoardingVolume.
7. Postać zostaje dołączona do PlatformCollision, a movement mode wyłączony na czas kursu.
8. Platforma wykonuje ruch ze swept collision i replikuje transform.
9. Po kursie platforma odłącza pasażerów.
10. Każdy UJumanjiTraversalComponent kończy stan Lift, ustawia końcową kotwicę i przywraca poprzedni movement mode.
11. UJumanjiTraversalPromptWidget pokazuje server-confirmed Completed albo Interrupted.

W trybie multiplayer kilka postaci może być dołączonych do tego samego kursu. Nie oznacza to jeszcze systemu wind wielokondygnacyjnych ani transportu między shardami.

### Stany platformy

| Stan | Znaczenie |
|---|---|
| AtLower | Platforma stoi na dolnym przystanku |
| MovingUp | Serwer prowadzi platformę do górnego przystanku |
| AtUpper | Platforma stoi na górnym przystanku |
| MovingDown | Serwer prowadzi platformę do dolnego przystanku |
| Blocked | Swept movement wykrył blokującą kolizję; kurs przerwany |

Po stanie Blocked platforma nie przyjmuje kolejnych pasażerów. Wymaga sprawdzenia geometrii i resetu/ponownego uruchomienia instancji przez operatora lub osobny system resetowania.

### Authoring windy w Unreal Editor

1. Skompiluj kod w UE 5.8.
2. Utwórz Blueprint child AJumanjiLiftPlatform o nazwie BP_JumanjiLiftPlatform.
3. Przypisz Static Mesh platformy do PlatformMesh.
4. Dostosuj PlatformCollision do rzeczywistego wymiaru platformy.
5. Dostosuj BoardingVolume tak, aby obejmował bezpieczny obszar wejścia.
6. Ustaw LowerStopAnchor i UpperStopAnchor w lokalnej przestrzeni aktora.
7. Umieść actor w pozycji dolnego przystanku; root powinien odpowiadać dolnemu położeniu platformy.
8. Ustaw TravelDurationSeconds i MaxPassengers.
9. Zapisz actor w odpowiedniej Editor Data Layer.
10. Przypisz go do AJumanjiTraversalLink typu Lift przez pole LiftPlatform.
11. W linku ustaw EntryAnchor i ExitAnchor w tych samych współrzędnych świata, co LowerStopAnchor i UpperStopAnchor.
12. Sprawdź ruch w dwóch kierunkach, overlap, swept collision i dwóch klientów.

## 4. Manifest rozmieszczenia Emerald Jungle

### Źródła

- Data/Traversal/EmeraldJungle_TraversalPlacement.csv — 12 linków.
- Data/Traversal/EmeraldJungle_LiftPlatformManifest.csv — 2 platformy.
- Data/Traversal/EmeraldJungle_TraversalManifest.json — oczekiwana liczba aktorów, typy i reguły authoringu.
- Data/Traversal/TraversalPresentationManifest.csv — teksty i prezentacja z Stage 24.

Starterowa dystrybucja:

| Typ | Liczba |
|---|---:|
| ContextJump | 4 |
| Climb | 3 |
| Zipline | 3 |
| Lift | 2 |
| **Razem** | **12** |

Dwie windy:
- EJ_DAWN_MIRRORS_LIFT_PLATFORM_01 — Galeria Świtu;
- EJ_FROST_GATE_LIFT_PLATFORM_01 — Lodowa Brama.

Wszystkie identyfikatory są stabilne i powinny pozostać niezmienne po rozmieszczeniu aktorów w mapie.

### Ważne ograniczenie współrzędnych

CSV używa szacunkowych współrzędnych greybox w centymetrach. Nie są to pozycje sprawdzone na docelowym landscape, nie są rzutowane na NavMesh i nie dowodzą bezpiecznego przejścia. Zanim mapa zostanie zatwierdzona:
- dopasuj pozycje do realnego terenu;
- ustaw kotwice w przestrzeni kapsuły postaci;
- sprawdź wolną przestrzeń;
- ustaw linki i windy we właściwych Editor Data Layers;
- przebuduj World Partition navigation data.

## 5. Walidacja źródeł i testy

Uruchom z katalogu głównego repozytorium:

    python Scripts/validate_emerald_jungle_traversal.py
    python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_traversal.py" -v

Walidator sprawdza:
- 12 unikalnych TraversalId;
- dokładny rozkład 4/3/3/2 dla czterech typów;
- zgodność RegionId i EditorDataLayer;
- poprawność klas linków;
- skończone współrzędne, długości, czasy i ArcHeight;
- obecność dwóch unikalnych platform;
- zgodność LiftPlatformId;
- zgodność entry/exit z lower/upper stop;
- zgodność czasu linku i platformy;
- zakres MaxPassengers i BoardingWindowAlpha;
- oznaczenie współrzędnych jako placeholderów.

GitHub Actions:
.github/workflows/emerald-jungle-traversal-validation.yml

PASS oznacza zgodność CSV/JSON. Nie jest wynikiem kompilacji Unreal Engine, walidacji kolizji w mapie ani testu multiplayer PIE.

## 6. Testy w Unreal Editor

### Test jednego gracza
- [ ] Utwórz BP_TraversalLink_Base i cztery typy potomne.
- [ ] Przypisz promień FocusVolume oraz anchor points.
- [ ] Ustaw aktywny WBP_TraversalPrompt w PlayerController Blueprint.
- [ ] Podejdź do linku i sprawdź automatyczne pokazanie promptu.
- [ ] Odejdź i sprawdź ukrycie promptu.
- [ ] Wybierz link, w którym prompt jest najbliższy, gdy overlapuje kilka.
- [ ] Wykonaj ContextJump, Climb i Zipline.
- [ ] Przerwij ruch przez blokującą kolizję.
- [ ] Zakończ przejście i sprawdź przywrócenie ruchu postaci.

### Test windy — dwa lub więcej klientów
- [ ] Uruchom serwer/listen server i co najmniej dwóch klientów.
- [ ] Przypisz ten sam replikowany AJumanjiLiftPlatform do właściwego linku.
- [ ] Oba klienty niech podejdą do dolnego przystanku.
- [ ] W pierwszym kliencie wywołaj prompt i potwierdź żądanie.
- [ ] W drugim kliencie dołącz w czasie BoardingWindowAlpha.
- [ ] Sprawdź, że platforma porusza się tylko na serwerze i jej transform jest widoczny u obu klientów.
- [ ] Sprawdź, że obie postacie jadą razem, bez niezależnej interpolacji.
- [ ] Sprawdź replikowane stany MovingUp i AtUpper.
- [ ] Z górnego przystanku wykonaj przejazd w dół.
- [ ] Wymuś kolizję platformy i sprawdź stan Blocked oraz przywrócenie kontroli pasażerom.
- [ ] Sprawdź, że trzeci gracz nie może przekroczyć MaxPassengers.
- [ ] Sprawdź, że gracz poza BoardingVolume lub z nieprawidłowego kierunku zostaje odrzucony.

### World Partition
- [ ] Umieść linki i windy w odpowiedniej Editor Data Layer.
- [ ] Sprawdź załadowanie obu końców linku w odpowiednich World Partition cells.
- [ ] Sprawdź, że po unloadzie cell nie zostają stale referencje focus.
- [ ] Zbuduj Navigation Data Chunks i sprawdź Smart Links.
- [ ] Weryfikuj kierunki linków jednokierunkowych i dwukierunkowych.

### Android
- [ ] Sprawdź wielkość promptu i przycisków na fizycznym urządzeniu.
- [ ] Sprawdź, czy overlap nie powoduje flicker przy granicy promienia.
- [ ] Zmierz częstotliwość i koszt overlap events.
- [ ] Sprawdź, czy platform movement replication nie powoduje widocznego szarpania pasażerów.
- [ ] Zapisz Device Profile, renderer, FPS, frame time, pamięć i temperaturę.

## 7. Pliki Unreal Editor wymagane poza repozytorium tekstowym

Ta gałąź dostarcza kod i manifesty, ale nie tworzy binarnych zasobów:
- Content/Maps/L_EmeraldJungle_World.umap;
- BP_JumanjiLiftPlatform;
- BP_TraversalLink_Base oraz dzieci czterech typów;
- przypisane Static Meshes i collision assets;
- DataTable DT_TraversalPresentation;
- WBP_TraversalPrompt;
- przypisane montages, dźwięki, ikony i Niagara;
- zbudowane World Partition Navigation Data;
- rozmieszczone i zapisane instancje aktorów.

Utworzone assety należy dodać do repozytorium przez Git LFS i zachować pochodzenie/licencje.

## 8. Kryteria akceptacji

- [ ] Walidator danych przechodzi.
- [ ] Testy Python przechodzą.
- [ ] Projekt kompiluje się w UE 5.8.
- [ ] Overlap linku rejestruje wyłącznie lokalnego gracza.
- [ ] Kontroler wybiera najbliższy dostępny link.
- [ ] Prompt nie znika podczas aktywnej traversal.
- [ ] Serwer odrzuca nieprawidłowe żądania.
- [ ] Platforma porusza się serwerowo i replikuje transform.
- [ ] Dwóch pasażerów może jechać razem.
- [ ] Limit pasażerów jest respektowany.
- [ ] Kolizja blokująca przerywa kurs.
- [ ] Każdy pasażer odzyskuje ruch po ukończeniu i przerwaniu.
- [ ] Test dwóch klientów przechodzi.
- [ ] Test fizycznego Androida zostaje zapisany.

## 9. Ograniczenia

Etap nie dostarcza:
- mapy .umap;
- finalnych modeli wind, mostów i tyrolek;
- gotowych assetów animacji i audio;
- pełnego systemu wielokondygnacyjnych wind lub automatycznego call-button;
- transportu pomiędzy serwerami/shardami;
- dedicated server deployment;
- APK/AAB ani pomiarów urządzenia.

Kod nie został skompilowany w Unreal Engine 5.8 w tej zdalnej aktualizacji GitHub. Przed scaleniem wykonaj build, PIE multiplayer i test na fizycznym urządzeniu.

## Następny etap

**Stage 26 — World Traversal QA, Lift Recovery & Traversal Rewards**: testy automatyczne zachowania linków przy unload/load World Partition, bezpieczny reset zablokowanych platform, śledzenie przebytych tras i powiązanie odkrywania skrótów z nagrodami eksploracyjnymi.
