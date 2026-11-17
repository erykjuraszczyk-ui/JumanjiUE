# JUMANJI 2.111.1 — Stage 27: Emerald Jungle World Encounter Integration

## Cel etapu

Połączyć istniejące systemy eksploracji, traversal, dziewięciu prób Great Spirit, walki GAS i progresji w jeden serwerowo sterowany scenariusz:

**odkrycie obszaru → zaliczenie przejść → dziewięć prób → fale przeciwników → boss → nagroda → zapis profilu.**

Stage 27 rozszerza istniejące World Atlas, Traversal, Adventure Chapter, Combat, PlayerState i ProgressionRewardSubsystem. Nie tworzy drugiego systemu zadań, traversal, walki, walut ani zapisu.

## 1. Nowe elementy runtime

- `EJumanjiWorldEncounterPhase` — fazy Inactive, Trials, Combat, Boss, Reward, Completed i Failed.
- `FJumanjiWorldEncounterState` — jeden zreplikowany snapshot spotkania: EncounterId, faza, numer fali, liczba fal, liczba aktywnych przeciwników i revision.
- `AJumanjiWorldEncounterDirector` — serwerowy koordynator prób, fal, bossa i nagrody.
- `FJumanjiEncounterEnemySpawn` i `FJumanjiEmeraldJungleEncounterWave` — authoring fal przeciwników.
- `AJumanjiEnemyCharacter::OnEnemyDefeated` — serwerowe zdarzenie emitowane raz po przejściu przeciwnika do stanu defeated.
- `AJumanjiGameState` — Begin/Update/Complete/FailWorldEncounter i replikacja snapshotu.
- `AJumanjiPlayerController::RequestStartWorldEncounter` — klient wysyła prośbę; serwer ponownie weryfikuje dystans, świat, profil i wymagania w dyrektorze.
- `FJumanjiWorldEncounterRewardDefinition` — konfiguracja nagrody końcowej.
- `UJumanjiProgressionRewardSubsystem` — walidacja katalogu, sprawdzenie receiptów traversal i idempotentne przyznanie nagrody przez istniejącą transakcję profilu.

Dyrektor spotkania jest aktorem replikowanym i Always Relevant, ponieważ jego referencja jest przesyłana w Server RPC. Należy umieścić go w persistent/always-loaded levelu, a nie w komórce World Partition, która może zostać wyładowana w trakcie spotkania.

## 2. Warunki startu — Frost Gate

Domyślna konfiguracja wymaga, aby profil inicjującego gracza miał:

- odkryty region `REGION_FROST_GATE`;
- odwiedzony landmark `LANDMARK_FROST_GATE`;
- zapisany receipt pierwszego poprawnego ukończenia traversal `EJ_FROST_GATE_LIFT_01`;
- zapisany receipt pierwszego poprawnego ukończenia traversal `EJ_FROST_GATE_JUMP_01`;
- aktywną, serwerowo zweryfikowaną tożsamość profilu.

Receipt jest sprawdzany w `ProcessedTransactionIds` istniejącego profilu. Samo naciśnięcie przycisku, lokalny overlap, rozpoczęcie kursu albo przerwane przejście nie spełnia warunku.

Serwer przyjmuje prośbę przez `AJumanjiPlayerController::RequestStartWorldEncounter`, sprawdza dystans do dyrektora (domyślnie 500 cm) i ponownie sprawdza wszystkie wymagania w `AJumanjiWorldEncounterDirector::StartEncounter`.

## 3. Pętla rozdziału — dziewięć prób

Dyrektor spotkania odwołuje się do istniejącego `AJumanjiAdventureChapterDirector`. Wymaga, aby ChapterId jego konfiguracji dokładnie odpowiadało `EMERALD_JUNGLE_NINE_SEALS`.

Rozdział zachowuje istniejące dziewięć ObjectiveId i istniejące klasy prób Stage 08–10:

| Great Spirit | ObjectiveId | Wymagany postęp |
|---|---|---:|
| I — Korzeni | TRIAL_ROOT_RUNES | 3 |
| II — Przypływu | TRIAL_TIDE_CHANNELS | 3 |
| III — Kamienia | TRIAL_STONE_BALANCE | 3 |
| IV — Burzy | TRIAL_STORM_WINDS | 3 |
| V — Żaru | TRIAL_EMBER_SEQUENCE | 3 |
| VI — Świtu | TRIAL_DAWN_MIRRORS | 3 |
| VII — Cienia | TRIAL_SHADOW_PATTERN | 3 |
| VIII — Życia | TRIAL_LIFE_GARDEN | 3 |
| IX — Mrozu | TRIAL_FROST_GATES | 3 |

Rozdział ukończony jest wyłącznie wtedy, gdy istniejący chapter director potwierdzi ukończenie wszystkich dziewięciu celów. Encounter Director nasłuchuje replikowanego stanu rozdziału na serwerze. UI nie może bezpośrednio przełączyć fazy spotkania.

## 4. Fale przeciwników i boss

Po ukończeniu rozdziału dyrektor uruchamia skonfigurowane fale.

W każdej pozycji `FJumanjiEncounterEnemySpawn` należy ustawić:

- EnemyClass — klasa potomna `AJumanjiEnemyCharacter`;
- SpawnPoint — aktor-znacznik położenia w tym samym świecie;
- Count — liczba 1–8;
- SpreadRadius — rozstawienie 0–500 cm.

Limity walidacji:

- 1–16 fal;
- unikalny, niepusty WaveId dla każdej fali;
- przynajmniej jeden wpis spawn w każdej fali;
- maksymalnie 64 aktory przeciwników w całym spotkaniu;
- każdy spawn point musi istnieć w tym samym UWorld;
- klasy przeciwników muszą dziedziczyć po `AJumanjiEnemyCharacter`.

Serwer tworzy przeciwników. Nie przyjmuje od klienta klasy, liczby, lokalizacji ani wyniku trafienia. Po pokonaniu ostatniego przeciwnika z fali uruchamiana jest kolejna. Po zakończeniu wszystkich fal dyrektor tworzy `BossClass` — potomka `AJumanjiBossCharacter` — w `BossSpawnPoint`.

Boss zachowuje system trzech faz zdrowia ze Stage 06. Jego śmierć jest potwierdzana przez to samo serwerowe zdarzenie `OnEnemyDefeated`.

## 5. Fazy replikowane do HUD

`AJumanjiGameState::WorldEncounterState` jest pojedynczym replikowanym snapshotem dla wszystkich klientów.

| Faza | Znaczenie |
|---|---|
| Inactive | Spotkanie nie zostało uruchomione |
| Trials | Aktywny rozdział Dziewięć Pieczęci |
| Combat | Trwa wskazana fala przeciwników |
| Boss | Walka z bossem |
| Reward | Serwer przetwarza nagrody |
| Completed | Spotkanie zakończone |
| Failed | Spotkanie przerwane błędem konfiguracji/spawnu |

UI odczytuje `GetWorldEncounterState()` i nasłuchuje `OnWorldEncounterChanged`. Nie zapisuje fazy i nie zmienia liczby przeciwników. Aktualny kod udostępnia Blueprint eventy na Encounter Directorze: `BP_OnEncounterPhaseChanged`, `BP_OnEncounterCompleted` i `BP_OnEncounterFailed`.

Stan Failed jest terminalny dla danego uruchomienia mapy. Nie ma automatycznego restartu ukończonych/aktywowanych prób w tej wersji.

## 6. Nagroda końcowa i idempotencja

Domyślny wiersz katalogu dla `ENCOUNTER_EMERALD_JUNGLE_CITADEL`:

| Nagroda | Ilość |
|---|---:|
| XP konta | 500 |
| XP aktywnego Great Spirit | 300 |
| Monety Odkrywców | 350 |
| Pamięć Ducha | 2 |

Każdy połączony profil, który ma zapisane ukończenie wymaganego rozdziału, otrzymuje osobną transakcję. Identyfikator jest stabilny dla pary profil + encounter:

`ENC_<ProfileHash>_<EncounterId>`

Ponowione wywołanie nie przyznaje pakietu drugi raz. Saldo, itemy i receipt przechodzą przez istniejące `AJumanjiPlayerState::ApplyProfileTransaction`. Klient nie wysyła kwoty nagrody ani TransactionId.

### DataTable

W Unreal Editor utwórz:

- folder: `/Game/Progression/Data`;
- DataTable: `DT_WorldEncounterRewards`;
- Row Structure: `FJumanjiWorldEncounterRewardDefinition`;
- asset path: `/Game/Progression/Data/DT_WorldEncounterRewards.DT_WorldEncounterRewards`.

Kolumny:

- EncounterId;
- RequiredChapterId;
- AccountExperienceReward;
- ActiveCharacterExperienceReward;
- CurrencyId;
- CurrencyAmount;
- ItemId;
- ItemAmount.

Dodatni CurrencyAmount wymaga dozwolonego CurrencyId. Dodatni ItemAmount wymaga znanego ItemId materiału. Gdy ilość wynosi zero, odpowiadające ID musi być puste. Niepoprawne lub zduplikowane wiersze są pomijane. Brak tabeli pozostawia natywną nagrodę startową.

Źródłowy manifest: `Data/Progression/WorldEncounterRewards.csv`.

## 7. Unreal Editor — konfiguracja spotkania

1. Skompiluj źródła w Unreal Engine 5.8.
2. Utwórz Blueprint child `BP_EmeraldJungleWorldEncounter` klasy `AJumanjiWorldEncounterDirector`.
3. Umieść dyrektora w persistent/always-loaded levelu; nie w streamowanej komórce World Partition.
4. Wskaż umieszczony w tym samym świecie `AJumanjiAdventureChapterDirector` rozdziału `EMERALD_JUNGLE_NINE_SEALS`.
5. Potwierdź, że jego TrialActors zawiera dziewięć skonfigurowanych prób, każdą dokładnie raz, a cele mają RequiredProgress 3.
6. Wypełnij tablicę Waves unikalnymi WaveId i spawn entries.
7. Utwórz/ustaw Blueprint children przeciwników dziedziczące po `AJumanjiEnemyCharacter`; przypisz im mesh, animację, initialization GE, atak GAS i AI.
8. Umieść spawn point actors w Persistent Level albo w obszarze gwarantowanie załadowanym na serwerze w chwili startu.
9. Utwórz Blueprint child bossa po `AJumanjiBossCharacter`, ustaw mesh, animacje, pasek zdrowia i fazowe ataki.
10. Umieść BossSpawnPoint; sprawdź collision i NavMesh w arenie.
11. Utwórz DataTable `DT_WorldEncounterRewards` opcjonalnie, albo pozostaw natywną nagrodę.
12. W PlayerController Blueprint przypnij przycisk interakcji do `RequestStartWorldEncounter` z referencją do dyrektora.
13. W HUD odczytuj GameState.WorldEncounterState i odświeżaj fazę, fale, liczbę przeciwników i status ukończenia.
14. Uruchom próbę w trybie serwer + co najmniej jeden klient.

## 8. Walidacja źródeł i CI

Uruchom:

```bash
python Scripts/validate_emerald_jungle_encounter_rewards.py
python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_encounter_rewards.py" -v
```

GitHub Actions workflow:

`.github/workflows/emerald-jungle-world-encounter-validation.yml`

Walidator sprawdza jeden pakiet nagrody, jego stabilne ID spotkania i rozdziału, dozwolone waluty/materiały, limity XP/ilości oraz obowiązkowe Notes. PASS potwierdza spójność manifestu CSV, a nie kompilację, import DataTable .uasset, spawn aktorów, PIE ani test Android.

## 9. Kryteria akceptacji

- [ ] C++ kompiluje się w Unreal Engine 5.8 z poprawnym UHT.
- [ ] Dynamic delegate handlers w encounter directorze są poprawnie widoczne dla refleksji Unreal.
- [ ] Dyrektor spotkania jest replikowany i Always Relevant, a jego referencja dociera do server RPC z klienta.
- [ ] Klient dalej niż MaxWorldEncounterStartDistance nie może wystartować spotkania.
- [ ] Niewłaściwy świat, pusty profil, brak regionu, landmarku lub jednego z traversal receiptów odrzuca start.
- [ ] Encouter Director odrzuca rozdział o innym ChapterId.
- [ ] Chapter Director odrzuca brakujące/duplikowane próby i cele o innym RequiredProgress.
- [ ] Spotkanie wchodzi do Trials i replikowany GameState przekazuje nową rewizję klientom.
- [ ] Chapter ukończy się dopiero po wszystkich dziewięciu próbach 3/3.
- [ ] Przeciwnicy pojawiają się wyłącznie przez serwer w skonfigurowanych falach.
- [ ] Błędna klasa, brak spawn point, pusty wave albo nieudany spawn przełącza encounter w Failed.
- [ ] Pokonanie przeciwnika zmniejsza ActiveEnemyCount dokładnie raz.
- [ ] Po ostatniej fali pojawia się boss, a jego fazy replikują się jak w Stage 06.
- [ ] Boss defeat przełącza spotkanie w Reward, a potem Completed.
- [ ] Każdy uprawniony profil dostaje nagrodę najwyżej raz.
- [ ] Ponowione wywołanie nagrody rozpoznaje zapisany transaction receipt.
- [ ] UI czyta GameState i nie może sam ustawić fazy ani nagrody.
- [ ] Test dwóch klientów potwierdza wspólny stan rozdziału, fal i bossa.

## 10. Ograniczenia

- encounter director, chapter director, próby, spawn pointy, wrogowie, boss i arena nie mają jeszcze zapisanych map/Blueprint .uasset w repozytorium;
- Stage 27 dostarcza runtime i authoring contract, nie finalne modele, rig, animacje, telegraph VFX, audio, boss attack patterns ani UI;
- spawn points muszą być dostępne na serwerze; obecny dyrektor nie implementuje asynchronicznego streamingu komórek podczas pojedynczej fali;
- stan spotkania jest sesyjny i replikowany przez GameState; nie jest trwale zapisywany do profilu;
- lokalny SaveGame i profil nadal są single-server prototypem, bez cross-shard transactions;
- nagroda jest rozdzielana do połączonych profili przy końcowym zdarzeniu, jeśli ich PlayerState ma ukończony wymagany rozdział;
- nie ma automatycznego wznowienia ani ponownego uruchomienia nieudanego/ukończonego spotkania;
- nie wykonano kompilacji UE 5.8, PIE multiplayer ani testu fizycznego urządzenia Android podczas tej operacji.

## 11. Weryfikacja

Pliki zapisano na osobnej gałęzi i odczytano ponownie przez GitHub API. Walidator CSV i testy jednostkowe są dodane do repozytorium/CI. Do momentu uruchomienia przez GitHub Actions nie deklaruje się ich wyniku PASS. Nie wykonano kompilacji Unreal Engine 5.8, testu PIE ani builda APK/AAB.

## Następny etap

**Stage 28 — Emerald Jungle Encounter Presentation & Multiplayer QA**: HUD spotkania, pasek zdrowia bossa, telegraphy ataków, komunikaty faz, feedback audio/VFX, testy serwer/klient i profilowanie Android.
