# JUMANJI 2.111.1 — Stage 41: Production Checkpoint Store Adapter & Deployment Hardening

## 1. Cel etapu

Stage 40 przygotował governance dla pomiarów pojemności, ale nie zatwierdził p95/p99, throughput ani produkcyjnej liczby równoczesnych encounterów. Stage 41 usuwa najważniejsze ograniczenie infrastrukturalne Stage 33: checkpoint service nie jest już ograniczony do SQLite WAL na jednym hoście.

Stage 41 dostarcza:
- PostgreSQL jako współdzielony provider transakcyjny, z row locking dla lease/fencing/CAS;
- SQLite WAL zachowany jako lokalny provider development/test;
- tabelę wersji schematu i idempotentną inicjalizację schematu;
- weryfikację starych podpisów z public keyringiem i kontrolowaną rotację klucza podpisującego;
- bezpośredni TLS 1.2+ i obowiązkowe mTLS dla nasłuchu poza loopback;
- odczyt tokenu serwerowego i URL bazy z plików sekretów;
- limit współbieżnych requestów, timeout połączenia i osobny readiness probe;
- obraz Docker non-root, read-only filesystem i Compose dla prywatnej sieci;
- narzędzie do kontrolowanego importu podpisanych checkpointów SQLite do PostgreSQL;
- testy SQLite, PostgreSQL, migracji, fencing, CAS i rotacji kluczy;
- runbook backup/restore, migracji i deploymentu.

**Stage 41 nie oznacza automatycznie produkcyjnego HA, multi-region, KMS/HSM, gwarantowanego SLA ani zatwierdzonej pojemności.** PostgreSQL jest współdzielonym transactional store; architektura HA, replikacja, failover i backup/PITR muszą być zapewnione i zweryfikowane przez wybrane środowisko bazodanowe.

## 2. Provider storage

### SQLite — development/test

- `JUMANJI_CHECKPOINT_DB` wskazuje plik lokalny.
- WAL i `BEGIN IMMEDIATE` zapewniają atomowość prototypu na jednym hoście.
- SQLite nie jest współdzielonym storage dla wielu hostów gry/usługi.
- Istniejące testy Stage 33 i Stage 38 nadal korzystają z tego providera.

### PostgreSQL — shared transactional store

- `JUMANJI_CHECKPOINT_DATABASE_URL` albo `JUMANJI_CHECKPOINT_DATABASE_URL_FILE` wybiera provider PostgreSQL.
- Wymagany jest `sslmode=verify-full`, chyba że jawny wyjątek `JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE=true` został ustawiony wyłącznie w środowisku development/test.
- Każda operacja otwiera własne połączenie Psycopg 3; nie ma procesu trzymającego lease w pamięci.
- Zapis i lease wykonują transakcję, blokują wiersz konkretnego EncounterId przez `SELECT ... FOR UPDATE`, a następnie sprawdzają token hash, server/shard, fencing epoch i sequence.
- Warunek CAS nadal wymaga `expectedSequence` oraz `checkpointSequence == expectedSequence + 1`.
- `BIGINT` przechowuje sequence i fencing epoch; `DOUBLE PRECISION` — czas wygaśnięcia.
- Odmowa lease, stale epoch i CAS conflict zachowują semantykę Stage 33.

Tabela `checkpoint_schema_migrations` rejestruje wersję schematu. Inicjalizacja jest idempotentna. Bieżący schemat ma wersję 1; kolejne zmiany muszą dostać nową, monotoniczną migrację i test upgrade z poprzedniej wersji. Nie edytuj istniejącej migracji wstecznie.

## 3. Rotacja kluczy Ed25519

Klucz prywatny aktywnego podpisu jest ładowany tylko przez proces checkpoint service. Publiczny keyring zawiera dodatkowe klucze, którymi można weryfikować już zapisane koperty.

Plik keyringu:
`Data/Encounter/EncounterCheckpointVerificationKeyringTemplate.json`

Format:
~~~json
{
  "keys": [
    {
      "keyId": "PREVIOUS-KEY-ID",
      "publicKeyFile": "/run/config/previous-public-key.pem"
    }
  ]
}
~~~

Aktywny klucz z `JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE` i `JUMANJI_CHECKPOINT_KEY_ID` jest dodawany automatycznie. Nie umieszczaj go ponownie na liście. Keyring przyjmuje wyłącznie unikalne keyId i publiczne klucze Ed25519.

### Procedura bez utraty odczytu

1. Wygeneruj nowy klucz prywatny poza repozytorium i poza obrazem kontenera. Opublikuj odpowiadający mu publiczny PEM w kontrolowanym magazynie sekretów/konfiguracji.
2. Wstaw do keyringu publiczny klucz poprzedniego keyId.
3. Wdróż tę samą zawartość keyringu do wszystkich replik checkpoint service.
4. Zmień `JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE` i `JUMANJI_CHECKPOINT_KEY_ID` na nowy klucz.
5. Nowe checkpointy są podpisywane nowym keyId; stare są nadal weryfikowane przez publiczny keyring.
6. Usuń stary public key dopiero po upływie retencji najstarszego checkpointu, zakończeniu wymaganych restore/backup recovery window oraz niezależnym potwierdzeniu, że nie ma już kopert wymagających tego klucza.

Private key nie jest wysyłany do Unreal, APK/AAB, repozytorium, logów ani klienta gry. Stage 41 nie ma jeszcze integracji KMS/HSM ani zautomatyzowanego key-management control plane.

## 4. TLS, mTLS i uwierzytelnienie aplikacyjne

Dla loopback development można uruchomić serwis bez TLS. Każdy bind poza loopback (`0.0.0.0`, adres interfejsu lub IPv6 wildcard) wymaga:
- certyfikatu serwera i klucza prywatnego;
- zaufanego CA certyfikatów klientów;
- `JUMANJI_CHECKPOINT_MTLS_REQUIRED=true`.

Serwis odrzuci start, gdy zdalny bind nie ma obowiązkowego mTLS. Minimalna wersja TLS to 1.2. Poświadczenie klienta musi być zaufane przez wskazany CA, a każda trasa `/v1` nadal wymaga `Authorization: Bearer ...`. mTLS zapewnia warstwę tożsamości transportowej; bearer token pozostaje oddzielną kontrolą aplikacyjną. Stage 41 nie mapuje jeszcze cert subject/SAN do odrębnych uprawnień per server/shard i nie wdraża rotacji tokenu zewnętrznym IAM.

Publiczne endpointy:
- `GET /healthz` — liveness procesu;
- `GET /readyz` — readiness zależny od faktycznego połączenia z magazynem.

Oba zwracają tylko ogólny status i nie ujawniają hosta, encounter ID, sekretów, profilu ani treści checkpointu. W produkcyjnym Compose readiness jest sprawdzany po TLS/mTLS z osobnym certyfikatem probe.

## 5. Hardening procesu i kontenera

`Backend/EncounterCheckpointService/Dockerfile`:
- Python 3.12 slim;
- instalacja zależności z pliku requirements;
- użytkownik UID/GID 10001, bez shella;
- brak prywatnych kluczy i sekretów w warstwie obrazu;
- entrypoint uruchamia service bezpośrednio.

`Backend/EncounterCheckpointService/compose.production.yml`:
- nie publikuje `ports` na hosta;
- udostępnia port 8787 tylko na zewnętrznej, wspólnej sieci ingress;
- wymaga mTLS;
- włącza read-only root filesystem, usuwa capabilities, `no-new-privileges`, ogranicza PID i montuje tylko `/tmp` jako tmpfs;
- montuje klucze/certyfikaty/token/DSN jako read-only secrets;
- wymaga jawnego `CHECKPOINT_MAX_CONCURRENCY` — wartość musi pochodzić z zatwierdzonej konfiguracji operatora i pomiarów; Stage 41 nie ustala targetu pojemności;
- sprawdza readiness przez mTLS.

Plik Compose jest szablonem dla wdrożenia zewnętrznego PostgreSQL i istniejącej prywatnej sieci ingress. Nie tworzy ani nie konfiguruje produkcyjnego klastra PostgreSQL, load balancera, firewalli, secret managera, backup storage ani DNS. Przy lokalnym Docker Compose plikowe sekrety są bind mountami; ustaw hostowe właścicielstwo/uprawnienia tak, aby kontenerowy użytkownik 10001 mógł odczytać tylko potrzebne pliki. W produkcji preferuj natywny secret provider platformy uruchomieniowej.

## 6. Zmienne procesu

Wspólne:
- `JUMANJI_CHECKPOINT_SERVER_TOKEN` albo `JUMANJI_CHECKPOINT_SERVER_TOKEN_FILE` — bearer token serwer–usługa, minimum 32 znaki;
- `JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE` — plik aktywnego prywatnego klucza Ed25519;
- `JUMANJI_CHECKPOINT_KEY_ID` — aktywny key ID;
- `JUMANJI_CHECKPOINT_VERIFY_KEYRING_FILE` — opcjonalny JSON z poprzednimi publicznymi kluczami;
- `JUMANJI_CHECKPOINT_MAX_CONCURRENCY` — twardy limit request threads, 1–1024;
- `JUMANJI_CHECKPOINT_BIND_HOST` i `JUMANJI_CHECKPOINT_BIND_PORT`.

Storage:
- SQLite: `JUMANJI_CHECKPOINT_DB`;
- PostgreSQL: `JUMANJI_CHECKPOINT_DATABASE_URL` albo `JUMANJI_CHECKPOINT_DATABASE_URL_FILE`.

TLS/mTLS:
- `JUMANJI_CHECKPOINT_TLS_CERT_FILE`;
- `JUMANJI_CHECKPOINT_TLS_KEY_FILE`;
- `JUMANJI_CHECKPOINT_MTLS_CA_FILE`;
- `JUMANJI_CHECKPOINT_MTLS_REQUIRED=true`.

Wyjątek `JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE=true` omija wymóg `sslmode=verify-full`. Nie ustawiaj go na serwerze produkcyjnym ani stagingu z prawdziwymi danymi.

## 7. Migracja SQLite → PostgreSQL

Narzędzie:
`Scripts/migrate_encounter_checkpoint_store.py`

Przed migracją:
1. Zatrzymaj wszystkie procesy zapisujące do źródłowego SQLite.
2. Wykonaj `PRAGMA wal_checkpoint(TRUNCATE)` i kontrolę `PRAGMA integrity_check`.
3. Wykonaj spójny backup pliku SQLite; zachowaj hash i źródłową kopię read-only.
4. Przygotuj PostgreSQL, aktywny nowy klucz Ed25519 oraz keyring zawierający publiczne klucze wszystkich podpisów w migrowanych kopertach.
5. Skonfiguruj zmienne destination przez bezpieczny secret provider, w tym `JUMANJI_CHECKPOINT_DATABASE_URL_FILE`, bearer token (wymagany przez loader konfiguracji) oraz signing key file.

Najpierw wykonaj walidację bez zapisu:

~~~bash
python Scripts/migrate_encounter_checkpoint_store.py \
  --source-sqlite /secure/backup/checkpoints.sqlite3 \
  --dry-run \
  --report /secure/reports/checkpoint-migration-dry-run.json
~~~

Jeśli wynik jest czysty, wykonaj import:

~~~bash
python Scripts/migrate_encounter_checkpoint_store.py \
  --source-sqlite /secure/backup/checkpoints.sqlite3 \
  --report /secure/reports/checkpoint-migration.json
~~~

Narzędzie:
- waliduje schema, TTL, keyId i podpis Ed25519 każdego checkpointu;
- sprawdza encounter ID, sequence i authority epoch z porównaniem do podpisanego payloadu;
- przenosi checkpoint envelope, checkpoint sequence, authority epoch i expiry;
- nie importuje aktywnych lease, hashy lease tokenów, identyfikatora starego właściciela ani tokenów;
- zachowuje istniejący sequence/epoch, więc pierwszy nowy lease w PostgreSQL dostaje kolejny fencing epoch;
- odrzuca istniejący, różny stan destination zamiast go nadpisywać;
- przy ponownym uruchomieniu akceptuje identyczny już zaimportowany rekord jako idempotentny.

Import nie jest migracją online. W czasie przełączenia utrzymuj write freeze, uruchom i sprawdź raport, przestaw serwery Unreal na nowy endpoint, sprawdź odczyt i nowy checkpoint, a dopiero potem usuń stary writer. Nie wykonuj równolegle zapisów do starego i nowego store.

## 8. Backup i restore PostgreSQL

Stage 41 nie uruchamia backup scheduler ani PITR. Konfigurację backupów zapewnia platforma PostgreSQL. Minimalny runbook:
1. Użyj roli backup-only i bezpiecznego `PGPASSFILE`, nie podawaj hasła w argumentach procesu.
2. Wykonuj regularny `pg_dump --format=custom --no-owner` do szyfrowanego, odrębnego storage.
3. Dla wymaganej ochrony przed utratą hosta/regionu włącz archiwizację WAL i backup base backup/PITR w zarządzanym PostgreSQL.
4. Zapisuj checksum, czas backupu, wersję PostgreSQL i identyfikator środowiska bez danych profili.
5. Regularnie odtwarzaj backup do odizolowanego staging DB.
6. Uruchom walidację readiness, odczyt podpisanych kopert, CAS/fencing test i sprawdź, że nowy lease zwiększa epoch po odtworzeniu.

Przykładowa forma backupu (po skonfigurowaniu PGHOST/PGPORT/PGUSER/PGDATABASE, TLS oraz restrykcyjnego PGPASSFILE):

~~~bash
pg_dump --no-password --format=custom --no-owner \
  --file=/secure/backup/jumanji-checkpoint.dump
~~~

Odtwarzaj najpierw do pustej, izolowanej bazy:

~~~bash
pg_restore --no-password --no-owner \
  --dbname="$PGDATABASE" \
  /secure/backup/jumanji-checkpoint.dump
~~~

Po odtworzeniu nie kopiuj aktywnych lease ani nie próbuj odtwarzać tokenów. Skontroluj sequence i podpisane koperty; przejęcie lease przez aktualny serwer ma wygenerować nowy fencing epoch przed pierwszym zapisem.

SQLite pozostaje trybem development/test. Do jego backupu używaj SQLite online backup API (`Connection.backup`) albo kontrolowanej kopii po zatrzymaniu writerów i checkpoint WAL; nie kopiuj pojedynczego pliku głównego podczas aktywnego zapisu WAL.

## 9. Monitoring i overload

Stage 41 dodaje:
- `/healthz` i `/readyz`;
- 10-sekundowy timeout socketu HTTP;
- domyślny hard cap request body 1 MiB;
- konfigurowalny limit współbieżnych requestów;
- natychmiastowe HTTP 503, gdy limit worker slots jest wyczerpany;
- ogólną odpowiedź 503 przy awarii PostgreSQL/SQLite;
- brak logowania body, bearer token, lease token i prywatnych kluczy.

Limit wątków jest bezpiecznikiem procesu, nie deklaracją obsługiwanej liczby encounterów. Dobierz go po pomiarach na reprezentatywnym PostgreSQL, hostach, sieci/TLS i rzeczywistym ruchu Unreal. Stage 40 SLO pozostają UNBASELINED. Stage 41 nie dodaje dashboardu, alertów ani metryk biznesowych per encounter.

## 10. Testy i CI

Lokalnie:
~~~bash
python Scripts/validate_encounter_checkpoint_backend.py
python -m unittest discover -s Scripts/Tests -p "test_encounter_checkpoint_service.py" -v
python -m unittest discover -s Scripts/Tests -p "test_encounter_checkpoint_postgres.py" -v
python Scripts/migrate_encounter_checkpoint_store.py --help
~~~

PostgreSQL integration tests są pomijane, jeśli nie ustawiono `JUMANJI_TEST_POSTGRES_URL`. GitHub Actions uruchamia je na tymczasowej usłudze PostgreSQL. Insecure sslmode w tym workflow jest jawnie ograniczony do testowego kontenera; nie jest przykładem produkcyjnym.

Workflow Stage 41 dodatkowo buduje obraz i sprawdza strukturę Compose. Build obrazu nie publikuje go do registry.

## 11. Ograniczenia i dowody

- PostgreSQL provider i row locking nie są równoznaczne z HA. Replikacja, failover, quorum/consensus, automatyczne promotion i cross-region recovery są poza zakresem kodu.
- Nie ma jeszcze connection pool; każde żądanie otwiera nowe połączenie Psycopg.
- mTLS weryfikuje certyfikat klienta wobec skonfigurowanego CA, ale nie mapuje certyfikatów na granularne role server/shard. Bearer token jest nadal współdzielony w konfiguracji instancji.
- Nie ma integracji HSM/KMS, automatycznej rotacji tokenów ani kluczy, zewnętrznego audit log store czy rate-limitingu per caller.
- Migracja SQLite→PostgreSQL jest kontrolowanym offline importem; nie jest replikacją online ani zero-downtime cutover.
- Backup/PITR jest procedurą i odpowiedzialnością operatora PostgreSQL; repozytorium nie uruchamia harmonogramu ani nie dowodzi wykonania restore drill.
- Brak zatwierdzonych SLO, capacity numbers, produkcyjnego SLA i testów WAN/multi-region.
- Nie wykonano w tej zmianie wdrożenia do rzeczywistego staging/prod, builda Unreal Engine 5.8, testów UE Dedicated Server, PIE, Android ani pomiarów storage w środowisku reprezentatywnym.

**Następny etap — Stage 42: Production Staging Validation & Recovery DR Drills** powinien przeprowadzić powtarzalne testy na odizolowanym środowisku staging z rzeczywistym TLS/mTLS, PostgreSQL, backup restore/PITR, rotacją kluczy, failover i pomiarem end-to-end Unreal recovery. Nie wolno przypisywać im statusu produkcyjnego przed zebraniem dowodów.
