# JUMANJI 2.111.1 — Stage 22: Emerald Jungle Greybox Validation & Mobile Performance

## Cel etapu

Przejść z importowalnych danych greybox (Stage 21) do powtarzalnego procesu kontroli jakości źródeł świata, rozmieszczenia aktorów, nawigacji World Partition i testów Android. Etap dodaje automatyczny walidator CSV/JSON, testy jednostkowe, workflow GitHub Actions i szablon zapisu wyników pomiarów na urządzeniu.

Stage 22 nie tworzy geometrii mapy i nie udaje, że NavMesh lub performance zostały przetestowane. Automatyczna walidacja źródeł nie zastępuje testu w Unreal Editorze ani na fizycznym urządzeniu.

## 1. Pliki etapu

Dodane:
- Scripts/validate_emerald_jungle_world.py — walidator standardowej biblioteki Pythona;
- Scripts/Tests/test_validate_emerald_jungle_world.py — testy parsera i test integracyjny na danych repozytorium;
- .github/workflows/emerald-jungle-world-validation.yml — automatyczna kontrola dla pull requestów i aktualizacji main;
- Data/World/MobileValidationRunTemplate.csv — szablon rejestracji prób wydajności Android.

Raporty lokalne:
- Saved/WorldValidation/EmeraldJungle-Validation.json;
- Saved/WorldValidation/EmeraldJungle-Validation.md.

Raporty workflow są publikowane jako artefakt GitHub Actions przez 14 dni. Nie dodawaj wygenerowanych raportów ani danych osobowych do repozytorium.

## 2. Uruchomienie walidatora

W katalogu głównym repozytorium:

```bash
python Scripts/validate_emerald_jungle_world.py --project-root . --report-dir Saved/WorldValidation
python -m unittest discover -s Scripts/Tests -p "test_*.py" -v
```

Walidator nie wymaga dodatkowych pakietów Python.

Exit code:
- 0 — wszystkie kontrole źródeł przeszły;
- 1 — wystąpił błąd danych lub brak wymaganego pliku.

### Zakres kontroli źródeł

Walidator sprawdza:
- 10 regionów, 10 landmarków i 29 wpisów placement ledger;
- zgodność nazw wierszy DataTable z RegionId/LandmarkId;
- komplet wymaganych identyfikatorów dziewięciu triali i powiązań z regionami;
- zgodność ElementTag, RelatedObjectiveId i EditorDataLayer;
- zakres znormalizowanych MapCoordinate 0..1;
- poprawność i skończoność wektorów NavigationAnchor oraz WorldLocation;
- zgodność MapCoordinate z kotwicą świata według manifestu 400 km × 400 km;
- zgodność położenia RegionVolume z kotwicą atlasu;
- dodatnie half-extents RegionVolume;
- położenie każdego Landmark i Trial wewnątrz właściwego RegionVolume;
- zgodność pozycji aktora Landmark z DataTable;
- kompletność listy Editor Data Layers w World Manifest;
- brak nieznanych typów aktorów i zduplikowanych ActorId.

Status manifestu musi nadal jasno wskazywać, że wartości są placeholderami greybox. Po pomiarze prawdziwej mapy utwórz osobny przegląd aktualizacji manifestu — nie usuwaj oznaczenia placeholderów bez weryfikacji terenu i NavMesh.

### Czego walidator nie robi

Nie otwiera .umap ani .uasset, nie sprawdza collision w edytorze, nie buduje NavMesh, nie testuje strumieniowania World Partition, nie uruchamia PIE, nie kompiluje C++ i nie mierzy FPS/RAM/temperatury Android. Raport PASS oznacza wyłącznie spójność źródeł CSV/JSON.

## 3. Import i kontrola danych w Unreal Editorze

1. Otwórz JumanjiUE.uproject w Unreal Engine 5.8.
2. Zaimportuj DT_Jumanji_WorldRegions.csv jako DataTable z row struct FJumanjiWorldRegionDefinition.
3. Zaimportuj DT_Jumanji_WorldLandmarks.csv jako DataTable z row struct FJumanjiWorldLandmarkDefinition.
4. Sprawdź po 10 wierszy i zgodność row key Name z RegionId/LandmarkId.
5. Ustaw oba soft-object paths w Config/DefaultGame.ini. Nie ustawiaj tylko jednej tabeli — runtime atlas używa override all-or-nothing.
6. Otwórz L_EmeraldJungle_World.umap. W World Outliner sprawdź 10 RegionVolume, 10 Landmark i 9 Trial actorów według EmeraldJungle_GreyboxPlacement.csv.
7. Sprawdź przypisanie do Editor Data Layers. W szczególności każdy RegionVolume, Landmark i Trial ma trafić do warstwy własnego regionu.
8. Obejrzyj granice regionów w trybie kolizji. Volume odkrywania ma generować overlap z Pawn i nie blokować chodzenia.
9. Sprawdź DiscoverySphere landmarku. Mesh wizualny ma mieć NoCollision, a sfera jedynie QueryOnly / Pawn overlap.
10. Sprawdź, czy trial actor znajduje się w obszarze regionu, jest osiągalny i nie jest ukryty pod landscape.

## 4. Walidacja granic regionów

W PIE wykonaj test dla każdego z 10 regionów:

- wejście do regionu odkrywa tylko jego RegionId;
- przejście do sąsiedniej strefy nie odkrywa regionu przez przypadkowy overlap granic;
- ponowne wejście nie zwiększa Revision ponownie dla już odkrytego regionu;
- landmark odkrywa swój RegionId i LandmarkId;
- aktor bez poprawnego ID lub z ID nieobecnym w atlasie nie zapisuje odkrycia;
- dwaj gracze w sesji multiplayer mają niezależny stan odkryć.

Sprawdź granice z różnych wysokości: ścieżki przy ziemi, mosty, półki skalne i wejścia do jaskiń. RegionVolume to logiczna granica odkrywania, nie niewidzialna ściana świata. Jeśli volume przecina ruchliwą ścieżkę sąsiedniego regionu, popraw geometrię volume zamiast zmieniać logikę PlayerState.

## 5. Walidacja World Partition i NavMesh

### Budowa nawigacji dla mapy World Partition

1. Zapisz wszystkie zmiany w mapie.
2. Włącz widok Navigation (P) i sprawdź, czy NavMesh obejmuje faktycznie przechodnie fragmenty greyboxu.
3. W konsoli edytora ustaw:

```text
n.bNavmeshAllowPartitionedBuildingFromEditor 1
```

4. Wybierz Build → Build Paths i wykonaj budowę nawigacji dla mapy World Partition.
5. Otwórz World Outliner i sprawdź wygenerowane NavDataChunk actors.
6. Zapisz mapę po zakończeniu budowy.
7. Uruchom PIE i przemieszczaj się między regionami. Obserwuj, czy potrzebne chunki są ładowane, a ścieżki nie kończą się na granicy komórki.
8. Sprawdź NavLinkProxy/Smart Link na każdym przejściu z osobnym ruchem aktora. Sama obecność linku w NavMesh nie animuje ani nie przenosi postaci gracza.

Dla pierwszego szarego bloku używaj statycznego NavMesh dla stałej geometrii. Navigation Invokers i Dynamic generation wprowadzaj wyłącznie po pomiarze kosztu CPU, RAM i czasu odbudowy na docelowym urządzeniu.

### Test tras

Dla każdego odkrytego regionu i landmarku:
- wybór celu zwraca właściwy TargetId i TargetLocation;
- pełna ścieżka ma co najmniej dwa PathPoints;
- ścieżka częściowa jest oznaczona jako Partial;
- brak NavMesh nie rysuje linii prostej jako prawdziwej trasy;
- DirectDistanceMeters pozostaje odrębne od PathLengthMeters;
- wyczyszczenie celu usuwa trasę i wyłącza odświeżanie;
- wybory nawigacji dwóch klientów nie wpływają na siebie.

Zanotuj czasy zapytań nawigacyjnych. Powtarzane zapytanie co klatkę jest błędem; komponent powinien korzystać z ograniczonego interwału odświeżania.

## 6. Android — procedura profilowania

### Macierz testowa

Rejestruj dla każdego urządzenia:
- model urządzenia i SoC;
- wersję Android;
- konfigurację Development/Shipping;
- renderer (Vulkan/OpenGL ES, jeśli używany);
- rozdzielczość, skalę renderowania i Device Profile;
- stan ładowania, temperaturę początkową i warunki testu.

Nie porównuj wyników z różnych urządzeń bez podania tych danych.

### Scenariusze

Uzupełnij Data/World/MobileValidationRunTemplate.csv osobno dla każdego urządzenia i scenariusza:

1. Hub idle — 10 minut w Serce Dżungli;
2. Heart to Root — pięć powtórzeń standardowej trasy;
3. World Partition region loop — 10 minut przemieszczania przez kolejne regiony;
4. Landmark discovery — 10 kolejnych punktów POI;
5. Trial interaction — dziewięć prób środowiskowych;
6. Navigation target refresh — 10 minut aktywnej trasy i odświeżania;
7. Thermal soak — 20 minut ciągłej eksploracji.

Wykonaj co najmniej trzy powtórzenia każdego scenariusza po rozgrzaniu urządzenia. Nie traktuj pojedynczej klatki ani najlepszego przebiegu jako miarodajnego wyniku.

### Statystyki i narzędzia

W Development buildzie użyj:
- stat unit — Game Thread, Render Thread i GPU;
- stat unitgraph — zmienność czasu klatki;
- stat texturegroup — pamięć grup tekstur;
- memreport -full — raport pamięci i załadowanej zawartości;
- Unreal Insights na Android — profilowanie CPU, GPU, pamięci i hitchy;
- Perfetto lub RenderDoc, gdy potrzebny jest głębszy profil systemowy albo pojedynczej klatki.

Czteropalcowy gest otwarcia konsoli UE jest dostępny wyłącznie w Development buildach, nie w Shipping/Testing. Raport memreport wykonuj na buildzie i urządzeniu docelowym, ponieważ wynik uwzględnia rzeczywiste ustawienia kompresji i platformy.

### Progi projektowe — nie są wynikami pomiarów

- Bazowy cel greybox: stabilne 30 FPS, czyli budżet 33,3 ms na klatkę.
- Cel rozszerzony: 60 FPS, czyli 16,7 ms, tylko gdy docelowy sprzęt utrzymuje go bez nadmiernego nagrzewania i skoków zużycia energii.
- Nie wpisuj arbitralnego limitu RAM dla całej rodziny Android. Najpierw zmierz pamięć procesu i texture pools na każdym urządzeniu docelowym, następnie ustal budżet z marginesem dla systemu i streamingu.
- Zapisuj P50/P95/P99 czasu klatki oraz CPU/GPU oddzielnie; sama średnia FPS nie ujawnia hitchy.
- Notuj streaming hitches, chwilowe spadki FPS, temperaturę i throttling.
- Po każdej optymalizacji powtarzaj ten sam scenariusz i ustawienia.

## 7. Optymalizacje po zebraniu baseline

Priorytet kolejności:
1. Usuń zbędne region volumes i collision z dekoracji.
2. Ogranicz NavMesh do rzeczywiście dostępnych ścieżek.
3. Zmniejsz koszt foliage przez instancing i LOD.
4. Sprawdź rozmiary tekstur i mipmaps na urządzeniu.
5. Ogranicz cienie i koszt materiałów/translucency.
6. Sprawdź liczbę aktorów tickujących i częstotliwość aktualizacji UI.
7. Dostrajaj World Partition loading range i Navigation Data Chunk Grid dopiero na podstawie pomiaru.
8. Rozważ osobne Device Profile / Scalability dla segmentów sprzętu.

Nie wprowadzaj równocześnie wielu zmian. Każdą optymalizację porównuj z tym samym baseline.

## 8. Kryteria akceptacji

### Automatyczne źródła
- [ ] Walidator przechodzi na aktualnym CSV/JSON.
- [ ] Unit tests przechodzą.
- [ ] Zmiana MapCoordinate poza 0..1 powoduje błąd.
- [ ] Duplikat ID lub niedopasowany region/landmark/trial powoduje błąd.
- [ ] Workflow GitHub Actions publikuje raport JSON/Markdown.

### Unreal Editor
- [ ] Obie DataTables importują się z prawidłowym row struct.
- [ ] Mapa zawiera komplet 29 aktorów z ledger.
- [ ] Granice 10 RegionVolume nie mają niepożądanych overlapów.
- [ ] Landmark discovery działa w odpowiednim regionie.
- [ ] Trial aktorzy są osiągalni, a interakcje działają w PIE.
- [ ] World Partition ładuje wymagane obszary i Navigation Data Chunks.
- [ ] Pełne, częściowe i niedostępne ścieżki są prezentowane poprawnie.
- [ ] Dwóch klientów utrzymuje niezależny cel nawigacji i odkrycia.

### Android
- [ ] Development build uruchamia wszystkie scenariusze testowe.
- [ ] Rejestr pomiarów zawiera model urządzenia, Android, renderer i Device Profile.
- [ ] Zapisano P50/P95/P99 frame time, CPU/GPU i Peak RSS.
- [ ] Zapisano memory report i streaming hitches.
- [ ] Wykonano test 20-minutowy i odnotowano temperaturę/throttling.
- [ ] Bazowy cel 30 FPS jest utrzymany lub zarejestrowano dokładny zakres i przyczynę odchyleń.
- [ ] Wyniki zostały porównane po optymalizacjach.

## 9. Ograniczenia

Stage 22 dostarcza automatyczne testy źródeł i procedurę testową. Nie zawiera:
- finalnej mapy .umap ani DataTable .uasset;
- realnego terrain/landscape, foliage, meshów, materiałów i efektów;
- zbudowanego NavMesh i NavDataChunk z edytora;
- raportu Unreal Insights lub memreport z Android;
- potwierdzonych wyników FPS, pamięci, temperatury albo zużycia baterii;
- finalnego APK/AAB.

Te wyniki wymagają kompilacji UE 5.8, edytora, złożonej mapy i fizycznego urządzenia.

## Oficjalne materiały Epic

- World Partitioned Navigation Mesh: https://dev.epicgames.com/documentation/unreal-engine/world-partitioned-navigation-mesh
- Mobile Optimization and Development Best Practices: https://dev.epicgames.com/documentation/unreal-engine/optimization-and-development-best-practices-for-mobile-projects-in-unreal-engine
- Performance and Optimization for Mobile: https://dev.epicgames.com/documentation/unreal-engine/performance-and-optimization-for-mobile-in-unreal-engine
- Unreal Insights / Performance Profiling: https://dev.epicgames.com/documentation/unreal-engine/introduction-to-performance-profiling-and-configuration-in-unreal-engine

## Następny etap

**Stage 23 — Emerald Jungle Traversal Interactions & Off-Mesh Links**: kontekstowe mosty, wspinaczka, liny/zipline, windy i animowane przejścia po uprzednim potwierdzeniu granic regionów, NavMesh oraz wydajności bazowej.
