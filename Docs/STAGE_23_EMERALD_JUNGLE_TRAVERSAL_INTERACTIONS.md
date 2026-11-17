# JUMANJI 2.111.1 — Stage 23: Emerald Jungle Traversal Interactions & Off-Mesh Links

## Cel etapu

Dodać grywalne przejścia kontekstowe pomiędzy fragmentami świata, których nie łączy zwykła powierzchnia NavMesh. Etap dostarcza wspólny runtime dla skoku kontekstowego, wspinaczki, tyrolki i windy oraz łączy go z nawigacją Stage 12 i kontrolą jakości Stage 22.

Ważne rozdzielenie:
- Smart Link / Off-Mesh Link informuje system nawigacji, że dwa obszary są połączone.
- AJumanjiTraversalLink przechowuje punkty wejścia/wyjścia i reguły interakcji.
- UJumanjiTraversalComponent wykonuje ruch postaci po zatwierdzeniu przez serwer.
- Blueprint postaci odpowiada za animację, dźwięk, kamerę i efekty prezentacyjne.

Sam NavLink nie animuje gracza.

## 1. Pliki etapu

Dodane:
- Source/JumanjiUE/Public/JumanjiTraversalLink.h
- Source/JumanjiUE/Private/JumanjiTraversalLink.cpp
- Source/JumanjiUE/Public/JumanjiTraversalComponent.h
- Source/JumanjiUE/Private/JumanjiTraversalComponent.cpp

Zmienione:
- Source/JumanjiUE/Public/JumanjiCharacter.h
- Source/JumanjiUE/Private/JumanjiCharacter.cpp
- Source/JumanjiUE/Public/JumanjiPlayerController.h
- Source/JumanjiUE/Private/JumanjiPlayerController.cpp
- README.md
- Content/World/README.md

## 2. Typy przejść

| Typ | Zachowanie runtime | Prezentacja do przypisania w Blueprint |
|---|---|---|
| ContextJump | Ruch po krzywej z łukiem w osi Z | start, lot, lądowanie |
| Climb | Interpolacja pomiędzy kotwicami wspinaczki | wejście, cykl wspinania, zejście |
| Zipline | Interpolacja pomiędzy końcami liny | zaczepienie, przejazd, odpięcie |
| Lift | Interpolacja pomiędzy dolnym i górnym punktem platformy | wejście, jazda, wyjście |

W tej wersji wszystkie ruchy są serwerowo interpolowane między dwoma punktami. Dla ContextJump dodawany jest łuk. Climb, Zipline i Lift otrzymują osobne typy, czas, kotwice i event prezentacyjny, ale docelowe modele i animacje trzeba przypisać jako zasoby edytora.

## 3. AJumanjiTraversalLink

Klasa dziedziczy po ANavLinkProxy. Każda instancja w świecie ma:
- stabilny TraversalId;
- TraversalType;
- EntryAnchor oraz ExitAnchor;
- MaxInteractionDistance;
- TraversalDuration;
- ArcHeight;
- bBidirectional;
- bTraversalEnabled;
- opcjonalny Smart Link dla nawigacji;
- opcjonalny soft-reference do UAnimMontage.

Dwa Scene Componenty wyznaczają pozycję kapsuły gracza przy początku i końcu przejścia. Punkty są pozycjami root/capsule center, a nie pozycjami stóp postaci. Dopasuj wysokość do rzeczywistej kapsuły AJumanjiCharacter.

Komponent Smart Link dostaje lokalne współrzędne obu kotwic. Ustawienie bTraversalEnabled=false wyłącza interakcję gracza oraz Smart Link nawigacyjny. Zmiana przechodzi przez serwer i jest replikowana.

Dla połączeń jednokierunkowych gracz może rozpocząć wyłącznie od EntryAnchor. Przy bBidirectional może rozpocząć z bliższego końca; komponent automatycznie odwraca kierunek.

## 4. UJumanjiTraversalComponent

Komponent jest tworzony automatycznie na AJumanjiCharacter.

Odpowiedzialność:
1. sprawdzenie, czy Character jest na ziemi i nie wykonuje innego przejścia;
2. walidacja odległości do kotwicy;
3. walidacja obu punktów oraz czasu przejścia;
4. zapis poprzedniego movement mode;
5. zatrzymanie standardowego ruchu postaci;
6. interpolacja po stronie serwera z collision sweep;
7. anulowanie w przypadku blokującej kolizji;
8. przywrócenie movement mode;
9. replikacja stanu przejścia i eventu prezentacyjnego.

Ruch nie jest wyliczany przez klienta. Klient może wysłać żądanie, ale serwer ponownie sprawdza aktora, zasięg, tryb ruchu, stan komponentu i kotwice.

Komponent posiada Blueprint event:
OnTraversalPresentationChanged(bActive, TraversalId, TraversalType, Link).

W dziecku Blueprint postaci:
- przy bActive=true odczytaj soft-reference z linku i załaduj montage asynchronicznie;
- uruchom właściwą animację i opcjonalny dźwięk;
- przy bActive=false przerwij lub domknij animację i przywróć normalny ruch prezentacyjny.

Nie odtwarzaj animacji sukcesu wyłącznie po kliknięciu przycisku. Event pochodzi ze stanu zaakceptowanego przez serwer.

## 5. PlayerController API

Dodano:
AJumanjiPlayerController::RequestTraversalLink(AJumanjiTraversalLink*).

Przepływ:
1. Widget lub interakcja świata wykrywa pobliski link.
2. Wywołuje RequestTraversalLink.
3. Klient wysyła ServerRequestTraversalLink.
4. Serwer sprawdza link, pawn, zasięg i komponent.
5. Serwer wywołuje TryStartTraversal.
6. Stan replikuje się do klientów, a CharacterMovement replikuje pozycję postaci.

Zwrócenie true po stronie klienta oznacza tylko zakolejkowanie requestu, nie potwierdzenie wykonania przejścia.

## 6. Unreal Editor — authoring

### 6.1. Blueprint klasy linku

1. Skompiluj moduł C++ w Unreal Engine 5.8.
2. Utwórz BP_TraversalLink_Base jako Blueprint child AJumanjiTraversalLink.
3. W viewport ustaw EntryAnchor i ExitAnchor na fizycznych końcach przejścia.
4. Ustaw TraversalId jako unikalny stabilny identyfikator.
5. Wybierz typ przejścia.
6. Dostosuj Duration, ArcHeight i InteractionDistance do skali i tempa gry.
7. Ustaw bBidirectional tylko wtedy, gdy postać ma móc przejść w obie strony.
8. Pozostaw bExposeSmartLinkToNavigation włączone, jeśli trasa ma być widoczna dla zapytań NavMesh.
9. Zbuduj Blueprinty pochodne, jeśli wymagają innych meshów, dźwięków lub montages.
10. Wstaw aktora na mapę i przypisz mu Editor Data Layer właściwego regionu.

### 6.2. NavMesh i Smart Link

- EntryAnchor oraz ExitAnchor muszą leżeć w pobliżu dwóch poprawnych obszarów NavMesh.
- Nie ustawiaj końca linku wewnątrz ściany, pod landscape ani poza komórką nawigacyjną.
- Obejrzyj NavMesh w widoku P.
- Sprawdź Smart Link po zmianie kotwic i ponownie zbuduj Navigation Data, jeśli statyczna mapa wymaga przebudowy.
- W World Partition sprawdź, czy odpowiednie Navigation Data Chunks ładują się przy obu końcach linku.
- Link nie może zastępować schodów lub chodnika, jeżeli istnieje normalne przejście, które działa lepiej.

Smart Link jest połączeniem nawigacyjnym. Nie odgrywa samodzielnie animacji postaci.

### 6.3. Punkty kotwic

Dla AJumanjiCharacter root jest środkiem kapsuły. Wysokość EntryAnchor i ExitAnchor musi uwzględniać:
- połowę wysokości kapsuły;
- pochylenie powierzchni;
- wolną przestrzeń nad głową;
- bezpieczne miejsce zakończenia ruchu;
- możliwość powrotu do Walking/NavWalking.

Przed zatwierdzeniem mapy sprawdź przejście z kilku kierunków oraz z pełną kolizją.

## 7. Interfejs dotykowy

WBP_AdventureHUD powinien udostępnić kontekstowy przycisk „Użyj przejścia” widoczny tylko, gdy:
- postać znajduje się w promieniu interakcji;
- link jest włączony;
- stan postaci pozwala na rozpoczęcie przejścia.

Przycisk wywołuje RequestTraversalLink na lokalnym PlayerController. Zablokuj przycisk do czasu otrzymania replikowanego stanu, aby ograniczyć wielokrotne żądania.

Minimalne komunikaty:
- „Podejdź bliżej”
- „Przejście niedostępne”
- „Trwa przejście”
- „Przejście przerwane — przeszkoda”
- „Przejście zakończone”

Sterowanie kamerą może pozostać aktywne, ale ruch postaci i aktywacja umiejętności są wstrzymane na czas przejścia.

## 8. Kolizja i autorytet

- Linki wstawione do mapy muszą mieć unikalne TraversalId.
- Serwer odrzuca link nieaktywny, nieznany, zbyt odległy lub niepoprawnie skonfigurowany.
- Serwer odrzuca próbę rozpoczęcia przejścia, gdy pawn nie jest na ziemi albo wykonuje już przejście.
- Ruch postaci jest wykonywany z collision sweep.
- Kolizja blokująca przerywa przejście i przywraca poprzedni movement mode.
- Nie umieszczaj punktu końcowego w geometrii stałej.
- Nie wykorzystuj linku do teleportacji pomiędzy odległymi regionami; jego odległość i czas muszą odpowiadać fizycznej geometrii.

## 9. Kryteria akceptacji

### Kod i replikacja
- [ ] Projekt kompiluje się w UE 5.8.
- [ ] AJumanjiCharacter posiada dokładnie jeden UJumanjiTraversalComponent.
- [ ] Błędny lub wyłączony link jest odrzucany.
- [ ] Klient może jedynie zażądać przejścia; serwer decyduje o jego rozpoczęciu.
- [ ] Nie można rozpocząć dwóch przejść jednocześnie.
- [ ] Postać nie może rozpocząć przejścia w powietrzu.
- [ ] Stan przejścia replikuje się do drugiego klienta.
- [ ] Ruch postaci jest widoczny dla drugiego klienta.
- [ ] Po zakończeniu MovementMode wraca do poprzedniej wartości.
- [ ] Blokująca kolizja przerywa przejście bez przeniesienia przez przeszkodę.
- [ ] Ability input i ruch nie działają podczas przejścia.

### Każdy typ
- [ ] ContextJump: krzywa ma czytelny łuk i poprawne lądowanie.
- [ ] Climb: kotwice są na dole/górze, a przejście kończy się na dostępnej powierzchni.
- [ ] Zipline: ścieżka jest wolna od kolizji i prowadzi na bezpieczny punkt.
- [ ] Lift: przejście ma poprawne punkty dolny/górny i nie blokuje kapsuły.
- [ ] Link jednokierunkowy odrzuca użycie z niewłaściwego końca.
- [ ] Link dwukierunkowy działa z obu końców.
- [ ] Wyłączony Smart Link znika z użytecznej trasy po aktualizacji danych nawigacji.
- [ ] Dwie postacie mogą niezależnie używać odrębnych linków.

### Prezentacja
- [ ] Blueprint otrzymuje OnTraversalPresentationChanged.
- [ ] Montage jest ładowany asynchronicznie.
- [ ] Animacja nie jest odtwarzana jako potwierdzenie przed decyzją serwera.
- [ ] Mobile HUD pokazuje interakcję tylko w odpowiednim stanie.

## 10. Ograniczenia etapu

Etap dodaje źródłowe klasy C++ i kontrakt authoringu. Nie tworzy:
- mapy .umap ani umieszczonych w niej link actorów;
- Blueprintów BP_TraversalLink_Base lub pochodnych;
- modeli mostów, lin, wind, uchwytów i platform;
- animacji lub plików montage;
- finalnego widgetu interakcji dotykowej;
- Navigation Data Chunks, zbudowanego NavMesh ani testów PIE;
- APK/AAB i wyników profilowania na Androidzie.

W tej wersji animacja jest prezentacją Blueprint, a ruch jest prostą interpolacją serwerową. Zaawansowany root motion, splines dla tyrolek, fizyka platform windy i dedykowane animacje zależą od finalnych zasobów i kolejnego etapu.

## Oficjalna dokumentacja Unreal Engine 5.8

- ANavLinkProxy: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/AIModule/ANavLinkProxy?application_version=5.8
- FNavigationLink: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FNavigationLink?application_version=5.8
- Smart Link / SetSmartLinkEnabled: https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/AI/Navigation/SetSmartLinkEnabled
- Navigation Link workflow: https://dev.epicgames.com/documentation/unreal-engine/overview-of-how-to-modify-the-navigation-mesh-in-unreal-engine

## Następny etap

**Stage 24 — Traversal Animation & Interaction Presentation** is implemented on branch `feature/traversal-animation-presentation-2.111.1`. Its draft PR adds the prompt widget base, presentation manifest and optional Zipline spline path. Final .uasset authoring and PIE/device validation remain separate.
