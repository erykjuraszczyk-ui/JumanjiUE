# JUMANJI 2.111.1 — Stage 33: Encounter Checkpoint Backend Adapter & Lease Service Prototype

## 1. Cel etapu

Zamienić kontrakt danych Stage 32 w działający, ograniczony prototyp zaufanej komunikacji **serwer gry → backend checkpointów**.

Stage 33 dostarcza:
- osobny testowy serwis Python;
- uwierzytelnianie wyłącznie serwerowym bearer tokenem;
- lease z monotonicznym fencing epoch;
- atomowe compare-and-swap dla kolejnych checkpointów;
- podpis Ed25519 nad kanonicznym envelope RFC 8785;
- ponowną walidację payloadu i weryfikację podpisu przy odczycie;
- natywny, asynchroniczny adapter HTTP Unreal Engine 5.8, odrzucający wywołania z NM_Client;
- testy obejmujące konkurencyjne lease, CAS, fencing, podpis i brak uwierzytelnienia.

To jest prototyp single-host, nie gotowa usługa produkcyjna MMO. Nie implementuje jeszcze budowania snapshotu ze świata Unreal, atomowego wznowienia aktorów ani migracji procesu/shardu.

## 2. Granica zaufania

~~~text
Unreal dedicated/listen server
  └── UJumanjiEncounterCheckpointBackendSubsystem
        └── HTTPS + server credential
              └── Trusted Encounter Checkpoint Service
                    ├── lease + fencing epoch
                    ├── sequence compare-and-swap
                    ├── SQLite WAL (single-host prototype)
                    └── Ed25519 signing/verification
~~~

Zasady:
1. Klient Android/PC nie wykonuje żadnych wywołań do backendu.
2. Adapter UE sprawdza GetNetMode() != NM_Client przed żądaniem sieciowym.
3. Sekrety serwerowe są pobierane ze środowiska procesu.
4. Prywatny klucz Ed25519 istnieje wyłącznie w procesie usługi checkpointów. Nie jest dołączany do APK, gry, DefaultGame.ini ani repozytorium.
5. Backend nie otrzymuje surowego ProfileId, portfela ani ekwipunku. Uczestnicy pozostają opaque participantKey zgodnie z kontraktem Stage 32.
6. Usługa nie przyznaje nagród. RewardPending wymaga sprawdzenia transactionId w autorytatywnym ledgerze profilu.

## 3. Artefakty repozytorium

### Backend Python

- Backend/EncounterCheckpointService/service.py — serwis HTTP, SQLite, lease, CAS, Ed25519.
- Backend/EncounterCheckpointService/requirements.txt — cryptography i rfc8785.
- Backend/EncounterCheckpointService/generate_dev_key.py — lokalny generator klucza testowego Ed25519.
- Backend/EncounterCheckpointService/README.md — uruchomienie, środowisko, endpointy i ograniczenia.
- Scripts/Tests/test_encounter_checkpoint_service.py — testy zachowania backendu.
- Scripts/validate_encounter_checkpoint_backend.py — walidator kontraktu integracji.
- .github/workflows/encounter-checkpoint-backend.yml — Python 3.12, walidator i unittest.

### Unreal Engine

- Source/JumanjiUE/Public/JumanjiEncounterCheckpointBackendSubsystem.h
- Source/JumanjiUE/Private/JumanjiEncounterCheckpointBackendSubsystem.cpp
- Source/JumanjiUE/JumanjiUE.Build.cs — moduły HTTP, Json, JsonUtilities.

UJumanjiEncounterCheckpointBackendSubsystem jest klasą native-only. Nie wystawia UFUNCTION ani API Blueprint. Callback jest asynchroniczny. Adapter przenosi JSON payloadu, lecz nie tworzy go z aktorów świata.

## 4. Kontrakt backendu

Wszystkie endpointy /v1 wymagają nagłówka:

Authorization: Bearer <JUMANJI_CHECKPOINT_SERVER_TOKEN>

| Metoda | Endpoint | Cel |
|---|---|---|
| POST | /v1/encounters/{EncounterId}/leases | Przejęcie lease i wydanie nowego epoch |
| POST | /v1/encounters/{EncounterId}/leases/renew | Odnowienie aktualnego lease |
| POST | /v1/encounters/{EncounterId}/leases/release | Zwolnienie lease |
| PUT | /v1/encounters/{EncounterId}/checkpoint | Zapis kolejnego checkpointu przez CAS |
| POST | /v1/encounters/{EncounterId}/checkpoint/read | Odczyt checkpointu przy ważnym lease |
| GET | /healthz | Lokalna kontrola procesu usługi |

Lease:
- TTL jest ograniczony do 15–120 sekund.
- Token jest losowy i zwracany tylko serwerowi, który przejął lease.
- Baza przechowuje SHA-256 tokenu, nie jawny token.
- Aktywny lease blokuje drugi serwer.
- Po wygaśnięciu lub zwolnieniu kolejny właściciel otrzymuje authorityEpoch + 1.
- Każdy zapis/odczyt z dawnym tokenem lub epoch jest odrzucany.

Checkpoint CAS:
- Serwer przekazuje expectedSequence.
- payload.checkpointSequence musi wynosić expectedSequence + 1.
- encounterId, serverInstanceId, shardId i authorityEpoch w payloadzie muszą być zgodne z aktywnym lease.
- Konflikt sequence zwraca HTTP 409; serwer musi pobrać aktualny stan i podjąć świadomą decyzję.
- Wygasły payload, brak wymaganych pól lub niedozwolone dane powodują odrzucenie przed zapisem.

Podpis:
1. Backend waliduje payload według walidatora Stage 32.
2. Tworzy envelope z envelopeVersion, payload oraz integrity.algorithm i integrity.keyId.
3. Pomija wyłącznie integrity.signature w danych wejściowych podpisu.
4. Kanonikalizuje JSON zgodnie z RFC 8785.
5. Podpisuje bajty kluczem Ed25519.
6. Zwraca Base64URL signature w integrity.signature.
7. Przy odczycie ponownie waliduje envelope i sprawdza podpis przed zwróceniem danych.

Podpis testowy z pliku Stage 32 jest wyłącznie syntaktycznym fixture. Nie należy go traktować jako poprawnego podpisu kryptograficznego.

## 5. Uruchomienie lokalne

Wymagany Python 3.12+.

Linux/macOS:

~~~bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -r Backend/EncounterCheckpointService/requirements.txt
python Backend/EncounterCheckpointService/generate_dev_key.py

export JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE="$PWD/.secrets/checkpoint-ed25519-dev.pem"
export JUMANJI_CHECKPOINT_KEY_ID="LOCAL-DEV-KEY-01"
export JUMANJI_CHECKPOINT_SERVER_TOKEN="wstaw-losowy-sekret-minimum-32-znaki"
export JUMANJI_CHECKPOINT_DB="$PWD/.secrets/checkpoints.sqlite3"
export JUMANJI_CHECKPOINT_BIND_HOST="127.0.0.1"
export JUMANJI_CHECKPOINT_BIND_PORT="8787"

python Backend/EncounterCheckpointService/service.py
~~~

PowerShell:

~~~powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r Backend/EncounterCheckpointService/requirements.txt
python Backend/EncounterCheckpointService/generate_dev_key.py

$env:JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE = "$PWD/.secrets/checkpoint-ed25519-dev.pem"
$env:JUMANJI_CHECKPOINT_KEY_ID = "LOCAL-DEV-KEY-01"
$env:JUMANJI_CHECKPOINT_SERVER_TOKEN = "wstaw-losowy-sekret-minimum-32-znaki"
$env:JUMANJI_CHECKPOINT_DB = "$PWD/.secrets/checkpoints.sqlite3"
$env:JUMANJI_CHECKPOINT_BIND_HOST = "127.0.0.1"
$env:JUMANJI_CHECKPOINT_BIND_PORT = "8787"

python Backend/EncounterCheckpointService/service.py
~~~

Sprawdź proces:

~~~bash
curl http://127.0.0.1:8787/healthz
~~~

Generator tworzy lokalny klucz development-only. Nie wysyłaj klucza, tokenu ani pliku SQLite do repozytorium. Lokalny adres loopback jest ustawieniem domyślnym.

## 6. Konfiguracja adaptera Unreal

Na serwerowym środowisku Unreal ustaw w Config/DefaultGame.ini lub w odpowiednim pliku serwera:

~~~ini
[/Script/JumanjiUE.JumanjiEncounterCheckpointBackendSubsystem]
ServiceBaseUrl=http://127.0.0.1:8787
RequestTimeoutSeconds=8.0
~~~

Adres http:// jest akceptowany tylko dla localhost i 127.0.0.1. Dla zdalnego serwisu wymagane jest https://.

W środowisku procesu Unreal ustaw:
- JUMANJI_CHECKPOINT_SERVER_TOKEN
- JUMANJI_SERVER_INSTANCE_ID
- JUMANJI_SHARD_ID

Przykładowe wartości identyfikatorów muszą spełniać format serwera i sharda; nie przekazuj ich z klienta.

Adapter udostępnia natywne metody:
- AcquireLease
- RenewLease
- ReleaseLease
- WriteCheckpoint
- ReadCheckpoint

Każda metoda przyjmuje callback. Odpowiedź zawiera status transportu, kod HTTP i tekst JSON odpowiedzi. true oznacza jedynie, że żądanie zostało uruchomione; o powodzeniu operacji rozstrzyga callback i status HTTP.

## 7. Walidacja i testy

Po instalacji zależności:

~~~bash
python Scripts/validate_encounter_checkpoint_backend.py
python -m unittest discover -s Scripts/Tests -p "test_encounter_checkpoint_service.py" -v
~~~

Suite pokrywa:
- poprawny podpis i weryfikację Ed25519;
- odrzucenie starego sequence (CAS);
- zgodność epoch payloadu z lease;
- wzrost epoch i odrzucenie starego właściciela;
- wykluczenie konkurencyjnego przejęcia lease;
- odrzucenie modyfikacji zapisanych danych po podpisaniu;
- brak dostępu HTTP bez bearer tokenu;
- ograniczenia TTL lease.

GitHub Actions uruchamia walidator i testy na Python 3.12 dla zmian backendu i adaptera.

To są testy prototypu Python i statycznej integracji. Nie zastępują kompilacji Unreal Header Tool, kompilacji C++, testu PIE z dedicated server, testu utraty połączenia ani testu Android.

## 8. Ograniczenia

- SQLite WAL i BEGIN IMMEDIATE zapewniają atomową synchronizację dla prototypu działającego na jednym hoście. Nie tworzą rozproszonego, wielowęzłowego lease service ani consensus pomiędzy regionami.
- Bearer token jest prototypowym uwierzytelnieniem usługa–serwer. Produkcja wymaga mTLS, tożsamości usługowej, rotacji sekretów i audytu.
- Klucz prywatny jest ładowany do procesu prototypowej usługi z pliku. Produkcja wymaga KMS/HSM lub równoważnego secret store, rotacji kluczy i procedury unieważniania.
- Nie ma rate limitingu, HA, replikacji bazy, backup/recovery operacyjnego ani ochrony przed awarią całej maszyny.
- Nie ma SafePointSnapshotBuilder w C++, który rozpoznaje bezpieczną granicę Trials/Wave/Boss/Reward.
- Nie ma RestoreStaging/preflight, ponownego tworzenia aktorów ani atomowej publikacji wznowionej sesji.
- Nie ma podłączenia trusted profile service i weryfikacji ledgeru nagród.
- Adapter nie zapisuje automatycznie danych przy EndPlay, awarii procesu ani w trakcie walki. Nie wykonuj best-effort snapshotu z dowolnego stanu.
- Nie wykonano builda UE 5.8, UHT, dedicated server, PIE, testu sieciowego ani pakowania APK/AAB w ramach tej aktualizacji.

## 9. Kryteria akceptacji

- [ ] Backend uruchamia się na Python 3.12 po instalacji wymagań.
- [ ] Health probe odpowiada.
- [ ] Brak bearer tokenu daje HTTP 401.
- [ ] Tylko jeden serwer może utrzymywać aktywny lease dla EncounterId.
- [ ] Każde nowe przejęcie lease zwiększa fencing epoch.
- [ ] Serwer po utracie lease nie może zapisać checkpointu.
- [ ] Stary expectedSequence jest odrzucony bez nadpisania nowszego checkpointu.
- [ ] Odczyt weryfikuje podpis Ed25519 i TTL.
- [ ] Wartości identity/epoch w payloadzie muszą pasować do lease.
- [ ] Testy automatyczne i workflow przechodzą.
- [ ] Adapter UE nie wykonuje żądania w NM_Client.
- [ ] Projekt przechodzi kompilację UE 5.8 i testy dedicated-server po stronie środowiska Unreal.

## 10. Następny etap

**Stage 34 — Safe-Point Snapshot Builder & Restore Staging**: zbudować serwerowy, allowlistowany snapshot z logicznych stanów encounteru wyłącznie na zatwierdzonych granicach. Przygotować prywatny restore staging, preflight wersji świata/rulesetu/uczestników oraz atomową publikację stanu dopiero po pełnej walidacji. Nadal bez serializacji żywych UObject/Actor, transformów, AI i aktywnej fizyki.
