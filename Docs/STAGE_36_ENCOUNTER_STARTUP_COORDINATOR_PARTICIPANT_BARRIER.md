# JUMANJI 2.111.1 — Stage 36: Encounter Startup Coordinator & Participant Barrier

## 1. Cel etapu

Stage 35 dostarczył pełny serwerowy punkt wejścia do odtworzenia checkpointu, ale pozostawił host integration jako pracę ręczną. Stage 36 zamyka tę lukę:

- automatycznie inicjuje checkpoint recovery po zakończeniu logowania i odtworzenia profilu;
- znajduje dyrektora encounteru wyłącznie w persistent levelu;
- wymaga dokładnie jednego skonfigurowanego encounter directora;
- próbuje zatwierdzić prywatnie zweryfikowany checkpoint po każdym kolejnym zakończonym PostLogin;
- utrzymuje staged candidate i recovery lease, gdy część uczestników z checkpointu jeszcze nie dołączyła;
- ogranicza oczekiwanie na brakującą grupę do 180 sekund;
- po upływie czasu usuwa candidate, zwalnia lease i pozostawia GameState bez częściowej zmiany.

Stage 36 nie zmienia schematu checkpointu v1, podpisu backendu, reguł anti-rollback, profili, ledgeru nagród ani kolejności atomic publish ze Stage 35.

## 2. Nowy przepływ hosta

    AJumanjiGameMode::PostLogin
      bind platform/stable profile identity
      load local profile snapshot (or initialize starter profile)
      apply restored profile progression and transaction receipts
      same-server reconnect handling
      TryStartEncounterCheckpointRecovery()
        find exactly one encounter director in PersistentLevel
        require GameState phase Inactive or Failed
        require at least one persistence-enabled profile
        EncounterDirector->BeginCheckpointRestore()
          acquire checkpoint lease
          read signed backend envelope
          StageVerifiedBackendResponse()
          TryCommitStagedRestore()
            map checkpoint participantKey values to live PlayerStates
            compare progression revisions and durable chapter receipts
            reconcile RewardPending with the profile transaction ledger
            materialize the logical encounter state
            atomically publish GameState and enable staged actor replication

TryStartEncounterCheckpointRecovery() jest wywoływane po tym, jak PostLogin zakończył synchroniczne LoadProfile, ApplyProfileSnapshot albo inicjalizację nowego profilu. Nie rozpoczyna odtwarzania na podstawie samego PlayerArray, zanim profil bieżącego użytkownika zostanie załadowany.

## 3. Wyszukiwanie encounter directora

Koordynator:
- wykonuje się wyłącznie po stronie serwera;
- szuka AJumanjiWorldEncounterDirector w aktualnym UWorld;
- akceptuje tylko aktora należącego do World->GetPersistentLevel();
- wymaga dokładnie jednej instancji w persistent levelu;
- nie wybiera dyrektora z losowej komórki World Partition;
- respektuje istniejący EncounterId i walidację konfiguracji z BeginCheckpointRestore.

Brak dyrektora oznacza brak próby restore podczas tego PostLogin. Więcej niż jeden persistent-level director jest błędem konfiguracji i blokuje recovery — koordynator nie wybiera arbitralnie jednego aktora.

## 4. Bariera uczestników

Backend checkpointów przechowuje pseudonimowe participantKey oraz profileRevision, nie surowe ProfileId.

Po otrzymaniu signed checkpointu Stage 35 nadal wykonuje pełne mapowanie kluczy na aktualne, serwerowo związane PlayerStates. Stage 36 zmienia wyłącznie zachowanie w przypadku, gdy jedno z wymaganych PlayerStates jeszcze nie jest obecne lub nie zakończyło przywracania tożsamości/profilu.

Jeżeli CommitStagedRestore zwróci:

    RESTORE_PARTICIPANT_NOT_CONNECTED_OR_AUTHENTICATED

Runtime:
1. zachowuje prywatny, zweryfikowany FJumanjiEncounterCheckpointRestoreCandidate;
2. zachowuje aktywny recovery lease oraz fencing epoch;
3. nie mutuje GameState, prób, sesji ani aktorów spotkania;
4. rozpoczyna jednorazowe okno oczekiwania 180 sekund;
5. pozostawia publiczne TryCommitStagedRestore dostępne dla zaufanego GameMode.

Po każdym następnym PostLogin, gdy profil został już przywrócony, GameMode sprawdza najpierw, czy candidate jest staged. Jeśli tak, ponawia TryCommitStagedRestore przed sprawdzeniem one-shot startup guard.

### Warunki zatwierdzenia

Commit jest dozwolony dopiero, gdy:
- każdy wymagany participantKey ma dokładnie jedno odpowiadające mu live PlayerState;
- profil ma serwerowo związany PersistentProfileId;
- IsProfilePersistenceEnabled() jest prawdziwe;
- aktualna rewizja progresji nie jest niższa od zapisanej w checkpoint;
- checkpoint Trials nie wyprzedza trwałego receipt ukończenia rozdziału;
- checkpoint Combat/Boss/Reward ma wymagany receipt ukończenia rozdziału;
- RewardPending przejdzie istniejącą kontrolę transactionId i idempotency ledgeru.

Nadmiarowy, ale prawidłowy uczestnik w serwerowej sesji nie jest dołączany do checkpointowej grupy, jeśli jego participantKey nie występuje w kandydacie. Duplikaty live profile ID lub participant key nadal są błędem, a nie warunkiem oczekiwania.

## 5. Timeout i zachowanie przy błędach

Limit oczekiwania wynosi 180 sekund od pierwszego nieudanego dopasowania uczestnika.

Po upływie limitu:
- prywatny candidate jest usuwany;
- flaga oczekiwania jest czyszczona;
- aktywny checkpoint lease jest zwalniany;
- GameState pozostaje w dotychczasowej fazie Inactive albo Failed;
- nie uruchamia się nowego encounteru automatycznie;
- nie przyznaje się nagrody i nie odtwarza części grupy.

Błędy inne niż brak dołączonego/uwierzytelnionego uczestnika nadal są terminalne dla bieżącego odczytu. Obejmują m.in. niezgodność rewizji profilu, brak chapter receipt, błędny transaction ID, niezgodność content/ruleset/build, błędny podpis/envelope lub nieudaną materializację.

Jeśli backend nie ma checkpointu, ścieżka recovery kończy się bez odtwarzania. Normalna ścieżka rozpoczęcia encounteru pozostaje niezależna.

## 6. Granice bezpieczeństwa

- BeginCheckpointRestore, TryCommitStagedRestore i MaterializeCheckpointRestore pozostają natywnymi API serwera.
- Żadne z tych wywołań nie jest RPC ani BlueprintCallable.
- Klient nie przesyła participant key, profile ID, checkpoint payloadu, klasy przeciwnika, reward transaction ID ani wyniku preflight.
- GameMode nie wykonuje samodzielnie materializacji ani nie zapisuje/odczytuje bezpośrednio backendu.
- Każda próba commit ponownie wykonuje walidację tożsamości, rewizji i receiptów.
- Snapshot profilu nigdy nie jest przywracany z checkpointu. Profil pozostaje źródłem prawdy dla konta i ekonomii.
- Brak jednego członka grupy nie powoduje częściowego restore.

## 7. Unreal Editor i host setup

1. Upewnij się, że BP_EmeraldJungleWorldEncounter (child AJumanjiWorldEncounterDirector) znajduje się w persistent levelu.
2. Upewnij się, że w persistent levelu jest dokładnie jeden encounter director obsługiwany przez GameMode.
3. Wskaż poprawny AJumanjiAdventureChapterDirector, boss spawn point, klasy bossa/przeciwników i wszystkie wave spawn anchors.
4. Ustaw GameMode na AJumanjiGameMode albo klasę potomną, która zachowuje jego PostLogin.
5. Na dedykowanym serwerze ustaw istniejące zmienne środowiskowe Stage 34/35:
   - JUMANJI_SERVER_INSTANCE_ID
   - JUMANJI_SHARD_ID
   - JUMANJI_SERVER_BUILD_ID
   - JUMANJI_RULESET_VERSION
   - JUMANJI_WORLD_CONTENT_VERSION
   - JUMANJI_CHECKPOINT_PARTICIPANT_SALT
6. Skonfiguruj istniejący backend adapter Stage 33 i zaufany serwis checkpointów.
7. Wystartuj serwer i dołącz uczestników checkpointu. Każdy musi zakończyć binding stabilnej tożsamości i restore profilu.
8. Obserwuj logi JUMANJI checkpoint recovery started, waiting for every authenticated participant oraz checkpoint restore committed albo kontrolowany timeout.
9. Zweryfikuj, że nowy klient nie może wywołać żadnego z natywnych recovery API.

Koordynator hosta nie zastępuje konfiguracji mapy ani aktorów Blueprint. Jeżeli dyrektor/Chapter Director lub anchor nie jest gotowy, BeginCheckpointRestore odrzuci wywołanie zgodnie z walidacją Stage 35.

## 8. Testy i walidacja źródeł

Uruchom:

    python Scripts/validate_encounter_startup_coordinator.py
    python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_startup_coordinator.py" -v

Nowy workflow:

.github/workflows/encounter-startup-coordinator.yml

Workflow uruchamia także walidatory i testy checkpoint schema/runtime/materialization oraz regresję backendu Stage 33.

## 9. Kryteria akceptacji

- [ ] PostLogin wywołuje koordynator dopiero po profile restore i reconnect handling.
- [ ] Coordinator działa tylko na serwerze.
- [ ] Wyszukuje tylko AJumanjiWorldEncounterDirector w persistent levelu.
- [ ] Brak directorów nie uruchamia przypadkowego aktora z innej komórki.
- [ ] Więcej niż jeden persistent-level director blokuje recovery z błędem konfiguracji.
- [ ] BeginCheckpointRestore jest wywoływane najwyżej raz na instancję GameMode.
- [ ] Po otrzymaniu staged candidate GameMode może ponawiać commit po kolejnym PostLogin nawet wtedy, gdy one-shot guard jest już ustawiony.
- [ ] Brak uczestnika utrzymuje candidate i lease w stanie staged/waiting.
- [ ] Brak uczestnika nie publikuje GameState i nie tworzy częściowych aktorów.
- [ ] Ukończenie profilu przez ostatniego uczestnika umożliwia ponowny commit.
- [ ] Timeout po 180 sekundach usuwa candidate i zwalnia lease.
- [ ] Błędy profilu, receipt, podpisu i content compatibility pozostają terminalne.
- [ ] RewardPending pozostaje idempotentny i nie jest wypłacany drugi raz.
- [ ] Wszystkie testy Stage 34–36 i backendu Stage 33 przechodzą.
- [ ] Unreal Engine 5.8 UHT/C++ compile, PIE multiplayer i crash/restart test są wykonane na rzeczywistym środowisku serwerowym przed wdrożeniem.

## 10. Ograniczenia

- Profile loading w aktualnym prototypie jest synchronicznym lokalnym SaveGame. Produkcyjny asynchroniczny profile service musi wywołać koordynator dopiero po swoim odpowiedniku profilu-ready.
- 180 sekund jest lokalnym limitem bariery hosta; nie wydłuża TTL podpisanego checkpointu ani lease backendu.
- Coordinator uruchamia jedną próbę backend restore na instancję GameMode. Błędy transportu/konfiguracji nie powodują nieskończonych retry.
- Jeżeli wymagany uczestnik nie dołączy w czasie, recovery kończy się bez częściowego odtwarzania. Obsługa administracyjna/ponowne uruchomienie serwera pozostaje osobną polityką hosta.
- Stage 33 nadal jest prototypem SQLite WAL na jednym hoście; nie zapewnia produkcyjnego distributed consensus, migracji shardów ani transakcji rozproszonej z profile service.
- Nie wykonano kompilacji Unreal Engine 5.8, UHT, PIE multiplayer, rzeczywistego crash/restart recovery ani builda APK/AAB w tej operacji.

## Następny etap

**Stage 37 — Recovery Observability & Operator Controls**: metryki recovery, jawne outcome codes, log correlation, diagnostyka lease/checkpoint i serwerowe narzędzia operatora do kontrolowanego retry/abort bez ujawniania danych profili.
