# JUMANJI 2.111.1 — Stage 47: Post-Deployment Verification & Evidence Reconciliation

## 1. Cel etapu

Stage 46 przygotowuje niezmienny, operator-gated deployment plan. Stage 47
zamyka lukę pomiędzy planem a dowodami z rzeczywiście wykonanego procesu
operatorskiego: przyjmuje dziennik czterech faz, wiąże go z dokładnymi bajtami
planu i artefaktów, sprawdza załączniki oraz weryfikuje dwa niezależne podpisy
Ed25519.

Stage 47 dostarcza:

- ścisły schemat dziennika wykonania i pusty szablon;
- ścisły envelope podpisów SERVICE_OPERATOR + INDEPENDENT_REVIEWER;
- offline CLI do podpisywania i weryfikacji dziennika;
- kontrole hashy planu, kandydata, rollback baseline i każdego załącznika;
- kontrolę kolejności czterech faz, kompletności dowodów, znaczników czasu,
  ścieżek, symlinków i rozmiarów;
- raport weryfikacyjny, który odnotowuje wykonanie deploymentu, ale nie
  zatwierdza produkcji, HA, SLO ani promocji wydania;
- testy z syntetycznymi kluczami i GitHub Actions bez dostępu do środowiska.

## 2. Granica bezpieczeństwa

Narzędzie Stage 47 jest weryfikatorem offline. Nie wykonuje deploymentu,
rollbacku, retry, restartu, zmiany routingu ani publikacji artefaktów. Nie
łączy się z Kubernetes, SSH, chmurą, CDN, sklepem mobilnym ani serwerami gry.
Nie pobiera sekretów i nie przechowuje kluczy prywatnych.

Dziennik jest tworzony przez uprawnionego operatora dopiero po wykonaniu
zatwierdzonej procedury poza repozytorium. Podpis kryptograficzny potwierdza
tożsamość klucza i integralność deklaracji; sam w sobie nie dowodzi, że
operator wykonał czynność poprawnie. Dlatego wymagany jest osobny,
niezależny podpis INDEPENDENT_REVIEWER.

Wynik POST_DEPLOYMENT_EVIDENCE_VERIFIED oznacza wyłącznie, że przekazany
zestaw jest kompletny, niezmieniony i podpisany przez dwa zaufane klucze.
Wynik nie zmienia żadnego statusu planu Stage 46 i zawsze zawiera:

- deploymentExecuted=true — potwierdzone w dzienniku zewnętrzne wykonanie;
- postDeploymentVerified=true — komplet wymaganych dowodów przechodzi walidację;
- rollbackExecuted=false — dopuszczony jest wyłącznie udany rollout bez rollbacku;
- productionReady=false;
- highAvailabilityApproved=false;
- sloStatus=UNBASELINED;
- promotionAllowed=false;
- sideEffectsPerformedByVerifier=false.

Nieudany rollout albo wykonany rollback nie może zostać przedstawiony jako
udane zamknięcie. Wymaga osobnego, zatwierdzonego procesu incydentowego i
nowego planu wydania.

## 3. Schematy i wejścia

### Dziennik wykonania

- Schemat: Data/Encounter/ProductionDeploymentEvidenceJournal.schema.json
- Szablon: Data/Encounter/ProductionDeploymentEvidenceJournal.template.json

Dziennik musi zawierać:

- UUID journalId i planId;
- SHA-256 dokładnych bajtów planu Stage 46;
- SHA-256 kandydata i rollback baseline skopiowane z planu;
- środowisko production i tryb MANUAL_OUT_OF_BAND;
- wynik SUCCEEDED oraz rollbackOutcome=NOT_REQUIRED;
- czasy UTC rozpoczęcia i zakończenia;
- cztery fazy w kolejności Stage 46: PREFLIGHT, CANARY,
  PROGRESSIVE_ROLLOUT, POST_DEPLOYMENT_VERIFICATION;
- status PASSED i kompletną listę wymaganych kategorii dowodów dla każdej fazy.

Każdy rekord dowodu zawiera UUID, kategorię, ścieżkę względną, SHA-256,
rozmiar i media type. Załączniki są przechowywane poza repozytorium, pod
jednym katalogiem --evidence-root. Dopuszczalny rozmiar jednego pliku to
1–25 MiB. Ścieżki absolutne, .., symlinki, powtórzone ścieżki oraz pliki
poza katalogiem dowodów są odrzucane.

### Podpisy dziennika

- Schemat: Data/Encounter/ProductionDeploymentEvidenceAttestation.schema.json
- Szablon: Data/Encounter/ProductionDeploymentEvidenceAttestation.template.json

Envelope podpisów wiąże SHA-256 dokładnego dziennika i dokładnego planu oraz
używa kanonikalizacji RFC 8785. Wymagane są dwa różne klucze:

- SERVICE_OPERATOR — osoba wykonująca i raportująca zmianę;
- INDEPENDENT_REVIEWER — niezależna osoba sprawdzająca pakiet dowodowy.

Klucze muszą być ACTIVE, mieć odpowiednią rolę w Stage 44 publicznym
keyringu i być ważne w momencie podpisu. Hash całego pliku keyringu musi
zgadzać się z fingerprintem przekazanym niezależnym kanałem. Jeden klucz nie
może podpisywać obu ról.

## 4. Wymagane kategorie dowodów

| Faza | Kategorie |
|---|---|
| PREFLIGHT | approved_change_window, production_backup_and_restore_point, operator_and_observer_identity, pre_change_service_health, rollback_baseline_retrieval_check |
| CANARY | canary_target_identity, canary_service_health, checkpoint_write_read_and_signature_smoke, operator_canary_observation |
| PROGRESSIVE_ROLLOUT | operator_approval_for_each_batch, per_batch_service_and_database_health, lease_fencing_and_recovery_smoke, client_compatibility_observation |
| POST_DEPLOYMENT_VERIFICATION | dedicated_server_startup_and_health, encounter_join_and_checkpoint_resume_smoke, android_install_launch_and_network_smoke, post_change_backup_and_restore_point, operator_and_independent_observer_acceptance |

Każda kategoria musi wskazywać osobny, niepusty plik. Pliki muszą być
zanonimizowane i oczyszczone z tokenów, haseł, kluczy, danych osobowych,
identyfikatorów kont, danych graczy i surowych dumpów produkcyjnej bazy.
Weryfikator nie interpretuje semantycznie logów — sprawdza ich metadane,
integralność i podpisany kontekst. Ocena znaczenia dowodów jest zadaniem
niezależnego recenzenta.

## 5. Procedura lokalna operatora

Wszystkie rzeczywiste wejścia trzymaj poza repozytorium, w chronionym
katalogu operatora. Nie używaj syntetycznych fixture CI jako dowodów
wdrożenia.

1. Wykonaj zatwierdzony plan Stage 46 w zewnętrznej procedurze operatorskiej.
2. Zbierz po jednym artefakcie dla każdej z 18 kategorii z tabeli.
3. Utwórz dziennik zgodny ze schematem i zapisz go poza repozytorium, np.
   /secure/deployment/journal.json.
4. Zweryfikuj SHA-256 i rozmiary plików względem manifestu.
5. Uzyskaj fingerprint publicznego keyringu Stage 44 niezależnym kanałem.
6. Operator i niezależny recenzent podpisują ten sam dziennik.

Podpis operatora:

~~~bash
python Scripts/encounter_checkpoint_deployment_evidence.py sign \
  --plan /secure/deployment/deployment-plan.json \
  --journal /secure/deployment/journal.json \
  --evidence-root /secure/deployment/evidence \
  --keyring /secure/trust/production-evidence-keyring.json \
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \
  --role SERVICE_OPERATOR \
  --key-id <SERVICE_OPERATOR_KEY_ID> \
  --private-key /secure/keys/service-operator-ed25519.pem \
  --attestation /secure/deployment/journal-attestation.json
~~~

Następnie niezależny recenzent uruchamia tę samą komendę z rolą
INDEPENDENT_REVIEWER, własnym key-id i własnym zaszyfrowanym kluczem
prywatnym, zachowując tę samą ścieżkę attestation. Hasło klucza jest
pobierane interaktywnie i nie powinno być przekazywane jako argument CLI.

Weryfikacja kompletu:

~~~bash
python Scripts/encounter_checkpoint_deployment_evidence.py verify \
  --plan /secure/deployment/deployment-plan.json \
  --journal /secure/deployment/journal.json \
  --evidence-root /secure/deployment/evidence \
  --keyring /secure/trust/production-evidence-keyring.json \
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \
  --attestation /secure/deployment/journal-attestation.json \
  --output /secure/deployment/post-deployment-report.json
~~~

Kod wyjścia 0 i outcome POST_DEPLOYMENT_EVIDENCE_VERIFIED potwierdzają
integralność pakietu. Kod 2 oznacza fail-closed rejection. Raport zawiera
hashy i statusy, nie kopiuje treści plików dowodowych.

## 6. Kontrole fail-closed

Weryfikator odrzuca co najmniej:

- plan inny niż Stage 46 albo plan o zmienionych bajtach;
- niezgodność planId, SHA planu, kandydata lub rollback baseline;
- niepełną fazę, złą kolejność, zły status lub niespójne czasy;
- brakującą, powtórzoną lub nieoczekiwaną kategorię dowodu;
- brak pliku, zmianę rozmiaru lub SHA-256, symlink albo path traversal;
- nieprawidłowy fingerprint keyringu, cofnięty/nieaktywny klucz lub złą rolę;
- brak podpisu, powtórzony klucz, podpis po terminie ważności albo błędny
  podpis Ed25519;
- dziennik z nieudanym wynikiem lub z zadeklarowanym rollbackiem.

Weryfikator nie wpisuje danych produkcyjnych do Stage 46 planu i nie
modyfikuje jego bajtów.

## 7. Testy i CI

- Scripts/encounter_checkpoint_deployment_evidence.py — sign i verify,
  z interaktywnym odblokowaniem klucza PEM i bez eksportu klucza prywatnego.
- Scripts/validate_encounter_checkpoint_stage47_post_deployment.py —
  kontrola repozytorium, schematów, pustych szablonów, dokumentacji i granic.
- Scripts/Tests/test_validate_encounter_checkpoint_stage47_post_deployment.py
  — pozytywny pakiet i negatywne przypadki hash/path/signature/role/phase.
- .github/workflows/encounter-checkpoint-stage47-post-deployment.yml —
  testy Stage 46/47 na syntetycznych fixture.

CI nie ma dostępu do produkcyjnego środowiska, nie wykonuje deploymentu,
nie uruchamia rollbacku i nie może zatwierdzić produkcji.

Uruchomienie lokalne:

~~~bash
python -m pip install "cryptography>=42.0" "rfc8785==0.1.4"
python -m compileall -q Scripts/encounter_checkpoint_deployment_evidence.py
python Scripts/validate_encounter_checkpoint_stage47_post_deployment.py
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_checkpoint_stage47_post_deployment.py" -v
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_checkpoint_stage46_deployment.py" -v
~~~

## 8. Stan faktyczny i kryteria akceptacji

- [ ] Istnieje poprawny plan Stage 46 zatwierdzony dla rzeczywistego wydania.
- [ ] Deployment wykonano w zatwierdzonym procesie out-of-band.
- [ ] Zebrano 18 zanonimizowanych dowodów z czterech faz.
- [ ] Dziennik odpowiada hashom planu, kandydata i rollback baseline.
- [ ] Operator i niezależny recenzent podpisali dziennik różnymi kluczami.
- [ ] Fingerprint keyringu został potwierdzony niezależnym kanałem.
- [ ] Raport Stage 47 przechodzi weryfikację i został zachowany w audycie.
- [ ] Unreal Dedicated Server/PIE, Android, rzeczywisty backup/restore,
  monitoring, DR, HA i SLO są osobno ocenione przed jakąkolwiek promocją.

W repozytorium dostarczono narzędzie, schematy, puste szablony, testy i CI.
Nie dostarczono prawdziwego planu produkcyjnego, dziennika, załączników,
kluczy ani podpisów. Nie wykonano rzeczywistego deploymentu. Szablony
pozostają NOT_RECORDED, a produkcyjne readiness pozostaje zablokowane.

**Stage 47 potwierdza integralność dowodów po faktycznej zmianie; nie jest
dowodem samodzielnym na bezpieczeństwo całej gry i nie daje zgody na
promocję.**
