# JUMANJI 2.111.1 — Stage 12: Emerald Jungle Traversal & Navigation

## Cel etapu

Połączyć atlas świata z rzeczywistym przemieszczaniem się po Szmaragdowej Dżungli. Gracz ma móc wybrać odkryty region albo punkt POI na mapie, zobaczyć trasę obliczoną na podstawie NavMesh i otrzymywać aktualizowane wskazówki na urządzeniu mobilnym.

Stage 12 rozwija Stage 11. Nie tworzy drugiej mapy ani drugiego systemu odkrywania.

## Zakres

1. Prowadzenie do odkrytych regionów i landmarków.
2. Nawigacja na podstawie NavMesh, nie prostej linii przecinającej przeszkody.
3. Połączenia nawigacyjne między kładkami, półkami, mostami i platformami.
4. Kontrakt dla przejść specjalnych: wspinaczka, lina, winda i skok kontekstowy.
5. Wskazówki mobilne: kompas, odległość, linia trasy, stan braku ścieżki i przycisk anulowania.
6. Konfiguracja World Partition Navigation dla mapy L_EmeraldJungle_World.

Nie jest to jeszcze pełny system parkour ani automatyczny system poruszania postaci. Komponent wyznacza lokalną trasę prezentacyjną; ruch postaci pozostaje sterowany przez gracza.

## 1. Nowy komponent C++

### UJumanjiWorldNavigationComponent

Pliki:
- Source/JumanjiUE/Public/JumanjiWorldNavigationComponent.h
- Source/JumanjiUE/Private/JumanjiWorldNavigationComponent.cpp

Komponent jest tworzony na AJumanjiPlayerController. Jego stan jest lokalny dla kontrolera i nie jest replikowany. Dzięki temu wybór pinezki przez jednego gracza nie zmienia nawigacji pozostałych.

Publiczne API:
- SetRegionTarget(RegionId)
- SetLandmarkTarget(LandmarkId)
- ClearNavigationTarget()
- RefreshNavigationRoute()
- HasNavigationTarget()
- GetActiveRoute()
- OnNavigationRouteChanged

Dane trasy FJumanjiNavigationRoute:
- typ i identyfikator celu;
- nazwa celu;
- lokalizacja celu;
- punkty ścieżki;
- długość ścieżki w metrach;
- odległość bezpośrednia w metrach;
- informacja o ścieżce częściowej;
- Revision do odświeżania UI.

Ścieżka jest liczona po stronie lokalnego klienta przez NavigationSystem. Serwer nadal odpowiada za odkrywanie regionów i punktów POI. Sam wybór celu nawigacji nie daje klientowi prawa do zmiany stanu świata.

### Walidacja celu

- Region musi istnieć w atlasie i być odkryty przez danego gracza.
- Landmark musi istnieć w atlasie, a jego region musi być odkryty.
- W przypadku braku kotwicy w DataTable komponent może odczytać transform załadowanego aktora.
- Jeżeli aktor nie jest załadowany przez World Partition, DataTable musi zawierać jego lokalizację świata.
- Gdy NavMesh nie obejmuje celu lub jego kafel nie jest załadowany, komponent zachowuje wybrany cel i ponawia próbę. UI powinno wtedy pokazywać stan „Trasa niedostępna / ładowanie nawigacji”, a nie rysować fałszywą linię.

## 2. Zmiany atlasu

W FJumanjiWorldRegionDefinition dodano:
- bHasNavigationAnchor;
- NavigationAnchor — pozycja świata w centymetrach UE.

W FJumanjiWorldLandmarkDefinition dodano:
- bHasWorldLocation;
- WorldLocation — pozycja świata w centymetrach UE.

Wartości należy uzupełnić dopiero po ustawieniu landscape i actorów w mapie. Nie wpisuj wartości przykładowych ani współrzędnych ekranu do pól świata. MapCoordinate pozostaje znormalizowaną pozycją pinezki na mapie 2D.

Przy zmianie struktur DataTable sprawdź row struct i ponownie zapisz/reimportuj tabele w Unreal Editor.

## 3. Zmiany w AJumanjiWorldRegionVolume

Dodano BlueprintPure getter GetRegionId(), aby komponent nawigacji mógł znaleźć załadowany wolumen, gdy DataTable nie ma jeszcze NavigationAnchor.

Wolumen regionu służy do wykrywania odkrycia, a nie do blokowania gracza. Jego collision ma generować overlap z Pawn.

## 4. Konfiguracja mapy w Unreal Editor

Utwórz/otwórz:
Content/Maps/L_EmeraldJungle_World.umap

### 4.1. NavMeshBoundsVolume

1. Dodaj NavMeshBoundsVolume obejmujący grywalne ścieżki pierwszego regionu.
2. Nie obejmuj od razu dekoracyjnego obszaru mapy, do którego gracz nie może wejść.
3. Włącz widok nawigacji klawiszem P.
4. Sprawdź, czy NavMesh pokrywa ścieżki, schody, mosty i wejścia do prób.
5. Ustaw Agent Radius i Agent Height zgodnie z kapsułą AJumanjiCharacter, nie na oko.

### 4.2. World Partition

Dla głównie statycznej geometrii zacznij od Navigation Generation = Static i zbuduj dane nawigacyjne dla mapy World Partition. Sprawdź osobno ładowanie Navigation Data Chunks podczas przemieszczania się gracza.

Jeśli projekt wymaga generowania navmesh w pobliżu gracza, przetestuj Navigation Invokers i tryb Dynamic na małym fragmencie. Nie włączaj ich globalnie bez pomiaru czasu generowania, pamięci i kosztu CPU na urządzeniu docelowym.

Rekomendowana kolejność:
- najpierw statyczna nawigacja dla stałych ścieżek;
- następnie Nav Modifier Volumes dla obszarów trudnych lub niedostępnych;
- następnie Nav Link Proxy / Smart Links dla połączeń specjalnych;
- dopiero na końcu dynamiczne generowanie w rejonach, gdzie jest rzeczywiście potrzebne.

### 4.3. Kładki i przejścia specjalne

Dla przejść między oddzielnymi obszarami NavMesh użyj Nav Link Proxy lub odpowiedniego Smart Link. Ustaw punkty startu i końca na powierzchniach, po których może poruszać się agent.

Link nawigacyjny nie animuje automatycznie gracza. W przypadku postaci gracza przejście musi być osobną interakcją kontekstową:
- BP_Traversal_JumpLink;
- BP_Traversal_ClimbLink;
- BP_Traversal_Zipline;
- BP_Traversal_Lift.

Każdy typ ma mieć kontrolę zasięgu, warunku aktywacji, animacji, kolizji i bezpiecznego punktu zakończenia. W tym etapie tworzymy kontrakt projektowy i połączenia NavMesh; finalne animacje i modele przejść są osobnymi zasobami.

## 5. Projekt mobilnego prowadzenia

WBP_WorldAtlas powinien zawierać:
- panel mapy z odkrytymi regionami;
- pinezki landmarków;
- stan odkryty / nieodkryty;
- przycisk „Prowadź mnie”;
- przycisk „Anuluj trasę”;
- nazwę celu;
- dystans do celu;
- opcjonalną linię ścieżki;
- komunikat o częściowej lub niedostępnej ścieżce.

Po kliknięciu regionu:
1. Wywołaj GetWorldNavigationComponent na lokalnym AJumanjiPlayerController.
2. Wywołaj SetRegionTarget(RegionId).
3. Zasubskrybuj OnNavigationRouteChanged.
4. Rysuj PathPoints w widoku mapy.
5. Odśwież dystans i stan trasy po zmianie Revision.

Po kliknięciu landmarku analogicznie wywołaj SetLandmarkTarget(LandmarkId). Zamknięcie atlasu nie powinno automatycznie kasować aktywnego celu. Anulowanie następuje tylko przez ClearNavigationTarget.

### 5.1. Kompas i HUD

WBP_AdventureHUD może pokazywać:
- ikonę celu;
- odległość w metrach;
- strzałkę kierunku;
- stan ścieżki.

Strzałka kierunku powinna być wyliczana względem yaw kamery/kontrolera, a nie względem absolutnej osi ekranu. Nie używaj dystansu w linii prostej jako długości trasy. Obie wartości są rozróżnione w FJumanjiNavigationRoute.

### 5.2. Sterowanie dotykowe

Minimalny zestaw:
- lewa strefa: ruch;
- prawa strefa: obrót kamery;
- osobne przyciski: skok, unik, interakcja;
- mapa: otwarcie i wybór celu;
- czytelne przyciski o dużych polach dotykowych;
- możliwość ukrycia elementów HUD, które zasłaniają widok.

Nie umieszczaj krytycznych akcji wyłącznie przy krawędziach ekranu. Uwzględnij różne proporcje ekranów Androida i obszary systemowe.

## 6. Połączenie z PlayerController

AJumanjiPlayerController tworzy komponent:
WorldNavigationComponent

Blueprint getter:
GetWorldNavigationComponent()

W Blueprintach nie twórz drugiego komponentu na Widget. Pobieraj komponent z lokalnego PlayerController. Jeśli klasa kontrolera w BP dziedziczy po AJumanjiPlayerController, komponent będzie dostępny automatycznie.

## 7. Optymalizacja

- Aktualizuj ścieżkę z ograniczoną częstotliwością; domyślnie co 1 sekundę, nie co klatkę.
- Nie wykonuj zapytań nawigacyjnych dla innych graczy na kliencie.
- Nie replikuj PathPoints.
- Zawężaj NavMesh do rzeczywiście dostępnych ścieżek.
- Stosuj proste collision dla geometrii nawigacyjnej.
- Nie używaj NavMesh jako systemu ruchu po linie, wodzie czy powietrzu. Dla takich tras twórz osobne punkty orientacyjne i interakcje.
- Testuj wydajność w docelowym profilu Androida, nie tylko w edytorze PC.

## 8. Kryteria akceptacji

- [ ] Projekt kompiluje się w UE 5.8.
- [ ] PlayerController posiada dokładnie jeden WorldNavigationComponent.
- [ ] Nieodkryty region nie może zostać wybrany jako cel.
- [ ] Landmark z regionu nieodkrytego nie może zostać wybrany.
- [ ] Wybranie odkrytego regionu ustawia cel nawigacji.
- [ ] Wybranie landmarku ustawia cel nawigacji.
- [ ] Komponent potrafi użyć lokalizacji z DataTable.
- [ ] Komponent potrafi użyć transformu załadowanego aktora jako fallbacku.
- [ ] Poprawna ścieżka zwraca przynajmniej dwa punkty.
- [ ] Brak NavMesh nie generuje fałszywej linii trasy.
- [ ] Częściowa ścieżka jest oznaczana jako częściowa.
- [ ] Trasa odświeża się z ustawionym interwałem.
- [ ] ClearNavigationTarget usuwa cel i wyłącza odświeżanie.
- [ ] WBP_WorldAtlas reaguje na OnNavigationRouteChanged.
- [ ] Dwie instancje klienta mogą wybrać różne cele bez wpływu na siebie.
- [ ] World Partition ładuje właściwe kafle nawigacyjne przy przemieszczaniu się.
- [ ] Przejścia Nav Link mają testowany punkt startu i końca.
- [ ] Test dotykowy nie wymaga klawiatury ani myszy.

## 9. Pliki etapu

Dodane:
- Source/JumanjiUE/Public/JumanjiWorldNavigationComponent.h
- Source/JumanjiUE/Private/JumanjiWorldNavigationComponent.cpp
- Docs/STAGE_12_EMERALD_JUNGLE_TRAVERSAL_NAVIGATION.md

Zmienione:
- Source/JumanjiUE/JumanjiUE.Build.cs
- Source/JumanjiUE/Public/JumanjiWorldAtlas.h
- Source/JumanjiUE/Private/JumanjiWorldAtlas.cpp
- Source/JumanjiUE/Public/JumanjiWorldRegionVolume.h
- Source/JumanjiUE/Public/JumanjiPlayerController.h
- Source/JumanjiUE/Private/JumanjiPlayerController.cpp
- Content/World/README.md
- README.md

## 10. Ograniczenia i dalszy etap

Stage 12 zapewnia źródłową logikę wyboru celu i zapytania o NavMesh. Nie tworzy binarnej mapy .umap, danych NavMesh, aktorów przejść, modeli, animacji, widgetów .uasset ani gotowego APK.

Następny planowany etap:
**Stage 13 — Player Profile & Exploration Persistence**: trwały zapis odkryć, profilu postaci, wyboru Great Spirit i postępu rozdziału. W wersji sieciowej ostatecznym źródłem prawdy będzie backend/serwer profilu, a lokalny SaveGame będzie pełnił rolę cache lub trybu offline.
