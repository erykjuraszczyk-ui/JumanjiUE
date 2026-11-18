# JUMANJI 2.111.1 — Stage 30: Encounter Session Persistence & Reconnect Recovery

## Cel etapu

Dodać ograniczone czasowo odzyskiwanie uczestnika po rozłączeniu klienta, jeśli ten sam encounter nadal działa na tym samym procesie serwera. Profil gracza jest odtwarzany z istniejącego systemu SaveGame, a serwerowa sesja spotkania pozostaje w pamięci działającego GameInstance.

Etap nie zapisuje żywych aktorów, bossa, przeciwników, timerów ani faz encounteru do profilu. Nie implementuje server migration, cross-shard handoff ani wznowienia po restarcie procesu serwera.

## 1. Architektura

### UJumanjiWorldEncounterSessionSubsystem

Nowy UGameInstanceSubsystem przechowuje wyłącznie ulotny rejestr jednej aktywnej sesji encounteru:
- ActiveEncounterId;
- zbiór zweryfikowanych PersistentProfileId uczestników;
- zbiór aktualnie podłączonych uczestników;
- czas rozłączenia uczestnika zapisany jako FDateTime;
- rosnący numer rewizji powrotu.

Nie przechowuje referencji do aktorów, PlayerControllerów, PlayerState, bossa ani przeciwników. Nie wywołuje SaveGame i nie wykonuje transakcji profilu.

Domyślne okno reconnect wynosi 300 sekund. Można je zmienić w DefaultGame.ini:

```ini
[/Script/JumanjiUE.JumanjiWorldEncounterSessionSubsystem]
ReconnectGraceWindowSeconds=300.0
```

Wartość runtime jest ograniczona do 30–1800 sekund.

### Uczestnicy

Podczas startu encounteru serwer rejestruje:
- inicjującego gracza;
- pozostałych graczy aktualnie znajdujących się w GameState::PlayerArray, jeżeli ich PlayerState ma zweryfikowany PersistentProfileId.

Klient nie wysyła ProfileId w RPC i nie może sam dopisać się do rejestru. Dodatkowy uczestnik jest rejestrowany wyłącznie przez serwerowe API.

Nie ma automatycznego dołączania nowych graczy do już uruchomionego spotkania w tej wersji.

## 2. Disconnect / Logout

AJumanjiGameMode::Logout:
1. Pobiera AJumanjiPlayerState od odłączanego PlayerControllera.
2. Pyta session subsystem, czy profil jest uczestnikiem aktywnego encounteru.
3. Jeżeli tak, usuwa profil ze zbioru podłączonych i zapisuje FDateTime::UtcNow() jako czas rozłączenia.
4. Następnie wywołuje istniejące RequestProfileSave().

Brak udziału w aktywnym spotkaniu jest normalnym przypadkiem i nie generuje błędu.

## 3. Reconnect / PostLogin

Po przydzieleniu stabilnego ID profilu i odtworzeniu profilu w AJumanjiGameMode::PostLogin:
1. Server-side subsystem sprawdza, czy PersistentProfileId znajduje się w zbiorze uczestników.
2. Profil musi mieć zapisany czas rozłączenia i nie może być już podłączony.
3. Sprawdzana jest różnica czasu względem ReconnectGraceWindowSeconds.
4. GameState musi nadal zawierać ten sam EncounterId.
5. Dozwolone fazy: Trials, Combat, Boss, Reward.
6. Fazy Inactive, Completed i Failed nie pozwalają na reconnect do aktywnego spotkania.
7. Po akceptacji profil wraca do zbioru ConnectedProfileIds, a session subsystem zwraca snapshot z EncounterId, Phase, EncounterRevision oraz ReconnectRevision.

Poza oknem grace:
- wpis rozłączenia i członkostwo w sesji są usuwane;
- encounter nie jest resetowany ani automatycznie kończony;
- gracz nie otrzymuje prawa do wznowienia tego samego encounteru;
- standardowy Stage 29 retry nadal dotyczy wyłącznie stanu Failed.

## 4. PlayerState — stan powrotu

AJumanjiPlayerState otrzymuje replikowany FJumanjiEncounterReconnectState:
- bWasReconnected;
- EncounterId;
- Phase;
- EncounterRevision;
- ReconnectRevision.

To jest wyłącznie jednorazowa informacja prezentacyjna dla nowego PlayerState. Nie jest dodana do FJumanjiPlayerProfileSnapshot, nie jest częścią schema v2 i nie jest zapisywana przez BuildProfileSnapshot().

Serwer ustawia snapshot dopiero po zaakceptowanym reconnect. Klient otrzymuje go przez normalną replikację PlayerState.

## 5. Encounter HUD

UJumanjiWorldEncounterHUDWidget:
- wiąże się z AJumanjiPlayerState::OnEncounterReconnectChanged;
- udostępnia GetReconnectState() dla Blueprint;
- emituje OnEncounterReconnectNotice(ReconnectState) po zaakceptowanym powrocie;
- odpina delegat w NativeDestruct.

W WBP_EmeraldJungleEncounterHUD można na tej podstawie pokazać komunikat, np. „Połączono ponownie — wróciłeś do aktywnego spotkania”. HUD nie startuje encounteru, nie odtwarza bossa i nie zapisuje postępu.

GameState i serwerowy director nadal są jedynym źródłem prawdy o bieżącej fazie, fali, bossie i aktorach.

## 6. Cykl życia session registry

Session subsystem jest czyszczony przez EndEncounterSession przy:
- poprawnym ukończeniu encounteru;
- przejściu encounteru do Failed;
- EndPlay encounter director;
- nieudanym rozpoczęciu po zarejestrowaniu sesji (rollback).

Po Completed i Failed nie pozostają wpisy, które mogłyby zostać użyte do ponownego dołączenia. Próba reconnect nie jest alternatywnym retry.

## 7. Granice trwałości

| Dane | Miejsce | Przeżywa disconnect klienta? | Przeżywa restart serwera? |
|---|---|---:|---:|
| Wybrany Great Spirit, eksploracja, poziomy, ekwipunek, waluty i transakcje | Profile SaveGame | Tak | Tak, w granicach lokalnego backendu |
| EncounterId / Phase / CurrentWave / ActiveEnemyCount | GameState | Tak, dopóki serwer/world działa | Nie |
| Żywy boss i przeciwnicy | Serwerowe aktory w World | Tak, dopóki serwer/world działa | Nie |
| Rozdział i postęp prób | GameState / trial actors | Tak, dopóki serwer/world działa | Nie |
| Uczestnik i czas disconnect | GameInstance session subsystem | Tak, w tym samym procesie | Nie |
| Reconnect notice | Nowy PlayerState | Tylko w bieżącym PlayerState | Nie |

Local SaveGame nie jest produkcyjnym backendem MMO. Aby obsłużyć server migration lub restart shardu, potrzebny będzie zaufany backend transakcyjny i osobny, wersjonowany kontrakt checkpointów encounteru. Stage 30 tego nie udaje.

## 8. Instrukcja Unreal Editor

1. Pobierz gałąź Stage 30 i skompiluj projekt w Unreal Engine 5.8.
2. Zachowaj BP_EmeraldJungleWorldEncounter jako persistent/always-loaded director.
3. Ustaw w DefaultGame.ini wartość ReconnectGraceWindowSeconds (domyślnie 300).
4. W WBP_EmeraldJungleEncounterHUD obsłuż event OnEncounterReconnectNotice.
5. Z poziomu HUD nie wykonuj RPC start encounter ani resetu prób po powrocie.
6. Uruchom serwer/listen server oraz dwóch klientów.
7. Rozpocznij encounter graczem A, odłącz klienta A w fazie Trials.
8. Ponownie zaloguj się tym samym platformowym ProfileId i sprawdź, czy HUD otrzyma snapshot bieżącej fazy i rewizji.
9. Powtórz w fazach Combat i Boss.
10. Sprawdź reconnect po upływie grace window, po Failed, po Completed i po restarcie procesu serwera — te przypadki mają być odrzucone.

## 9. Automatyczna walidacja źródeł

Uruchom:

```bash
python Scripts/validate_emerald_jungle_encounter_reconnect.py
python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_encounter_reconnect.py" -v
```

Workflow: .github/workflows/emerald-jungle-encounter-reconnect-validation.yml

Kontrole statyczne pilnują:
- zgodności profilu i EncounterId;
- grace window i dozwolonych faz;
- oznaczania disconnect przed usunięciem PlayerState;
- odtwarzania profilu przed próbą reconnect;
- czyszczenia sesji przy terminalnym wyniku encounteru;
- braku aktorów i danych profilu w session subsystem;
- braku transient reconnect notice w SaveGame profile snapshot.

Walidator i testy źródeł nie zastępują kompilacji UE, multiplayer PIE ani testu rozłączenia/relogowania na uruchomionym serwerze.

## 10. Kryteria akceptacji

- [ ] Projekt przechodzi UHT i kompilację Unreal Engine 5.8.
- [ ] Początkowy encounter rejestruje inicjatora i pozostałych aktualnie podłączonych graczy z ProfileId.
- [ ] Logout uczestnika zapisuje czas rozłączenia w session subsystem.
- [ ] PostLogin odtwarza profil przed próbą reconnect.
- [ ] Ten sam ProfileId może wrócić do tego samego aktywnego EncounterId w ciągu 300 sekund.
- [ ] Reconnect przyjmuje fazy Trials, Combat, Boss i Reward.
- [ ] Inny ProfileId, inny EncounterId, duplicate connected profile i profil poza grace window są odrzucane.
- [ ] Failed, Completed i Inactive nie są traktowane jako aktywna sesja do reconnect.
- [ ] HUD otrzymuje replikowany snapshot reconnect i może pokazać komunikat.
- [ ] Stan encounteru, fale i boss pozostają na serwerze; klient nie odtwarza aktorów.
- [ ] Session registry jest czyszczony przy Completed, Failed, EndPlay i rollback.
- [ ] Profile SaveGame nie zawiera FJumanjiEncounterReconnectState ani żywych aktorów.
- [ ] Retry Stage 29 pozostaje odrębną ścieżką i działa tylko z Failed.
- [ ] Dwa klienty PIE potwierdzają poprawny disconnect/reconnect jednego uczestnika.
- [ ] Reconnect po restarcie serwera jest odrzucony; nie jest deklarowany jako obsługiwany.

## Nieobjęte

- Server migration, cross-shard handoff, restart-resume lub host migration.
- Snapshot/serializacja bieżącego AI, bossa, przeciwników, timerów i aktorów prób.
- Produkcyjna autentykacja i współdzielony backend MMO.
- Mid-encounter join dla nowych profili, które nie były w PlayerArray podczas startu.
- Finalny reconnect banner Widget Blueprint .uasset.
- Unreal Engine 5.8 compile logs, multiplayer PIE evidence lub Android APK/AAB.

## Następny etap

Stage 31 — Encounter Reconnect UX & Session QA: dopracować banner reconnect, status utraty grace window, komunikaty odrzuconych prób i macierz testów przerwań sieciowych na urządzeniach mobilnych.