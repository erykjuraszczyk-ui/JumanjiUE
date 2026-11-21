# JUMANJI 2.111.1 — Stage 40: Recovery Capacity Planning & SLO Baselines

## 1. Cel

Stage 40 przekształca raporty Stage 39 w kontrolowany, powtarzalny proces planowania pojemności checkpoint recovery.

Dostarcza:
- jawne profile pomiarowe i wymagania minimalne dla raportów;
- walidator governance dla progów SLO;
- kalkulator konserwatywnego limitu równoczesnych encounterów;
- szablon inwentaryzacji hosta, dysku i wersji środowiska;
- testy raportów prawidłowych, błędnych i niekompletnych;
- workflow CI, który wykonuje wyłącznie krótki test smoke i sprawdza cały przepływ planowania.

**Stage 40 nie zatwierdza żadnego produkcyjnego SLO i nie wpisuje fikcyjnych wartości p95/p99 ani throughput.** Profile w repozytorium pozostają UNBASELINED, dopóki nie zostaną uzupełnione rzeczywistym dowodem z reprezentatywnej infrastruktury, recenzją i datą zatwierdzenia.

## 2. Relacja do Stage 39

Stage 39 mierzy rzeczywistą ścieżkę Python/HTTP/Ed25519/SQLite WAL na pojedynczym hoście. Raport zawiera m.in.:
- liczbę workerów i czas pomiaru;
- liczbę udanych par checkpoint write + signed readback;
- throughput par na sekundę;
- błędy requestów i błędy startu workerów;
- p50/p95/p99 osobno dla checkpoint_write i checkpoint_read;
- liczbę próbek percentyli;
- metadane Python, SQLite, procesora, systemu i trybu journal.

Stage 40 przyjmuje ten JSON jako wejście. Nie zmienia Stage 32 schema, Stage 33 protokołu lease/CAS ani kodu runtime Unreal.

## 3. Artefakty

### Profile i inwentaryzacja

- Data/Encounter/EncounterRecoverySLOProfiles.csv — profile CI smoke, local baseline, soak 30 min i soak 60 min.
- Data/Encounter/EncounterRecoveryHostInventoryTemplate.json — szablon anonimowej inwentaryzacji sprzętu/środowiska.

### Narzędzia

- Scripts/validate_encounter_recovery_capacity_slo.py — kontrola profili i reguł zatwierdzania progów.
- Scripts/plan_encounter_recovery_capacity.py — waliduje raport Stage 39 i buduje plan pojemności.
- Scripts/Tests/test_validate_encounter_recovery_capacity_slo.py — testy governance SLO.
- Scripts/Tests/test_encounter_recovery_capacity_planning.py — testy planera z jawnie syntetycznymi raportami testowymi.
- .github/workflows/encounter-recovery-capacity-slo.yml — testy oraz realny, krótkotrwały smoke Stage 39 → Stage 40.

Raporty syntetyczne z testów jednostkowych nie są publikowane ani traktowane jako pomiar sprzętu.

## 4. Profile SLO

Plik EncounterRecoverySLOProfiles.csv zawiera:

| Pole | Znaczenie |
|---|---|
| CheckpointCadenceSeconds | Zakładana częstotliwość checkpointu jednego encounteru |
| UtilizationFactor | Rezerwa pojemności pozostawiana ponad planowaną pracę |
| MinimumDurationSeconds | Minimalny czas rzeczywistego benchmarku |
| MinimumWorkers | Minimalna liczba równoczesnych workerów w teście |
| MinimumCompletedPairs | Minimalna liczba poprawnych write/readback |
| MinimumLatencySamplesPerOperation | Minimalna liczba próbek dla każdego p95/p99 |
| Target* | Budżety latency/throughput zatwierdzone dopiero po zebraniu dowodów |
| Status | UNBASELINED, PROPOSED, APPROVED lub RETIRED |
| EvidenceId | Identyfikator raportu lub pakietu pomiarowego |
| Approver / ReviewedAtUtc | Recenzent i data zatwierdzenia |

Profile startowe:
- CI-SMOKE — tylko kontrola działania narzędzi na runnerze GitHub.
- LOCAL-BASELINE — trzy powtarzalne pomiary 60 sekund na tym samym hoście i nośniku.
- LOCAL-SOAK30 — 30-minutowy test długotrwały z odnowieniem lease.
- LOCAL-SOAK60 — 60-minutowy test długotrwały i porównanie początku/środka/końca.

Wszystkie progi docelowe p95, p99 i pairs/s pozostają puste. UNBASELINED nie może zawierać żadnych wartości target, EvidenceId ani danych zatwierdzenia. Profil PROPOSED lub APPROVED musi mieć wszystkie targety dodatnie, EvidenceId, recenzenta i datę ISO-8601. Budżet p95 nie może być większy od odpowiadającego mu p99.

## 5. Inwentaryzacja hosta

Przed porównaniem raportów należy przypisać je do konkretnego, anonimowego fingerprintu sprzętu.

Skopiuj EncounterRecoveryHostInventoryTemplate.json do lokalnego katalogu pomiarowego i uzupełnij:
- nieidentyfikujący hostId;
- rolę hosta i czas pomiaru;
- OS, CPU, liczbę rdzeni fizycznych/logicznych i RAM;
- model/typ nośnika oraz filesystem;
- wersję Python i SQLite;
- commit benchmarku i SHA-256 raportu.

Nie wpisuj hostname, adresów IP, identyfikatorów kont, ProfileId ani sekretów. Szablon w repozytorium ma wartości null/placeholder i nie jest gotowym raportem.

Planner sprawdza kompletność inwentaryzacji oraz zgodność wersji Python/SQLite z raportem. Jeżeli podano inwentaryzację, jej benchmarkReportSha256 musi być identyczny z SHA-256 wskazanego pliku raportu. Chroni to przed przypadkowym przypięciem innego raportu do sprzętu.

## 6. Uruchomienie lokalne

Z katalogu głównego repozytorium:

~~~bash
python -m pip install -r Backend/EncounterCheckpointService/requirements.txt

# Trzy powtórzenia baseline na tej samej maszynie:
python Scripts/benchmark_encounter_checkpoint_recovery.py \
  --duration-seconds 60 --workers 4 --warmup-iterations 3 \
  --report Artifacts/baseline-01.json

python Scripts/benchmark_encounter_checkpoint_recovery.py \
  --duration-seconds 60 --workers 4 --warmup-iterations 3 \
  --report Artifacts/baseline-02.json

python Scripts/benchmark_encounter_checkpoint_recovery.py \
  --duration-seconds 60 --workers 4 --warmup-iterations 3 \
  --report Artifacts/baseline-03.json

# Utwórz plan z jednego raportu pomiarowego:
python Scripts/plan_encounter_recovery_capacity.py \
  --report Artifacts/baseline-03.json \
  --profile LOCAL-BASELINE \
  --output Artifacts/capacity-plan-baseline-03.json
~~~

Po uzupełnieniu lokalnego szablonu hosta dodaj:

~~~bash
--host-inventory Artifacts/host-inventory.json
~~~

Dla soak30/soak60 użyj komend Stage 39 z czasem 1800/3600 sekund i odpowiednio większą liczbą workerów. Planer odrzuci raport, który nie spełnia minimalnego czasu, workerów, liczby par, próbek latency, trybu WAL lub zawiera jakikolwiek błąd.

Walidacja repozytorium:

~~~bash
python Scripts/validate_encounter_recovery_capacity_slo.py
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_recovery_capacity_slo.py" -v
python -m unittest discover -s Scripts/Tests -p "test_encounter_recovery_capacity_planning.py" -v
~~~

## 7. Jak powstaje plan pojemności

Planer oblicza trzy ograniczenia:

1. throughputLimitedEncounters — przepustowość udanych par write/readback × interwał checkpointu.
2. latencyLimitedEncounters — interwał w milisekundach podzielony przez sumę p99 osobnych operacji write i read.
3. testedWorkerLimit — liczba workerów faktycznie obciążających backend w raporcie Stage 39.

Następnie stosuje współczynnik wykorzystania i wybiera najmniejszy wynik:

~~~text
projectedSafeConcurrentEncounters =
  max(1, floor(
    min(
      pairRate * checkpointCadence,
      checkpointCadenceMs / (writeP99 + readP99),
      testedWorkers
    ) * utilizationFactor
  ))
~~~

To **konserwatywny wskaźnik planistyczny**, a nie model kolejkowania ani gwarantowana pojemność produkcyjna.

Ważne granice:
- planner nigdy nie ekstrapoluje ponad liczbę workerów przetestowanych w raporcie;
- suma p99 write + p99 read jest proxy planistycznym, nie zmierzonym percentylem całej pary;
- współczynnik utilization jest rezerwą przyjętą w profilu, a nie wynikiem pomiaru;
- każdy błąd benchmarku dyskwalifikuje raport;
- wynik z CI-SMOKE nie jest dowodem wydajności sprzętu docelowego.

Wynik JSON ma evidenceClass=STAGE39_SINGLE_HOST_SQLITE_WAL_LOOPBACK, zawiera SHA-256 raportu, fingerprint środowiska, ograniczenia, założenia, wynik planowania i status oceny SLO. Nie zawiera credentiali, lease tokenów, kluczy, ProfileId ani checkpoint payload.

## 8. Ocena SLO

Możliwe wyniki:
- NOT_ASSESSED — profil UNBASELINED lub PROPOSED; żaden SLO nie jest deklarowany jako spełniony.
- PASS — profil APPROVED, a wszystkie zmierzone write/read p95/p99 i pairs/s mieszczą się w zatwierdzonych budżetach.
- FAIL — profil APPROVED, ale co najmniej jeden budżet nie został spełniony.

approvalGate.approved wymaga zarówno statusu APPROVED, jak i kompletnej inwentaryzacji hosta. Samo przejście testu CI nie zatwierdza SLO.

Nie zatwierdzaj targetów tylko dlatego, że pomiar je osiągnął. Budżet musi wynikać z umowy produktu/rozgrywki (ile może trwać checkpoint/recovery), przewidywanego obciążenia, zapasu pojemności i powtarzalnych pomiarów na reprezentatywnym hoście.

## 9. GitHub Actions

Workflow: .github/workflows/encounter-recovery-capacity-slo.yml

Wykonuje:
- walidację profili i host inventory template;
- testy governance oraz planera;
- regresję walidatora/benchmarku Stage 39;
- 3-sekundowy prawdziwy benchmark na lokalnym runnerze;
- wygenerowanie planu dla CI-SMOKE;
- upload smoke report i capacity plan jako artifact.

GitHub-hosted runner jest środowiskiem testowym, nie docelowym serwerem gry. Workflow nie uruchamia soak30/soak60 i nie zatwierdza produkcyjnych SLO.

## 10. Kryteria akceptacji

- [ ] Profile SLO przechodzą walidator.
- [ ] Żaden profil nie deklaruje niezmierzonych targetów jako APPROVED.
- [ ] Planner akceptuje tylko czyste raporty Stage 39 z odpowiednią liczbą workerów/próbek.
- [ ] Planner odrzuca raport z błędami, zbyt krótkim czasem i niewłaściwym journal mode.
- [ ] Limit projected concurrency nigdy nie przekracza liczby przetestowanych workerów.
- [ ] Raport/inventory są związane przez SHA-256.
- [ ] Host inventory nie zawiera danych identyfikujących ani sekretów.
- [ ] Testy Stage 40 i workflow CI przechodzą.
- [ ] Trzy baseline runs na tym samym hoście są zachowane i porównane.
- [ ] Soak30 lub Soak60 jest wykonany na reprezentatywnym sprzęcie przed zatwierdzeniem SLO.
- [ ] Targety p95/p99/throughput są formalnie zatwierdzone z EvidenceId, recenzentem i datą.
- [ ] UE 5.8 Dedicated Server/PIE, sieć i Android są mierzone osobno przed ogłoszeniem SLO całej gry.

## 11. Ograniczenia i dowody

Stage 40 dostarcza narzędzia planowania, governance i CI smoke. Nie wykonano w tej zmianie benchmarku na docelowym sprzęcie, trzech powtórzeń baseline, soak30/soak60, pomiarów WAN/TLS/mTLS, produkcyjnego storage, Unreal Dedicated Server, multiplayer PIE ani Android.

**Nie ma zatwierdzonych wartości p95, p99, throughput ani pojemności produkcyjnej.** Nie należy utożsamiać planu z jednego raportu SQLite loopback z liczbą obsługiwanych graczy MMO.

## Następny etap

**Stage 41 — Production Checkpoint Store Adapter & Deployment Hardening**: zaprojektować i zaimplementować adapter trwałego magazynu oraz wdrożenie zewnętrznego checkpoint service z TLS/mTLS, rotacją kluczy, migracjami, backup/restore, monitoringiem i testami na środowisku staging. Prototyp SQLite WAL pozostaje lokalnym/testowym providerem.
