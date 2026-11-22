# JUMANJI 2.111.1 — Stage 43: Operator Evidence Intake & Production Readiness Gate

## 1. Cel etapu

Stage 42 zakończył narzędzia do wykonywania staging smoke i restore fencing. Jego readiness manifest nadal poprawnie wskazuje, że rzeczywisty staging, PostgreSQL failover/PITR, key rotation, UE recovery, Android package oraz SLO nie zostały wykazane.

Stage 42 mówi wprost, że Stage 43 powinien rozpocząć się po zebraniu operator evidence packet. W repozytorium nie ma takiego pakietu. Stage 43 nie fabrykuje brakujących dowodów ani nie uruchamia operacji na zewnętrznej infrastrukturze. Dostarcza bezpieczny, weryfikowalny **mechanizm przyjęcia, integralności i niezależnej recenzji** tych dowodów.

Stage 43 dostarcza:

- ścisły JSON Schema dla pakietu Stage 43;
- pusty template z `AWAITING_OPERATOR_EVIDENCE`;
- walidator SHA-256 i bezpiecznego katalogu załączników;
- walidację istniejącego Stage 42 staging report oraz fencing dry-run/apply reports;
- ścisłe summary contracts dla PostgreSQL failover/PITR, rotacji kluczy, Unreal 5.8 Dedicated Server/PIE, Android i Stage 40 SLO;
- blokadę dowodów `ephemeral-ci` jako substytutu rzeczywistego operator staging;
- wymóg dwóch ról recenzenckich do finalnego zatwierdzenia pakietu;
- regresyjne testy bezpieczeństwa i workflow GitHub Actions.

**Stage 43 nie oznacza production-ready, nie zatwierdza HA, nie zatwierdza SLO i nie aktualizuje manifestów Stage 41/42.** Nawet kompletny i zaakceptowany pakiet otrzymuje tylko wynik `ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW`.

## 2. Zasada dowodowa

Każdy dowód musi być rzeczywistym, sanitizowanym artefaktem utworzonym przez operatora/testera. Wpis manifestu zawiera:

- stabilny EvidenceId i wymagany rodzaj dowodu;
- klasę źródła i nieidentyfikujący EnvironmentId;
- datę UTC;
- role wykonującego i niezależnego recenzenta;
- względną ścieżkę do pliku JSON;
- SHA-256 pliku oraz media type.

Walidator otwiera plik tylko z katalogu przekazanego przez `--evidence-root`. Odrzuca path traversal, symlinki, pliki poza tym katalogiem, pliki większe niż 5 MiB, niepoprawny JSON, niezgodny hash i pola, których nazwy wskazują na sekret lub identyfikator użytkownika.

SHA-256 potwierdza integralność załącznika względem packet manifest. Nie dowodzi samodzielnie, kto wykonał operację ani czy środowisko operatora było uczciwe. Role i wynik końcowej recenzji są jawną attestation governance; tożsamość operatorów i kontrola dostępu należą do zewnętrznego procesu operatora.

## 3. Wymagany pakiet — 10 rodzajów dowodu

| Rodzaj | Źródło | Minimalne kryteria |
|---|---|---|
| `staging_smoke` | `operator-staging` | Oryginalny Stage 42 report, `result=PASS`, `environment.class=staging`, PostgreSQL, HTTPS/mTLS i syntetyczny EncounterId |
| `postgres_primary_failover` | `dr-rehearsal` | Nowy primary gotowy, stary writer odrzucony, signed readback i sequence zachowane, epoch zwiększony, zmierzone RTO/RPO |
| `postgres_pitr_restore` | `dr-rehearsal` | Backup/PITR odtworzony do izolowanego celu, write freeze i ingress block przed fencing, podpisany checkpoint odczytany, sequence zachowane, epoch zwiększony |
| `post_restore_fencing_dry_run` | `dr-rehearsal` | Rzeczywisty Stage 42 fencing report w trybie dry-run; zero modyfikacji wierszy/lease/epoch |
| `post_restore_fencing_apply` | `dr-rehearsal` | Rzeczywisty Stage 42 apply report; każdy wiersz fenced, stare lease ownerzy unieważnieni, overflow 0, checkpointy zachowane |
| `signing_key_overlap_rotation` | `dr-rehearsal` | Stary public key nadal weryfikuje stare podpisy, nowy klucz podpisuje nowe checkpointy, overlap potwierdzony |
| `ue58_dedicated_server_recovery` | `ue-dedicated-server` | UE 5.8 Dedicated Server zbudowany i rzeczywiście odtworzył syntetyczny checkpoint; barrier i GameState poprawne, bez podwójnej nagrody |
| `ue58_pie_multiplayer` | `ue-pie-multiplayer` | Co najmniej dwóch klientów PIE, scenariusz restart/reconnect, spójny stan replikowany, brak client restore authority i duplikacji nagrody |
| `android_device_package` | `android-device` | APK/AAB z hash, instalacja na fizycznym urządzeniu i rzeczywisty smoke restore |
| `stage40_slo_approval` | `stage40-slo-approval` | Status profilu APPROVED, 3 baseline runs, co najmniej 30-minutowy soak, host inventory, benchmark i capacity plan związane hashami, dwa zatwierdzające role |

Każdy z tych dowodów występuje dokładnie raz. Walidator nie akceptuje samych deklaracji w opisie PR ani wyniku CI jako dowodu zewnętrznego.

## 4. Artefakty i schematy

Nowe pliki:

- `Data/Encounter/ProductionReadinessEvidencePacket.schema.json` — ścisły JSON Schema, bez dodatkowych nieznanych pól;
- `Data/Encounter/ProductionReadinessEvidencePacket.template.json` — pusty, fail-closed pakiet;
- `Scripts/validate_encounter_checkpoint_stage43_evidence.py` — walidator packetu i załączników;
- `Scripts/Tests/test_validate_encounter_checkpoint_stage43_evidence.py` — testy hash, path traversal, privacy, provenance i decyzji;
- `Scripts/validate_encounter_checkpoint_stage43.py` — statyczny governance validator Stage 43;
- `.github/workflows/encounter-checkpoint-stage43-evidence.yml` — CI dla schema, istniejących granic readiness i nowych testów.

Wszystkie summary JSON muszą mieć dokładny schema/version i `result=PASS`. Wymagane pola claims są walidowane osobno dla każdego rodzaju dowodu. Nieznane pola są odrzucane.

### 4.1 Stage 42 staging report

Dla `staging_smoke` załącz bez przeróbek raport wygenerowany przez:

```bash
python Scripts/run_encounter_checkpoint_staging_drill.py \
  --confirm-staging-mutations \
  --report /secure/evidence/staging-smoke.json

python Scripts/validate_encounter_checkpoint_staging_report.py \
  /secure/evidence/staging-smoke.json
```

Raport musi wskazywać `environment.class=staging`. Raport z `ephemeral-ci` jest zawsze odrzucany jako dowód realnego staging.

### 4.2 Fencing reports

Dry-run i apply muszą pochodzić z dwóch rzeczywistych uruchomień Stage 42 tool na tym samym, odizolowanym recovery target:

```bash
python Scripts/fence_checkpoint_leases_after_restore.py \
  --report /secure/evidence/fencing-dry-run.json

# Po ocenie dry-run, zatrzymaniu wszystkich writerów i blokadzie ingress:
python Scripts/fence_checkpoint_leases_after_restore.py \
  --apply \
  --confirm-restored-target \
  --report /secure/evidence/fencing-apply.json
```

Walidator wymaga dry-run bez mutacji. Apply musi zwiększyć epoch każdego wiersza i unieważnić wszystkie istniejące lease owners. Stage 42 runbook nadal obowiązuje: zatrzymanie writerów i blokada ingress muszą pozostać aktywne do zakończenia ćwiczenia.

### 4.3 Summary evidence dla pozostałych systemów

Operatorzy dostarczają sanitizowane JSON zgodne z poniższymi schematami:

- `JUMANJI.PostgresPrimaryFailoverEvidence.v1`
- `JUMANJI.PostgresPitrRestoreEvidence.v1`
- `JUMANJI.SigningKeyOverlapEvidence.v1`
- `JUMANJI.UE58DedicatedServerRecoveryEvidence.v1`
- `JUMANJI.UE58PIMultiplayerRecoveryEvidence.v1`
- `JUMANJI.AndroidDeviceRecoveryEvidence.v1`
- `JUMANJI.Stage40SLOApprovalEvidence.v1`

Każdy summary ma dokładnie pola `schema`, `result`, `environmentId`, `capturedAtUtc`, `claims`. Szczegółowe klucze `claims` są egzekwowane przez walidator i testy. Nie dołączaj surowego logu z poświadczeniami ani całej podpisanej koperty.

## 5. Tworzenie packet manifest

Pracuj na kopii template w bezpiecznym katalogu operatora — nie wpisuj rzeczywistych danych do repository template:

```bash
cp Data/Encounter/ProductionReadinessEvidencePacket.template.json \
  /secure/stage43/packet.json
mkdir -p /secure/stage43/evidence
```

Dla każdego wpisu:

1. Przekopiuj zatwierdzony, sanitizowany JSON do `/secure/stage43/evidence/`.
2. Oblicz SHA-256 oryginalnego pliku.
3. Dodaj wpis z wymaganym `kind`, `sourceClass`, `environmentId`, `capturedAtUtc`, rolą operatora, rolą recenzenta i relative path do pliku.
4. Nie dodawaj osobistych nazw, e-maili, ProfileId, participantKey, tokenów, certyfikatów prywatnych, DSN, connection stringów ani treści checkpoint envelope.
5. Po komplecie wpisów ustaw `submissionStatus=SUBMITTED_FOR_REVIEW`.
6. Po niezależnej recenzji co najmniej dwóch ról, w tym Security albo Database Reviewer, ustaw `decision=APPROVED`, `reviewerRoles`, `reviewedAtUtc` i `submissionStatus=REVIEWED`.

Przykładowe pole artifact:

```json
{
  "path": "staging-smoke.json",
  "sha256": "<64 lowercase hex characters>",
  "mediaType": "application/json"
}
```

Placeholder hash nie przejdzie walidacji — musi odpowiadać dokładnym bajtom dołączonego pliku.

## 6. Uruchomienie gate

```bash
python Scripts/validate_encounter_checkpoint_stage43_evidence.py \
  --packet /secure/stage43/packet.json \
  --evidence-root /secure/stage43/evidence \
  --report /secure/reports/stage43-gate.json
```

Dla kontrolowanej próby wymagającej kompletnego pakietu:

```bash
python Scripts/validate_encounter_checkpoint_stage43_evidence.py \
  --packet /secure/stage43/packet.json \
  --evidence-root /secure/stage43/evidence \
  --report /secure/reports/stage43-gate.json \
  --require-eligible
```

Exit codes:

- `0` — packet poprawny strukturalnie; może nadal być zablokowany przez brakujące dowody lub oczekiwać na recenzję;
- `1` — niepoprawny packet, hash, privacy, schema lub istniejąca granica readiness;
- `2` — `--require-eligible` i packet nie osiągnął `ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW`.

Raport gate jest sanitizowany i nie kopiuje treści załączników. Zawiera zaakceptowane rodzaje dowodów, braki, kody odrzuceń i wynik.

## 7. Decyzje gate

| Outcome | Znaczenie |
|---|---|
| `INVALID_PACKET` | Naruszony schema, integralność, prywatność lub granice projektu |
| `BLOCKED_MISSING_EVIDENCE` | Co najmniej jeden z 10 wymaganych dowodów nie istnieje lub został odrzucony |
| `WAITING_FOR_INDEPENDENT_REVIEW` | Wszystkie dowody są poprawne, lecz brak recenzji przez dwie niezależne role |
| `REJECTED_BY_REVIEW` | Niezależna recenzja odrzuciła pakiet |
| `ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW` | Wszystkie wymagane dowody są poprawne i pakiet został niezależnie zaakceptowany |

Ostatni stan **nie jest** zgodą na deployment. Oznacza jedynie, że materiał można przekazać do odrębnej finalnej decyzji release board. `promotionAllowed` pozostaje false.

## 8. Bezpieczeństwo i fail-closed

Walidator odrzuca:

- report staging z `ephemeral-ci`;
- dry-run, który zmienił choć jeden epoch/lease;
- apply, który nie sfence'ował każdego wiersza lub nie zachował liczby checkpointów;
- niezgodne EnvironmentId między packet a DB evidence;
- brak rzeczywistego podpisanego readback, failover/PITR lub key rotation claim;
- UE evidence bez wersji 5.8, Dedicated Server build albo minimum dwóch klientów PIE;
- Android evidence bez fizycznego urządzenia i hash pakietu;
- SLO evidence bez 3 baseline runs, 30-minutowego soak, host inventory/benchmark/capacity hashes i dwóch ról zatwierdzających;
- pliki zawierające pola o nazwach wskazujących na token, sekret, klucz prywatny, raw ProfileId lub participantKey;
- path traversal, symlink, niespójny SHA-256 lub plik spoza evidence root.

Stage41 `productionReady=false`, `highAvailabilityApproved=false`, `sloStatus=UNBASELINED` i Stage42 wszystkie external evidence flags pozostają fail-closed. Stage43 nie modyfikuje tych manifestów automatycznie — nawet po pozytywnej recenzji.

## 9. CI i kryteria akceptacji

Workflow:

`.github/workflows/encounter-checkpoint-stage43-evidence.yml`

CI wykonuje:

- Stage41 production boundary validator;
- Stage42 staging/DR validator;
- Stage43 static governance validator;
- testy Stage41/42 governance;
- testy Stage43 dla brakujących dowodów, sha mismatch, path traversal, sensitive data, ephemeral-vs-staging, fencing invariants i dwurolo-wej recenzji.

CI korzysta z pustego template i tymczasowego katalogu dowodów. Oczekiwany wynik to `BLOCKED_MISSING_EVIDENCE`, a nie production PASS.

Akceptacja Stage43 wymaga:

- [ ] Wszystkie 10 typów operator evidence złożone w zewnętrznym secure evidence store.
- [ ] Każdy plik ma zweryfikowany SHA-256 i przechodzi odpowiedni schema/claim validator.
- [ ] Staging report pochodzi z realnego operator staging, nie z CI ephemeral.
- [ ] Failover, PITR, fencing i signing-key overlap zostały faktycznie wykonane.
- [ ] UE 5.8 Dedicated Server, multiplayer PIE i Android physical-device recovery zostały faktycznie wykonane.
- [ ] Stage40 SLO evidence zawiera reprezentatywne baseline/soak/host inventory oraz formalne zatwierdzenie targetów.
- [ ] Finalna recenzja obejmuje co najmniej dwie różne role, w tym Security albo Database Reviewer.
- [ ] Release board podejmuje osobną decyzję po inspekcji sanitizowanego raportu.

## 10. Status faktyczny

W chwili utworzenia Stage43 packet template jest pusty, a wynik gate jest `BLOCKED_MISSING_EVIDENCE`. Repozytorium nie zawiera rzeczywistego pakietu operatora Stage42. Stage42 readiness manifest pozostaje:

- `productionReady=false`;
- `highAvailabilityApproved=false`;
- `sloStatus=UNBASELINED`;
- real staging, failover, PITR, fencing apply, key rotation, UE i Android evidence flags = false.

Nie wykonano przez Stage43 żadnego realnego wdrożenia staging, failover/PITR, key rotation, UE build/PIE, Android packaging ani testu urządzenia. To są obowiązki operatora i wymagają zewnętrznych zasobów.

Następny krok po zebraniu packetu to niezależna inspekcja operator evidence i osobna decyzja release board — nie automatyczne ustawienie production readiness przez CI.
