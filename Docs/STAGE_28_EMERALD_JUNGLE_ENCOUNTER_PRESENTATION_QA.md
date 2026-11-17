# JUMANJI 2.111.1 — Stage 28: Emerald Jungle Encounter Presentation & Multiplayer QA

## Cel etapu

Stage 28 rozwija istniejący World Encounter ze Stage 27. Nie tworzy nowego systemu walki ani nowych nagród. Dodaje natywną warstwę prezentacji i synchronizacji telegraphów bossa:

1. replikowaną referencję do bossa w snapshotcie spotkania;
2. serwerowy cykl ostrzeżenia i wykonania ataku bossa;
3. replikowaną geometrię telegraphu: typ, pozycja, kierunek, promień, długość, szerokość i czas;
4. bazę UMG dla HUD-u spotkania;
5. źródłową matrycę dziewięciu przykładowych telegraphów dla trzech faz bossa;
6. walidator CSV, testy jednostkowe i workflow GitHub Actions.

To jest kodowa i danych wejściowych baza prezentacji. Stage 28 nie tworzy finalnych Blueprintów, VFX ani gotowej mapy i nie jest równoznaczny z ukończonym bossem.

## 1. Replikowany stan bossa w spotkaniu

Plik `FJumanjiWorldEncounterState` otrzymuje pole `BossActor`. Serwer może je ustawić przez:

`AJumanjiGameState::SetWorldEncounterBoss(EncounterId, BossActor)`

Zasady:
- aktor musi należeć do tego samego świata;
- niepusty boss może być przypisany tylko w fazie Boss;
- klient odczytuje referencję ze snapshotu GameState;
- HUD nie przydziela ani nie usuwa referencji i nie steruje fazami spotkania;
- przy porażce dyrektor czyści referencję przed ustawieniem fazy Failed;
- po pokonaniu bossa referencja może pozostać do chwili usunięcia aktora, aby HUD pokazał końcowe dane zdrowia.

## 2. Cykl telegraphu bossa

Klasa `AJumanjiBossCharacter` otrzymuje:
- `EJumanjiBossTelegraphPattern`: Radial, Line, Cone, Focused;
- `EJumanjiBossTelegraphStage`: None, Warning, Executing;
- `FJumanjiBossTelegraphState`: AttackId, Pattern, Stage, Origin, Direction, ServerStartTime, WarningDuration, ActiveDuration, Radius, Length, Width, Revision;
- `BeginBossTelegraph(...)` i `CancelBossTelegraph()`;
- replikowany `BossTelegraphState` i delegat `OnBossTelegraphChanged`;
- Blueprint eventy do prezentacji i uruchomienia zatwierdzonego zachowania bossa.

### Przepływ

1. Serwerowa logika bossa wybiera AttackId z autoryzowanego zestawu.
2. Serwer wywołuje `BeginBossTelegraph` z kształtem, kierunkiem i czasami.
3. Stan Warning jest replikowany; klienci wyświetlają ostrzeżenie.
4. Po upływie WarningDuration stan przechodzi do Executing.
5. Serwer wywołuje `BP_OnBossTelegraphExecutionRequested(AttackId, Pattern)`.
6. Blueprint bossa uruchamia rzeczywisty, serwerowo autorytatywny atak/GAS ability i właściwy Gameplay Effect.
7. Po ActiveDuration stan jest czyszczony do None.

Walidacja czasów i kształtu:
- WarningDuration: 0.25–8.0 s;
- ActiveDuration: 0.1–6.0 s;
- Radius: 25–2500 cm;
- Length: 0–4000 cm;
- Width: 0–1500 cm.

Telegraph sam z siebie nie zadaje obrażeń i nie wykonuje ataku. To sygnał ostrzegawczy, który musi odpowiadać temu samemu AttackId i geometrii użytej później przez serwerową logikę ataku.

### Gameplay Tags

- `State.Boss.Telegraph.Warning`;
- `State.Boss.Telegraph.Active`;
- istniejące `State.Boss.Phase.Two` i `State.Boss.Phase.Three`.

Tagi telegraphu są aktualizowane po stronie bossa i replikowane przez ASC. Zmiana fazy lub pokonanie bossa anuluje bieżący telegraph i timery.

## 3. Matryca telegraphów

Źródłowy plik:

`Data/Encounter/EmeraldJungleBossTelegraphs.csv`

Zawiera dziewięć przykładowych pozycji:
- faza 1: Radial, Line, Cone;
- faza 2: Radial, Line, Focused;
- faza 3: Radial, Line, Cone.

AttackId jest stabilnym identyfikatorem treści. Nie zmieniaj go po przypisaniu do Blueprintów. Ten CSV jest manifestem dla projektantów i walidacji; nie jest automatycznie importowany do runtime DataTable.

### Znaczenie geometrii

| Pattern | Interpretacja |
|---|---|
| Radial | obszar wokół Origin; Radius określa promień |
| Line | korytarz wzdłuż Direction; Length i Width określają rozmiar |
| Cone | sektor w kierunku Direction; Radius/Length/Width są parametrami prezentacji |
| Focused | ostrzeżenie skupione na celu/małym obszarze; wybór celu musi zostać zatwierdzony przez serwer |

Klasy telegraphu opisują prezentację, a nie pełną kolizję. Rzeczywisty hit test/Gameplay Effect musi korzystać z tego samego położenia, kierunku i wymiarów.

## 4. Unreal Editor — Blueprint bossa

Po kompilacji projektu w UE 5.8:

1. Otwórz `BP_BoardGuardian` lub Blueprint child właściwego `AJumanjiBossCharacter`.
2. W zdarzeniu rozpoczęcia wzorca, tylko na instancji serwerowej, wywołaj `BeginBossTelegraph`.
3. Nadaj unikalny AttackId z manifestu.
4. W `BP_OnBossTelegraphChanged` pokaż, zaktualizuj albo usuń wskaźnik ostrzegawczy według Stage, Pattern, Origin, Direction, Radius, Length i Width.
5. W `BP_OnBossTelegraphExecutionRequested` uruchom rzeczywisty atak. Używaj istniejącego GAS, serwerowego trace/overlap i Gameplay Effectów; nie przyjmuj wyniku kolizji z klienta jako źródła prawdy.
6. W `BP_OnBossPhaseChanged` zmień dopuszczalną listę AttackId i rytm fazy.
7. Wywołuj `CancelBossTelegraph` przy przerwaniu wzorca, zmianie taktyki lub wycofaniu ataku.
8. VFX/audio przypinaj do Gameplay Cue albo kontrolowanych komponentów Blueprint. Zadbaj, aby sygnał ostrzegawczy był czytelny na urządzeniu mobilnym.

### Przykład zachowania Blueprint

- Phase 1: naprzemiennie radial pulse i line lance.
- Phase 2: większy radial burst, linia i focused mark.
- Phase 3: szeroki radial surge, długi korytarz i cone sweep.

To konfiguracja demonstracyjna. Nie jest jeszcze gotową, zbalansowaną AI bossa ani implementacją obrażeń.

## 5. Encounter HUD — Widget Blueprint

Utwórz Widget Blueprint:

- nazwa: `WBP_EmeraldJungleEncounterHUD`;
- parent class: `UJumanjiWorldEncounterHUDWidget`;
- folder: `/Game/UI/Encounter`.

Na `BP_JumanjiPlayerController` przypisz klasę do `WorldEncounterHUDWidgetClass`. Lokalny PlayerController tworzy widget na BeginPlay.

### Dane dostępne w Blueprint

- `GetEncounterState`: EncounterId, Phase, CurrentWave, TotalWaves, ActiveEnemyCount, Revision i BossActor;
- `GetEncounterStatusText`: lokalizowany status fazy;
- `GetCompletedTrialCount` / `GetTotalTrialCount`;
- `GetBossHealth` / `GetBossMaxHealth` / `GetBossHealthPercent`;
- `GetBossPhase`;
- `GetBossTelegraphState` / `IsBossTelegraphActive`;
- `GetTelegraphTimeRemaining` / `GetTelegraphRemainingPercent`.

Implementuj:
- `OnEncounterHUDPresentationChanged`: status, licznik fal, liczba przeciwników, zdrowie i faza bossa;
- `OnEncounterPhaseCue`: krótki komunikat/audio po zmianie fazy spotkania; zignoruj Inactive, jeśli nie ma pokazywać komunikatu;\n- `OnBossPhaseCue`: jednorazowy komunikat/audio przy przejściu bossa do fazy 1, 2 lub 3;
- `OnBossTelegraphPresentationChanged`: pokaż/odśwież/usuń wizualny telegraph.

Widget tylko odczytuje dane. Nie wolno z niego zmieniać fazy spotkania, HP bossa, liczby przeciwników, nagrody ani stanu telegraphu.

### Synchronizacja odliczania

Telegraph przechowuje serwerowy czas rozpoczęcia etapu. HUD odczytuje czas przez `AGameStateBase::GetServerWorldTimeSeconds()`, a nie z lokalnego zegara urządzenia. Pozwala to klientom liczyć pozostały czas według wspólnego czasu symulacji.

Odliczanie UI jest tylko prezentacją. Serwerowy timer bossa decyduje, kiedy ostrzeżenie się kończy i kiedy uruchomić event wykonania.

## 6. Multiplayer QA

Uruchom serwer i przynajmniej dwóch klientów.

| Test | Oczekiwany wynik |
|---|---|
| Start spotkania | wszyscy klienci widzą identyczną fazę Trials |
| Dziewięć prób | status/licznik postępu aktualizuje się po replikowanym snapshotcie |
| Fale | wszyscy widzą ten sam CurrentWave, TotalWaves i ActiveEnemyCount |
| Spawn bossa | BossActor dociera do klientów przez GameState |
| Zdrowie bossa | pasek HUD wszystkich klientów reaguje na replikację atrybutu Health |
| Faza bossa | wszyscy widzą Phase 1 → 2 → 3 |
| Ostrzeżenie | AttackId, kształt, kierunek i czas są spójne na wszystkich klientach |
| Wykonanie | atak uruchamia się tylko z serwerowego eventu Blueprint/GAS |
| Zmiana fazy | aktywny telegraph zostaje anulowany, timery i tagi są czyszczone |
| Pokonanie bossa | telegraph zostaje anulowany, stan encounter przechodzi do Reward/Completed |
| Porażka konfiguracji | referencja do bossa jest czyszczona, stan przechodzi do Failed |
| Join-in-progress | nowy klient po replikacji otrzymuje bieżący stan encounter i telegraphu |

Zweryfikuj również, że HUD nie może wywołać żadnej autorytatywnej zmiany stanu.

## 7. Mobile profiling — run sheet

Nie wpisuj wyników, których nie zmierzono. Dla każdego builda zapisz:
- urządzenie, Android version, rozdzielczość i tryb renderowania;
- build configuration (Development/Shipping);
- średni FPS oraz 1% low;
- GPU/CPU frame time;
- pamięć RAM procesu;
- liczbę aktywnych komponentów Niagara i ich overdraw;
- koszt UMG, liczbę widgetów i częstotliwość odświeżania;
- czas replikacji zmiany fazy/telegraphu przy normalnym i słabym łączu.

Domyślny refresh HUD to 0.1 s. W razie kosztu UI zwiększ go; animacja odliczania może lokalnie płynnie interpolować pomiędzy snapshotami, ale nie może zastępować serwerowej autorytatywności.

## 8. Walidator i CI

Uruchom:

```bash
python Scripts/validate_emerald_jungle_boss_telegraphs.py
python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_boss_telegraphs.py" -v
```

GitHub Actions:
`.github/workflows/emerald-jungle-encounter-presentation-validation.yml`

Walidator sprawdza:
- wymagane kolumny i niepuste pola;
- unikalne AttackId;
- dozwolone Pattern;
- obecność faz 1, 2 i 3;
- ograniczenia czasów i wymiarów;
- poprawny format identyfikatorów.

CSV PASS nie potwierdza, że Blueprint ataku, telegraph VFX, DataTable .uasset, animacje, dźwięk lub runtime są poprawne.

## 9. Ograniczenia etapu

- Nie utworzono `.uasset` dla WBP_EmeraldJungleEncounterHUD.
- Nie utworzono końcowych modeli, rigów, montages, Niagara, Sound Waves ani cue assets.
- Manifest CSV nie jest automatycznie importowany jako runtime DataTable.
- Rzeczywiste wzorce ataków i ich obrażenia muszą zostać skonfigurowane w Blueprint/GAS i sprawdzone przez serwer.
- Klasyfikacja i logika walki pozostają ograniczone do istniejącego prototypu PvE.
- Nie wykonano kompilacji Unreal Engine 5.8, UHT, PIE multiplayer ani testu fizycznego urządzenia Android w tym środowisku.
- Nie przygotowano APK/AAB.

## 10. Kryteria akceptacji

- [ ] Projekt kompiluje się w UE 5.8 bez błędów UHT/C++.
- [ ] BossTelegraphState replikuje wszystkie wymagane pola.
- [ ] Serwer odrzuca błędne AttackId, timingi i wymiary.
- [ ] Warning przechodzi w Executing po upływie WarningDuration.
- [ ] Cancel, zmiana fazy i defeat usuwają timery oraz tagi.
- [ ] GameState publikuje BossActor wyłącznie w fazie Boss i czyści go przy Failed.
- [ ] Lokalny PlayerController tworzy skonfigurowany Encounter HUD.
- [ ] HUD pokazuje Trials, Combat, Boss, Reward, Completed i Failed.
- [ ] Boss Health i Phase aktualizują się na dwóch klientach.
- [ ] Telegraph countdown korzysta ze wspólnego czasu serwera.
- [ ] HUD nie wywołuje obrażeń ani nie zmienia Encounter state.
- [ ] Wszystkie dziewięć rekordów CSV przechodzi walidator.
- [ ] Testy jednostkowe manifestu przechodzą.
- [ ] Blueprint uruchamia prawdziwy atak tylko z serwerowego eventu.
- [ ] Mobilny run sheet został uzupełniony rzeczywistymi pomiarami przed wydaniem.

## Następny etap

**Stage 29 — Encounter Replay & Recovery**: bezpieczny reset spotkania po Failed, walidowana ponowna próba, porządkowanie aktorów i prób, reset sesyjnego stanu encounter bez duplikowania nagród i bez cofania trwałej progresji profilu.
