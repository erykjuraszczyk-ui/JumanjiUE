# JUMANJI 2.111.1 — Stage 34: Safe-Point Snapshot Builder & Restore Staging

## 1. Cel etapu

Połączyć istniejący Stage 33 backend checkpointów z rzeczywistym, serwerowym stanem encounteru w Unreal Engine 5.8 — bez serializacji żywych aktorów i bez częściowego odtwarzania świata.

Stage 34 dodaje:
- serwerowy builder snapshotów zgodnych ze schematem Stage 32 v1;
- automatyczny zapis wyłącznie w czterech zatwierdzonych rodzajach safe point;
- dzierżawę backendu, fencing epoch, okresowe renewal i sekwencjonowanie CAS;
- pseudonimowe klucze uczestników zamiast surowych ProfileId;
- kontrolę wersji serwera, rulesetu, zawartości świata i konfiguracji encounteru;
- odczyt checkpointu i pełny preflight do prywatnego obiektu restore candidate;
- brak mutacji świata przed końcowym przypisaniem zweryfikowanego kandydata.

Stage 34 nie uruchamia jeszcze odtworzenia encounteru. Nie spawnuje przeciwników, nie ustawia zdrowia bossa, nie odtwarza prób, nie przenosi graczy i nie publikuje częściowego GameState. Te operacje wymagają Stage 35 — kontrolowanego przepływu materializacji i publikacji.

## 2. Architektura zaufania

    Unreal dedicated/listen server
      UJumanjiEncounterCheckpointRuntimeSubsystem
        Safe-point builder: GameState + ChapterDirector + Boss
        opaque participant keys + content/version checks
        private restore candidate / preflight
        UJumanjiEncounterCheckpointBackendSubsystem
          HTTPS + server credential
            Stage 33 trusted checkpoint service
              lease + fencing epoch
              sequence compare-and-swap
              SQLite WAL (single-host prototype)
              Ed25519 sign + verify / RFC 8785

Zasady:
1. Wszystkie decyzje o stanie gry wykonuje serwer.
2. Klient PC/Android nie otrzymuje endpointu ani tokenu backendu.
3. Lease i fencing epoch wyznaczają jeden aktywny proces zapisujący dany EncounterId.
4. Nieznany wynik zapisu nie jest ponawiany z tym samym sequence. Najpierw trzeba uzgodnić aktualny head checkpointu.
5. Snapshot obejmuje wyłącznie logiczne identyfikatory i liczniki z kontraktu v1.
6. Odczyt HTTP 200 pochodzi z backendu, który przed odpowiedzią ponownie sprawdza payload, TTL i podpis Ed25519. Unreal wykonuje dodatkowy preflight semantyczny i kompatybilnościowy; nie implementuje osobnego weryfikatora Ed25519.
7. Restore candidate pozostaje prywatny, dopóki wszystkie kontrole nie przejdą.

## 3. Nowe pliki i integracje

Nowe:
- Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h
- Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp
- Docs/STAGE_34_ENCOUNTER_SAFEPOINT_SNAPSHOT_RESTORE_STAGING.md

Zmodyfikowane:
- WorldEncounterSessionSubsystem — ephemeral encounter SessionId;
- WorldEncounterDirector — safe-point hooks, lease lifecycle i event listeners;
- AdventureChapterDirector — objective/trial ID allowlists, completed IDs i mapping;
- TrialActor — opcjonalny CheckpointTrialId z fallbackiem do ObjectiveId;
- BossCharacter — BossPhaseRevision, phase delegate i health fraction;
- ProgressionRewardSubsystem — dostęp do istniejącego, stabilnego transaction receipt.

## 4. Cztery dozwolone safe pointy

| Faza encounteru | Kind | Warunek zapisu |
|---|---|---|
| Trials | TrialBoundary | Żadna próba nie jest InProgress; chapter i Living Board nadal są Active; zapis obejmuje aktualny completed objective/trial set |
| Combat | WaveBoundary | Bieżąca fala ma numer 1..N i ActiveEnemyCount = 0 |
| Boss | BossPhaseBoundary | Boss żyje, faza 1..3, brak aktywnego telegraphu, revision fazy jest dodatnia |
| Reward | RewardPending | Boss pokonany, encounter jest w fazie Reward, brak przeciwników, istnieje dokładny transaction receipt |

Zapis nie jest wykonywany w środku ataku bossa/telegraphu, podczas aktywnej fali, w połowie interakcji próby, podczas traversal/windy, na żądanie klienta, przy braku lease ani przy niezgodnej wersji świata.

### 4.1 TrialBoundary

WorldEncounterDirector nasłuchuje OnAdventureObjectivesChanged i dopuszcza zapis tylko wtedy, gdy:
- działa serwer;
- encounter jest aktywny i fala nie wystartowała;
- ChapterId jest właściwy, a chapter pozostaje Active;
- wszystkie aktory prób są prawidłowe;
- żadna próba nie jest w stanie InProgress.

W SubmitOption logiczny TrialState/CurrentStepIndex finalnego kroku jest ustawiany przed AdvanceBoardAdventureObjective; przy odrzuceniu objective następuje rollback. Dzięki temu listener widzi granicę Completed, a nie stan pośredni.

Stage 32 ma osobne tablice completedObjectiveIds i completedTrialIds. AJumanjiTrialActor ma opcjonalny CheckpointTrialId. Jeżeli Blueprint nie przypisze nowej wartości, używany jest dotychczasowy ObjectiveId. To zachowuje kompatybilność ze starszą zawartością. ChapterDirector sprawdza mapowanie TrialId → ObjectiveId.

### 4.2 WaveBoundary

Po pokonaniu ostatniego przeciwnika fali:
1. serwer usuwa go z ActiveEnemies;
2. publikuje ActiveEnemyCount = 0;
3. wywołuje CaptureSafePoint(WaveBoundary);
4. dopiero potem uruchamia kolejną falę albo bossa.

currentWave jest numerem zakończonej fali (1-based). totalWaves musi dokładnie pasować do aktualnie skonfigurowanej liczby fal.

### 4.3 BossPhaseBoundary

BossCharacter publikuje OnBossPhaseChanged(NewPhase, PhaseRevision). Encounter Director zapisuje po zmianie do fazy 2 lub 3, gdy telegraph jest zakończony.

Snapshot bossa zawiera wyłącznie:
- stable definitionId;
- stable phaseId (BOSS_PHASE_01–BOSS_PHASE_03);
- healthFraction w zakresie 0..1;
- phaseRevision.

Nie zapisuje transformu, Actor reference, ASC handle, aktywnych ability tasków, targetów AI, blackboarda ani timerów.

### 4.4 RewardPending

Przed wywołaniem istniejącego GrantWorldEncounterCompletionReward dla pojedynczego trwałego uczestnika Director pobiera transaction receipt przez GetWorldEncounterRewardTransactionId i próbuje zapisać status PendingLedgerCheck.

Kontrakt v1 ma jedno reward.transactionId. Dlatego Stage 34 pomija RewardPending, jeśli encounter ma więcej niż jednego trwałego uczestnika. Pozostałe safe pointy mogą zapisać do 64 uczestników. Grupowa atomowa rekonsyliacja nagród wymaga rozszerzenia kontraktu i backendu.

PendingLedgerCheck nie oznacza, że nagrodę należy wypłacić. Po odzyskaniu trzeba sprawdzić istniejący ledger transakcji profilu. Stage 34 nie przyznaje nagród podczas preflight.

## 5. Payload v1

Snapshot jest zgodny z Data/Encounter/EncounterCheckpoint.schema.json.

| Sekcja | Zawartość |
|---|---|
| Header | schemaVersion, checkpointId, EncounterId, SessionId, sequence, created/expires UTC |
| Authority | serverInstanceId, shardId, fencing epoch, serverBuildId, rulesetVersion, worldContentVersion |
| Safe point | phase, kind, encounterRevision |
| Chapter | ChapterId, BoardEventId, chapter revision, completed objective IDs, completed trial IDs |
| Combat | currentWave, totalWaves; opcjonalnie logiczny boss snapshot |
| Deterministic | encounterSeed, spawnSetId |
| Participants | opaque participantKey i profileRevision |
| Reward | tylko w fazie Reward: rewardDefinitionId, transactionId, PendingLedgerCheck |

Zabronione dane:
- żywe UObject/Actor reference, NetGUID, soft/object path;
- transform, location, velocity, rotacja i fizyka;
- AI blackboard, aktywne AbilityTask/latent action i timery;
- surowy ProfileId, tokeny, sekrety, klucze prywatne;
- wallet balance, inventory, ekwipunek i surowe payloady klienta.

### 5.1 Opaque participantKey

Serwer tworzy pseudonimowy klucz pk_... z sekretnego JUMANJI_CHECKPOINT_PARTICIPANT_SALT oraz PersistentProfileId. W payload trafia wyłącznie klucz pochodny. Salt:
- musi mieć co najmniej 32 znaki;
- jest ustawiany tylko w środowisku procesu serwera;
- nie może znaleźć się w repozytorium, APK, logach ani DefaultGame.ini;
- musi być taki sam na serwerach, które mają rozpoznawać te same profile;
- wymaga strategii rotacji i wersjonowania w produkcji.

Stage 34 nie udostępnia jeszcze backendowego mapowania participantKey na uwierzytelniony profil ani porównania profileRevision z live profile service. To jest bramka Stage 35.

### 5.2 Czas życia

Builder używa 30 minut TTL. Backend/schema dopuszczają najwyżej 24 godziny. Timestampy są generowane jako UTC ISO-8601. Backend ponownie sprawdza TTL względem zaufanego czasu usługi.

## 6. Lease, fencing i sequence

UJumanjiEncounterCheckpointRuntimeSubsystem:
- zaczyna lease po utworzeniu UJumanjiWorldEncounterSessionSubsystem SessionId;
- prosi backend o TTL 120 s;
- odnawia lease co 60 s;
- po nieudanym renew blokuje kolejne zapisy;
- aktualizuje LastCommittedSequence tylko po HTTP 200;
- wysyła expectedSequence równe ostatniej zatwierdzonej sekwencji;
- nie ponawia niejednoznacznego zapisu z tą samą sekwencją;
- zwalnia lease po stanie terminalnym, czekając na zakończenie rozpoczętego zapisu/odczytu.

Przy nowym encounterze sequence startuje od 1. Istniejącego checkpointu nie wolno nadpisywać przez zwykły StartEncounter. Odtworzenie istniejącego checkpointu musi przejść oddzielną procedurę BeginRestoreStaging, a następnie Stage 35.

## 7. Konfiguracja serwera

Stage 33 adapter:

    [/Script/JumanjiUE.JumanjiEncounterCheckpointBackendSubsystem]
    ServiceBaseUrl=http://127.0.0.1:8787
    RequestTimeoutSeconds=8.0

Na serwerze Unreal ustaw:
- JUMANJI_CHECKPOINT_SERVER_TOKEN
- JUMANJI_SERVER_INSTANCE_ID
- JUMANJI_SHARD_ID
- JUMANJI_SERVER_BUILD_ID
- JUMANJI_RULESET_VERSION
- JUMANJI_WORLD_CONTENT_VERSION
- JUMANJI_CHECKPOINT_PARTICIPANT_SALT

Wszystkie wartości są odczytywane z environment procesu. Adapter tokenu nie może trafić do klienta.

Na procesie backendu Stage 33 osobno ustaw:
- JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE
- JUMANJI_CHECKPOINT_KEY_ID
- JUMANJI_CHECKPOINT_SERVER_TOKEN
- JUMANJI_CHECKPOINT_DB
- JUMANJI_CHECKPOINT_BIND_HOST
- JUMANJI_CHECKPOINT_BIND_PORT

Klucz prywatny Ed25519 istnieje tylko w procesie backendu. Stage 34 Unreal nie przechowuje klucza prywatnego i nie podpisuje danych.

## 8. Restore staging — co dokładnie robi

BeginRestoreStaging(EncounterDirector):
1. wymaga serwera i prawidłowego EncounterDirector;
2. odmawia pracy, gdy istnieje aktywny lease/zapis/odczyt;
3. wymaga GameState w fazie Inactive albo Failed;
4. przejmuje nowy lease i fencing epoch;
5. odczytuje checkpoint przez adapter Stage 33;
6. otrzymuje envelope, który backend wcześniej sprawdził kryptograficznie;
7. sprawdza strukturę, wersję schema, UUID, daty TTL i EncounterId;
8. wymaga poprzedniego authorityEpoch mniejszego od nowego epoch i zgodności shard/build/ruleset/world content;
9. waliduje ChapterId, BoardEventId, spawn set, liczbę fal, fazę, safe point i allowlist objective/trial IDs;
10. waliduje boss definition/phase/healthFraction/revision;
11. waliduje format i unikalność participantKey oraz nieujemne profileRevision;
12. waliduje RewardPending, stable transactionId, status PendingLedgerCheck i limit jednego uczestnika dla reward phase;
13. dopiero wtedy podmienia prywatny StagedRestoreCandidate jednym przypisaniem.

Niepowodzenie dowolnego kroku:
- nie zmienia GameState;
- nie spawnuje lub nie niszczy aktorów;
- nie ustawia zdrowia ani fazy bossa;
- nie przyznaje nagród;
- czyści staging i zwalnia lease.

### 8.1 Co jeszcze nie jest restore

Stage 34 nie wykonuje:
- powiązania participantKey z aktualnie zalogowanymi uczestnikami;
- porównania zapisanych profileRevision z autorytatywnym profile backendem;
- ledger lookup dla PendingLedgerCheck;
- odtworzenia trial/chapter runtime state;
- deterministycznego odtworzenia fal/AI/bossa;
- ponownego przypisania graczy do encounteru;
- atomowej publikacji GameState po odtworzeniu.

Te elementy tworzą Stage 35. GetStagedRestoreCandidate jest aktualnie wejściem dla zaufanego kodu serwera, nie API klienta/Blueprint.

## 9. Testy acceptance

### Snapshot builder — Trials
- [ ] Uruchom rozdział dziewięciu prób.
- [ ] Ukończ pierwszą próbę; potwierdź checkpoint z jednym completed objective/trial.
- [ ] Potwierdź, że pozostałe triale Available, ale nie InProgress, nie blokują granicy.
- [ ] Ustaw dowolny trial na InProgress; potwierdź odmowę snapshotu.
- [ ] Ustaw duplikat CheckpointTrialId w dwóch trialach; potwierdź odmowę.
- [ ] Zweryfikuj, że osobne CheckpointTrialId i ObjectiveId zapisują się do osobnych tablic.

### Combat i Boss
- [ ] Ukończ falę i potwierdź zapis przed rozpoczęciem następnej.
- [ ] Potwierdź currentWave 1-based i zgodność totalWaves z aktualnym encounterem.
- [ ] Zapis fazy bossa tylko po zakończeniu telegraphu.
- [ ] Potwierdź boss healthFraction 0..1 i fazy 2/3.

### RewardPending
- [ ] Pojedynczy uczestnik: snapshot PendingLedgerCheck przed wypłatą nagrody.
- [ ] Wielu uczestników: potwierdź pominięcie RewardPending w schemacie v1.
- [ ] Sprawdź, że transactionId pochodzi z istniejącego subsystemu profilu.
- [ ] Po staging nie przyznawaj nagrody bez ledger lookup.

### Lease/CAS
- [ ] Serwer A przejmuje lease.
- [ ] Serwer B otrzymuje 409 Lease Held.
- [ ] Odnowienie co 60 s przedłuża lease.
- [ ] Stary epoch jest odrzucany przez backend.
- [ ] HTTP 409/timeout blokuje kolejne zapisy do reconcile.
- [ ] Zakończenie encounteru czeka na trwający write/read przed release.

### Restore staging
- [ ] Odczyt poprawnego checkpointu tworzy staged candidate.
- [ ] GameState w fazie Active blokuje BeginRestoreStaging.
- [ ] Nieznana wersja schema jest odrzucana.
- [ ] Expired TTL jest odrzucany.
- [ ] Stary lub równy authority epoch jest odrzucany.
- [ ] Inny shard/build/ruleset/worldContentVersion jest odrzucany.
- [ ] Nieznany chapter/board/spawn set/boss definition jest odrzucany.
- [ ] Zmieniony currentWave/totalWaves niezgodny z aktualnym encounterem jest odrzucany.
- [ ] Nieznany lub zduplikowany objective/trial/participant ID jest odrzucany.
- [ ] Niepoprawny boss fraction/revision/phase jest odrzucany.
- [ ] Niepoprawny RewardPending/transaction receipt jest odrzucany.
- [ ] Każdy odrzucony przypadek pozostawia GameState i świat bez zmian.
- [ ] Backend odrzuca podpis, TTL lub authority przed HTTP 200.

### Android i multiplayer
- [ ] Token/klucz/salt nie trafia do pakietu klienta.
- [ ] Podczas testu z dwoma klientami snapshot wykonuje tylko serwer.
- [ ] Sprawdź utratę backendu i brak lease bez blokowania rozgrywki.
- [ ] Sprawdź profile uczestników po reconnect — Stage34 tylko zapisuje opaque key/revision; mapowanie i ledger validation wymagają Stage35.
- [ ] Logi zawierają sequence, epoch, safe point i HTTP status, ale nie sekret ani raw profile ID.

## 10. Ograniczenia i status

- Backend Stage33 to SQLite WAL single-host prototype, nie multi-region consensus.
- Lease i podpis nie tworzą produkcyjnego systemu kont/ekonomii MMO.
- RewardPending dla grup jest wyłączony przy schema v1 z jednym transactionId.
- Opaque participantKey jest deterministyczny względem sekretnego salt; rotacja wymaga jawnej strategii migracji.
- Stage34 nie robi participant lookup ani reward ledger lookup.
- Stage34 nie publikuje odtworzonego GameState i nie spawnuje świata.
- Binary .umap/.uasset, APK/AAB i fizyczny test Android nie są generowane przez te zmiany.
- Nie wykonano kompilacji Unreal Engine 5.8, UHT ani multiplayer PIE w tym środowisku. Static checks/Python workflow nie są dowodem kompilacji C++.

## 11. Kryteria akceptacji

- [ ] Runtime safe-point builder jest serwerowy i korzysta z istniejącego backend adaptera.
- [ ] TrialBoundary, WaveBoundary, BossPhaseBoundary i RewardPending są wywoływane tylko po swoich warunkach.
- [ ] Snapshot jest zgodny ze Stage32 schema v1.
- [ ] Raw ProfileId, Actor/UObject reference, transform, AI, wallet/inventory są nieobecne.
- [ ] Lease renewal, fencing i sequence CAS są obsłużone.
- [ ] Odczyt backendu otrzymuje już zweryfikowany podpis envelope.
- [ ] Preflight sprawdza TTL, schema, content, participant key format i reward pending.
- [ ] Kandydat jest publikowany wyłącznie po przejściu wszystkich kontroli.
- [ ] Nie następuje mutacja świata podczas Stage34 restore staging.
- [ ] Walidator/UnitTests/GitHub Actions przechodzą.
- [ ] Projekt kompiluje się w UE 5.8; test PIE/dedicated-server wykonany i udokumentowany.

## 12. Następny etap

**Stage 35 — Encounter Restore Materialization & Atomic Publish**: przypisanie opaque participant keys do uwierzytelnionych profili, profileRevision reconciliation, ledger lookup dla PendingLedgerCheck, odtworzenie rozdziału/fal/bossa z allowlistowanych danych i atomowa publikacja GameState dopiero po pełnym sukcesie. Żaden klient nie może wywołać restore bez autorytetu serwera.
