# JUMANJI 2.111.1 — Stage 46: Controlled Deployment Orchestration & Rollback Readiness

## 1. Cel etapu

Stage 45 weryfikuje trzy niezależne podpisy Release Board i wiąże decyzję z
konkretnymi bajtami kandydata wydania. Stage 46 zamienia tę autoryzację
w kontrolowany plan operacyjny, ale **nie wykonuje wdrożenia**.

Etap dodaje:

- ponowną, kryptograficzną ocenę decyzji Stage 45 zamiast zaufania samemu
  plikowi raportu;
- ścisły manifest poprzedniego, działającego wydania produkcyjnego;
- kontrolę SHA-256, rozmiaru, typu pliku, ścieżki i zestawu platform dla
  rollback baseline;
- plan związany z hashami manifestu kandydata, decyzji Release Board,
  poprzedniego wydania i jego artefaktów;
- sekwencję bramek PREFLIGHT → CANARY → PROGRESSIVE_ROLLOUT →
  POST_DEPLOYMENT_VERIFICATION;
- fail-closed CLI `prepare` i `verify`, bez komend wykonujących deployment;
- schematy, puste szablony, testy regresji i CI.

## 2. Granica bezpieczeństwa

Tryb planu to wyłącznie `MANUAL_OUT_OF_BAND`. Narzędzie:

- nie łączy się z Kubernetes, SSH, chmurą, CDN ani sklepami mobilnymi;
- nie pobiera sekretów, tokenów, certyfikatów ani poświadczeń;
- nie kopiuje i nie publikuje artefaktów;
- nie restartuje serwerów i nie zmienia ruchu użytkowników;
- nie wykonuje automatycznego rollbacku ani retry;
- nie zmienia statusu production readiness, HA, SLO ani promotion.

Każda utworzona plansza ma niezmiennie:

- `sideEffectsPerformed=false`;
- `deploymentExecuted=false`;
- `postDeploymentVerified=false`;
- `rollbackExecuted=false`;
- `productionReady=false`;
- `highAvailabilityApproved=false`;
- `sloStatus=UNBASELINED`;
- `promotionAllowed=false`.

To celowe. Plan gotowy do przeglądu nie jest dowodem wdrożenia ani jego
poprawnego działania. Rzeczywiste wykonanie musi nastąpić w zatwierdzonym,
zewnętrznym procesie operatorskim.

## 3. Wymagane wejście

### 3.1 Release Board

Stage 46 ponownie uruchamia `encounter_checkpoint_release_board.evaluate()`
na następujących danych:

- Stage 43 evidence packet i katalog jego dowodów;
- Stage 44 attestation;
- publiczny trust keyring oraz hash pobrany niezależnym kanałem;
- manifest kandydata i katalog dokładnych plików UE 5.8 / Android;
- decyzja Release Board z trzema podpisami Ed25519.

Wynik musi być dokładnie `APPROVED_FOR_DEPLOYMENT`, a role
`SERVICE_OWNER`, `SECURITY_REVIEWER` i `PLATFORM_REVIEWER` muszą być
zweryfikowane. Sam tekstowy Stage 45 report nie jest traktowany jako źródło
zaufania.

### 3.2 Rollback baseline

Schemat: `Data/Encounter/ProductionRollbackBaseline.schema.json`

Szablon: `Data/Encounter/ProductionRollbackBaseline.template.json`

Operator tworzy go na podstawie faktycznie wdrożonego, ostatniego stabilnego
wydania produkcyjnego. Wymagane są:

- `baselineId`, środowisko `production`, czas UTC i source commit;
- artefakt poprzedniego UE 5.8 Dedicated Server;
- poprzedni Android APK/AAB;
- relatywna ścieżka, SHA-256, liczba bajtów i media type każdego pliku.

Rollback baseline musi być wcześniejszy niż utworzenie kandydata. Zestaw
platform musi odpowiadać zestawowi platform nowego kandydata. Skrypt sprawdza
pliki względem `--rollback-artifact-root`; odrzuca ścieżki wychodzące poza
katalog, symlinki, różne rozmiary i różne hashe.

Szablon w repozytorium pozostaje pusty. Nie ma w nim rzeczywistych buildów ani
danych środowiska produkcyjnego.

## 4. Plan wdrożenia

Schemat: `Data/Encounter/ProductionDeploymentPlan.schema.json`

Szablon: `Data/Encounter/ProductionDeploymentPlan.template.json`

Plan zawiera:

- identyfikator i hash kandydata, source commit, build ID, engine version
  oraz hashe artefaktów;
- ID decyzji Release Board i hash jej dokładnych bajtów;
- zweryfikowane role komisji;
- hash całego rollback baseline oraz hashe poprzednich artefaktów;
- cztery fazy operatorskie i wymagane kategorie dowodów;
- zasadę zatrzymania wdrożenia przy nieudanej bramce;
- obowiązek ręcznego przywrócenia ostatniego stabilnego wydania.

Stage 46 celowo nie narzuca procentów canary, rozmiaru partii, progów p95/p99,
limitów błędów ani okna maintenance. Te parametry zależą od rzeczywistej
topologii, liczby instancji i zatwierdzonych SLO. Nie wolno ich zgadywać.

## 5. Uruchomienie operatora

Wszystkie rzeczywiste pliki należy trzymać poza repozytorium, w chronionym
katalogu operatora. Nie wolno używać syntetycznych fixture CI jako dowodów.

Przygotuj plan:

```bash
python Scripts/encounter_checkpoint_deployment.py prepare \
  --packet /secure/evidence/packet.json \
  --evidence-root /secure/evidence \
  --attestation /secure/evidence/attestation.json \
  --keyring /secure/trust/production-evidence-keyring.json \
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \
  --candidate /secure/release/candidate.json \
  --artifact-root /secure/release/artifacts \
  --decision-file /secure/release/board-decision.json \
  --rollback-baseline /secure/rollback/last-known-good.json \
  --rollback-artifact-root /secure/rollback/artifacts \
  --plan-file /secure/release/deployment-plan.json
```

Polecenie zapisuje plan create-only. Jeżeli plik wyjściowy już istnieje,
operacja kończy się błędem zamiast nadpisywać zatwierdzony plan.

Przed przekazaniem planu operatorom zweryfikuj wszystkie powiązania ponownie:

```bash
python Scripts/encounter_checkpoint_deployment.py verify \
  --packet /secure/evidence/packet.json \
  --evidence-root /secure/evidence \
  --attestation /secure/evidence/attestation.json \
  --keyring /secure/trust/production-evidence-keyring.json \
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \
  --candidate /secure/release/candidate.json \
  --artifact-root /secure/release/artifacts \
  --decision-file /secure/release/board-decision.json \
  --rollback-baseline /secure/rollback/last-known-good.json \
  --rollback-artifact-root /secure/rollback/artifacts \
  --plan-file /secure/release/deployment-plan.json
```

Wynik `PLAN_VERIFIED_NO_SIDE_EFFECTS` potwierdza zgodność planu z aktualnym
materiałem wejściowym. Nie oznacza, że którykolwiek etap wdrożenia został
wykonany.

## 6. Sekwencja operatorska po zatwierdzeniu planu

| Faza | Przykładowe wymagane potwierdzenia | Warunek kontynuacji |
|---|---|---|
| PREFLIGHT | okno zmiany, backup/restore point, identyfikacja operatora i obserwatora, health przed zmianą, dostępność rollback artefaktów | wszystkie potwierdzenia przyjęte |
| CANARY | tożsamość canary, health serwera, signed checkpoint write/read smoke, obserwacja operatora | niezależny operator zatwierdza przejście |
| PROGRESSIVE_ROLLOUT | akceptacja każdej partii, health usługi i bazy, lease fencing/recovery smoke, kompatybilność klienta | każda partia przechodzi kontrolę |
| POST_DEPLOYMENT_VERIFICATION | start Dedicated Server, wejście do encounter, wznowienie checkpointu, test Android, backup po zmianie i akceptacja dwóch osób | komplet zewnętrznych dowodów |

Nie ma automatycznej kontynuacji między fazami. Nieudana bramka oznacza
zatrzymanie i eskalację. Decyzję o rollback podejmuje uprawniony operator.
Rollback używa polityki `MANUAL_REDEPLOY_LAST_KNOWN_GOOD`: przywraca dokładnie
artefakty z zatwierdzonego baseline i ponownie sprawdza ich hash przed użyciem.

## 7. Walidacja i testy

- `Scripts/encounter_checkpoint_deployment.py` — Stage 45 re-verification,
  rollback hash validation, plan builder i plan verifier.
- `Scripts/validate_encounter_checkpoint_stage46_deployment.py` — walidator
  schematów, szablonów, dokumentacji, testów i granic bezpieczeństwa.
- `Scripts/Tests/test_validate_encounter_checkpoint_stage46_deployment.py`
  — syntetyczne pozytywne i negatywne przypadki.
- `.github/workflows/encounter-checkpoint-stage46-deployment.yml` — CI
  Stage 43–46; nie wykonuje deploymentu.

Testy pokrywają brak/odrzucenie zgody Release Board, mutację artefaktu,
niezgodny hash rollback, path traversal, niezgodny zestaw platform, zmianę
decyzji i mutację gotowego planu. CI wykorzystuje syntetyczne klucze i pliki.

## 8. Stan faktyczny i kryteria akceptacji

- [ ] Dostarczono rzeczywisty, niezależnie zatwierdzony pakiet Stage 43.
- [ ] Stage 44 attestation i zewnętrzny trust-keyring hash są poprawne.
- [ ] Stage 45 ma trzy podpisy odrębnych członków Release Board.
- [ ] Kandydat zawiera rzeczywisty UE 5.8 Dedicated Server i APK/AAB.
- [ ] Poprzedni produkcyjny baseline obejmuje te same platformy.
- [ ] Wszystkie bieżące i rollback artefakty zgadzają się z SHA-256 oraz rozmiarem.
- [ ] Plan przechodzi ponowną weryfikację tuż przed oknem zmiany.
- [ ] Operator i niezależny obserwator zatwierdzili procedurę i rollback.
- [ ] Zewnętrzne wdrożenie i testy po wdrożeniu zostały wykonane i udokumentowane.

W momencie przygotowania etapu repozytorium nie zawiera prawdziwego pakietu
Stage 43, produkcyjnego trust keyringu, podpisów Release Board, builda UE 5.8,
APK/AAB ani poprzedniego wydania produkcyjnego. Nie wykonano deploymentu ani
testu po wdrożeniu. Puste szablony pozostają zablokowane.

**Stage 46 dostarcza kontrolę i gotowy plan operacyjny — nie produkcyjne
wdrożenie.** `deploymentExecuted=false`, `productionReady=false`,
`sloStatus=UNBASELINED` i `promotionAllowed=false` pozostają bez zmian.
