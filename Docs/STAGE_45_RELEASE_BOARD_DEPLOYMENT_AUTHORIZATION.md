# JUMANJI 2.111.1 — Stage 45: Release Board Authorization & Deployment Candidate Binding

## 1. Cel i granica etapu

Stage 44 kończy weryfikację pakietu Stage 43 wynikiem
\`ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW\`. To nie jest zatwierdzenie wdrożenia.
Stage 45 dodaje niezależną decyzję Release Board, kryptograficznie związaną z
konkretnym kandydatem wydania i dokładnymi bajtami artefaktów.

Etap dostarcza:

- ścisły manifest kandydata wydania dla UE 5.8 Dedicated Server i Android APK/AAB;
- powiązanie manifestu z \`packetId\`, kanonicznym SHA-256 pakietu Stage 43,
  środowiskiem staging oraz SHA commit source;
- weryfikację hashy i rozmiarów plików builda z katalogu artefaktów;
- kontrolę, że hash APK/AAB i \`buildId\` odpowiadają rzeczywistemu wpisowi
  \`android_device_package\` w pakiecie Stage 43;
- kontrolę, że dowód UE potwierdza wersję 5.8 i udany build Dedicated Server;
- decyzję APPROVE/REJECT podpisaną przez trzy niezależne role;
- raport weryfikacji, który nie uruchamia wdrożenia ani nie zmienia stanu
  production/HA/SLO.

**Stage 45 nie zbiera rzeczywistych dowodów, nie buduje UE/Android, nie rejestruje
kluczy, nie podejmuje decyzji za ludzi i nie wykonuje wdrożenia.** Weryfikacja CI
korzysta wyłącznie z syntetycznych fixture testowych.

## 2. Łańcuch zaufania

Kolejność bramek jest obowiązkowa:

1. Stage 43: kompletny pakiet dziesięciu typów rzeczywistego dowodu oraz
   niezależna recenzja dwóch ról.
2. Stage 44: podpis operatora i niezależnego recenzenta dla każdego dowodu,
   podpisy końcowej recenzji, aktywny keyring oraz hash keyringu uzyskany
   niezależnym kanałem.
3. Stage 45: kandydat wydania z plikami artefaktów, które zgadzają się z
   pakietem Stage 43; dopiero potem podpisy Release Board.

Każda zmiana bajtów packetu, koperty Stage 44, keyringu, manifestu kandydata
lub artefaktu powoduje niezgodność digestu albo hashy i blokuje autoryzację.
Nie wolno podmieniać plików po podpisaniu decyzji.

## 3. Manifest kandydata

Schemat: \`Data/Encounter/ProductionReleaseCandidate.schema.json\`

Szablon: \`Data/Encounter/ProductionReleaseCandidate.template.json\`

Wymagane identyfikatory i powiązania:

- \`candidateId\`, \`buildId\`, \`sourceCommitSha\` i \`createdAtUtc\`;
- \`packetId\`, \`packetSha256\` i \`environmentId\` zgodne z pakietem Stage 43;
- \`engineVersion=5.8\`;
- lista artefaktów z relatywną ścieżką, SHA-256, rozmiarem i media type.

Wymagane cele:

- \`ue58-dedicated-server\` — archiwum/pakiet serwera dedykowanego UE 5.8;
- \`android-package\` — APK albo AAB wskazany w dowodzie fizycznego urządzenia.

Opcjonalnie można dodać \`windows-client\`. Każdy cel musi być unikalny.
Katalog wskazany przez \`--artifact-root\` jest jedynym źródłem artefaktów.
Narzędzie odrzuca path traversal, symlinki, pliki spoza katalogu, niezgodny
rozmiar i SHA-256.

### Powiązanie Android

Dowód Stage 43 \`android_device_package\` musi zawierać:

- \`packageFormat=APK\` lub \`AAB\`;
- \`packageSha256\` identyczny z hashem artefaktu w kandydacie;
- \`packageBuildId\` identyczny z \`buildId\` manifestu;
- media type odpowiedni dla APK/AAB.

### Powiązanie serwera UE

Dowód \`ue58_dedicated_server_recovery\` musi zawierać
\`engineVersion=5.8\` i \`serverBuildSucceeded=true\`.
Raport recovery nie zastępuje samego artefaktu — plik serwera również musi
przejść weryfikację SHA-256 i rozmiaru.

## 4. Role Release Board

Wymagane są trzy role:

| Rola | Odpowiedzialność |
|---|---|
| \`SERVICE_OWNER\` | decyzja właściciela usługi i zakresu wydania |
| \`SECURITY_REVIEWER\` | niezależna ocena bezpieczeństwa i ryzyka |
| \`PLATFORM_REVIEWER\` | ocena platformy, buildów i gotowości operacyjnej |

Wszystkie trzy role muszą podpisać tę samą decyzję. Każdy podpis musi pochodzić
z innego \`keyId\`. Klucz Release Board nie może być użyty jako operator dowodu,
recenzent dowodu ani końcowy recenzent Stage 44. Wymagane są osobne klucze
Release Board, zarejestrowane w zaufanym keyringu.

Stage 45 używa istniejącego keyring Stage 44. Nie tworzy ani nie przechowuje
kluczy prywatnych. Prywatne klucze pozostają poza repozytorium, najlepiej
w zatwierdzonym menedżerze sekretów lub HSM/KMS. Weryfikator nadal wymaga
\`--trusted-keyring-sha256\` z niezależnego kanału.

## 5. Podpis decyzji

Podpis Ed25519 obejmuje kanoniczny payload RFC 8785 z:

- \`decisionId\`, decyzją i docelowym środowiskiem;
- \`candidateId\` i SHA-256 całego manifestu kandydata;
- \`packetId\` i kanonicznym SHA-256 packetu;
- SHA-256 koperty Stage 44 i keyringu;
- rolą oraz czasem podpisującego.

Podpisy są niezależne od siebie. Zmiana decision, manifestu, dowodów,
koperty, keyringu, środowiska lub daty decyzji unieważnia istniejące podpisy.
Czas podpisu nie może poprzedzać recenzji Stage 43, manifestu ani utworzenia
decyzji. Klucze muszą być aktywne, ważne w chwili podpisu i przypisane do roli.

## 6. Procedura operatora

Wszystkie pliki wejściowe i artefakty powinny znajdować się poza repozytorium,
w chronionym katalogu operatora. Nie używaj fixture testowych jako dowodu.

Najpierw uruchom Stage 43 i Stage 44 zgodnie z ich runbookami. Stage 44 musi
zwrócić \`ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW\`. Pobierz zatwierdzony
keyring i jego hash z niezależnego kanału. Następnie przygotuj manifest
kandydata, wskazując rzeczywiste pliki UE 5.8 i APK/AAB:

\`\`\`bash
python Scripts/encounter_checkpoint_release_board.py prepare \\
  --packet /secure/evidence/packet.json \\
  --evidence-root /secure/evidence \\
  --attestation /secure/evidence/attestation.json \\
  --keyring /secure/trust/production-evidence-keyring.json \\
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \\
  --candidate /secure/release/candidate.json \\
  --artifact-root /secure/release/artifacts \\
  --decision-file /secure/release/board-decision.json
\`\`\`

Decyzja powstaje z \`decision=APPROVE\`, ale nie ma jeszcze podpisów i nie
autoryzuje wdrożenia. Każdy członek komisji podpisuje plik niezależnie, własnym
kluczem i poświadczeniem:

\`\`\`bash
python Scripts/encounter_checkpoint_release_board.py sign \\
  --packet /secure/evidence/packet.json \\
  --evidence-root /secure/evidence \\
  --attestation /secure/evidence/attestation.json \\
  --keyring /secure/trust/production-evidence-keyring.json \\
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \\
  --candidate /secure/release/candidate.json \\
  --artifact-root /secure/release/artifacts \\
  --decision-file /secure/release/board-decision.json \\
  --board-role SERVICE_OWNER \\
  --key-id <SERVICE_OWNER_KEY_ID> \\
  --private-key /secure/keys/service-owner.pem
\`\`\`

Powtórz polecenie dla \`SECURITY_REVIEWER\` i \`PLATFORM_REVIEWER\`, używając
ich własnych \`keyId\` i kluczy prywatnych. Hasło do zaszyfrowanego PEM jest
pobierane z \`JUMANJI_EVIDENCE_PRIVATE_KEY_PASSWORD\`; nie przekazuj go w
argumentach CLI, historii powłoki ani logach.

Na końcu zweryfikuj decyzję:

\`\`\`bash
python Scripts/encounter_checkpoint_release_board.py verify \\
  --packet /secure/evidence/packet.json \\
  --evidence-root /secure/evidence \\
  --attestation /secure/evidence/attestation.json \\
  --keyring /secure/trust/production-evidence-keyring.json \\
  --trusted-keyring-sha256 <OUT_OF_BAND_KEYRING_SHA256> \\
  --candidate /secure/release/candidate.json \\
  --artifact-root /secure/release/artifacts \\
  --decision-file /secure/release/board-decision.json \\
  --report /secure/reports/stage45-release-board-report.json \\
  --require-approved
\`\`\`

Opcja \`--require-approved\` zwraca kod niezerowy, dopóki wynik nie będzie
\`APPROVED_FOR_DEPLOYMENT\`.

## 7. Wyniki

- \`BLOCKED_UPSTREAM_NOT_ELIGIBLE\` — Stage 43/44 nie dopuścił pakietu.
- \`INVALID_RELEASE_CANDIDATE\` — manifest, powiązanie lub artefakt jest błędny.
- \`INVALID_RELEASE_BOARD_DECISION\` — decyzja lub podpisy są niepoprawne.
- \`WAITING_FOR_RELEASE_BOARD_SIGNATURES\` — brakuje co najmniej jednej z trzech ról.
- \`REJECTED_BY_RELEASE_BOARD\` — trzy ważne podpisy potwierdzają decyzję REJECT.
- \`APPROVED_FOR_DEPLOYMENT\` — trzy niezależne podpisy zatwierdzają dokładnie
  ten kandydat i jego artefakty.

Nawet wynik pozytywny oznacza wyłącznie autoryzację do osobnej, kontrolowanej
procedury wdrożenia. Raport zachowuje:

- \`deploymentAuthorizationGranted=true\` tylko przy pełnym quorum APPROVE;
- \`deploymentExecuted=false\`;
- \`productionReady=false\`;
- \`highAvailabilityApproved=false\`;
- \`sloStatus=UNBASELINED\`;
- \`promotionAllowed=false\`.

Etap 45 nie wykonuje wdrożenia i nie zmienia bramek produkcyjnych.

## 8. Automatyczna walidacja

- \`Scripts/encounter_checkpoint_release_board.py\` — CLI, walidacja manifestu,
  wiązanie artefaktów, podpisy i ocena Release Board.
- \`Scripts/validate_encounter_checkpoint_stage45_release_board.py\` — kontrola
  kompletności kontraktu repozytorium.
- \`Scripts/Tests/test_validate_encounter_checkpoint_stage45_release_board.py\`
  — testy syntetyczne: quorum, niezależność kluczy, mutacje plików, binding
  manifestu, REJECT i blokada pustych szablonów.
- \`.github/workflows/encounter-checkpoint-stage45-release-board.yml\` — CI
  dla etapów 43–45.

CI testuje mechanikę kryptograficzną na tymczasowych kluczach i syntetycznych
plikach. Nie weryfikuje prawdziwego staging, produkcyjnego keyringu, działania
serwera, APK/AAB z urządzenia ani procesu deployment.

## 9. Kryteria akceptacji

- [ ] Stage 43 packet i Stage 44 attestation są eligible, zewnętrzny hash
      keyringu jest prawidłowy.
- [ ] Kandydat zawiera UE 5.8 Dedicated Server oraz Android APK/AAB.
- [ ] SHA-256 i rozmiary wszystkich artefaktów odpowiadają plikom.
- [ ] Hash/build ID Android odpowiada dowodowi Stage 43.
- [ ] Podpisy komisji pochodzą z trzech odrębnych kluczy i ról.
- [ ] Żaden klucz komisji nie był użyty w Stage 44.
- [ ] Zmiana packetu, koperty, keyringu, manifestu lub artefaktu blokuje wynik.
- [ ] REJECT nigdy nie daje autoryzacji.
- [ ] Puste szablony pozostają zablokowane.
- [ ] Po pozytywnej decyzji \`deploymentExecuted=false\`,
      \`productionReady=false\` oraz \`promotionAllowed=false\`.

## 10. Stan faktyczny po Stage 45

Repozytorium dostarcza mechanizm i testy. Szablony pozostają puste; nie
wprowadzono rzeczywistego operator packetu, publicznego keyringu, podpisów
komisji ani artefaktów release. Nie zbudowano Unreal Engine 5.8 Dedicated
Server, APK/AAB ani nie wykonano fizycznego testu Android. Nie uruchomiono
deployment do staging ani production. Żadne statusy production/HA/SLO nie
zostały podniesione.

**Następny etap: Stage 46 — Controlled Deployment Execution & Post-Deployment
Verification**, wyłącznie po dostarczeniu rzeczywistych Stage 43/44 evidence
i odrębnej decyzji Release Board z Etapu 45.
