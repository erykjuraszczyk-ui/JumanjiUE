# JUMANJI 2.111.1 — Stage 38: Recovery Chaos Testing & Failure Injection

## 1. Cel etapu

Stage 38 sprawdza zachowanie warstwy odzyskiwania spotkania, gdy operacje kończą się błędem, odpowiedź ginie, lease wygasa albo backend zostaje uruchomiony ponownie.

Zakres:
- testy awarii i niejednoznacznego wyniku zapisu w rzeczywistym prototypie Python/SQLite;
- utracony HTTP acknowledgement po zatwierdzonym zapisie;
- odtworzenie obiektu usługi nad istniejącą bazą SQLite i ponowne przejęcie lease po jego wygaśnięciu;
- konflikt compare-and-swap, konkurencyjne zapisy, błędy SQLite, podpis i TTL;
- walidacja zachowania źródeł Unreal przy utracie lease, braku uczestnika, rollbacku profilu, błędzie materializacji i operator abort;
- macierz awarii i osobny workflow GitHub Actions.

To etap testowy. Nie dodaje nowej ścieżki gameplay ani nie zmienia protokołu checkpointu.

## 2. Co jest wykonywane automatycznie

Nowy zestaw:

- Scripts/Tests/test_encounter_checkpoint_chaos.py
- Scripts/validate_encounter_recovery_chaos.py
- Scripts/Tests/test_validate_encounter_recovery_chaos.py
- Data/Encounter/EncounterRecoveryChaosMatrix.csv
- .github/workflows/encounter-recovery-chaos.yml

Testy backendowe wykorzystują prawdziwy EncounterCheckpointBackend, bazę tymczasową SQLite, rzeczywisty handler HTTP oraz klucz Ed25519 generowany wyłącznie na potrzeby testu. Żaden sekret produkcyjny nie jest potrzebny.

### 2.1 Utracona odpowiedź po commit

Test HTTP zamyka połączenie bez wysłania odpowiedzi po tym, jak backend zakończył transakcję PUT. Klient otrzymuje błąd transportu, ale checkpoint może już istnieć.

Weryfikacja:
1. odczyt aktualnego head sequence z backendu;
2. potwierdzenie, że dokładnie jeden checkpoint został zapisany;
3. próba ponowienia z dawnym expectedSequence;
4. oczekiwany SEQUENCE_CONFLICT;
5. ponowny odczyt head i potwierdzenie, że sequence nie wzrosło drugi raz.

Reguła: po timeout lub zerwanym połączeniu nie wolno zakładać ani sukcesu, ani porażki. Najpierw należy uzgodnić autorytatywny head.

### 2.2 Restart backendu

Test tworzy drugi obiekt usługi z tym samym plikiem SQLite i kluczem testowym. Weryfikuje, że:
- aktywny lease nie jest przejmowany przed wygaśnięciem;
- po kontrolowanym przesunięciu zegara testowego nowy właściciel otrzymuje wyższy authorityEpoch;
- stary token/epoch zostaje odrzucony;
- podpisany checkpoint nadal jest czytelny dla nowego właściciela.

To test trwałości bazy i semantyki usługi po ponownej inicjalizacji obiektu. Nie jest to test kill -9, awarii maszyny ani odtwarzania procesu Unreal.

### 2.3 Atomowość i wyścigi

- Trigger SQLite wymusza ABORT aktualizacji checkpointu; po błędzie sequence i envelope muszą pozostać niezmienione.
- Dwóch writerów równocześnie wysyła ten sam expectedSequence; dokładnie jeden może wygrać CAS.
- Modyfikacja signed payload musi dać SIGNATURE_INVALID.
- Checkpoint po wygaśnięciu musi zwrócić CHECKPOINT_EXPIRED.
- Zatrzymany lokalny socket ma być błędem transportu, a nie podstawą do publikacji świata.

## 3. Kontrole istniejącego runtime Unreal

Walidator Stage 38 ponownie sprawdza kontrakty z Etapów 34–37:

- nieudany lease renewal ustawia bLeaseReady = false i blokuje zapis do czasu resynchronizacji;
- TryCommitStagedRestore nie publikuje świata, jeśli brakuje uwierzytelnionego uczestnika;
- profile checkpointu nie są cofane do starszej rewizji;
- nagroda jest uzgadniana z istniejącym transaction receipt, zamiast wypłacać ją ponownie;
- aktory są przygotowywane bez replikacji i sprzątane po błędzie;
- AJumanjiGameState::ApplyEncounterCheckpointRestore pozostaje pojedynczym punktem publikacji po zakończeniu preflight/materializacji;
- operator Abort usuwa tylko staged candidate w dozwolonym stanie oczekiwania, a nie przerywa aktywnej walki.

Walidator źródeł nie wykonuje C++ ani nie udaje symulacji świata. Jest regresyjnym alarmem, gdy ktoś przypadkiem usunie wymagany guard lub kontrakt.

## 4. Macierz awarii

Pełna macierz znajduje się w Data/Encounter/EncounterRecoveryChaosMatrix.csv.

BackendIntegration — wykonywany automatycznie na prawdziwym kodzie Python/SQLite/HTTP.

StaticSourceAndPIE, StaticSourceAndServerTest — część kontraktu jest sprawdzana automatycznie; część wymaga uruchomienia serwera/PIE.

ManualServerTest i ManualPIE — scenariusz wymaga rzeczywistego runtime i operatora testu. Nie jest liczony jako automatycznie zaliczony przez GitHub Actions.

## 5. Polecenia lokalne

Z katalogu głównego repozytorium:

~~~bash
python -m pip install -r Backend/EncounterCheckpointService/requirements.txt

python Scripts/validate_encounter_recovery_chaos.py
python -m unittest discover -s Scripts/Tests -p "test_encounter_checkpoint_chaos.py" -v
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_recovery_chaos.py" -v

# Regresja backendu Stage 33:
python Scripts/validate_encounter_checkpoint_backend.py
python -m unittest discover -s Scripts/Tests -p "test_encounter_checkpoint_service.py" -v

# Regresja kontraktów Stage 32–37:
python Scripts/validate_encounter_checkpoint_contract.py
python Scripts/validate_encounter_checkpoint_runtime.py
python Scripts/validate_encounter_restore_materialization.py
python Scripts/validate_encounter_startup_coordinator.py
python Scripts/validate_encounter_recovery_observability.py
~~~

Workflow: .github/workflows/encounter-recovery-chaos.yml.

## 6. Ręczne testy UE 5.8 / Dedicated Server / PIE

Przed wdrożeniem wykonaj i zapisz rzeczywiste wyniki:

1. Uruchom backend lokalny oraz Dedicated Server z poprawnym środowiskiem serwerowym.
2. Rozpocznij encounter i doprowadź go do co najmniej jednego safe point.
3. Zablokuj połączenie z backendem przed write i potwierdź, że runtime nie fabrykuje lokalnego checkpointu.
4. Opuść odpowiedź po zatwierdzonym write; sprawdź korelację po EncounterId, sequence i epoch, bez logowania tokenu/payloadu.
5. Odłącz backend, wymuś lease renewal failure i potwierdź blokadę kolejnych zapisów do czasu kontrolowanego resync.
6. Uruchom ponownie backend z tym samym trwałym storage i sprawdź fencing starego właściciela.
7. Uruchom dwa serwery testowe na tym samym EncounterId; tylko jeden może utrzymać aktywny lease.
8. W PIE z dwoma klientami nie dopuszczaj jednego wymaganego uczestnika do profilu-ready. Oczekuj RESTORE_WAITING_FOR_PARTICIPANTS; GameState i świat pozostają bez zmian.
9. Podaj profil o starszej rewizji niż checkpoint. Oczekuj odrzucenia bez cofania profilu i bez publikacji świata.
10. Powtórz RewardPending z istniejącym transaction receipt. Saldo i ekwipunek nie mogą wzrosnąć drugi raz.
11. Wymuś błąd spawn/initialization jednego staged aktora. Wszystkie staged actors powinny zostać sprzątnięte, a GameState nie może otrzymać częściowego stanu.
12. W trakcie participant wait wykonaj autoryzowany operator Abort. Sprawdź wyczyszczenie candidate/timera/lease bez ingerencji w aktywny świat.

Każdy run sheet powinien zapisywać commit SHA, build UE, tryb serwera, mapę, liczbę klientów, fault injection point, correlation ID i oczekiwany/rzeczywisty outcome. Nigdy nie umieszczaj lease tokenów, kluczy ani danych profili w logach testowych.

## 7. Kryteria akceptacji

- [ ] Test utraconego acknowledgement wykazuje dokładnie jeden commit i konflikt przy ślepym retry.
- [ ] Restart obiektu backendu odczytuje zachowany checkpoint i respektuje wygaśnięcie lease.
- [ ] Stary epoch nie może wykonać read/write po przejęciu lease przez nowego właściciela.
- [ ] SQLite ABORT nie zmienia sequence ani envelope.
- [ ] Wyścig dwóch writerów ma dokładnie jednego zwycięzcę.
- [ ] Zmieniony podpis i wygasły checkpoint są odrzucane.
- [ ] Niedostępny backend nie powoduje lokalnego, fikcyjnego sukcesu.
- [ ] Lease renewal failure blokuje dalszy zapis do resynchronizacji.
- [ ] Missing participant i profile rollback nie publikują GameState.
- [ ] Reward receipt nie jest przyznawany drugi raz.
- [ ] Błąd materializacji sprząta staged actors przed publikacją.
- [ ] Operator Abort nie zmienia aktywnej rozgrywki.
- [ ] Macierz, walidator, testy Stage 38 i regresje Stage 32–37 przechodzą.
- [ ] UE 5.8 UHT/C++ build, multiplayer PIE i kontrolowane restart/crash testy są wykonane w rzeczywistym środowisku.

## 8. Ograniczenia i status dowodów

GitHub Actions może potwierdzić testy Python/SQLite/HTTP oraz statyczne kontrakty plików źródłowych. Nie uruchamia tutaj silnika Unreal Engine.

W szczególności:
- restart obiektu backendu na tym samym pliku SQLite nie jest równoważny awarii hosta lub kill -9;
- testy backendowe nie wykonują materializacji actorów ani replikacji;
- statyczny walidator nie dowodzi poprawności runtime;
- testy PIE/Dedicated Server i Android muszą zostać wykonane w rzeczywistym środowisku.

Nie wykonano kompilacji Unreal Engine 5.8, UHT, multiplayer PIE, fizycznego crash/restart serwera ani testu Android. Nie należy oznaczać tych pozycji jako PASS na podstawie samego CI.

## 9. Następny etap

Stage 39 — Recovery Load, Soak & Performance Benchmarks: powtarzalne testy długotrwałego lease renewal, rozmiaru checkpointu, latencji zapisu/odczytu, konkurencji wielu encounterów i stabilności pod obciążeniem; limity SLO oraz profilowanie na rzeczywistym serwerze.
