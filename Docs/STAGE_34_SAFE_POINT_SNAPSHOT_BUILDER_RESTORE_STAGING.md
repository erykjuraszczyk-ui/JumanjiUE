# JUMANJI 2.111.1 — Stage 34: Safe-Point Snapshot Builder & Restore Staging

## 1. Cel etapu

Zamknąć lukę pomiędzy kontraktem Stage 32 i backendem Stage 33 a runtime'em encounteru. Serwer ma tworzyć logiczny checkpoint wyłącznie na zatwierdzonej granicy gry. Odczytany checkpoint ma przejść walidację i zostać umieszczony w prywatnym, wartościowym planie restore — bez tworzenia aktorów, zmiany GameState, publikacji replikowanej fazy ani przyznawania nagród.

Stage 34 dodaje:
- UJumanjiEncounterCheckpointRuntimeSubsystem — server-only lease, sequence reconciliation, safe-point payload builder i private restore staging;
- wersjonowane struktury wejścia, oczekiwań zgodności i planu odtwarzania;
- bariery Trials, Combat, Boss i Reward;
- kontrolę wersji build/ruleset/world-content i allowlist bieżącego rozdziału;
- bramkę opaque participant identity oraz kontrolę profile revision;
- blokadę równoległych zapisów i kontrolę receipt CAS;
- testowalne manifesty, walidator źródeł i GitHub Actions.

**Nie jest to jeszcze automatyczne wznowienie świata po migracji serwera.** Plan restore pozostaje prywatny. Etap 35 powinien dostarczyć osobny hydrator i transakcyjny commit nowej sesji.

## 2. Zaufanie i przepływ danych

```text
Server-authoritative Unreal runtime
  ├─ WorldEncounterDirector / ChapterDirector / Boss
  ├─ server-derived FJumanjiEncounterCheckpointCaptureInput
  └─ UJumanjiEncounterCheckpointRuntimeSubsystem
       ├─ acquire / renew lease
       ├─ validate safe-point barrier
       ├─ build allowlisted JSON payload
       ├─ write using expectedSequence + 1
       └─ read signed envelope
            ├─ Stage 33 backend verifies Ed25519 / RFC 8785
            └─ UE checks schema, key ID, epoch, versions, allowlists
                 └─ private FJumanjiEncounterCheckpointRestorePlan
```

Klient Android/PC nie ma API do budowania, zapisu ani odczytu checkpointu. Runtime nie wystawia metod UFUNCTION ani Blueprint RPC.

Backend weryfikuje podpis Ed25519 przed zwróceniem zapisanego envelope. Unreal sprawdza obecność metadanych integralności oraz zgodność key ID z `JUMANJI_CHECKPOINT_EXPECTED_KEY_ID`, ale nie wykonuje lokalnej kryptograficznej weryfikacji Ed25519. Backend pozostaje jedynym właścicielem klucza prywatnego i kanonikalizacji podpisu RFC 8785.

## 3. Kontrakt snapshotu

Typy:
- `FJumanjiEncounterCheckpointCaptureInput` — serwerowy, logiczny input buildera;
- `FJumanjiEncounterCheckpointRestoreExpectations` — bieżące ID/wersje/allowlisty wymagane do odczytu;
- `FJumanjiEncounterCheckpointRestorePlan` — prywatny, wartościowy plan odtworzenia;
- `FJumanjiCheckpointParticipant` — opaque key i revision profilu;
- `FJumanjiCheckpointRuntimeResult` — wynik operacji backend/runtime.

Dozwolone dane:
- EncounterId, SessionId i monotoniczny checkpointSequence;
- phase i rodzaj safe point;
- chapterId, boardEventId, chapterRevision;
- completedObjectiveIds i completedTrialIds;
- currentWave/totalWaves i EncounterSeed/SpawnSetId;
- logical BossDefinitionId, BossPhase, BossHealthFraction i BossPhaseRevision;
- RewardDefinitionId i stabilny reward-intent TransactionId;
- participantKey i profileRevision.

**Zakazane dane:** żywe referencje `AActor`/`UObject`, NetGUID, object paths, transformy i world-space coordinates, velocity/movement mode, AI/perception/behavior tree, timery/latent actions/montages, raw ProfileId, auth secrets, wallet/inventory contents, ledger rows, aktywne Gameplay Ability handles/effects.

Szczegółowy kontrakt pozostaje w `Data/Encounter/EncounterCheckpoint.schema.json`. Stage 34 go nie rozszerza i nie zmienia numeru schemaVersion.

## 4. Bezpieczne punkty

### Trials — TrialBoundary

`AJumanjiAdventureChapterDirector` publikuje natywny `OnCheckpointBoundaryReady` dopiero po zdarzeniu `AJumanjiTrialActor.OnTrialStateChanged` ze stanem Completed. Builder porównuje stan aktora próby z odpowiadającym celem Living Board.

Snapshot może zawierać ukończony podzbiór prób rozdziału. Odrzuca się go, gdy choć jedna próba jest InProgress lub Dormant, listy ukończonych celów i prób nie zgadzają się albo którykolwiek aktor/obiekt ma niepoprawny stan. W ten sposób nie zapisujemy stanu pośredniego pomiędzy aktualizacją celu a przełączeniem aktora na Completed.

Po ukończeniu ostatniej próby, gdy rozdział jest Completed, WorldEncounterDirector najpierw zleca zapis Trials, a następnie uruchamia pierwszą falę.

### Combat — WaveBoundary

Po pokonaniu ostatniego przeciwnika fali serwer zleca snapshot przed utworzeniem kolejnej fali lub bossa. `currentWave` jest numerem właśnie ukończonej fali (1-based). Przyszły hydrator rozpoczyna następną falę; gdy currentWave == totalWaves, tworzy bossa.

Warunki: ActiveEnemies = 0, brak oczekującego spawnu, brak aktywnego traversal i brak próby w toku.

### Boss — BossPhaseBoundary

Zmiana fazy bossa nie wystarcza do zapisu. Blueprint bossa musi wywołać `ConfirmBossPhaseCheckpointBoundary` dopiero po zakończeniu animacji/presentation/efektów związanych z przejściem fazy. Runtime wymaga:
- aktualnego BossPhaseRevision;
- braku telegraph Warning/Executing;
- jawnego bCheckpointQuiescent;
- skończonego healthFraction w zakresie 0..1.

Snapshot nie przechowuje attack ID, pattern geometry, timerów, transformów ani AI. Po fazie bossa żadne kolejne checkpointy nie zostaną przyjęte, dopóki nowa faza nie zostanie jawnie potwierdzona.

### Reward — RewardPending

Checkpoint jest budowany po pokonaniu bossa i przejściu encounteru do Reward, ale przed przyznaniem pakietu nagród. Nie zawiera wysokości nagród ani ledger entries.

W obecnym katalogu `EncounterId` pełni funkcję stabilnego `RewardDefinitionId` (np. `ENCOUNTER_EMERALD_JUNGLE_CITADEL`). `RewardTransactionId` jest globalnym identyfikatorem intencji pakietu: `TX_ENCOUNTER_REWARD_<EncounterId>_V1`. Nie jest to per-profile transaction row. Na późniejszym etapie odtwarzania ten intent trzeba rozliczyć osobno dla każdego uczestnika przez jego autorytatywny PlayerState/profile ledger. Powtórzenie nie może powodować drugiej wypłaty.

## 5. Opaque identity uczestników — obowiązkowa bramka

Checkpoint nie może przechowywać surowego `PersistentProfileId`. Stage 34 dodaje serwerowe, nietrwałe pola `CheckpointParticipantKey` i `CheckpointProfileRevision` do `AJumanjiPlayerState` oraz metodę `SetCheckpointParticipantIdentity`.

Warstwa zaufanego profile/account service ma:
1. po uwierzytelnieniu gracza uzyskać backend-issued opaque key w formacie `pk_[A-Za-z0-9_-]{20,96}`;
2. przekazać go do `SetCheckpointParticipantIdentity(key, profileRevision)`;
3. zarejestrować w GameInstance runtime `SetParticipantRevisionValidator`, który potwierdzi, że opaque key istnieje i wskazana revision jest aktualna.

**Ta integracja z zewnętrznym profile service nie jest automatycznie podłączona w Stage 34.** Bez niej validator jest pusty, a capture celowo kończy się błędem `PARTICIPANT_RESOLVER_MISSING` lub `PARTICIPANTS_INVALID`. Nie wolno naprawiać tego przez hashowanie, skracanie lub inne wyprowadzanie key z raw ProfileId. Gameplay encounteru działa dalej, ale checkpoint nie jest zapisywany.

## 6. Lease, sequence i zapis

`AcquireEncounterLease`:
- działa wyłącznie w serwerowym NetMode;
- używa Stage33 backend subsystem;
- po 201 odczytuje head: 404 oznacza sequence 0, 200 oznacza istniejący sequence;
- akceptuje head tylko z właściwym encounter i fencing epoch.

`CaptureSafePoint`:
1. odrzuca capture bez lease, przy drugim trwającym zapisie lub bez safe-point preconditions;
2. buduje nowy payload z NextSequence = LastCheckpointSequence + 1;
3. odnawia lease przed zapisem;
4. używa backend CAS z expectedSequence równym ostatniemu zaakceptowanemu sequence;
5. akceptuje 201 tylko gdy receipt potwierdzi dokładnie NextSequence, aktywny authorityEpoch i encounter;
6. przy HTTP 409 lub niepoprawnym receipt unieważnia lokalną gotowość lease; nie próbuje ślepo ponowić starego payloadu.

Jedna instancja runtime dopuszcza tylko jeden zapis w locie. W razie błędu backendu checkpoint może zostać pominięty; encounter nie jest zatrzymywany i nie tworzy się checkpointu z niebezpiecznego stanu.

## 7. Restore staging — bez mutacji świata

`ReadAndStageRestorePlan` wymaga ważnego lease i kompletnych oczekiwań bieżącego serwera:
- EncounterId, ChapterId, BoardEventId, SpawnSetId i BossDefinitionId;
- AllowedObjectiveIds i AllowedTrialIds;
- AllowedRewardDefinitionId i ExpectedRewardTransactionId;
- ServerBuildId, RulesetVersion i WorldContentVersion.

Parser sprawdza exact-key schema, UUID, timestamp/TTL, sequence, aktualny fencing epoch, stary source epoch, fazę i odpowiadający jej kind, wersje świata, allowlisty treści, participants i reward intent.

Po sukcesie plan pozostaje w prywatnym buforze GameInstance:
- `GetStagedRestorePlan` zwraca kopię wartości;
- `ClearStagedRestorePlan` usuwa bufor.

Stage 34 **nie**:
- spawnuje enemy/boss/trial actors;
- przywraca transformów, AI, timerów i montages;
- przyznaje nagród;
- zmienia PlayerState/GameState ani replikowanej fazy;
- uruchamia server migration lub shard transfer;
- wykonuje automatycznie ReadAndStageRestorePlan po restarcie procesu.

To osobna granica transakcyjna kolejnego etapu. Nie publikuj GameState, dopóki przyszły hydrator nie rozwiąże wszystkich klas/ID, uczestników, objective states i reward ledger oraz nie przygotuje kompletnego świata.

## 8. Konfiguracja procesu serwera

Wymagane zmienne środowiskowe:
- `JUMANJI_CHECKPOINT_SERVER_TOKEN`;
- `JUMANJI_SERVER_INSTANCE_ID`;
- `JUMANJI_SHARD_ID`;
- `JUMANJI_SERVER_BUILD_ID`;
- `JUMANJI_RULESET_VERSION`;
- `JUMANJI_WORLD_CONTENT_VERSION`;
- `JUMANJI_CHECKPOINT_EXPECTED_KEY_ID`.

Konfiguracja URL/timeout i uruchomienie Stage33 backendu pozostają opisane w `Docs/STAGE_33_ENCOUNTER_CHECKPOINT_BACKEND_LEASE_SERVICE.md`.

Nie zapisuj tokenów, signing keys, lease tokens ani ProfileId w manifestach, logach, DataTables lub repozytorium.

## 9. Walidacja i testy

Uruchom z root repozytorium:

    python Scripts/validate_encounter_checkpoint_runtime.py
    python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_checkpoint_runtime.py" -v

Walidator sprawdza:
- manifest i cztery fazy safe-point;
- źródło schema Stage32 i backendu Stage33;
- payload exclusions i restore non-mutation contract;
- wymagane API buildera/staging;
- obecność wszystkich barier w źródłach chapter/trial/wave/boss/reward;
- brak niedozwolonych typów referencyjnych w logicznych strukturach checkpointu;
- brak UFUNCTION/client-exposed checkpoint runtime API.

Workflow:
`.github/workflows/encounter-checkpoint-runtime-validation.yml`

PASS oznacza spójność manifestu i statycznych kontraktów. Nie oznacza UHT, kompilacji C++ w Unreal Engine 5.8, poprawności zewnętrznego profile provider, testu backendu z kluczami produkcyjnymi ani automatycznego server migration.

## 10. Unreal Editor / server acceptance

- [ ] Build UHT + Development Server w UE 5.8 przechodzi.
- [ ] Dedicated/listen server acquire'uje lease; NM_Client nie może wywołać backendu.
- [ ] Trusted profile adapter wiąże opaque key i revision dla każdego PlayerState.
- [ ] Bez participant resolvera capture odrzucany jest bez serializacji raw ProfileId.
- [ ] Po ukończeniu jednej próby TrialBoundary zapisuje wyłącznie ukończony podzbiór.
- [ ] Przy próbie InProgress capture jest odrzucany.
- [ ] Po ostatniej próbie checkpoint Trials powstaje przed SpawnWave(0).
- [ ] WaveBoundary zapisuje po zakończeniu fali, przed spawnem kolejnej.
- [ ] Boss przechodzi zmianę fazy, a Blueprint jawnie potwierdza quiescence dopiero po animacji/efektach.
- [ ] Aktywny telegraph blokuje Boss checkpoint.
- [ ] RewardPending powstaje przed wypłatą i niesie wyłącznie stabilny reward intent.
- [ ] Restart serwera: backend daje nowy epoch; stary server lease jest odrzucany.
- [ ] Mismatch build/ruleset/world-content/allowlist blokuje staging.
- [ ] Plan staging nie tworzy aktorów i nie zmienia GameState.
- [ ] W razie backend unavailable encounter nadal działa, ale nie deklaruje checkpoint committed.

## 11. Ograniczenia i następny etap

Checkpoint builder jest zintegrowany z aktualnym server encounter prototype. To nadal nie jest produkcyjne MMO checkpoint/recovery:
- profile service nie jest spięty z opaque identity provider;
- Stage33 backend pozostaje single-host SQLite prototype;
- podpis kryptograficznie sprawdza backend, nie lokalna biblioteka UE;
- brak atomowego hydratora/restore commit;
- brak migrowania żywych graczy i sesji pomiędzy shardami;
- brak testów build/PIE/dedicated server/Android.

**Stage 35 — Transactional Encounter Hydrator & Recovery Commit** powinien przygotować wszystkie wymagane obiekty w prywatnym stagingu, odtworzyć zatwierdzone objective/wave/boss state, rozliczyć reward intent z profile ledgerem i dopiero wtedy opublikować nowy GameState/authority epoch. W przypadku błędu świat pozostaje w stanie nieopublikowanym, a lease jest zwalniany.
