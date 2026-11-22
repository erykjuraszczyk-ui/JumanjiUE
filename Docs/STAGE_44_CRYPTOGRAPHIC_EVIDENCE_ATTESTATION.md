# JUMANJI 2.111.1 — Stage 44: Cryptographic Evidence Attestation

## 1. Cel i granica etapu

Stage 43 przyjmuje dziesięć typów rzeczywistych, sanitizowanych dowodów operatora, sprawdza SHA-256 załączników i wymaga niezależnej recenzji. Pozostawia jednak jawne ograniczenie: pola `operatorRole` i `reviewerRole` są deklaracjami, a sam SHA-256 nie potwierdza, który klucz zatwierdził dokładną treść.

Stage 44 dodaje niezależną warstwę poświadczeń Ed25519:

- operator podpisuje dokładny wpis dowodowy i hash załącznika;
- niezależny recenzent podpisuje ten sam wpis;
- recenzenci końcowi podpisują cały pakiet i decyzję z Stage 43;
- każdy podpis jest związany z `packetId` oraz SHA-256 kanonicznego pakietu;
- publiczny keyring jest sprawdzany względem hasha dostarczonego poza repozytorium;
- wygasłe, unieważnione, błędne, nieznane i niezgodne rolą klucze są odrzucane;
- podpisujący operator i recenzent danego wpisu muszą być różnymi kluczami;
- końcowy recenzent nie może być kluczem użytym przez żadnego wykonawcę dowodu;
- wymagane są różne klucze i różne role dla końcowych recenzentów.

Stage 44 **nie wykonuje failover/PITR, staging, build UE, testów PIE, pakowania Androida ani testów urządzenia**. Nie uwierzytelnia automatycznie operatora w zewnętrznym systemie tożsamości. Podpis potwierdza, że posiadacz zaufanego klucza podpisał konkretny artefakt i metadane — nie dowodzi samodzielnie, że deklarowane zdarzenie naprawdę zaszło.

## 2. Artefakty

- `Data/Encounter/ProductionEvidenceTrustKeyring.schema.json` — ścisły schemat publicznego keyringu.
- `Data/Encounter/ProductionEvidenceTrustKeyring.template.json` — pusty keyring; brak kluczy jest stanem blokującym.
- `Data/Encounter/ProductionEvidenceAttestation.schema.json` — ścisły schemat podpisów wpisów i końcowej recenzji.
- `Data/Encounter/ProductionEvidenceAttestation.template.json` — pusty envelope, bez podpisów.
- `Scripts/encounter_checkpoint_evidence_attestation.py` — generowanie kluczy, podpisywanie wpisów i recenzji, weryfikacja.
- `Scripts/validate_encounter_checkpoint_stage44_attestation.py` — walidator kontraktu repozytorium.
- `Scripts/Tests/test_validate_encounter_checkpoint_stage44_attestation.py` — testy Ed25519, keyring, binding, niezależności i fail-closed.
- `.github/workflows/encounter-checkpoint-stage44-attestation.yml` — testy Stage 43/44 i kontrola pustych szablonów.

## 3. Model podpisu

Podpisy używają Ed25519 z biblioteki `cryptography` i kanonikalizacji JSON **RFC 8785 (JCS)** z pakietu `rfc8785`.

### 3.1 Podpis wpisu dowodowego

Dla każdego wpisu Stage 43 wymagane są dwa podpisy:

1. `operator` — klucz musi mieć rolę równą `operatorRole` danego wpisu.
2. `reviewer` — klucz musi mieć rolę równą `reviewerRole` danego wpisu.

Podpis obejmuje:
- wersję schematu i algorytm kanonikalizacji;
- PacketId oraz SHA-256 całego pakietu;
- EvidenceId, typ dowodu, klasę źródła, EnvironmentId i czas przeprowadzenia;
- role operatora i recenzenta;
- ścieżkę, media type i SHA-256 oryginalnego załącznika;
- typ podpisującego, rolę oraz czas podpisu.

Zmiana dowolnego pola pakietu powoduje zmianę packet digest i unieważnia wszystkie wcześniejsze podpisy. Zmiana pliku załącznika jest wykrywana przez Stage 43 SHA-256 oraz podpis wpisu.

### 3.2 Podpisy końcowej recenzji

Jeżeli Stage 43 ma `decision=APPROVED` albo `REJECTED`, każdy wpis `reviewerRoles` musi mieć osobny podpis. Podpis obejmuje:
- cały packet digest;
- decyzję i dokładną listę ról recenzentów;
- `reviewedAtUtc`;
- rolę i czas podpisu danego recenzenta.

Dla `APPROVED` obowiązuje istniejąca reguła Stage 43: minimum dwie role, w tym `SECURITY_REVIEWER` albo `DATABASE_REVIEWER`. Stage 44 dodaje kontrolę, że klucze końcowych recenzentów są różne i żaden z nich nie podpisał dowodu jako operator.

## 4. Zaufany publiczny keyring

Keyring zawiera wyłącznie publiczne klucze. Każdy wpis ma `keyId`, `publicKeyBase64`, dozwolone `roles`, `status`, `validFromUtc` i `validUntilUtc`.

Prywatne klucze:
- nie mogą trafiać do repozytorium, artefaktów CI ani koperty atestacji;
- powinny być przechowywane w kontrolowanym systemie sekretów lub HSM/KMS, zgodnie z polityką operatora;
- w trybie plikowym narzędzie generuje zaszyfrowany PEM z hasłem wprowadzanym interaktywnie;
- hasło do PEM jest odczytywane ze zmiennej środowiskowej `JUMANJI_EVIDENCE_PRIVATE_KEY_PASSWORD`, nigdy z argumentu CLI.

Pusty plik `ProductionEvidenceTrustKeyring.template.json` jest wyłącznie szablonem. Rzeczywisty keyring musi zostać zatwierdzony przez niezależny proces rejestracji zaufanych kluczy. Weryfikator wymaga `--trusted-keyring-sha256`. Oczekiwany hash musi zostać przekazany operatorowi poza tym samym repozytorium i kanałem, którym dostarczono keyring. Odczytanie hasha z tego samego, potencjalnie podmienionego pliku nie ustanawia zaufania.

`status=REVOKED` unieważnia podpisy tego klucza. Po unieważnieniu klucza wszystkie zależne wpisy trzeba ponownie poświadczyć kluczem zatwierdzonym w keyringu.

## 5. Przygotowanie klucza

Uruchom na zaufanym stanowisku operatora, poza katalogiem repozytorium:

```bash
python Scripts/encounter_checkpoint_evidence_attestation.py keygen \\
  --key-id OPS-STAGING-01 \\
  --role SERVICE_OPERATOR \\
  --private-key-out /secure/jumanji/keys/ops-staging-01.pem \\
  --public-key-out /secure/jumanji/keys/ops-staging-01.public.json
```

Narzędzie poprosi o hasło i zapisze prywatny PEM w formacie zaszyfrowanym. Plik prywatny jest tworzony bez nadpisywania istniejącego pliku. Publiczny rekord z pliku `.public.json` należy dodać do kontrolowanego keyringu. Powtórz proces dla wymaganych ról wykonawców i recenzentów.

W keyringu nie mogą występować:
- powtórzone `keyId`;
- powtórzone publiczne klucze;
- nieznane role;
- okresy ważności z niepoprawnym zakresem.

Nie dodawaj prawdziwych prywatnych kluczy do żadnego pliku w repository.

## 6. Podpisywanie pakietu

Najpierw wykonaj Stage 43:

```bash
python Scripts/validate_encounter_checkpoint_stage43_evidence.py \\
  --packet /secure/stage43/packet.json \\
  --evidence-root /secure/stage43/evidence \\
  --require-eligible
```

Utwórz kopię pustej koperty Stage 44:

```bash
cp Data/Encounter/ProductionEvidenceAttestation.template.json \\
  /secure/stage43/attestation.json
```

Dla każdego `evidenceId` w pakiecie uruchom podpis operatora:

```bash
python Scripts/encounter_checkpoint_evidence_attestation.py sign-entry \\
  --packet /secure/stage43/packet.json \\
  --attestation /secure/stage43/attestation.json \\
  --keyring /secure/trust/production-evidence-keyring.json \\
  --evidence-id <EVIDENCE_UUID> \\
  --signer-type operator \\
  --key-id <OPERATOR_KEY_ID> \\
  --private-key /secure/jumanji/keys/operator.pem
```

Niezależny recenzent podpisuje ten sam wpis z `--signer-type reviewer`, używając własnego klucza i roli zgodnej z `reviewerRole` wpisu.

Po kompletnej recenzji Stage 43 każdy element `review.reviewerRoles` podpisuje końcową decyzję:

```bash
python Scripts/encounter_checkpoint_evidence_attestation.py sign-final-review \\
  --packet /secure/stage43/packet.json \\
  --attestation /secure/stage43/attestation.json \\
  --keyring /secure/trust/production-evidence-keyring.json \\
  --evidence-root /secure/stage43/evidence \\
  --key-id <REVIEWER_KEY_ID> \\
  --reviewer-role SECURITY_REVIEWER \\
  --private-key /secure/jumanji/keys/security-reviewer.pem
```

Narzędzie odmawia podpisania końcowej recenzji, jeśli Stage 43 nie uznaje pakietu za `ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW`, recenzent nie ma przypisanej roli albo jego klucz występował jako operator dowodu.

Hasło do zaszyfrowanego PEM ustaw w środowisku procesu, np. po bezpiecznym pobraniu z menedżera sekretów:

```bash
export JUMANJI_EVIDENCE_PRIVATE_KEY_PASSWORD
```

Nie umieszczaj hasła w historii powłoki, argumentach procesu ani logach CI.

## 7. Weryfikacja i zaufanie do keyringu

Zdobądź z niezależnego kanału zatwierdzony SHA-256 pliku publicznego keyringu. Następnie uruchom:

```bash
python Scripts/encounter_checkpoint_evidence_attestation.py verify \\
  --packet /secure/stage43/packet.json \\
  --attestation /secure/stage43/attestation.json \\
  --keyring /secure/trust/production-evidence-keyring.json \\
  --trusted-keyring-sha256 <OUT_OF_BAND_SHA256> \\
  --evidence-root /secure/stage43/evidence \\
  --report /secure/reports/stage44-attestation-report.json \\
  --require-eligible
```

Możliwe wyniki:
- `INVALID_PACKET` — pakiet Stage 43 jest niepoprawny;
- `INVALID_ATTESTATION` — błędny podpis, keyring, rola, timestamp, binding lub trust hash;
- `BLOCKED_MISSING_EVIDENCE` — nie ma wszystkich dziesięciu rodzajów rzeczywistego dowodu;
- `BLOCKED_MISSING_ATTESTATIONS` — brakuje podpisu operatora, recenzenta lub końcowego recenzenta;
- `BLOCKED_MISSING_TRUSTED_KEYS` — nie ma zatwierdzonych publicznych kluczy;
- `BLOCKED_TRUST_ANCHOR_UNPINNED` — nie podano niezależnie uzyskanego hash keyringu;
- `WAITING_FOR_INDEPENDENT_REVIEW` / `REJECTED_BY_REVIEW` — status wynika z Stage 43;
- `ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW` — podpisy i Stage 43 gate są poprawne; pakiet może trafić do odrębnej decyzji release board.

Każdy raport zachowuje:
- `productionReady=false`;
- `highAvailabilityApproved=false`;
- `sloStatus=UNBASELINED`;
- `promotionAllowed=false`.

## 8. Prywatność, integralność i ograniczenia

Koperta atestacji zawiera tylko identyfikatory kluczy, role, czasy podpisów i podpisy. Nie zawiera kluczy prywatnych, tokenów, treści checkpointu, danych konta ani surowych identyfikatorów profili.

SHA-256 wiąże plik z manifestem, a Ed25519 wiąże podpis z kanonicznym pakietem i rolą klucza. Wiarygodność zależy dodatkowo od:
- bezpiecznej rejestracji klucza i przypisania ról;
- niezależnego przechowywania/pinningu hasha zaufanego keyringu;
- ochrony prywatnych kluczy;
- rzeczywistego wykonania testu przez operatora;
- niezależnej recenzji i kontroli release board.

CI korzysta z tymczasowych kluczy i syntetycznych fixture tylko w celu przetestowania mechaniki podpisów. Takie wyniki nie mogą być wpisane do packetu jako operator staging evidence i nie stanowią dowodu produkcyjnego.

## 9. Kryteria akceptacji

- [ ] Publiczny keyring spełnia ścisły schemat i zawiera tylko zatwierdzone klucze.
- [ ] Dla każdego z dziesięciu dowodów istnieje podpis właściwego operatora i niezależnego recenzenta.
- [ ] Każdy podpis obejmuje hash całego pakietu i załącznika.
- [ ] Końcowi recenzenci podpisują identyczny PacketId, packet digest, decyzję oraz role.
- [ ] Końcowi recenzenci używają dwóch różnych kluczy i nie są operatorami dowodów.
- [ ] Hash keyringu pochodzi z niezależnego kanału.
- [ ] Testy odrzucają mutacje, błędne role, unieważnione klucze, brakujące podpisy i ponowne użycie klucza.
- [ ] Stage 43 nadal wymaga wszystkich dziesięciu rzeczywistych artefaktów.
- [ ] Żaden workflow nie ustawia automatycznie produkcji, HA ani SLO na stan zatwierdzony.

## 10. Stan faktyczny po Stage 44

Szablony w repository pozostają puste. Nie zarejestrowano prawdziwego keyringu, nie podpisano operator evidence packet i nie wykonano zewnętrznych testów staging/DR, Unreal Engine 5.8, Android ani SLO.

Stage 44 dostarcza kryptograficzny mechanizm poświadczeń i jego testy. Nie twierdzi, że którekolwiek z dziesięciu wymaganych dowodów już istnieje. Aktualny status pozostaje `BLOCKED_MISSING_EVIDENCE`, `productionReady=false`, `highAvailabilityApproved=false` i `sloStatus=UNBASELINED`.

Następny operacyjny krok to zdobycie rzeczywistych dowodów Stage 43, rejestracja publicznych kluczy w niezależnym trust store, podpisanie pakietu i przeprowadzenie osobnej recenzji release board.
