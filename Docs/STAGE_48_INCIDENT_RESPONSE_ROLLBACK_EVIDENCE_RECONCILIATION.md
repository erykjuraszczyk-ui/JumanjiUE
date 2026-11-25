# JUMANJI 2.111.1 — Stage 48: Incident Response & Rollback Evidence Reconciliation

## 1. Cel etapu

Stage 47 weryfikuje wyłącznie udany rollout bez rollbacku. Zgodnie z jego
granicą bezpieczeństwa nieudany rollout albo wykonany rollback wymaga
oddzielnego, zatwierdzonego procesu incydentowego. Stage 48 wprowadza
niezależny zapis takiego incydentu, dowody działań i odtworzenia usług oraz
dwuosobową attestation.

Stage 48 dostarcza:
- ścisły schemat rekordu incydentu i pusty, zablokowany template;
- oś czasu z kontrolowaną kolejnością zdarzeń od wykrycia do przeglądu;
- obowiązkowy wpis wykonania rollbacku, jeżeli rollback był wymagany;
- binding planu Stage 46, kandydata wydania i rollback baseline przez SHA-256;
- bezpieczny katalog sanitizowanych załączników z kontrolą hashy, rozmiarów,
  ścieżek i symlinków;
- kontrolę podstawowej sanitizacji danych JSON/logów;
- dwa niezależne podpisy Ed25519 ról SERVICE_OPERATOR i INDEPENDENT_REVIEWER;
- użycie Stage 44 public trust keyring z fingerprintem dostarczonym
  niezależnym kanałem;
- offline CLI sign i verify, walidator kontraktu, testy regresji i CI.

## 2. Granica bezpieczeństwa

Wszystkie działania rzeczywiste wykonuje upoważniony operator poza
repozytorium, w zatwierdzonym procesie out-of-band. Narzędzie Stage 48:
- nie wykonuje rollbacku, ponownego deploymentu, retry ani restartu;
- nie zmienia routingu, ruchu, konfiguracji ani danych produkcyjnych;
- nie łączy się z chmurą, Kubernetes, SSH, sklepami mobilnymi ani serwerami;
- nie pobiera sekretów i nie przechowuje kluczy prywatnych;
- nie uznaje raportów CI/syntetycznych fixture za dowody zewnętrznego incydentu.

Wynik INCIDENT_EVIDENCE_VERIFIED oznacza wyłącznie, że przekazany rekord,
załączniki i dwa podpisy są kompletne, powiązane z planem i kryptograficznie
spójne. To nie dowodzi samodzielnie jakości decyzji operatorów ani prawdziwości
źródłowego środowiska. Role i zewnętrzna tożsamość osób muszą być zarządzane
poza narzędziem.

Stałe granice każdego raportu:
- incidentClosedForAudit=true — wyłącznie zamknięcie ścieżki dowodowej;
- releaseDisposition=HOLD;
- productionReady=false;
- highAvailabilityApproved=false;
- sloStatus=UNBASELINED;
- promotionAllowed=false;
- sideEffectsPerformedByVerifier=false.

Weryfikacja incydentu nie jest zgodą na wznowienie rollout’u.

## 3. Model incydentu

Rekord: Data/Encounter/ProductionDeploymentIncidentRecord.schema.json.
Pusty template: Data/Encounter/ProductionDeploymentIncidentRecord.template.json.

Wymagane powiązania:
- planId i planSha256 dokładnego Stage 46 planu;
- candidateSha256 oraz rollbackBaselineSha256 skopiowane z planu;
- środowisko production i tryb MANUAL_OUT_OF_BAND;
- wynik rollout’u: ABORTED, FAILED albo PARTIALLY_APPLIED;
- severity SEV1–SEV4;
- serviceState=RESTORED i dataIntegrityState=VERIFIED;
- releaseDisposition=HOLD.

Dozwolone ścieżki odzyskiwania:
- rollbackOutcome=SUCCEEDED wymaga resolutionAction=ROLLBACK;
- rollbackOutcome=FAILED wymaga alternatywnego FORWARD_FIX lub
  TRAFFIC_ISOLATION oraz osobnego dowodu przywrócenia;
- rollbackOutcome=NOT_REQUIRED nie może deklarować działania ROLLBACK.

Jeżeli rollback był podjęty (sukces albo porażka), oś czasu musi zawierać
ROLLBACK_EXECUTED. Nieudany rollback nie zamyka incydentu sam w sobie:
weryfikator wymaga osobno przywróconej usługi i zweryfikowanej integralności
danych.

## 4. Wymagana oś czasu i dowody

Wymagana kolejność zdarzeń:
1. DETECTED — wykrycie i źródło alarmu.
2. DECLARED — formalne ogłoszenie incydentu i przypisanie właściciela.
3. IMPACT_ASSESSED — zasięg wpływu na usługę, sesje i dane.
4. CONTAINED — potwierdzone ograniczenie rozprzestrzeniania skutków.
5. RECOVERY_DECISION — udokumentowana decyzja i upoważnienie.
6. ROLLBACK_EXECUTED — wymagane tylko, gdy rollbackOutcome nie jest NOT_REQUIRED.
7. SERVICE_RESTORED — dowód przywrócenia wymaganych health gates.
8. DATA_INTEGRITY_VERIFIED — kontrola checkpointów, fencing, reward ledger
   i spójności postaci/sesji.
9. POST_INCIDENT_REVIEW — niezależny przegląd i działania korygujące.

Każde zdarzenie wymaga UUID zdarzenia i dowodu, UTC timestamp, roli
wykonawcy, podsumowania oraz osobnego pliku dowodowego z SHA-256, rozmiarem
i media type. Maksymalny rozmiar załącznika wynosi 25 MiB. Odrzucane są
path traversal, symlinki, duplikaty oraz niezgodne hashe/rozmiary.
JSON jest sprawdzany pod kątem wrażliwych nazw pól, a tekst pod kątem
popularnych wzorców sekretów i adresów e-mail. To filtr pomocniczy, nie
pełny DLP — operator nadal odpowiada za pełną anonimizację.

## 5. Attestation i zaufanie

Envelope: Data/Encounter/ProductionDeploymentIncidentAttestation.schema.json.
Podpisy wiążą dokładny SHA-256 rekordu incydentu i planu Stage 46 przez
kanonikalizację RFC 8785. Wymagane są dwa różne klucze Ed25519:
SERVICE_OPERATOR oraz INDEPENDENT_REVIEWER.

Publiczne klucze muszą mieć status ACTIVE, właściwą rolę i ważność w
momencie podpisu w Stage 44 trust keyring. Fingerprint SHA-256 całego
keyringu musi być przekazany niezależnym kanałem. Odwołanie klucza blokuje
weryfikację. Prywatne klucze pozostają poza repozytorium; hasło szyfrowanego
PEM jest pobierane interaktywnie.

## 6. Procedura operatora

Wszystkie realne wejścia trzymaj w bezpiecznym katalogu poza repozytorium.
Nie zapisuj rzeczywistych raportów, logów ani prywatnych kluczy do Git.

1. Zatrzymaj dalszą promocję. Wykonaj tylko zatwierdzone działania
   containment/recovery poza narzędziem.
2. Skopiuj dokładny Stage 46 plan do katalogu audytowego.
3. Zbierz i zanonimizuj osobne pliki dowodowe dla wymaganych zdarzeń.
4. Utwórz rekord Stage 48 na podstawie schematu; ustaw status na
   RESOLVED_FOR_REVIEW dopiero po przywróceniu usługi i weryfikacji danych.
5. Ustaw startedAtUtc na timestamp DETECTED oraz resolvedAtUtc na
   timestamp POST_INCIDENT_REVIEW.
6. Zweryfikuj keyring i jego fingerprint niezależnym kanałem.

Podpis operatora:

~~~bash
python Scripts/encounter_checkpoint_incident_evidence.py sign \
  --plan /secure/incident/deployment-plan.json \
  --incident /secure/incident/incident-record.json \
  --evidence-root /secure/incident/evidence \
  --keyring /secure/trust/production-evidence-keyring.json \
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \
  --role SERVICE_OPERATOR \
  --key-id <SERVICE_OPERATOR_KEY_ID> \
  --private-key /secure/keys/service-operator-ed25519.pem \
  --attestation /secure/incident/incident-attestation.json
~~~

Niezależny recenzent używa własnego klucza, roli INDEPENDENT_REVIEWER i
tego samego pliku attestation. Następnie wykonaj weryfikację:

~~~bash
python Scripts/encounter_checkpoint_incident_evidence.py verify \
  --plan /secure/incident/deployment-plan.json \
  --incident /secure/incident/incident-record.json \
  --evidence-root /secure/incident/evidence \
  --keyring /secure/trust/production-evidence-keyring.json \
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \
  --attestation /secure/incident/incident-attestation.json \
  --report /secure/incident/incident-report.json
~~~

Kod wyjścia 0 i wynik INCIDENT_EVIDENCE_VERIFIED zamykają wyłącznie
audytową ścieżkę incydentu. Kod 2 oznacza fail-closed rejection.

## 7. Testy i kryteria akceptacji

Lokalnie:
~~~bash
python -m pip install "cryptography>=42.0" "rfc8785==0.1.4"
python -m compileall -q Scripts/encounter_checkpoint_incident_evidence.py
python Scripts/validate_encounter_checkpoint_stage48_incident.py
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_checkpoint_stage48_incident.py" -v
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_checkpoint_stage47_post_deployment.py" -v
~~~

Kryteria:
- [ ] rekord jest związany z niezmienionym Stage 46 planem i artefaktami;
- [ ] wymagana oś czasu jest kompletna i chronologiczna;
- [ ] każdy załącznik przechodzi kontrole ścieżki, symlink, rozmiaru,
      SHA-256 i podstawowej sanitizacji;
- [ ] dwa aktywne klucze Ed25519 podpisują ten sam rekord;
- [ ] usługa jest przywrócona, a integralność danych zweryfikowana;
- [ ] raport zachowuje release HOLD i brak promocji;
- [ ] testy Stage 47 i Stage 48 przechodzą.

Puste template pozostają zablokowane. Nie dodano prawdziwego incydentu,
załączników, podpisów ani kluczy. CI używa wyłącznie syntetycznych fixture.

**Stage 48 tworzy audytowalny proces obsługi nieudanego rollout’u/rollbacku,
ale nie daje zgody na wznowienie wydania.**