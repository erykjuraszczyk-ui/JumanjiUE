# JUMANJI 2.111.1 — Stage 35: Encounter Restore Materialization & Atomic Publish

## 1. Cel etapu

Dokończyć przepływ Stage 34: odczytać zweryfikowany checkpoint, dopasować jego uczestników do uwierzytelnionych profili bieżącego serwera, uzgodnić rewizje profili i receipt nagrody, odtworzyć wyłącznie dozwolony stan logiczny oraz opublikować stan encounteru jednym serwerowym punktem commit.

Stage 35 dodaje:

- serwerowy `CommitStagedRestore` po zakończonym preflight Stage 34;
- mapowanie `participantKey` na aktywne, serwerowo związane `PersistentProfileId` przez ten sam sekret salt i BLAKE3-160;
- porównanie `profileRevision` checkpointu z aktualnym profilem bez cofania danych profilu;
- kontrolę completion receipt rozdziału i idempotency ledgeru transakcji nagrody;
- odtworzenie completed/incomplete state dla prób Living Board;
- wznowienie następnej fali z autoryzowanego SpawnSet albo bossa w zapisanej fazie i z zapisanym health fraction;
- odtworzenie krótkotrwałego identyfikatora sesji i listy uczestników;
- przygotowanie nowych aktorów poza replikacją, a następnie pojedynczą publikację GameState i włączenie replikacji aktorów;
- regresyjny walidator źródeł, testy jednostkowe i workflow GitHub Actions.

## 2. Przepływ wykonania

```text
Server coordinator (after map + authenticated profiles are ready)
  AJumanjiWorldEncounterDirector::BeginCheckpointRestore()
    UJumanjiEncounterCheckpointRuntimeSubsystem::BeginRestoreStaging()
      acquire backend lease + fencing epoch
      read signed checkpoint
      StageVerifiedBackendResponse()      [Stage 34]
      CommitStagedRestore()
        map opaque participant keys -> live PlayerStates
        profile revision + chapter receipt checks
        RewardPending -> check/grant idempotent profile receipt
        AJumanjiWorldEncounterDirector::MaterializeCheckpointRestore()
          validate content IDs and authored spawn set
          stage trial state / wave actors / boss actor
          keep newly spawned combat actors non-replicating
          restore encounter session membership
          AJumanjiGameState::ApplyEncounterCheckpointRestore()
            assign complete server snapshot
            broadcast delegates only after every field is assigned
          bind chapter/encounter delegates
          publish trial actor state and enable actor replication
```

Restore jest natywną operacją serwera. `BeginCheckpointRestore` i `MaterializeCheckpointRestore` nie są UFUNCTION, BlueprintCallable ani RPC. Klient nie może podać checkpointu, participant key, profile ID, transakcji ani klasy aktora.

## 3. Tożsamość i rewizje profili

Checkpoint zawiera jedynie pseudonimowy `participantKey`, nie surowe `ProfileId`. Serwer oblicza klucz dla każdego PlayerState, którego profile ID zostało wcześniej związane po uwierzytelnieniu:

`pk_ + BLAKE3-160(ParticipantSalt + "|" + PersistentProfileId)`

Wymagane zmienne środowiskowe pozostają zgodne ze Stage 34:

- `JUMANJI_SERVER_INSTANCE_ID`
- `JUMANJI_SHARD_ID`
- `JUMANJI_SERVER_BUILD_ID`
- `JUMANJI_RULESET_VERSION`
- `JUMANJI_WORLD_CONTENT_VERSION`
- `JUMANJI_CHECKPOINT_PARTICIPANT_SALT`

Wszystkie profile z checkpointu muszą być jednocześnie obecne w `AJumanjiGameState::PlayerArray`, mieć serwerowo związany identyfikator i przejść dokładne dopasowanie klucza. Brak jednego uczestnika, duplikat profilu lub duplikat klucza przerywa restore całej grupy — nie odtwarzamy niepełnego party.

Dla każdego profilu aktualna `ProgressionState.Revision` nie może być niższa od zapisanej w checkpoint. Wyższa rewizja jest akceptowana, ale żadne saldo, ekwipunek, doświadczenie ani lista ukończonych rozdziałów nie są kopiowane z checkpointu. Źródłem prawdy pozostaje aktualny zapis profilu.

Dodatkowa kontrola anti-rollback:

- checkpoint `Trials` jest odrzucany, gdy profil ma już trwały receipt ukończenia tego rozdziału — profil wyprzedził zapisany TrialBoundary;
- checkpoint `Combat`, `Boss` lub `Reward` wymaga receipt ukończenia rozdziału na każdym uczestniczącym profilu;
- brakujący lub starszy profil nie jest automatycznie naprawiany danymi z checkpointu.

## 4. RewardPending i idempotency ledger

Schema v1 przechowuje jeden `reward.transactionId`, dlatego Stage 34 dopuszcza `RewardPending` tylko dla jednego uczestnika. Stage 35 ponownie sprawdza:

1. `rewardDefinitionId == EncounterId`;
2. zgodność transaction ID z wartością wyliczoną przez `UJumanjiProgressionRewardSubsystem` dla profilu;
3. czy `AJumanjiPlayerState::HasProcessedProfileTransaction` już zawiera receipt;
4. jeżeli receipt nie istnieje — czy `GrantWorldEncounterCompletionReward` zakończyło się sukcesem;
5. czy receipt jest widoczny po transakcji.

Jeśli receipt już istnieje, nagroda nie jest przyznawana ponownie. Jeśli nie ma go i grant się nie powiedzie, restore jest przerywany przed publikacją świata. Gdy transakcja profilu została zapisana, a dalsza materializacja świata nie powiedzie się, kolejna próba rozpozna receipt i nie wypłaci nagrody drugi raz.

To jest kontrola lokalnego, istniejącego ledgeru profilu. Stage 35 nie wprowadza zewnętrznego, rozproszonego serwisu profili ani transakcji rozproszonej między bazą profili i backendem checkpointów.

## 5. Reguły materializacji

| Safe point | Docelowy stan po restore |
|---|---|
| `Trials / TrialBoundary` z nieukończonym rozdziałem | Aktywna plansza, odtworzone ukończone próby; nieukończone próby są Available i zaczynają się od pierwszego kroku |
| `Trials / TrialBoundary` z kompletem ukończonych prób | Normalizacja do pierwszej fali Combat; zapobiega utknięciu w fazie Trials |
| `Combat / WaveBoundary` | Wznowienie następnej fali z aktualnego, zatwierdzonego SpawnSet |
| `Combat / WaveBoundary` po ostatniej fali | Utworzenie bossa w jego autorskim punkcie spawnu, faza 1 |
| `Boss / BossPhaseBoundary` | Utworzenie bossa w autorskim punkcie spawnu; przywrócenie fazy 2 lub 3, health fraction i phase revision |
| `Reward / RewardPending` | Uzgodnienie receipt i publikacja stanu Completed bez ponownego uruchamiania walki |

Próby odtwarzane są ze stabilnych `CheckpointTrialId` i `ObjectiveId`, a nie z referencji UObject. Ukończona próba otrzymuje `CurrentStepIndex == Steps.Num()`; nieukończona jest ustawiana na `Available` z indeksem zero.

Fala nie zachowuje indywidualnych przeciwników. Safe point WaveBoundary jest rejestrowany po wyczyszczeniu fali; dlatego wznowienie rozpoczyna następną falę, a nie odtwarza pokonanych aktorów ani ich położeń.

Boss odtwarza tylko stabilną definicję, fazę 2/3, health fraction i revision. Nie są serializowane transformacja, aktualny ruch AI, aktywny telegraph, timer ataku ani stan konkretnej Gameplay Ability. Boss zaczyna ponownie z ustawionego w poziomie `BossSpawnPoint`, a jego następny atak podejmuje normalna logika AI/Blueprint.

## 6. Serwerowy commit i sprzątanie

Nowi przeciwnicy/boss są przygotowywani z replikacją wyłączoną. Przed publikacją serwer:

- waliduje cały candidate i skonfigurowane klasy/anchory;
- sprawdza wszystkie profile oraz receipt;
- tworzy aktory do wznowienia i weryfikuje ich inicjalizację;
- odtwarza membership sesji z checkpointowym SessionId;
- aplikuje stan logiczny prób;
- wywołuje `ApplyEncounterCheckpointRestore`.

Ta funkcja GameState konstruuje nowe snapshoty, przypisuje Board Event, Chapter, Objective Set i World Encounter State, a dopiero potem emituje delegaty i `ForceNetUpdate`. Po udanym commit aktory są podpinane do eventów, stan prób jest publikowany i replikacja aktorów zostaje włączona.

To jest pojedynczy punkt commit po stronie serwera, a nie gwarancja jednoczesnego otrzymania pakietu przez wszystkie urządzenia. Replikacja Unreal nadal dociera do klientów zgodnie z normalnym harmonogramem sieci.

Przy nieudanym spawn/preflight staged aktory są niszczone, a utworzone membership sesji jest zwalniane. GameState nie jest publikowany. Jeżeli ledger nagrody został już zatwierdzony przed późniejszym błędem, receipt pozostaje prawidłowy i zapobiega ponownej wypłacie.

## 7. Integracja startowa — wymagany krok hosta

Stage 35 dostarcza natywny punkt wejścia, ale nie wywołuje go automatycznie z `BeginPlay`. Serwerowy coordinator powinien wywołać:

```cpp
EncounterDirector->BeginCheckpointRestore();
```

dopiero gdy:

1. właściwy persistent level i wszystkie wymagane aktory świata są załadowane;
2. istnieje skonfigurowany `AJumanjiWorldEncounterDirector` i Chapter Director;
3. uwierzytelnieni uczestnicy z checkpointu weszli do gry;
4. ich `AJumanjiPlayerState` ma już przywrócony profil i związany `PersistentProfileId`;
5. wymagane zmienne środowiskowe backendu są ustawione.

W razie braku dowolnego uczestnika restore zostanie odrzucony. Koordynator może ponowić wywołanie po zakończeniu logowania wszystkich wymaganych profili. Nie wolno wystawiać tego wywołania bezpośrednio klientowi.

## 8. Poprawka regresyjna Stage 34

Stage 34 miało zbyt restrykcyjne zakończenie `AreCheckpointTrialIdsConsistent`: wymagało liczebności ukończonych prób równej liczbie wszystkich skonfigurowanych prób. W efekcie prawidłowy częściowy `TrialBoundary` był odrzucany. Stage 35 zmienia kontrolę na równoliczność ukończonych TrialId i ObjectiveId, przy zachowaniu walidacji mapowania każdego skonfigurowanego aktora oraz allowlisty ID w parserze checkpointu.

## 9. Walidacja i ograniczenia

Nowe pliki:

- `Scripts/validate_encounter_restore_materialization.py`
- `Scripts/Tests/test_validate_encounter_restore_materialization.py`
- `.github/workflows/encounter-restore-materialization.yml`

Walidator kontroluje granicę serwerową, mapowanie kluczy uczestników, porównanie rewizji, kontrolę receipt, kolejność sesji i materializacji, bramkowanie replikacji aktorów oraz kolejność pól GameState przed emisją delegatów.

To są testy statycznego kontraktu źródeł. Nie zastępują kompilacji C++/Unreal Header Tool, PIE z kilkoma klientami, testów awarii procesu ani rzeczywistego testu backendu na środowisku serwerowym.

Nadal poza zakresem:

- automatyczny startup coordinator i polityka oczekiwania na komplet uczestników;
- zewnętrzny, autorytatywny profile service i transakcja rozproszona;
- odtwarzanie pozycji graczy/AI, transformacji aktorów, aktywnych ataków i timerów;
- migracja między hostami/shardami w środowisku produkcyjnym;
- test kompilacji w Unreal Engine 5.8, PIE multiplayer i walidacja urządzenia Android.

Stage 33 nadal korzysta z SQLite WAL na jednym hoście jako prototypu backendu. Nie jest to jeszcze produkcyjny system MMO/distributed consensus.
