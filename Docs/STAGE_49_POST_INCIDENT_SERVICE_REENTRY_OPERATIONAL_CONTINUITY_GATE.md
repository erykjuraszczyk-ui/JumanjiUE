# JUMANJI 2.111.1 — Stage 49: Post-Incident Service Re-entry & Operational Continuity Gate

## 1. Cel etapu

Stage 48 zamyka audytową ścieżkę incydentu i potwierdza integralność jej dowodów. Nie daje jednak zgody na wznowienie promocji ani ruchu. Stage 49 buduje osobną, fail-closed bramkę dowodową dla decyzji o ponownym dopuszczeniu usługi do pracy po incydencie.

Stage 49 sprawdza kompletność dowodów, ale **nie wykonuje ponownego uruchomienia usługi i nie podejmuje decyzji za właściciela systemu**.

## 2. Zakres kontroli

Pakiet musi zawierać dokładnie 11 kontroli PASS:

1. `service_health` — health/readiness wymaganych usług i zależności.
2. `database_integrity` — kontrola integralności PostgreSQL, schematu i odczytu danych.
3. `lease_fencing` — potwierdzenie aktualnego authority epoch i odrzucenia dawnych lease/tokenów.
4. `signing_key_verification` — weryfikacja podpisów checkpointów i aktualnego keyring.
5. `queue_backlog` — stan kolejek, retry i brak niekontrolowanego narastania zaległości.
6. `telemetry_alerts` — telemetry, alerty, dashboardy oraz brak nierozpoznanych krytycznych alarmów.
7. `gameplay_synthetic` — syntetyczna sesja gry: wejście, checkpoint, zapis i odczyt.
8. `client_reconnect` — reconnect klienta po kontrolowanym zerwaniu sesji.
9. `security_access` — uprawnienia, ingress, certyfikaty i ograniczenie dostępu operatorów.
10. `oncall_handover` — właściciel dyżuru, eskalacja, kontakt i jawne przekazanie odpowiedzialności.
11. `stability_soak` — udokumentowane okno obserwacji stabilności po przywróceniu usługi.

Każda kontrola wymaga UTC timestamp, krótkiego podsumowania oraz osobnego załącznika z SHA-256, rozmiarem i media type. Wszystkie załączniki muszą być sanitizowane. Maksymalny rozmiar jednego pliku to 25 MiB, a całego pakietu 100 MiB.

Wymagane są dwa odrębne zatwierdzenia audytowe: SERVICE_OWNER oraz INDEPENDENT_OPERATIONS_REVIEWER. Referencje recenzentów nie mogą być identyczne. Tożsamość i uprawnienia osób zatwierdzających są ustalane poza tym narzędziem.

## 3. Powiązanie z Stage 48

Stage49 packet wiąże:
- dokładne bajty Stage48 incident record;
- dokładne bajty Stage48 dual-signature attestation;
- dokładne bajty raportu wygenerowanego przez Stage48 `verify`;
- incidentId i incident SHA-256.

Najpierw uruchom Stage48 `verify` na oryginalnym planie Stage46, rekordzie incydentu, dowodach, attestation, trust keyring oraz niezależnie potwierdzonym fingerprintie keyringu. Dopiero raport z wynikiem `INCIDENT_EVIDENCE_VERIFIED` może zostać wejściem Stage49.

Stage49 kontroluje hash raportu i spójność jego powiązań. Nie zastępuje ponownej weryfikacji podpisów Ed25519 ani zewnętrznego potwierdzenia fingerprintu keyringu — te czynności należą do Stage48.

## 4. Kontrakt wyniku

Sukces walidatora oznacza wyłącznie:

- `REENTRY_EVIDENCE_VERIFIED_FOR_HUMAN_REVIEW`;
- 11 kompletnych kontroli PASS;
- 2 niezależne referencje zatwierdzających;
- zgodność Stage48 report/record/attestation i hashy załączników;
- `serviceReentryDecision=HUMAN_REVIEW_REQUIRED`.

Niezmienione granice:
- `releaseDisposition=HOLD`;
- `trafficMutationAllowed=false`;
- `promotionAllowed=false`;
- `productionReady=false`;
- `highAvailabilityApproved=false`;
- `sloStatus=UNBASELINED`;
- `sideEffectsPerformedByVerifier=false`.

Nawet kompletny pakiet nie zmienia routingu, nie odblokowuje ruchu i nie stanowi automatycznej zgody na wydanie. Właściciel usługi i niezależny reviewer podejmują końcową decyzję w zatwierdzonym procesie out-of-band.

## 5. Uruchomienie

Przygotuj poza repozytorium bezpieczny katalog zawierający sanitizowane dowody oraz dokładne pliki Stage48. Nie commituj raportów produkcyjnych, logów, prywatnych kluczy, tokenów ani danych graczy.

Przykład:

~~~bash
python Scripts/validate_encounter_checkpoint_stage49_reentry.py \
  --gate /secure/reentry/stage49-gate.json \
  --incident-report /secure/reentry/stage48-incident-report.json \
  --incident-record /secure/reentry/stage48-incident-record.json \
  --incident-attestation /secure/reentry/stage48-incident-attestation.json \
  --evidence-root /secure/reentry/evidence \
  --report /secure/reentry/stage49-verification-report.json
~~~

Walidator odrzuca brakujące kontrole, status inny niż PASS, niezgodne hashe/rozmiary, path traversal, symlinki, duplikaty ścieżek, niezgodny incident binding, niezweryfikowany raport Stage48, niepełne role zatwierdzeń oraz próbę otwarcia bramki przez zmianę stałych granic release.

Walidacja kontraktu repozytorium:

~~~bash
python Scripts/validate_encounter_checkpoint_stage49_reentry.py --check-repository
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_checkpoint_stage49_reentry.py" -v
~~~

## 6. Ograniczenia i ryzyka

- Stage49 nie ma poświadczeń do produkcyjnej infrastruktury i nie łączy się z nią.
- Nie wykonuje restartu, rollout, rollbacku, retry, failover ani zmiany ruchu.
- Nie sprawdza bezpośrednio prawdziwości źródłowych obserwacji. Weryfikuje spójność i integralność dostarczonych plików.
- Skanowanie sekretów jest pomocnicze; operator musi wykonać pełną anonimizację i kontrolę DLP przed przekazaniem materiałów.
- Dwa reviewerRef nie dowodzą tożsamości osób. Tożsamość, role i uprawnienia muszą zostać potwierdzone zewnętrznym systemem IAM/procesem.
- CI wykorzystuje wyłącznie syntetyczne fixture. Nie jest dowodem, że produkcyjna usługa jest gotowa.
- Stage40 SLO pozostaje UNBASELINED, dopóki nie zostaną niezależnie zatwierdzone pomiary.

## 7. Kryteria akceptacji

- [ ] Strict JSON Schema i blokowany template Stage49.
- [ ] Powiązanie hashami do raportu, rekordu i attestation Stage48.
- [ ] Wymagane 11 kontroli operacyjnych, każda PASS.
- [ ] Hash, rozmiar, typ pliku, ścieżka i symlink validation wszystkich załączników.
- [ ] Dwa rozłączne zatwierdzenia: SERVICE_OWNER i INDEPENDENT_OPERATIONS_REVIEWER.
- [ ] Testy negatywne: brak checku, FAIL, hash tampering, path traversal, symlink, błędny raport Stage48, zmieniony release boundary i powtórzony reviewer.
- [ ] Stage48 regression suite pozostaje w CI.
- [ ] Brak komend deploy/rollback/traffic mutation w workflow.
- [ ] Wszystkie granice production/HA/SLO pozostają zamknięte.

**Stage49 dostarcza kontrolę dowodów ciągłości operacyjnej. Nie deklaruje produkcyjnej gotowości ani zgody na wznowienie ruchu.**
