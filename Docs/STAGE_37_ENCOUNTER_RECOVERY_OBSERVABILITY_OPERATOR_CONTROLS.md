# JUMANJI 2.111.1 — Stage 37: Recovery Observability & Operator Controls

## 1. Cel etapu

Stage 37 domyka warstwę operacyjną recovery z Etapów 32–36. Dodaje:
- bezpieczny, procesowy snapshot diagnostyczny checkpoint runtime;
- liczniki prób i błędów lease, checkpoint write/read, restore commit i bariery uczestników;
- wspólny correlation ID i strukturalne logi recovery;
- jawne, stabilne outcome/error codes;
- natywne API operatora do ponowienia odzyskiwania i anulowania wyłącznie staged restore czekającego na uczestników;
- walidator źródeł, testy regresyjne i GitHub Actions quality gate.

To nie jest panel administracyjny, publiczne API HTTP ani narzędzie dostępne z klienta. Zaufana warstwa hosta/operatora musi uwierzytelnić i autoryzować operatora przed wywołaniem natywnych metod C++.

## 2. Diagnostyka runtime

Typy:
- `EJumanjiEncounterRecoveryOutcome`
- `EJumanjiEncounterRecoveryAbortReason`
- `FJumanjiEncounterRecoveryDiagnostics`

API odczytu:
`UJumanjiEncounterCheckpointRuntimeSubsystem::GetRecoveryDiagnostics()`

Snapshot zawiera:
- correlation ID, EncounterId i ostatni outcome/error code;
- ostatni HTTP status, sequence checkpointu i authority epoch;
- status lease, requestów read/write, staged candidate, participant wait i commit;
- wiek bieżącej/ostatniej próby recovery;
- liczniki: lease acquire/renew, checkpoint write, restore read/commit, participant timeout, operator retry/abort.

### Dane celowo wykluczone

Snapshot nie zawiera:
- lease tokenu, bearer credential, podpisu ani klucza prywatnego;
- surowego ProfileId, participantKey, danych konta lub identyfikatorów platformowych;
- JSON checkpointu, payloadu, nagród, sald ani inventory;
- referencji UObject/AActor, transformów, AI, timerów lub stanu World.

Diagnostyka jest procesowa i ulotna. Liczniki resetują się po restarcie serwera. Nie są replikowane, serializowane do SaveGame, wysyłane do klienta ani automatycznie eksportowane do zewnętrznego systemu metryk.

## 3. Outcome codes i korelacja logów

Każdy nowy cykl lease/checkpoint dostaje nowy FGuid correlation ID. Kluczowe zdarzenia emitują jednoliniowe rekordy:

```text
JUMANJI_RECOVERY correlation=<guid> outcome=<CODE> encounter=<id> sequence=<n> epoch=<n> http=<status> error=<CODE>
```

Przykładowe outcome:
- `SESSION_STARTED`
- `LEASE_ACQUIRED`
- `LEASE_ACQUIRE_FAILED`
- `LEASE_RENEWED`
- `LEASE_RENEWAL_FAILED`
- `CHECKPOINT_WRITE_STARTED`
- `CHECKPOINT_WRITE_COMMITTED`
- `CHECKPOINT_WRITE_FAILED`
- `RESTORE_READ_STARTED`
- `RESTORE_READ_FAILED`
- `RESTORE_CANDIDATE_STAGED`
- `RESTORE_WAITING_FOR_PARTICIPANTS`
- `RESTORE_COMMITTED`
- `RESTORE_REJECTED`
- `RESTORE_TIMED_OUT`
- `OPERATOR_RETRY_REQUESTED`
- `OPERATOR_ABORT_COMPLETED`
- `LEASE_RELEASED`

ErrorCode jest stałym kodem z runtime, nie dowolnym tekstem z backendu. Nie loguj response body, tokenów, ProfileId, participantKey, payloadu ani sekretów. W celu korelacji zdarzeń używaj correlation ID, EncounterId, sequence, epoch i HTTP status.

## 4. Natywne API operatora

Metody są zwykłymi metodami C++; nie są oznaczone UFUNCTION i nie są dostępne z Blueprint, UMG, client RPC ani endpointu HTTP.

### 4.1 Retry

`RequestOperatorRetry(EncounterDirector, OutError)`

Dozwolone zachowanie:
1. Wymaga serwerowego runtime, ważnego EncounterDirector i HasAuthority().
2. Jeśli istnieje staged candidate i ważny lease dla tego samego EncounterId, ponawia `TryCommitStagedRestore`.
3. Jeśli brak aktywnej sesji/recovery i runtime jest idle, uruchamia nową próbę `BeginRestoreStaging` (lease acquire + read).
4. Jeśli operacja I/O/commit jest w toku — odrzuca polecenie.
5. Jeśli aktywny jest zwykły gameplay checkpoint session, odrzuca retry. Nie zwalnia lease i nie przerywa zapisu normalnego encounteru.
6. Brak uczestnika po ponownej próbie pozostawia candidate w stanie waiting; nie publikuje częściowej grupy.

Zwrócenie `true` oznacza, że żądanie operatora zostało przyjęte/wywołano próbę. Nie oznacza, że checkpoint został zatwierdzony. Ostateczny rezultat należy odczytać z diagnostics i logów.

### 4.2 Abort

`RequestOperatorAbort(Reason, OutError)`

Abort jest celowo zawężony tylko do stanu:
- signed candidate jest staged;
- runtime czeka na brakujących uczestników;
- lease jest gotowy;
- nie trwa read, write ani commit.

Metoda:
- zatrzymuje timer 180 sekund;
- usuwa prywatny staged candidate i oczekujący director;
- zapisuje kontrolowany reason code;
- zwalnia lease przez istniejący backend adapter.

Powody są allowlistowane:
- OperatorCancelled;
- MaintenanceWindow;
- ConfigurationCorrection;
- IncidentContainment.

Abort nie resetuje GameState, prób, fal, bossów, aktorów, profilu, portfela ani ledgeru. Nie może przerwać aktywnego gameplay encounteru ani operacji commit.

## 5. Granica autoryzacji operatora

To repozytorium dostarcza natywny serwerowy control surface, nie kompletny system IAM/administracji. Wywołujący musi pochodzić z zaufanej warstwy serwera, która już uwierzytelniła operatora i sprawdziła jego uprawnienia.

Nie wolno:
- wystawiać tych metod jako Server RPC wywoływanego przez dowolnego klienta;
- dodawać `UFUNCTION(BlueprintCallable)` do operator controls;
- wywoływać ich z widgetu lub zwykłego PlayerController UI;
- umieszczać sekretu operatora w kliencie, DataTable, SaveGame lub repozytorium;
- udawać, że samo `HasAuthority()` uwierzytelnia człowieka-operatora.

Integracja z panelem operatorskim, IAM, MFA, audytem zewnętrznym i eksportem Prometheus/OpenTelemetry pozostaje zadaniem hosta/produkcyjnej platformy.

## 6. Instrukcja integracji hosta

1. Odczytaj runtime subsystem z serwerowego GameInstance.
2. Dla podglądu wywołaj `GetRecoveryDiagnostics()`; eksportuj wyłącznie do prywatnego serwerowego systemu logów/telemetrii.
3. Zanim operator wybierze Retry lub Abort, zweryfikuj jego tożsamość i rolę w zaufanej warstwie hosta.
4. Retry wywołuj z jedynym, zweryfikowanym persistent-level `AJumanjiWorldEncounterDirector` dla EncounterId.
5. Abort udostępniaj tylko dla recovery w stanie `WaitingForParticipants`; nie pokazuj go jako przycisku „przerwij walkę”.
6. Po wywołaniu pobierz nowy diagnostics snapshot i skoreluj wynik przez ten sam correlation ID.
7. Zapisz identyfikator operatora w audycie hosta, ale nie przekazuj jego danych do gameplay checkpointu.
8. Przed wdrożeniem wykonaj testy Stage 32–37, UE 5.8 build oraz multiplayer crash/restart recovery test.

## 7. Runbook diagnostyczny

| Objaw / outcome | Interpretacja | Bezpieczna czynność |
|---|---|---|
| LEASE_ACQUIRE_FAILED | Brak/odmowa lease lub niedostępny backend | Sprawdź status usługi i konfigurację identity; nie twórz lokalnego checkpointu |
| LEASE_RENEWAL_FAILED | Lease utracony; zapis checkpointu jest fenced off | Nie ponawiaj write z tym samym sequence; wykonaj kontrolowany resync/recovery |
| CHECKPOINT_WRITE_FAILED | Zapis mógł być niejednoznaczny | Nie wysyłaj ponownie sequence w ciemno; odczyt signed head i reconciliation |
| RESTORE_READ_FAILED | Backend read/transport/shape failed | Sprawdź HTTP status i server logs po correlation ID |
| RESTORE_WAITING_FOR_PARTICIPANTS | Brakuje co najmniej jednego wymaganego profilu | Poczekaj do 180 s; sprawdź auth/profile-ready przepływ |
| RESTORE_TIMED_OUT | Bariera 180 s wygasła | Zweryfikuj brakujące logowania; operator może rozpocząć kontrolowany nowy read, jeśli host jest idle |
| RESTORE_REJECTED | Preflight/commit odrzucił checkpoint | Nie omijaj walidacji; sprawdź konkretne error code i profile/receipt/content compatibility |
| RESTORE_COMMITTED | Atomic server-side publish zakończony | Potwierdź normalną replikację do klientów i monitoring encounteru |
| OPERATOR_ABORT_COMPLETED | Staged candidate anulowany, lease release requested | Potwierdź brak active lease i zachowany GameState |

## 8. QA matrix

- [ ] Diagnostics snapshot działa na Dedicated/Listen Server i zwraca empty/default na świeżej instancji.
- [ ] Snapshot nie zawiera tokenu, ProfileId, participantKey ani checkpoint payloadu.
- [ ] Correlation ID jest wspólny dla zdarzeń jednego cyklu i zmienia się dla kolejnego.
- [ ] Liczniki incrementują się dokładnie na odpowiednich przejściach.
- [ ] Lease failure, read failure, wait timeout, rejection i commit są rozróżnialne.
- [ ] Logi zawierają correlation, outcome, EncounterId, sequence, epoch, HTTP i error code.
- [ ] Retry z candidate + pełną grupą próbuje commit.
- [ ] Retry z candidate + brakującym uczestnikiem pozostawia staged candidate.
- [ ] Retry w trakcie aktywnego gameplay lease jest odrzucony i nie zwalnia lease.
- [ ] Retry w trakcie read/write/commit jest odrzucony.
- [ ] Retry z idle runtime uruchamia fresh restore read.
- [ ] Abort bez staged participant wait jest odrzucony.
- [ ] Abort przy staged participant wait czyści tylko candidate, timer i lease.
- [ ] Abort nie zmienia GameState, actorów, nagród, profili ani ledgeru.
- [ ] Nie istnieje client RPC, UFUNCTION ani Blueprint ścieżka do operator controls.
- [ ] Trusted host wywołuje controls dopiero po zewnętrznym authz operatora.
- [ ] Stage 32–36 regression tests przechodzą bez zmian.

## 9. Walidacja automatyczna

```bash
python Scripts/validate_encounter_recovery_observability.py
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_recovery_observability.py" -v
```

Workflow: `.github/workflows/encounter-recovery-observability.yml`

CI weryfikuje statyczne kontrakty API, brak sekretów w diagnostics, server-only guards, operator gating oraz regresję walidatorów/testów Stage 32–36 i Stage 33 backend suite.

PASS oznacza walidację źródeł/testów Python. Nie oznacza kompilacji C++/UHT, rzeczywistego IAM operatora, działającego eksportera metryk, testów PIE ani testu urządzenia Android.

## 10. Ograniczenia

- Diagnostics i liczniki są procesowe; restart serwera zeruje je.
- Nie dodano dashboardu operatorskiego, uwierzytelnionego HTTP admin API, MFA ani RBAC.
- Nie dodano Prometheus/OpenTelemetry exporter; host może odczytać native diagnostics i zmapować je do własnej telemetrii.
- Nie dodano operator control dla aktywnego encounteru gameplay; abort dotyczy wyłącznie staged participant wait.
- Nie wykonano UE 5.8 UHT/C++ compile, multiplayer PIE, rzeczywistego restart recovery ani APK/AAB.

## Następny etap

**Stage 38 — Recovery Chaos Testing & Failure Injection**: kontrolowane błędy transportu, timeouty, restart backendu i serwera, utrata lease, brak uczestnika, konflikt fencing oraz testy potwierdzające, że GameState nigdy nie otrzymuje częściowego restore.
