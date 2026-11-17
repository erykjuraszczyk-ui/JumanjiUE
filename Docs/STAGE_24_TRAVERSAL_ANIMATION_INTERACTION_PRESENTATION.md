# JUMANJI 2.111.1 — Stage 24: Traversal Animation & Interaction Presentation

## Cel etapu

Domknąć źródłową warstwę prezentacji przejść kontekstowych z Stage 23. Gracz ma otrzymać czytelny, dotykowy prompt, a Blueprint postaci ma dostać jednoznaczny, replikowany sygnał rozpoczęcia, ukończenia lub przerwania przejścia. Tyrolka otrzymuje opcjonalną ścieżkę spline, próbkowaną po stronie serwera.

Stage 24 rozwija istniejące:
- AJumanjiTraversalLink;
- UJumanjiTraversalComponent;
- AJumanjiPlayerController;
- AJumanjiCharacter.

Nie tworzy drugiego systemu traversal ani drugiego kanału RPC.

## 1. Zakres plików

Dodane:
- Source/JumanjiUE/Public/JumanjiTraversalPresentationSubsystem.h
- Source/JumanjiUE/Private/JumanjiTraversalPresentationSubsystem.cpp
- Source/JumanjiUE/Public/JumanjiTraversalPromptWidget.h
- Source/JumanjiUE/Private/JumanjiTraversalPromptWidget.cpp
- Data/Traversal/TraversalPresentationManifest.csv
- Scripts/validate_traversal_presentation.py
- Scripts/Tests/test_validate_traversal_presentation.py
- .github/workflows/traversal-presentation-validation.yml
- Docs/STAGE_24_TRAVERSAL_ANIMATION_INTERACTION_PRESENTATION.md
- Content/Traversal/README.md

Zmienione:
- Source/JumanjiUE/Public/JumanjiTraversalLink.h
- Source/JumanjiUE/Private/JumanjiTraversalLink.cpp
- Source/JumanjiUE/Public/JumanjiTraversalComponent.h
- Source/JumanjiUE/Private/JumanjiTraversalComponent.cpp
- Source/JumanjiUE/Public/JumanjiPlayerController.h
- Source/JumanjiUE/Private/JumanjiPlayerController.cpp
- Source/JumanjiUE/JumanjiUE.Build.cs
- Config/DefaultGame.ini
- README.md
- Docs/STAGE_23_EMERALD_JUNGLE_TRAVERSAL_INTERACTIONS.md

## 2. Manifest prezentacji przejść

### FJumanjiTraversalPresentationDefinition

Każdy rekord zawiera:
- typ przejścia;
- nazwę i prompt;
- tekst stanu aktywnego, ukończonego i przerwanego;
- ikonę promptu;
- StartMontage, LoopMontage i EndMontage;
- StartSound i EndSound;
- StartEffect i EndEffect (Niagara);
- ustawienia kamery i paska postępu;
- czas utrzymania komunikatu po zakończeniu.

Źródłowy CSV:
`Data/Traversal/TraversalPresentationManifest.csv`

Import w Unreal Editor:
1. Content Browser → Import.
2. Wskaż CSV.
3. Wybierz row struct `FJumanjiTraversalPresentationDefinition`.
4. Nazwij asset `DT_TraversalPresentation`.
5. Zapisz w `/Game/Traversal/`.
6. Sprawdź cztery row keys:
   - TRAVERSAL_CONTEXT_JUMP;
   - TRAVERSAL_CLIMB;
   - TRAVERSAL_ZIPLINE;
   - TRAVERSAL_LIFT.

Row key jest stabilnym `TraversalId` aktora AJumanjiTraversalLink. Nie zmieniaj go przy przesuwaniu aktora na mapie. Jeżeli rekord jest nieobecny, subsystem daje wbudowany polski tekst dla typu przejścia. Jeśli rekord istnieje, ale ma inny typ niż aktor, lookup zostaje odrzucony.

### UJumanjiTraversalPresentationSubsystem

Subsystem GameInstance:
- ładuje małą DataTable na starcie;
- sprawdza row struct, liczbę rekordów, unikalne klucze, wymagane teksty i czasy promptu;
- przechowuje zatwierdzone rekordy;
- zwraca fallback dla brakującego wiersza;
- nie ładuje synchronicznie montages, dźwięków ani efektów.

Soft references powinny być ładowane asynchronicznie przez warstwę Blueprint/UMG i utrzymywane przy pomocy Streamable Handle tak długo, jak długo są używane.

Konfiguracja:
`Config/DefaultGame.ini`

```ini
[/Script/JumanjiUE.JumanjiTraversalPresentationSubsystem]
PresentationDataTable=/Game/Traversal/DT_TraversalPresentation.DT_TraversalPresentation
```

Jeżeli asset DataTable nie jest jeszcze utworzony, subsystem loguje ostrzeżenie, a prompt nadal korzysta z wbudowanych polskich etykiet.

## 3. Kontekstowy prompt dotykowy

### UJumanjiTraversalPromptWidget

Pliki:
- Source/JumanjiUE/Public/JumanjiTraversalPromptWidget.h
- Source/JumanjiUE/Private/JumanjiTraversalPromptWidget.cpp

Utwórz Widget Blueprint:
`WBP_TraversalPrompt`

Parent Class:
`UJumanjiTraversalPromptWidget`

View model:
- TraversalId;
- TraversalType;
- DisplayName;
- StatusText;
- Icon;
- bCanRequest;
- bAllowCameraLook;
- bShowProgress;
- DurationSeconds;
- PromptHoldSeconds;
- Revision;
- Status.

Stany promptu:
- Hidden;
- Available;
- PendingServer;
- InProgress;
- Completed;
- Interrupted.

API:
- SetFocusedTraversalLink(Link);
- ClearFocusedTraversal();
- RequestFocusedTraversal();
- GetPromptViewModel();
- OnPromptChanged.

Widget:
- nie wykrywa samodzielnie całej mapy;
- nie wykonuje ruchu;
- nie wysyła własnego RPC;
- korzysta z RequestTraversalLink istniejącego PlayerController;
- aktualizuje stan dopiero po replikowanym wyniku komponentu traversal;
- ma trzysekundowy timeout dla oczekującego potwierdzenia klienta.

### Integracja z PlayerController

W Blueprint child AJumanjiPlayerController przypisz:
`TraversalPromptWidgetClass = WBP_TraversalPrompt`

PlayerController tworzy widget tylko dla lokalnego kontrolera.

Gdy postać wejdzie w zasięg przejścia:
1. Blueprint overlap lub krótki line trace wykrywa kandydata.
2. Blueprint sprawdza, czy to najbliższy poprawny AJumanjiTraversalLink.
3. Wywołuje `SetTraversalFocus(Link)` na lokalnym PlayerController.
4. Po utracie zasięgu lub zniszczeniu linku wywołuje `ClearTraversalFocus()`.

Nie skanuj wszystkich aktorów traversal co klatkę. Użyj overlap events albo throttled trace, orientacyjnie 4–8 razy na sekundę.

W WBP_TraversalPrompt:
- pokaż ikonę i nazwę;
- pokaż tekst promptu;
- włącz przycisk tylko w stanie Available;
- OnPressed przycisku wywołuje `RequestFocusedTraversal()`;
- status PendingServer wyłącza przycisk;
- InProgress pokazuje postęp;
- Completed/Interrupted pokazuje krótki komunikat, a potem prompt się chowa.

Układ mobilny powinien mieć duże pole dotykowe, bezpieczne odstępy od krawędzi ekranu i nie może blokować całego widoku gry.

## 4. Serwerowo potwierdzony stan

W FJumanjiTraversalReplicatedState dodano:
- bReverse — kierunek używany do odwróconej tyrolki;
- bLastTraversalCompleted — wynik ostatniego przejścia.

UJumanjiTraversalComponent publikuje:
`OnTraversalStateChanged(State)`

Zdarzenie jest nadawane:
- po akceptacji rozpoczęcia na serwerze;
- po ukończeniu lub przerwaniu na serwerze;
- po otrzymaniu nowego stanu przez OnRep na kliencie.

Istniejący Blueprint event `OnTraversalPresentationChanged` nadal działa.

Blueprint może odtwarzać:
1. StartMontage i StartSound po stanie aktywnym;
2. LoopMontage podczas przejścia, jeśli montaż został zaprojektowany jako bezpieczny dla serwerowej interpolacji;
3. EndMontage, EndSound i EndEffect po stanie ukończonym;
4. animację przerwania po stanie Interrupted.

Nie używaj root motion do przesuwania postaci, gdy serwer nadal steruje jej pozycją przez prostą interpolację i collision sweep. W tej architekturze montages są prezentacją, a pozycja nadal należy do serwera.

Nie odtwarzaj animacji sukcesu po samym kliknięciu. Poczekaj na zatwierdzony stan replikowany.

## 5. Spline dla tyrolki

AJumanjiTraversalLink otrzymuje opcjonalny komponent:
`TraversalSpline`

Dla Zipline:
1. Włącz `bUseSplinePath`.
2. Ustaw pierwszy punkt spline w EntryAnchor.
3. Ustaw ostatni punkt w ExitAnchor.
4. Prowadź spline po rzeczywistej trasie liny, omijając landscape i przeszkody.
5. Zadbaj o zgodność tangensu spline z orientacją postaci.
6. Dostosuj TraversalDuration do długości trasy.
7. Dla linku dwukierunkowego przetestuj obie strony.

Serwer próbuje odczytać transform z spline przy aktualnym Alpha i wykonuje SetActorLocation z collision sweep. Jeśli spline jest nieaktywny, ma mniej niż dwa punkty albo długość poniżej minimalnego progu, runtime użyje interpolacji pomiędzy EntryAnchor i ExitAnchor.

Spline ruchu postaci nie tworzy kabla. Kabel jest osobnym wizualnym Spline Mesh Component w Blueprint child linku.

## 6. Prezentacja windy

W tym etapie Lift nadal przesuwa kapsułę postaci pomiędzy kotwicami. Nie jest to jeszcze fizyczny, replikowany aktor platformy.

W Blueprint child przygotuj:
- platform mesh;
- collision platformy;
- szyny i zabezpieczenia;
- wejście i wyjście;
- StartMontage/EndMontage;
- dźwięki i Niagara cues.

EntryAnchor i ExitAnchor określają pozycję środka kapsuły, a nie poziom stóp postaci.

Nie animuj platformy niezależnie od ruchu postaci w sposób, który może doprowadzić do rozjechania kolizji. Dedykowany, replikowany lift-platform actor z obsługą kilku pasażerów wymaga oddzielnego etapu.

## 7. Walidacja źródeł i CI

Walidator:
`Scripts/validate_traversal_presentation.py`

Testy:
`Scripts/Tests/test_validate_traversal_presentation.py`

Uruchom w katalogu głównym repozytorium:

```bash
python Scripts/validate_traversal_presentation.py
python -m unittest discover -s Scripts/Tests -p "test_validate_traversal_presentation.py" -v
```

Kontrole obejmują:
- komplet wymaganych kolumn;
- dokładnie cztery starter rows;
- unikalne row names;
- cztery obsługiwane typy;
- wymagane teksty;
- poprawne flagi bool;
- finite PromptHoldSeconds w przedziale 0.2–3.0;
- poprawny format soft object paths, jeśli asset path został podany.

GitHub Actions wykonuje walidację w workflow:
`.github/workflows/traversal-presentation-validation.yml`

PASS oznacza spójność CSV. Nie oznacza, że DataTable została zaimportowana, montage istnieje lub link działa w PIE.

## 8. Kryteria akceptacji

### Runtime i replikacja
- [ ] Projekt kompiluje się w Unreal Engine 5.8.
- [ ] PlayerController tworzy prompt tylko na lokalnym kliencie.
- [ ] SetTraversalFocus pokazuje najbliższy poprawny link.
- [ ] ClearTraversalFocus chowa prompt i odłącza delegate.
- [ ] Zdalny klient widzi PendingServer, a potem replikowany InProgress.
- [ ] Odrzucenie żądania nie blokuje przycisku na stałe.
- [ ] Completed i Interrupted są rozróżnione.
- [ ] Prompt znika po czasie ustawionym w manifest.
- [ ] Nie można rozpocząć drugiego przejścia w trakcie pierwszego.

### Prezentacja
- [ ] DT_TraversalPresentation importuje się z czterema wierszami.
- [ ] Brak wiersza daje poprawny tekst fallback.
- [ ] Rekord o innym typie niż TraversalLink jest odrzucany.
- [ ] Widget ładuje ikonę asynchronicznie.
- [ ] Montage i Niagara są ładowane asynchronicznie.
- [ ] Animacja sukcesu jest uruchamiana dopiero po stanie server-confirmed.
- [ ] Prompt działa na dotyku i nie blokuje kamery poza stanem przejścia.

### Spline i kolizja
- [ ] Zipline używa spline przy poprawnej konfiguracji.
- [ ] Brak spline daje fallback na anchor interpolation.
- [ ] Odwrócona tyrolka porusza się w przeciwną stronę.
- [ ] Kolizja przerywa ruch bez przeniesienia kapsuły przez geometrię.
- [ ] Koniec spline pokrywa się z ExitAnchor.
- [ ] Lift kończy się na bezpiecznej pozycji kapsuły.

### Źródła
- [ ] Walidator CSV przechodzi.
- [ ] Testy jednostkowe przechodzą.
- [ ] Workflow GitHub Actions przechodzi na PR.
- [ ] W repozytorium nie ma nieprzypisanych lub fałszywych ścieżek do binarnych assetów.

## 9. Ograniczenia etapu

Stage 24 zawiera źródłowe klasy, manifest, walidator i kontrakt authoringu. Nie zawiera:
- zaimportowanego DataTable .uasset;
- WBP_TraversalPrompt;
- finalnych montages;
- ikon, sound cues i Niagara systemów;
- blueprintów przejść i rozmieszczonych aktorów na mapie;
- spline mesh cable dla tyrolek;
- fizycznej windy z platformą i obsługą kilku graczy;
- gotowej mapy .umap, zbudowanych Navigation Data Chunks ani APK/AAB.

Źródło nie zostało skompilowane w Unreal Editor w ramach tej aktualizacji GitHub. Potrzebne jest lokalne otwarcie projektu i wykonanie listy akceptacji.

## Następny etap

**Stage 25 — Traversal World Integration & Multiplayer Validation**: rozmieszczenie linków na mapie, połączenie promtów z rzeczywistymi overlap volumes, synchronizacja fizycznej platformy windy, testy dwóch i większej liczby graczy oraz testy kolizji/World Partition na urządzeniu docelowym.
