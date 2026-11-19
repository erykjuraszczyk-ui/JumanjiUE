# JUMANJI 2.111.1 — Stage 31: Encounter Reconnect UX & Session QA

## Cel etapu

Dokończyć warstwę użytkownika i QA dla same-server reconnect z Stage 30. Po ponownym wejściu do gry serwer rozróżnia trzy widoczne wyniki:
- udany powrót do nadal aktywnego spotkania;
- upływ okna reconnect;
- zakończenie spotkania podczas nieobecności gracza.

Normalne logowanie, nowy profil, profil bez stabilnej tożsamości oraz duplikat już podłączonej sesji pozostają ciche. Klient nie uzyskuje prawa do wznowienia, resetowania ani odtwarzania encounteru.

## 1. Nowe typy i kontrakt sesji

### EJumanjiEncounterReconnectOutcome

- None — brak notice; normalny login i przypadki niekwalifikujące się nie tworzą fałszywego banneru.
- Reconnected — serwer zaakceptował ten sam profil i aktywny EncounterId.
- GraceWindowExpired — gracz wrócił po przekroczeniu grace window.
- EncounterEnded — encounter zakończył się podczas nieobecności gracza.

Nie ma osobnego wyniku dla nieznanego profilu lub nowego gracza. To celowe: UI nie powinno ujawniać szczegółów rejestru profili.

### FJumanjiEncounterReconnectFeedbackState

Transient replicated snapshot na AJumanjiPlayerState:
- Outcome;
- EncounterId;
- Phase;
- Revision;
- DisplayDurationSeconds.

Stan jest odrębny od FJumanjiEncounterReconnectState. Stary snapshot nadal oznacza wyłącznie zaakceptowany powrót; nowy snapshot służy do prezentacji także odrzuconego powrotu.

PlayerState przyjmuje go tylko po stronie serwera, z niepustym EncounterId, znanym wynikiem, rosnącą rewizją i ograniczonym czasem wyświetlania. OnRep publikuje OnEncounterReconnectFeedbackChanged.

Stan jest replikowany na potrzeby UX, ale nie jest częścią FJumanjiPlayerProfileSnapshot i nie jest zapisywany przez BuildProfileSnapshot() ani SaveGame.

## 2. Session subsystem — ograniczone tombstones wyniku terminalnego

UJumanjiWorldEncounterSessionSubsystem nadal nie przechowuje aktorów, PlayerControllerów, PlayerState, bossów, fal, timerów ani danych profilu.

Dodano ograniczony rejestr terminalnych notice dla uczestników, którzy byli rozłączeni w chwili zakończenia spotkania:
- RecentlyEndedEncounterIds;
- RecentlyEndedAtUtc;
- RecentlyEndedPhases.

Po EndEncounterSession:
- aktywna sesja i listy uczestników są czyszczone;
- wyłącznie wcześniej rozłączeni uczestnicy mogą dostać jednorazowe EncounterEnded notice;
- notice jest przechowywany maksymalnie 600 sekund;
- rejestr ma limit 256 wpisów;
- wpis jest usuwany po prezentacji, po wygaśnięciu albo przy przekroczeniu limitu;
- wpis nie umożliwia reconnect i nie odtwarza encounteru.

Dla aktywnego encounteru grace window nadal wynosi domyślnie 300 sekund i jest ograniczone do 30–1800 sekund. Przekroczenie tego limitu zwraca GraceWindowExpired i usuwa członkostwo profilu z aktywnej sesji.

W zwykłych przypadkach:
- nowy gracz / nieuczestniczący profil: brak notice;
- profil już podłączony: brak notice, duplicate rejoin odrzucony;
- brak zweryfikowanego ProfileId: brak próby reconnect;
- brak aktywnego encounteru i brak terminalnego tombstone: brak notice.

### Kompatybilność

Dwupunktowa wersja TryRejoinPlayer z Stage 30 pozostaje jako overload kompatybilności. GameMode używa nowego overloadu z OutReconnectState i OutFeedbackState.

## 3. GameMode — PostLogin

PostLogin zachowuje kolejność:
1. serwer wiąże zweryfikowany ProfileId;
2. odtwarza profil z istniejącego profile subsystem;
3. pyta session subsystem o reconnect;
4. przy sukcesie ustawia dotychczasowy FJumanjiEncounterReconnectState i nowy feedback Outcome=Reconnected;
5. przy GraceWindowExpired lub EncounterEnded ustawia wyłącznie feedback notice;
6. przy Reward catch-up nadal korzysta z istniejącej idempotentnej transakcji nagrody.

Feedback nie zmienia fazy spotkania, liczby fal, trial progress, bossa, aktorów ani nagród. Stage 29 retry pozostaje oddzielnym mechanizmem i nadal dotyczy wyłącznie Failed.

## 4. Encounter HUD — banner

UJumanjiWorldEncounterHUDWidget udostępnia:
- GetReconnectFeedbackState();
- GetReconnectFeedbackText();
- IsReconnectFeedbackSuccess();
- GetReconnectFeedbackDisplayDuration();
- OnEncounterReconnectFeedbackPresentationChanged(FeedbackState, Message, bIsSuccess, DisplayDurationSeconds).

Stare OnEncounterReconnectNotice(ReconnectState) pozostaje dla istniejących Blueprintów.

### WBP_EmeraldJungleEncounterHUD

1. Otwórz istniejący Widget Blueprint derived from UJumanjiWorldEncounterHUDWidget.
2. Dodaj panel ReconnectBanner w górnej bezpiecznej strefie ekranu.
3. Wewnątrz dodaj ikonę/status, TextBlock i opcjonalny dekoracyjny pasek czasu.
4. Zaimplementuj OnEncounterReconnectFeedbackPresentationChanged.
5. Ustaw widoczność banneru i tekst z argumentu Message.
6. Gdy bIsSuccess=true, użyj spokojnego stylu potwierdzenia; dla false użyj neutralnego ostrzeżenia. Nie używaj migającego czerwonego alarmu.
7. Odtwórz krótki fade/slide-in, utrzymaj banner przez DisplayDurationSeconds, następnie wykonaj fade-out i Collapsed.
8. Zastosuj Timer w Widget Blueprint albo animację o kontrolowanym czasie; usuń poprzedni Timer przed uruchomieniem nowego.
9. Nie dodawaj przycisku Retry do bannera. Ponowienie spotkania należy do odrębnego Stage 29 UI i jego serwerowych warunków.
10. Nie wywołuj RPC start, reset trial ani modyfikacji GameState z tego callbacku.

Wybierz jedno źródło prezentacji: rekomendowany jest nowy OnEncounterReconnectFeedbackPresentationChanged. Stary callback OnEncounterReconnectNotice pozostaw bez podłączonej drugiej kopii banneru, aby sukces nie wyświetlał się dwa razy.

### Mobile UX

- Banner ma mieścić się w Safe Zone i nie zasłaniać przycisków ruchu, ataku, interakcji ani mapy.
- Minimalny czas odczytu: 4–6 sekund.
- Bez wymuszania pauzy, zmiany trybu sterowania lub blokowania ruchu.
- Nie emituj banneru przy Outcome=None.
- Używaj lokalizowanych FText; nie formatuj ProfileId ani wewnętrznych identyfikatorów w komunikacie.

## 5. QA matrix

Źródło: Data/Encounter/EncounterReconnectSessionQAMatrix.csv

13 scenariuszy:
- reconnect w Trials, Combat, Boss i Reward;
- upływ grace window;
- powrót po Completed i Failed;
- nowy uczestnik podczas aktywnego encounteru;
- duplicate connected profile;
- brak zweryfikowanego ProfileId;
- seria krótkich network flap;
- Android app background poza grace window;
- jednorazowy terminal notice.

Matrix opisuje oczekiwany outcome, widoczność banneru, zakaz modyfikacji encounteru i warstwę walidacji. To plan testów, a nie dowód, że testy PIE/Android już wykonano.

## 6. Automatyczna walidacja i CI

Walidator: Scripts/validate_emerald_jungle_encounter_reconnect_ux.py
Testy: Scripts/Tests/test_validate_emerald_jungle_encounter_reconnect_ux.py
Workflow: .github/workflows/encounter-reconnect-ux-session-qa.yml

Uruchom w katalogu głównym:

    python Scripts/validate_emerald_jungle_encounter_reconnect.py
    python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_encounter_reconnect.py" -v

    python Scripts/validate_emerald_jungle_encounter_reconnect_ux.py
    python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_encounter_reconnect_ux.py" -v

Kontrole sprawdzają:
- outcome enum i transient feedback snapshot;
- serwerową replikację PlayerState;
- brak feedback w BuildProfileSnapshot;
- kolejność restore profilu → reconnect attempt;
- bounded terminal tombstones i ich jednorazowe zużycie;
- callbacki UMG i delegate cleanup;
- komplet 13 scenariuszy QA;
- spójność outcome z oczekiwaną widocznością banneru.

PASS oznacza kontrolę źródeł i macierzy. Nie oznacza kompilacji UE, testu UHT, działania Widget Blueprint, multiplayer PIE ani testu na fizycznym urządzeniu.

## 7. Multiplayer PIE — ręczna procedura

Uruchom listen server i dwóch klientów z dwoma zweryfikowanymi profilami.

1. Gracz A rozpoczyna encounter.
2. Gracz B pozostaje podłączony i obserwuje bieżącą fazę.
3. Odłącz A w Trials, wróć przed upływem 300 sekund. Oczekuj Reconnected i identycznego snapshotu spotkania.
4. Powtórz w Combat i Boss. Potwierdź brak ponownego spawnu bossa i brak resetu fali.
5. Odłącz A w Reward, wróć i sprawdź idempotentny catch-up nagrody.
6. Rozłącz A i wróć po 301 sekundach podczas aktywnego encounteru. Oczekuj GraceWindowExpired; faza i aktorzy pozostają bez zmian.
7. Odłącz A, zakończ encounter jako Completed, następnie zaloguj A ponownie. Oczekuj jednorazowego EncounterEnded.
8. Powtórz z Failed. Upewnij się, że banner nie uruchamia retry.
9. Zaloguj świeży profil w trakcie aktywnego encounteru. Nie powinno być reconnect banneru ani late-join rejestracji.
10. Spróbuj podłączyć ten sam profil drugi raz, gdy pierwszy klient jest nadal connected. Oczekuj odrzucenia bez banneru.
11. Sprawdź szybkie rozłączenie/ponowne połączenie trzy razy. Powinna istnieć tylko jedna connected membership dla profilu.
12. Po terminalnym notice wykonaj drugi login tego samego profilu. Ten sam tombstone nie powinien wyświetlić się po raz drugi.

## 8. Android — ręczna procedura sieciowa

Na urządzeniu testowym:
- uruchom Development build i zaloguj się profilem, który uczestniczy w encounterze;
- rozpocznij spotkanie z drugim klientem/serwerem;
- zasymuluj utratę sieci przez tryb samolotowy lub kontrolowane rozłączenie;
- przywróć sieć przed 300 sekundami i sprawdź powrót do istniejącej fazy;
- powtórz po przekroczeniu 300 sekund;
- zminimalizuj aplikację i pozostaw ją w tle dłużej niż grace window;
- zarejestruj czas od PostLogin do wyświetlenia banneru, brak podwójnych komunikatów, FPS, pamięć procesu i zachowanie temperatury.

Nie wpisuj fikcyjnych wyników do run sheet. Zapisuj model urządzenia, wersję Androida, renderer i typ builda.

## 9. Kryteria akceptacji

### Źródła i serwer
- [ ] Projekt przechodzi UHT i kompilację UE 5.8.
- [ ] Tylko serwer ustawia FJumanjiEncounterReconnectFeedbackState.
- [ ] Feedback replikuje się przez PlayerState i OnRep emituje delegate.
- [ ] Feedback nie jest serializowany do profile SaveGame.
- [ ] Sukces oznacza ten sam EncounterId, profil i aktywną fazę.
- [ ] Przekroczenie 300 sekund zwraca GraceWindowExpired bez resetu spotkania.
- [ ] Completed/Failed zwraca EncounterEnded, nie retry.
- [ ] Terminal tombstone ma retencję 600 sekund i limit 256 wpisów.
- [ ] Terminal tombstone jest konsumowany tylko raz.
- [ ] Nowy profil i duplicate connected profile pozostają bez komunikatu.

### HUD
- [ ] WBP implementuje nowy callback.
- [ ] Success i warning mają odrębne, spokojne style.
- [ ] Banner auto-hide używa DisplayDurationSeconds.
- [ ] Zmiana PlayerState odpina stare delegate.
- [ ] NativeDestruct odpina wszystkie delegate reconnect.
- [ ] Nie ma podwójnego banneru ze starego i nowego callbacku.
- [ ] Banner nie zatrzymuje ruchu i nie wykonuje RPC gameplay.

### QA
- [ ] 13 scenariuszy matrix przechodzi walidator.
- [ ] Stare Stage 30 validator i testy nadal przechodzą.
- [ ] Dwa klienty PIE potwierdzają reconnect i niezależność sesji.
- [ ] Android Development build przechodzi testy przed i po grace window.
- [ ] Zapisano realne wyniki testu sieci i urządzenia.

## 10. Ograniczenia

- Nie utworzono binarnego WBP_EmeraldJungleEncounterHUD .uasset.
- Nie wykonano kompilacji Unreal 5.8, UHT, multiplayer PIE ani testu fizycznego Android.
- Nie wdrożono server migration, cross-shard reconnect ani restart-resume.
- Terminal tombstones są ulotne i znikają po zakończeniu procesu serwera.
- Stage 30 i Stage 31 nie zapewniają produkcyjnej autentykacji kont ani współdzielonego backendu MMO.

## Następny etap

**Stage 32 — Encounter Session Checkpoint Contract & Server Recovery Design**: zaprojektować wersjonowany, bezpieczny kontrakt checkpointów do zaufanego backendu przed próbą wznowienia spotkania po restarcie procesu lub migracji shardu. Nie serializować arbitralnego stanu actorów ani przyjmować checkpointów od klienta.
