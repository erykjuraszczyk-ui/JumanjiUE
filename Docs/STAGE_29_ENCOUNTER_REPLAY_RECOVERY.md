# JUMANJI 2.111.1 — Stage 29: Encounter Replay & Recovery

## Cel etapu

Dodać kontrolowaną ponowną próbę dla nieudanego World Encounter z Etapu 27–28. Retry ma odtworzyć sesyjny stan dziewięciu prób, wyczyścić aktorów spawnowanych przez poprzednią próbę i ponownie uruchomić ten sam EncounterId.

Etap nie cofa trwałej progresji profilu i nie tworzy drugiego systemu nagród.

## Zakres

- retry jest dostępne wyłącznie, gdy GameState ma fazę Failed i ten sam EncounterId;
- zakończony encounter (Completed) pozostaje terminalny i nie może zostać uruchomiony ponownie tą ścieżką;
- rozdział Living Board i aktory prób są resetowane tylko przez zaufaną logikę serwerową;
- aktory wszystkich spawnów spotkania są śledzone, również pokonane aktory, które pozostały jeszcze przez krótki lifespan kosmetyczny;
- reset zmienia wyłącznie sesyjny stan GameState, stan actorów prób i encounter-owned actors;
- zapis profilu, historia ukończonych rozdziałów, waluty, przedmioty i ledger transakcji nie są cofane ani czyszczone.

## Przepływ retry

1. Spotkanie przechodzi do `Failed` przez istniejący `FailEncounter`.
2. HUD odczytuje zreplikowany `FJumanjiWorldEncounterState` i może pokazać możliwość ponownej próby.
3. Gracz ponownie wywołuje istniejące `AJumanjiPlayerController::RequestStartWorldEncounter`. Nie dodano nowego RPC ani zaufania do klienta.
4. Serwer ponownie weryfikuje dystans, profil, wymagania eksploracji, traversal receipts i konfigurację encounteru.
5. `AJumanjiWorldEncounterDirector::StartEncounter` rozpoznaje `Failed` dla tego samego EncounterId.
6. `AJumanjiAdventureChapterDirector::ResetChapterForEncounterRetry` sprawdza, czy EncounterId i faza pasują, czy istnieje ten sam rozdział i czy wszystkie dziewięć aktorów prób może zostać bezpiecznie zresetowanych.
7. Jeśli Board Event jest nadal aktywny, serwer kończy wyłącznie ten BoardEventId. Nie kończy obcego wydarzenia.
8. Każda próba wraca do `Dormant`, `CurrentStepIndex = 0`, a jej revision wzrasta. GameState ustawia rozdział na `Inactive` i czyści tylko sesyjny snapshot celów.
9. Encounter Director usuwa wszystkie aktory spawnowane w poprzednim podejściu, łącznie z pokonanymi aktorami będącymi w okresie wygaszania.
10. GameState rozpoczyna nową rewizję tego samego EncounterId w fazie `Trials`, a istniejący chapter director uruchamia dziewięć prób od początku.

Jeżeli dowolna walidacja nie przejdzie, retry jest odrzucany. Nie jest wykonywany częściowy start nowej fali lub bossa.

## Implementacja runtime

### AJumanjiGameState

`BeginWorldEncounter` przyjmuje:
- początkowe uruchomienie z fazy `Inactive`;
- ponowne uruchomienie wyłącznie z fazy `Failed` i dla identycznego EncounterId.

`ResetAdventureChapterForEncounterRetry` jest wewnętrzną metodą serwerową. Wymaga matching failed EncounterId, ChapterId i BoardEventId oraz nieaktywnego Board Event. Ustawia rozdział na `Inactive`, czyści sesyjny zestaw celów i zwiększa rewizje.

Metoda nie odczytuje ani nie zapisuje profili i celowo nie modyfikuje:
- `CompletedAdventureChapterIds`;
- sald walut;
- stosów ekwipunku;
- poziomów konta lub Great Spirit;
- `ProcessedTransactionIds`.

### AJumanjiTrialActor

Nowe metody wewnętrzne:
- `CanResetForEncounterRetry` — sprawdza identyfikator i konfigurację kroków;
- `ResetForEncounterRetry` — wyłącznie po stronie serwera resetuje stan do Dormant / krok 0 i zwiększa TrialRevision.

Odpowiedzi prób nie są ujawniane ani zmieniane przez ten mechanizm. Po retry gracz wykonuje istniejące kroki ponownie.

### AJumanjiAdventureChapterDirector

`ResetChapterForEncounterRetry`:
- akceptuje wyłącznie odpowiedni encounter w fazie Failed;
- wymaga dokładnego one-to-one mapowania celów i actorów;
- odrzuca brakujące, powielone, obce lub źle skonfigurowane aktory;
- kończy aktywny Board Event tylko wtedy, gdy jego ID odpowiada temu rozdziałowi;
- usuwa subskrypcję objective-change przed resetem;
- resetuje wszystkie skonfigurowane aktory prób;
- czyści sesyjny snapshot rozdziału w GameState.

### AJumanjiWorldEncounterDirector

- `CanRetryEncounter` udostępnia Blueprintowi informację, czy ten sam encounter ma fazę Failed;
- istniejące `StartEncounter` obsługuje pierwszy start i retry;
- `SpawnedEncounterActors` przechowuje każdego spawnowanego przeciwnika i bossa aż do terminalnego wyniku spotkania;
- retry niszczy cały poprzedni zestaw actorów encounter-owned, nawet gdy opcja `bDestroyEncounterEnemiesOnFailure` wcześniej wybrała samo odpięcie delegate'ów;
- przy Completed lista referencji jest czyszczona, a istniejące efekty wygaszania pokonanych actorów mogą zakończyć się naturalnie.

Retry nie może wejść z fazy Completed, nie może zmienić EncounterId i nie może być wywołany przez bezpośredni zapis klienta do GameState.

## Nagrody i trwała progresja

Etap nie zmienia żadnego kodu przyznawania nagród.

- Nagrody rozdziału są przyznawane przez istniejący `GrantChapterCompletionReward` i ledger profilu.
- Nagroda spotkania jest przyznawana dopiero po śmierci bossa i wejściu w Reward.
- Idempotentne TransactionId w istniejącym profilu pozostaje źródłem ochrony przed podwójnym przyznaniem.
- Retry nie kasuje receiptów, nie zmienia saldo i nie cofa odblokowań.
- Completed spotkanie jest terminalne w tym mechanizmie. Nie jest to farmowalny reset nagrody.

Jeżeli podczas wcześniejszego podejścia rozdział został już oznaczony jako Completed, ten trwały zapis pozostaje. Ponowne zaliczenie rozdziału przechodzi przez dotychczasową idempotentną ścieżkę nagród.

## Unreal Editor — konfiguracja

1. Zaktualizuj źródła i skompiluj projekt w Unreal Engine 5.8.
2. Zachowaj `BP_EmeraldJungleWorldEncounter` jako persistent/always-loaded director.
3. Zachowaj ten sam `EncounterId`, `RequiredChapterId`, chapter director oraz dziewięć przypisanych aktorów prób.
4. Upewnij się, że dziewięć aktorów prób ma unikalne ObjectiveId, poprawne trzy kroki i prawidłowe odpowiedzi.
5. W HUD encounteru pokaż przycisk ponownej próby tylko wtedy, gdy `CanRetryEncounter` zwraca true.
6. Przycisk ponownej próby ma wywoływać istniejące `RequestStartWorldEncounter` z referencją do tego samego dyrektora.
7. Nie wywołuj resetu actorów prób, GameState, profilu ani nagród bezpośrednio w Blueprint UI.
8. Sprawdź retry w przypadku awarii podczas Trials, Combat i Boss.
9. Sprawdź, że stan Completed nie pokazuje przycisku retry.

## QA / test plan

| Test | Oczekiwany wynik |
|---|---|
| Start pierwszego podejścia | Inactive → Trials |
| Awaria podczas Trials | Failed; Board Event zakończony przy retry |
| Retry z aktywnym chapterem po Failed | Chapter resetowany; próby Dormant / 0 |
| Awaria podczas Combat | wszystkie encounter-owned spawny usunięte przy retry |
| Awaria podczas Boss | boss i jego telegraph/timery znikają wraz z aktorem |
| Retry | ten sam EncounterId, nowa rewizja, faza Trials |
| Nieprawidłowe TrialActor | retry odrzucony przed nowym startem |
| Inny EncounterId | retry odrzucony |
| Completed | retry odrzucony |
| Profil | ukończone rozdziały i eksploracja pozostają zapisane |
| Ekonomia | waluty, przedmioty i receipt transakcji bez zmian przez reset |
| Duplikat nagrody | istniejący ledger nie przyznaje ponownie tej samej transakcji |
| Multiplayer | wszyscy klienci obserwują jeden nowy snapshot encounteru i nowe rewizje prób |

## Walidator i CI

Uruchom:

```bash
python Scripts/validate_emerald_jungle_encounter_recovery.py
python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_encounter_recovery.py" -v
```

Workflow:
`.github/workflows/emerald-jungle-encounter-recovery-validation.yml`

Walidacja źródeł kontroluje obecność kontraktów retry, ograniczenie resetu do Failed, wymaganie matching EncounterId, reset trial state, śledzenie wszystkich spawned actors i brak modyfikacji ledgeru profilu w metodzie resetu. Testy te nie zastępują kompilacji ani multiplayer PIE.

## Ograniczenia

- Nie ma automatycznego wznowienia w środku fali; retry zaczyna cały encounter od dziewięciu prób.
- Brak częściowego restartu pojedynczej próby.
- Nie dodano cross-server/shared-shard locka; istniejący profil jest single-server prototypem.
- Nie utworzono nowego widgetu retry .uasset — istniejący Encounter HUD należy rozszerzyć w Unreal Editor.
- Nie wykonano kompilacji UE 5.8, UHT, PIE multiplayer ani testu urządzenia Android w tej operacji.
- Nie wygenerowano APK/AAB.

## Kryteria akceptacji

- [ ] Projekt kompiluje się w Unreal Engine 5.8.
- [ ] Pierwszy start z Inactive nadal działa.
- [ ] Retry przyjmuje tylko matching EncounterId w fazie Failed.
- [ ] Retry odrzuca fazę Completed.
- [ ] Retry podczas aktywnego, nieudanego rozdziału bezpiecznie kończy Board Event.
- [ ] Dziewięć prób wraca do Dormant, krok 0, ze zwiększonym revision.
- [ ] GameState chapter/objective snapshot jest resetowany i replikowany.
- [ ] Wszystkie spawn actors — żywe, pokonane i boss — są usuwane przy ponownej próbie.
- [ ] Nowy start generuje świeży snapshot Trials i resetuje licznik fal.
- [ ] Nie zmienia się persistent profile, inventory, wallet ani transaction ledger.
- [ ] Powtórne ukończenie nie duplikuje nagród.
- [ ] Multiplayer PIE dwóch klientów potwierdza spójny retry state.

## Następny etap

**Stage 30 — Encounter Session Persistence & Reconnect Recovery**: jawny kontrakt wznowienia sesji dla rozłączenia klienta i ponownego dołączenia, bez serializacji żywych actorów do profilu i bez obejścia serwerowej autorytatywności.
