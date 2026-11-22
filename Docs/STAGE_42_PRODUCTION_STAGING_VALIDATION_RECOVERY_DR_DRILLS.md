# JUMANJI 2.111.1 — Stage 42: Production Staging Validation & Recovery DR Drills

## 1. Cel etapu

Stage 41 dodał współdzielony provider PostgreSQL, weryfikację Ed25519 keyring, TLS/mTLS, ograniczenie współbieżności i szablon prywatnego deploymentu. Nie dostarczył jednak rzeczywistego wdrożenia staging, failover, PITR, wykonanej próby restore ani pomiarów na reprezentatywnej infrastrukturze.

Stage 42 dostarcza:
- staging smoke runner dla HTTPS/TLS/mTLS → PostgreSQL → lease/fencing/CAS → Ed25519 signed readback;
- syntetyczne identyfikatory encounterów DRILL_STAGE42_*;
- sanitizowany raport JSON bez poświadczeń, lease tokenów, surowych identyfikatorów profili i treści koperty;
- schema raportu i walidator dowodów;
- offline utility do unieważnienia lease po restore PostgreSQL przez zwiększenie authority epoch i wyczyszczenie dawnych właścicieli;
- testy integracyjne na odizolowanym PostgreSQL dla dry-run, fencing, stale-owner rejection, zachowania podpisanego checkpointu i granic BIGINT;
- workflow CI z ephemeral PostgreSQL i lokalnie wygenerowanymi certyfikatami testowymi;
- readiness manifest, runbook failover/PITR/rotacji kluczy i jasne kryteria dopuszczenia dowodów.

**Stage 42 nie wdraża środowiska produkcyjnego, nie tworzy klastra HA i nie zatwierdza produkcyjnego SLO.** CI nie jest zamiennikiem rzeczywistego stagingu, testu PITR ani walidacji Unreal Engine 5.8.

## 2. Artefakty repozytorium

### Narzędzia
- Scripts/run_encounter_checkpoint_staging_drill.py — kontrolowany staging drill po HTTPS/mTLS z syntetycznym encounterem.
- Scripts/fence_checkpoint_leases_after_restore.py — dry-run/apply dla odtworzonej bazy PostgreSQL; podbija fencing epoch i usuwa stary lease ownership.
- Scripts/validate_encounter_checkpoint_staging_report.py — walidacja kompletności i sanitizacji raportu.
- Scripts/validate_encounter_checkpoint_stage42.py — walidator statycznego kontraktu Stage42.
- Scripts/Tests/test_encounter_checkpoint_stage42.py — test payloadu Stage32, kluczy publicznych, podpisu, raportów i granic prywatności.
- Scripts/Tests/test_checkpoint_postgres_restore_fencing.py — testy na disposable PostgreSQL.

### Dane i governance
- Data/Encounter/EncounterCheckpointStagingDrillReport.schema.json — schema raportu.
- Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json — stan dowodów staging/DR, jawne ograniczenia i wymagane manual drills.
- Docs/STAGE_42_PRODUCTION_STAGING_VALIDATION_RECOVERY_DR_DRILLS.md — ten runbook.

### CI
- .github/workflows/encounter-checkpoint-staging-dr.yml — ephemeral PostgreSQL, testowy HTTPS/mTLS checkpoint service, staging runner, report validator, regresja Stage32/33/41/42 i artefakt sanitizowanego raportu.

## 3. Granica środowiska

Stage 42 rozróżnia trzy rodzaje dowodu:

| Rodzaj | Co potwierdza | Czego nie potwierdza |
|---|---|---|
| Unit/static CI | kontrakt źródeł, schema, walidatory i zachowanie testowych funkcji | TLS do infrastruktury operatora, PostgreSQL HA, UE runtime |
| Ephemeral CI drill | prawdziwe HTTPS/mTLS, Postgres, HTTP API, lease, CAS, podpis i readback w tymczasowym środowisku | staging deployment, failover, PITR, WAN, capacity produkcyjne |
| Operator staging/DR evidence | rzeczywiste wdrożenie, certyfikaty, sieć, backup/restore, failover i konfigurację serwera | UE gameplay/Android, jeśli nie wykonano osobnego testu |

CI używa wygenerowanego CA, certyfikatu serwera, certyfikatu klienta i klucza Ed25519 oraz usługi PostgreSQL uruchomionej na czas joba. Wynik tego testu należy oznaczać jako ephemeral-ci, nigdy jako staging lub production.

## 4. Stage 42 staging smoke runner

### 4.1 Zakres

Runner:
1. wymaga environment class staging lub ephemeral-ci, environment ID i jawnego --confirm-staging-mutations;
2. wymaga HTTPS URL bez credentials/path/query, z weryfikacją CA i hostname;
3. używa certyfikatu klienta i wymusza TLS 1.2+;
4. odrzuca staging, który zaakceptuje klienta bez certyfikatu;
5. sprawdza /healthz i /readyz;
6. sprawdza, że błędny bearer token daje HTTP 401;
7. przejmuje lease i potwierdza wykluczenie drugiego ownera;
8. odnawia lease bez zmiany epoch;
9. opcjonalnie odczytuje istniejący checkpoint o podanym --encounter-id — ten krok służy do walidacji danych po przełączeniu/failover/PITR;
10. tworzy Stage32 schema-v1 TrialBoundary z syntetycznym participantKey;
11. wykonuje write i sprawdza CAS conflict dla starego sequence;
12. odczytuje kopertę i niezależnie weryfikuje Ed25519 z public keyring;
13. zwalnia lease, przejmuje nowy i potwierdza monotoniczny epoch;
14. sprawdza, że stary token/epoch jest fenced out;
15. odczytuje ponownie tę samą kopertę, sequence i hash po reacquire;
16. zwalnia końcowy lease.

### 4.2 Ograniczenie celu

Runner akceptuje wyłącznie syntetyczne ID w formacie:
DRILL_STAGE42_YYYYMMDDTHHMMSS_RANDOM

Można podać --encounter-id dla wcześniej utworzonego syntetycznego ID. Jest to wymagane po zewnętrznym failover/PITR, aby sprawdzić, czy dokładnie ten sam checkpoint przeżył przeniesienie danych.

Nie podawaj EncounterId prawdziwego gracza, produkcyjnego encounteru ani danych osobowych. Runner tworzy lub nadpisuje checkpoint tylko dla syntetycznego klucza testowego.

### 4.3 Zmienne środowiskowe

Wymagane:
- JUMANJI_STAGING_ENVIRONMENT_CLASS — staging lub ephemeral-ci;
- JUMANJI_STAGING_ENVIRONMENT_ID — nieidentyfikujący identyfikator środowiska;
- JUMANJI_STAGING_BASE_URL — HTTPS origin;
- JUMANJI_STAGING_SERVER_TOKEN albo JUMANJI_STAGING_SERVER_TOKEN_FILE;
- JUMANJI_STAGING_CA_FILE;
- JUMANJI_STAGING_CLIENT_CERT_FILE;
- JUMANJI_STAGING_CLIENT_KEY_FILE;
- JUMANJI_STAGING_VERIFY_KEYRING_FILE;
- JUMANJI_STAGING_SERVER_INSTANCE_ID;
- JUMANJI_STAGING_SHARD_ID;
- JUMANJI_STAGING_SERVER_BUILD_ID;
- JUMANJI_STAGING_RULESET_VERSION;
- JUMANJI_STAGING_WORLD_CONTENT_VERSION.

Publiczny verification keyring musi zawierać aktywny publiczny klucz Ed25519 oraz wszystkie stare klucze potrzebne do odczytu istniejących checkpointów. Nigdy nie umieszczaj prywatnego klucza podpisującego w keyringu.

### 4.4 Uruchomienie

Najpierw przygotuj wartości z secret managera operatora w lokalnym, chronionym shellu. Nie commituj ich do repozytorium, .ini, workflow, raportu ani artefaktu.

Uruchom:
~~~bash
python Scripts/run_encounter_checkpoint_staging_drill.py \
  --confirm-staging-mutations \
  --report reports/stage42-staging-drill.json

python Scripts/validate_encounter_checkpoint_staging_report.py \
  reports/stage42-staging-drill.json
~~~

Wynik PASS oznacza, że pełna macierz runnera przeszła w wskazanym środowisku. Wynik FAIL zawiera wyłącznie kod błędu i ukończone, sanitizowane checki. Nie zawiera request/response body ani tokenów.

### 4.5 Public key overlap — weryfikacja rotacji

Staging runner odczytuje podpisane koperty przy pomocy pliku JUMANJI_STAGING_VERIFY_KEYRING_FILE. Aby sprawdzić overlap:
1. utwórz osobny syntetyczny encounter przez starą instancję checkpoint service, podpisany poprzednim keyId;
2. dodaj poprzedni public key do keyringu obok aktualnego;
3. uruchom runner z --encounter-id DRILL_STAGE42_... dla tego starego rekordu;
4. sprawdź preexisting_checkpoint_probe i signed_readback;
5. wykonaj kontrolowaną rotację aktywnego private key + keyId zgodnie ze Stage41;
6. zapisz nowy syntetyczny checkpoint nowym kluczem;
7. potwierdź, że stary rekord nadal weryfikuje się starym public key, a nowy ma aktualny keyId.

Nie usuwaj poprzedniego public key przed końcem retencji wszystkich kopert, backupów i recovery windows, które wymagają tego podpisu. Stage42 nie automatyzuje KMS/HSM ani rotacji kluczy.

## 5. Fencing po restore PostgreSQL

### 5.1 Po co

Backup lub PITR może przywrócić wiersze zawierające:
- poprzedni authority_epoch;
- lease_server_instance_id;
- lease_shard_id;
- lease_token_hash;
- lease_expires_at.

Przywrócony token/lease owner nie może odzyskać prawa zapisu tylko dlatego, że baza wróciła do starszego punktu. Przed ponownym uruchomieniem serwerów gry należy unieważnić wszystkie odtworzone lease.

### 5.2 Bezwzględna kolejność operatora

1. Zablokuj ingress do checkpoint service i zatrzymaj wszystkie jego repliki/writers.
2. Potwierdź, że żaden stary proces Unreal nie ma dostępu sieciowego do odtwarzanej bazy.
3. Odtwórz PostgreSQL do osobnego, odizolowanego celu staging/DR; nie nadpisuj produkcji podczas ćwiczenia.
4. Zweryfikuj PostgreSQL schema, kopertę Stage32, TTL i Ed25519 przez read-only kontrolę operatora.
5. Ustaw JUMANJI_CHECKPOINT_DATABASE_URL_FILE, JUMANJI_CHECKPOINT_RESTORE_ENVIRONMENT_CLASS=dr-rehearsal i nieidentyfikujący JUMANJI_CHECKPOINT_RESTORE_ENVIRONMENT_ID.
6. Uruchom fencing w trybie dry-run.
7. Zweryfikuj liczbę wierszy, max epoch, liczbę istniejących lease ownerów i brak overflow.
8. Drugi operator potwierdza właściwy, izolowany cel oraz write freeze.
9. Dopiero wtedy uruchom --apply --confirm-restored-target.
10. Zachowaj sanitizowany raport fencing.
11. Uruchom checkpoint service na odtworzonym PostgreSQL, ale nadal utrzymuj ograniczony ingress.
12. Uruchom staging runner z tym samym syntetycznym --encounter-id.
13. Potwierdź, że readback zwraca ten sam checkpointId/sequence/hash, a nowe przejęcie lease ma wyższy epoch.
14. Dopiero po ocenie dowodów i zakończeniu testu odblokuj staging ingress.

### 5.3 Dry-run i apply

Domyślnie utility niczego nie modyfikuje:
~~~bash
python Scripts/fence_checkpoint_leases_after_restore.py \
  --report reports/stage42-fencing-dry-run.json
~~~

Po recenzji wyniku, na odizolowanym celu:
~~~bash
python Scripts/fence_checkpoint_leases_after_restore.py \
  --apply \
  --confirm-restored-target \
  --report reports/stage42-fencing-apply.json
~~~

Utility:
- wymaga PostgreSQL;
- wymaga sslmode=verify-full, chyba że jawny wyjątek jest ustawiony wyłącznie w izolowanym development/test;
- wymaga environment class staging lub dr-rehearsal;
- blokuje encounter_checkpoint_state w trybie ACCESS EXCLUSIVE;
- zwiększa authority_epoch o jeden dla każdego wiersza;
- czyści lease owner, shard, token hash i expiry;
- zachowuje checkpoint_sequence, signed envelope JSON i expiration checkpointu;
- przerywa całą transakcję, jeśli jakikolwiek epoch osiągnął BIGINT maximum;
- domyślnie wykonuje dry-run.

**Ważne:** ACCESS EXCLUSIVE blokuje równoległe zapisy do tabeli w czasie transakcji, ale nie uniemożliwia staremu procesowi połączenia się ponownie po commit. Dlatego zatrzymanie wszystkich writerów i blokada ingress przed apply są obowiązkowe. Fencing jest operacją dla całej bazy, a nie pojedynczego EncounterId.

## 6. Testy DR na PostgreSQL

PostgreSQL integration tests wymagają JUMANJI_TEST_POSTGRES_URL skierowanego na osobną, disposable bazę. Nie uruchamiaj globalnego fencing testu na współdzielonym store z danymi użytkowników.

Uruchom:
~~~bash
python -m unittest discover -s Scripts/Tests \
  -p "test_checkpoint_postgres_restore_fencing.py" -v
~~~

Testy sprawdzają:
- dry-run nie zmienia lease ani checkpoint;
- apply usuwa stare lease ownership i zwiększa epoch;
- stary token otrzymuje LEASE_INVALID;
- nowy lease otrzymuje kolejny epoch;
- podpisana koperta i sequence pozostają identyczne;
- overflow BIGINT powoduje rollback bez częściowego fencing.

Stage41 PostgreSQL provider tests:
~~~bash
python -m unittest discover -s Scripts/Tests \
  -p "test_encounter_checkpoint_postgres.py" -v
~~~

## 7. Backup/PITR/primary failover — czego CI nie robi

CI uruchamia tymczasowy PostgreSQL bez replikacji ani PITR. Nie próbuje symulować prawdziwego primary failover i nie oznacza tego checku jako PASS.

Operator musi wykonać na wybranej platformie:
- managed PostgreSQL backup;
- WAL archiving / PITR albo udokumentowany odpowiednik;
- odtworzenie do osobnego recovery target;
- przełączenie/odtworzenie primary zgodnie z runbookiem dostawcy;
- restore fencing w trybie dry-run i apply;
- staging readback istniejącego syntetycznego signed checkpointu;
- potwierdzenie nowego authority epoch i starego owner fencing;
- raport czasu odtworzenia, RPO/RTO i faktycznych obserwacji.

Wyniki RPO/RTO należy mierzyć. Nie zakładaj ani nie wpisuj wartości oczekiwanych bez zatwierdzonego SLO.

## 8. Raport i prywatność

Schema:
Data/Encounter/EncounterCheckpointStagingDrillReport.schema.json

Walidator:
Scripts/validate_encounter_checkpoint_staging_report.py

Raport może zawierać:
- environment ID i service host;
- statusy checków i czasy;
- synthetic EncounterId;
- sequence i authority epoch;
- SHA-256 payload/envelope;
- signing key ID.

Raport nie może zawierać:
- bearer token, lease token, token hash;
- prywatnego klucza/certyfikatu;
- surowego ProfileId, participantKey;
- request/response body;
- całej podpisanej koperty.

Fencing report także nie zawiera tokenów ani zawartości checkpointu.

## 9. CI — zakres i dowód

Workflow:
.github/workflows/encounter-checkpoint-staging-dr.yml

Workflow tworzy:
- disposable PostgreSQL;
- testowy CA/server cert/client cert;
- jednorazowy Ed25519 signing key i publiczny keyring;
- lokalny checkpoint service HTTPS/mTLS;
- syntetyczny staging drill;
- raport JSON i walidację prywatności;
- testy Stage32/33/41/42, w tym PG restore fencing.

Workflow CI jest dowodem na poprawne działanie testowego, lokalnego środowiska ephemeral. Nie jest dowodem wdrożenia staging ani produkcji.

Po udanym workflow raport ma być zachowany jako artefakt GitHub Actions. Do operator evidence packet należy dołączyć oddzielnie:
- staging runner report;
- fencing dry-run i apply report;
- backup/PITR identifiers i checksumy (bez sekretów);
- odczyt podpisanej koperty po restore;
- key rotation evidence;
- logi Dedicated Server/PIE/Android tylko po ich rzeczywistym wykonaniu.

## 10. Kryteria akceptacji

- [ ] CI tworzy Postgres i HTTPS/mTLS service bez stałych sekretów w repozytorium.
- [ ] Runner odrzuca brak client cert i błędny bearer token.
- [ ] Lease exclusion/renew/release/fencing przechodzi.
- [ ] Stary CAS sequence jest odrzucany.
- [ ] Podpis Ed25519 jest lokalnie weryfikowany publicznym keyringiem.
- [ ] Readback po reacquire zwraca tę samą kopertę.
- [ ] Raport nie zawiera credentials, tokenów ani profili.
- [ ] Dry-run fencing nie zmienia danych.
- [ ] Apply fencing na disposable PostgreSQL unieważnia stare lease i zachowuje signed checkpoint.
- [ ] BIGINT overflow powoduje rollback bez częściowego fencing.
- [ ] Stage32/33/41 regressions i Stage42 tests przechodzą.
- [ ] Rzeczywisty operator staging deploy jest wykonany i udokumentowany.
- [ ] Rzeczywisty PostgreSQL primary failover / PITR restore drill jest wykonany i udokumentowany.
- [ ] Ed25519 overlap rotation jest wykonana na staging.
- [ ] Unreal Engine 5.8 Dedicated Server/PIE recovery smoke test wykonany.
- [ ] Android package/device validation wykonana dla wspieranego urządzenia.

## 11. Stan gotowości i ograniczenia

Stage42 readiness manifest:
Data/Encounter/EncounterCheckpointStage42ReadinessManifest.json

Na początku Stage42 wszystkie zewnętrzne dowody pozostają false: productionReady=false, highAvailabilityApproved=false, a sloStatus=UNBASELINED. CI może potwierdzić jedynie ephemeral-ci. Nie wolno automatycznie podnosić:
- productionReady;
- highAvailabilityApproved;
- sloStatus;
- real staging deployment;
- failover/PITR drill;
- UE build;
- Android package/device.

Nie wykonano przez samą zmianę repozytorium prawdziwego wdrożenia staging, managed PostgreSQL failover, backup/PITR, KMS/HSM, testu WAN/multi-region, kompilacji Unreal Engine 5.8, Dedicated Server/PIE ani Android APK/AAB. Te dowody wymagają dostępu do konkretnego, zarządzanego środowiska operatora i sprzętu.

**Stage 43 powinien rozpocząć się dopiero po zebraniu Stage42 operator evidence packet.** Następnym krokiem nie jest deklarowanie produkcyjnej gotowości na podstawie CI, lecz recenzja rzeczywistych wyników staging/DR i decyzja o zamknięciu luk.
