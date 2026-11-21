# JUMANJI 2.111.1 — Stage 39: Recovery Load, Soak & Performance Benchmarks

## 1. Cel

Dodać powtarzalny pomiar istniejącego Stage 33 checkpoint backendu podczas obciążenia współbieżnego i długotrwałego. Benchmark ma mierzyć rzeczywistą ścieżkę serwisową:

~~~text
worker → loopback HTTP → handler → Ed25519 / RFC 8785 → SQLite WAL
                         ↘ lease acquire / renew / release
                         ↘ checkpoint write (CAS) / signed readback
~~~

Stage 39 nie zmienia formatu checkpointu, semantyki lease, fencing epoch ani kodu runtime Unreal. Nie dodaje nowego API gry. Dostarcza harness pomiarowy, macierz profili, walidację, test smoke i workflow CI.

## 2. Zakres pomiaru

Harness: Scripts/benchmark_encounter_checkpoint_recovery.py

Uruchamia efemeryczny EncounterCheckpointBackend oraz ten sam make_handler() używany przez usługę. Tworzy:
- tymczasową bazę SQLite w trybie WAL;
- klucz Ed25519 generowany wyłącznie na potrzeby pomiaru;
- losowy token serwerowy jedynie na czas procesu benchmarku;
- prywatny listener HTTP na 127.0.0.1 z losowym portem;
- osobny EncounterId i lease dla każdego workera.

Worker rozgrzewa ścieżkę, a następnie w pętli wykonuje:
1. PUT /v1/encounters/{id}/checkpoint — walidacja payloadu, podpis Ed25519, transakcja SQLite i CAS sequence;
2. POST /v1/encounters/{id}/checkpoint/read — odczyt, walidacja TTL i weryfikacja podpisu;
3. POST /leases/renew — odnowienie lease, gdy zbliża się jego połowa TTL;
4. POST /leases/release po zakończeniu testu.

Przy każdym write następny sequence wynika z poprzedniego. Nie ma ślepego retry niejednoznacznego zapisu. Worker zatrzymuje się po błędzie i zapisuje jego kod, zamiast sztucznie zawyżać throughput retry.

## 3. Raportowane metryki

Dla każdej operacji osobno:
- liczba żądań, sukcesów i błędów;
- średnia, minimum i maksimum latency w milisekundach;
- p50, p95 i p99 latency;
- liczba próbek użyta do obliczenia percentyli.

Dodatkowo:
- poprawne pary write + signed readback na sekundę;
- udane operacje HTTP checkpointu na sekundę;
- łączna liczba requestów i błędów;
- liczba worker setup failures;
- wersja Python i SQLite, platforma, liczba CPU;
- tryb journal bazy i rozmiar głównego pliku DB;
- czas okna pomiarowego i czas wall-clock z cleanupem.

Długie testy nie przechowują wszystkich próbek latency. Każda operacja korzysta z ograniczonego reservoir sample (parametr --sample-cap, domyślnie 50 000). Liczniki sukcesów i błędów obejmują wszystkie żądania, natomiast percentyle są obliczane z zachowanej próbki.

Lease acquire jest mierzony jako operacja setup, a release jako cleanup. Throughput checkpoint pairs liczy wyłącznie udane pary write/read z głównego okna obciążenia.

## 4. Profile benchmarku

Źródło profili: Data/Encounter/EncounterRecoveryBenchmarkProfiles.csv

| Profil | Czas | Workery | Cel |
|---|---:|---:|---|
| Smoke | 3 s | 2 | Krótki test funkcjonalny CI |
| Baseline | 60 s | 4 | Punkt odniesienia na konkretnym hoście |
| Load | 180 s | 12 | Współbieżność i serializacja zapisów SQLite |
| Soak30 | 30 min | 8 | Stabilność i odnowienia lease |
| Soak60 | 60 min | 8 | Długotrwała stabilność i dryf latency |

To profile startowe, a nie gwarantowane limity pojemności. Benchmark ma konfigurowalne parametry, ale ogranicza czas do 3600 sekund, liczbę workerów do 64 i TTL lease do 15–120 sekund.

## 5. Uruchomienie lokalne

Z katalogu głównego repozytorium:

~~~bash
python -m pip install -r Backend/EncounterCheckpointService/requirements.txt

# Krótkie uruchomienie zgodne z profilem CI:
python Scripts/benchmark_encounter_checkpoint_recovery.py \
  --duration-seconds 3 --workers 2 --warmup-iterations 1 \
  --lease-ttl-seconds 120 \
  --report Artifacts/recovery-benchmark-smoke.json

# Baseline na docelowym komputerze:
python Scripts/benchmark_encounter_checkpoint_recovery.py \
  --duration-seconds 60 --workers 4 --warmup-iterations 3 \
  --report Artifacts/recovery-benchmark-baseline.json

# Obciążenie:
python Scripts/benchmark_encounter_checkpoint_recovery.py \
  --duration-seconds 180 --workers 12 --warmup-iterations 3 \
  --report Artifacts/recovery-benchmark-load.json

# Soak 30 / 60 minut:
python Scripts/benchmark_encounter_checkpoint_recovery.py \
  --duration-seconds 1800 --workers 8 --warmup-iterations 3 \
  --report Artifacts/recovery-benchmark-soak30.json

python Scripts/benchmark_encounter_checkpoint_recovery.py \
  --duration-seconds 3600 --workers 8 --warmup-iterations 3 \
  --report Artifacts/recovery-benchmark-soak60.json
~~~

Walidacja i testy:

~~~bash
python Scripts/validate_encounter_recovery_benchmark.py
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_recovery_benchmark.py" -v
python -m unittest discover -s Scripts/Tests -p "test_encounter_recovery_benchmark.py" -v
~~~

Raport jest JSON-em. Nie zawiera tokenu serwera, lease tokenów, klucza prywatnego ani danych profilu. Katalog Artifacts/ jest lokalnym miejscem na wyniki; nie commituj raportów, jeśli zawierają metadane środowiska, których nie chcesz publikować.

## 6. Jak interpretować wyniki

1. Porównuj wyniki tylko na tej samej maszynie, systemie plików, wersji Python, wersji SQLite i podobnym obciążeniu hosta.
2. Zapisz CPU, RAM, typ i stan nośnika, wersję systemu oraz commit SHA obok raportu. Sam plik JSON raportuje część tych pól.
3. Wykonaj co najmniej trzy Baseline runs i porównaj medianę throughput oraz p95/p99, nie pojedynczy najlepszy wynik.
4. Porównaj początek, środek i koniec Soak30/Soak60. Sprawdź, czy latency lub błędy rosną wraz z czasem.
5. Każdy nieoczekiwany błąd oznacza, że wynik nie jest czystą próbką wydajności. Najpierw wyjaśnij errors_by_operation_and_code.
6. Nie ustawiamy sztywnego progu p95 lub requests/s dla wszystkich środowisk. SQLite na szybkim NVMe i SQLite na wolnym nośniku nie są porównywalnymi targetami.

## 7. Co ten benchmark mierzy — a czego nie

Mierzy realny kod Python checkpoint service, JSON, HTTP loopback, signing/verification Ed25519, SQLite WAL, transakcje, CAS, odczyt i obsługę lease.

Nie mierzy:
- ruchu sieciowego pomiędzy hostami, TLS/mTLS, reverse proxy, WAN lub multi-region;
- produkcyjnego KMS/HSM, rotacji kluczy, zewnętrznej IAM ani infrastruktury kontenerowej;
- pełnego kosztu C++ adaptera Unreal, GameThread, UHT/C++ build, Dedicated Server lub serializacji z runtime;
- GameState replication, actor materialization, AI, grafiki, FPS, pamięci procesu Unreal ani zachowania Android;
- rozproszonego konsensusu lub HA. Backend Stage 33 nadal jest prototypem single-host SQLite WAL.

Benchmark nie jest wynikiem multi-region ani produkcyjnym SLO. Wyniki CI służą wyłącznie jako functional smoke na runnerze GitHub, a nie jako docelowy wynik wydajnościowy gry.

## 8. GitHub Actions

Workflow: .github/workflows/encounter-recovery-benchmarks.yml

Uruchamia:
- walidator profili i testy Stage 39;
- rzeczywisty krótki benchmark HTTP/SQLite i zapis JSON jako artifact;
- regresję Stage 33 signed checkpoint / CAS / lease;
- chaos tests Stage 38 i walidatory kontraktów Stage 32–37.

CI nie uruchamia 30- lub 60-minutowego soak. Te profile należy wykonać lokalnie lub na dedykowanym runnerze, a następnie zachować raport jako artefakt pomiarowy.

## 9. Kryteria akceptacji

- [ ] Smoke działa na rzeczywistym handlerze Stage 33 i tymczasowym SQLite WAL.
- [ ] Każdy write jest potwierdzany signed readback z oczekiwanym checkpointId i sequence.
- [ ] Każdy worker ma osobny EncounterId i lease.
- [ ] Lease jest odnawiany w dłuższym biegu i zwalniany na końcu.
- [ ] Raport zawiera p50/p95/p99, throughput i error counts.
- [ ] Reservoir sample jest ograniczony limitem pamięci.
- [ ] Raport nie zawiera bearer tokenu, lease tokenu, klucza prywatnego ani ProfileId.
- [ ] Macierz profili przechodzi walidator.
- [ ] Stage 33 backend tests i Stage 38 chaos regression przechodzą w CI.
- [ ] Na docelowym hoście wykonano Baseline x3 oraz co najmniej Soak30; wyniki zapisano z metadanymi sprzętu.

## 10. Ograniczenia i dowody

W ramach aktualizacji repozytorium dostarczono harness i test funkcjonalny. Nie wykonano tutaj pełnego 30/60-minutowego soak na docelowym sprzęcie, pomiaru Dedicated Server Unreal ani testu Android. Nie wpisujemy fikcyjnych wartości p95, p99 lub throughput do dokumentacji.

Kolejny etap: **Stage 40 — Recovery Capacity Planning & SLO Baselines**: określić docelowe budżety latency/throughput oraz zasoby hosta na podstawie raportów z reprezentatywnej infrastruktury, a nie arbitralnych liczb.
