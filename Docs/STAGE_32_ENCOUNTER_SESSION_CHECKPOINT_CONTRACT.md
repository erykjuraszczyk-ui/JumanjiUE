# JUMANJI 2.111.1 — Stage 32: Encounter Session Checkpoint Contract & Server Recovery Design

## Cel

Zdefiniować wersjonowany, serwerowo podpisywany format logicznego checkpointu spotkania, który w przyszłości umożliwi odzyskanie encounteru po restarcie procesu lub migracji shardu.

Stage 30–31 obsługują wyłącznie ponowne dołączenie do tej samej sesji, która nadal istnieje w pamięci GameInstance. Stage 32 nie zmienia tego ograniczenia: dostarcza kontrakt danych, reguły bezpieczeństwa, fixture, macierz przypadków odrzucenia i automatyczną kontrolę źródeł. Nie wdraża backendu, lease service, kluczy podpisu ani runtime resume.

## 1. Granice zaufania

- Jedynym autorem checkpointu jest autorytatywny proces serwera, po zatwierdzonym punkcie kontrolnym.
- Klient nie może tworzyć, przesyłać, podpisywać, aktualizować ani odtwarzać checkpointu.
- Profil pozostaje oddzielnym systemem trwałości. Checkpoint zawiera wyłącznie opaque participantKey i rewizję profilu, nigdy surowe ProfileId, tokeny logowania, dane portfela ani ekwipunek.
- Zaufany backend przechowuje checkpoint i kontroluje monotoniczne sequence, aktywny lease oraz fencing epoch.
- Podpis Ed25519 obejmuje kanoniczny obiekt payload, a nie kopię payload z polem signature. Przed podpisaniem należy użyć JSON Canonicalization Scheme (RFC 8785). Weryfikacja klucza i podpisu jest obowiązkiem backendu/serwera.
- Klucz prywatny nie może znajdować się w kliencie, repozytorium, pliku konfiguracyjnym gry ani w checkpoint payload. W produkcji klucz przechowuje KMS/HSM albo równoważny secret store.

## 2. Artefakty kontraktu

- Schema: Data/Encounter/EncounterCheckpoint.schema.json — JSON Schema Draft 2020-12, envelope v1.
- Przykład: Data/Encounter/EncounterCheckpointExample.json — syntaktyczny fixture testowy z kluczem TEST-ONLY; jego podpis nie jest prawdziwym podpisem kryptograficznym.
- Macierz: Data/Encounter/EncounterCheckpointRecoveryMatrix.csv — wymagane scenariusze akceptacji i odrzucenia.
- Walidator: Scripts/validate_encounter_checkpoint_contract.py.
- Testy: Scripts/Tests/test_validate_encounter_checkpoint_contract.py.
- CI: .github/workflows/encounter-checkpoint-contract.yml.

Walidator lokalny sprawdza strukturę, semantykę i fixture. Nie wykonuje kryptograficznej weryfikacji Ed25519 i nie zastępuje backendu.

## 3. Envelope v1

Envelope ma dokładnie trzy klucze:

| Pole | Znaczenie |
|---|---|
| envelopeVersion | Wersja opakowania, obecnie 1 |
| payload | Wersjonowany, logiczny stan encounteru |
| integrity | Algorytm, identyfikator klucza i podpis Ed25519 |

Payload zawiera:
- schemaVersion, checkpointId, encounterId, sessionId;
- checkpointSequence — ściśle rosnący numer dla konkretnej sesji;
- createdAtUtc i expiresAtUtc;
- authority — serverInstanceId, shardId, authorityEpoch, serverBuildId, rulesetVersion, worldContentVersion;
- safePoint — fazę, rodzaj bariery, rewizję encounteru, snapshot postępu rozdziału i logiczny stan walki;
- deterministic — seed encounteru i stabilny identyfikator zestawu spawnów;
- participants — opaque participantKey oraz profileRevision;
- reward — wyłącznie w fazie Reward, ze stabilnym rewardDefinitionId i transactionId oraz statusem PendingLedgerCheck.

Schema v1 dopuszcza maksymalnie 24 godziny między createdAtUtc i expiresAtUtc. Backend przy odczycie musi dodatkowo sprawdzić, czy checkpoint nie wygasł według własnego zaufanego zegara.

## 4. Bezpieczne punkty zapisu

Checkpoint można utworzyć wyłącznie po osiągnięciu stabilnej bariery serwerowej:

| Faza | safePoint.kind | Warunek |
|---|---|---|
| Trials | TrialBoundary | Próba zakończona, zmiany rozdziału zatwierdzone, brak interakcji w toku |
| Combat | WaveBoundary | Fala zakończona, brak żywych przeciwników tej fali, brak oczekującego spawnu |
| Boss | BossPhaseBoundary | Boss zakończył przejście fazy; brak aktywnego telegraphu/ataku i brak nierozliczonych efektów |
| Reward | RewardPending | Encounter zakończył walkę; reward transaction ma stabilny idempotency key i jest sprawdzany w ledgerze |

Nie zapisuj checkpointu w środku animacji, fizycznej symulacji, traversal, ataku, telegraphu, spawnu fali, transakcji ekonomicznej ani resetu próby. Nie próbuj tworzyć „best effort” checkpointu w EndPlay lub podczas awarii procesu.

## 5. Co wolno zapisać

Dozwolone są wyłącznie identyfikatory treści i logiczne wartości potrzebne do deterministycznego odtworzenia:
- faza i rewizje encounteru/rozdziału;
- zakończone ObjectiveId i TrialId;
- indeks i liczba fal na granicy fali;
- identyfikator definicji bossa, identyfikator zatwierdzonej fazy, znormalizowany healthFraction i rewizja fazy;
- seed encounteru i allowlistowany spawnSetId;
- lista uczestników w postaci opaque participantKey;
- rewardDefinitionId i transactionId wymagane do idempotentnego sprawdzenia nagrody.

Wartości są walidowane względem bieżącego ruleset/content manifestu. ID nieznane, wycofane lub niezgodne z wersją treści powodują odrzucenie albo jawnie zatwierdzoną migrację.

## 6. Czego nie wolno serializować

Checkpoint nie jest serializacją świata Unreal. Zabronione są:
- UObject/AActor/UActorComponent, wskaźniki, soft object path niezatwierdzonego zasobu, NetGUID i referencje do World;
- transformy aktorów, pozycje/rotacje/velocity, fizyka i stan CharacterMovement;
- AIController, Blackboard, target references, aktualne taski BT/EQS i arbitralne runtime state;
- timery, delegaty, latent actions, anim montage position, Niagara state, audio playback i otwarte RPC;
- pełny stan bossa, przeciwników lub prób jako surowe właściwości obiektów;
- surowy ProfileId, e-mail, token, hasło, klucz prywatny, cookie lub dane uwierzytelniające;
- saldo, inventory, nagrody lub ledger profilu. Ekonomia ma własny trwały, idempotentny kontrakt transakcyjny.

Boss po odzyskaniu jest tworzony na nowo z allowlistowanej definicji, fazy i healthFraction. Przeciwnicy fali są odtwarzani z zatwierdzonego spawnSetId i deterministycznego seeda, a nie z zapamiętanych actorów.

## 7. Zapobieganie rollback i split-brain

Każdy checkpoint jest przypisany do authorityEpoch oraz monotonicznego checkpointSequence.

1. Serwer żąda zapisu z oczekiwanym poprzednim sequence/ETag (compare-and-swap).
2. Backend odrzuca duplicate, niższy sequence, niezgodny EncounterId i nieaktualny authorityEpoch.
3. Odzyskiwanie rozpoczyna się od AcquireRecoveryLease. Backend wydaje jednego właściciela i zwiększa fencing epoch.
4. Poprzedni proces traci prawo zapisu po zmianie epoch, nawet gdy jego połączenie sieciowe nadal działa.
5. Lease ma krótki TTL, odnawianie i jawny release. Checkpoint nie przechowuje lease tokenu.
6. Dwie instancje nie mogą jednocześnie publikować GameState dla tego samego EncounterId.

Lease service, compare-and-swap store i fencing są wymaganiami przyszłego backendu. Nie są implementowane przez ten etap.

## 8. Procedura odzyskiwania

1. Nowa instancja uwierzytelnia się jako serwer, nie jako gracz.
2. Żąda lease dla EncounterId i otrzymuje nowy fencing epoch.
3. Pobiera najnowszy checkpoint, sprawdza podpis, keyId, schemaVersion, TTL, sequence, authority epoch i kompatybilność build/ruleset/content.
4. Odrzuca checkpoint wygasły, cofnięty, uszkodzony, podpisany nieznanym kluczem lub nieobsługiwany przez aktualny kod.
5. Ładuje zatwierdzony world/content manifest i weryfikuje, że wszystkie potrzebne ID istnieją.
6. Odtwarza stan w prywatnym buforze/transaction context, bez publikowania częściowego encounteru.
7. Sprawdza profile uczestników przez zaufany profile service oraz nagrodę przez ledger transakcji.
8. Odtwarza aktorów wyłącznie przez allowlistowane fabryki klas i spawnSetId.
9. Po pełnym preflight publikuje nową rewizję FJumanjiWorldEncounterState i dopiero wtedy dopuszcza graczy.
10. Jeżeli dowolny krok nie przejdzie, nie rozpoczyna częściowej sesji. Oznacza recovery jako rejected/manual intervention i nie przyznaje nagrody.

RewardPending nigdy nie oznacza „wypłać drugi raz”. Backend najpierw odczytuje transactionId w autorytatywnym ledgerze; ponowienie tej samej transakcji musi być idempotentne.

## 9. Kompatybilność i migracja

- envelopeVersion i schemaVersion są oddzielnymi polami.
- Nieznana wersja jest domyślnie odrzucana.
- Migracja musi być jawna, deterministyczna, wersjonowana i przetestowana na fixture.
- serverBuildId, rulesetVersion oraz worldContentVersion są wymagane do weryfikacji kompatybilności.
- Nie używaj „best effort defaults” dla usuniętych ID, faz, fal ani nagród.
- Przechowuj poprzedni checkpoint do czasu zatwierdzenia nowego recovery oraz zakończenia okresu retencji.

## 10. QA i kryteria akceptacji

Macierz RecoveryMatrix.csv opisuje minimalny zestaw przypadków. Przyszła implementacja backendu musi pokryć: poprawne bariery czterech faz, wygasanie, rollback sequence, podpis, epoch, ruleset, niebezpieczny punkt, zakazane referencje, surowe dane profilu, brak transactionId i konkurencyjny lease.

Stage 32 przechodzi odbiór źródłowy, gdy:
- schema JSON jest poprawna składniowo i ma zamknięte obiekty;
- fixture v1 przechodzi walidator;
- faza i rodzaj bariery są spójne;
- Boss wymaga boss snapshot, a Reward wymaga reward transaction;
- duplicate participantKey i forbidden runtime/identity fields są odrzucane;
- TTL jest dodatni i nie przekracza 24 godzin;
- macierz zawiera wymagane przypadki;
- CI uruchamia walidator i unittest.

Przed uznaniem systemu za gotowy produkcyjnie wymagane są osobne testy: prawdziwe Ed25519, KMS key rotation, mTLS, storage CAS, lease fencing, restart process, shard migration, rollback/replay i exactly-once reward semantics.

## 11. Ograniczenia

- Brak implementacji API backendu, bazy checkpointów, KMS/HSM, mTLS i recovery lease.
- Brak runtime snapshot builder/restore service w C++.
- Brak server migration lub resume po restarcie w działającej grze.
- Brak kompilacji UE 5.8, UHT, PIE, dedicated server i testu Android w tym etapie.
- Stage 30 same-server reconnect działa bez zmian; nie należy reklamować go jako cross-server recovery.

## Oficjalne punkty odniesienia

- JSON Schema Draft 2020-12: https://json-schema.org/draft/2020-12
- JSON Canonicalization Scheme (RFC 8785): https://www.rfc-editor.org/rfc/rfc8785
- Ed25519 (RFC 8032): https://www.rfc-editor.org/rfc/rfc8032
- Unreal Engine GameInstanceSubsystem: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Subsystems/UGameInstanceSubsystem

## Następny etap

**Stage 33 — Encounter Checkpoint Backend Adapter & Lease Service Prototype**: zaimplementować serwerowy adapter do testowego trusted backendu z compare-and-swap, monotonicznym fencing epoch, podpisem Ed25519 i testami konkurencyjnego recovery. Nie udostępniać tego API klientowi.
