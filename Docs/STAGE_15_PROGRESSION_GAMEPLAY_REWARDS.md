# JUMANJI 2.111.1 — Stage 15: Progression Gameplay Integration & Reward Tables

## Cel etapu

Połączyć progresję zapisywaną w profilu z rzeczywistymi atrybutami GAS oraz dodać serwerowo autorytatywne nagrody i koszty ulepszeń.

Stage 15 rozszerza istniejące:
- AJumanjiPlayerState;
- UJumanjiProgressionCatalogSubsystem;
- UJumanjiProgressionRewardSubsystem;
- AJumanjiGameState;
- AJumanjiPlayerController;
- FJumanjiProfileTransaction.

Nie tworzy drugiego systemu walut, ekwipunku ani zapisu. Każda zmiana waluty i materiałów przechodzi przez istniejący ApplyProfileTransaction.

## 1. Progresja Great Spirit → GAS

### 1.1. Wybrana postać

Poziom aktualnie wybranego Great Spirit pobierany jest z FJumanjiPlayerProgressionState::CharacterProgress. Serwer wylicza wartości bazowe i ustawia je w UJumanjiAttributeSet przez UAbilitySystemComponent::SetNumericAttributeBase.

Aktualizacja następuje:
- po wyborze Great Spirit;
- po utworzeniu nowego profilu;
- po odtworzeniu profilu;
- po transakcji XP dotyczącej aktualnie wybranego Great Spirit.

### 1.2. Krzywa bazowych atrybutów

Dla poziomu postaci L, ograniczonego do zakresu 1–80:

| Atrybut | Wzór | Poziom 1 | Poziom 80 |
|---|---|---:|---:|
| MaxHealth | 100 + 12 × (L − 1) | 100 | 1048 |
| MaxEnergy | 100 + 2 × (L − 1) | 100 | 258 |
| MaxResonance | 100 | 100 | 100 |
| AttackPower | 10 + 1,5 × (L − 1) | 10 | 128,5 |
| Defense | 5 + 0,8 × (L − 1) | 5 | 68,2 |

To początkowa krzywa testowa, a nie docelowy balans. Wartości należy stroić po testach walki i czasu pokonania przeciwników.

### 1.3. Zachowanie zasobów

Gdy poziom podnosi maksymalne Health, Energy lub Resonance, system zachowuje dotychczasowy procent napełnienia zasobu. Nie przywraca automatycznie pełnego zdrowia ani nie zeruje rezonansu podczas zmiany postaci.

Modyfikatory czasowe nadal powinny pochodzić z Gameplay Effects. Krzywa poziomu wyznacza bazę, a nie zastępuje buffów, debuffów ani efektów walki.

## 2. Chapter Reward Catalog

Nowy system: UJumanjiProgressionRewardSubsystem.

Katalog nagród ma dwa źródła:
1. wbudowane, walidowane wartości prototypu;
2. opcjonalne DataTables ładowane przy inicjalizacji GameInstance.

Brak DataTable nie blokuje uruchomienia gry. W takim przypadku pozostają wartości natywne.

### 2.1. Domyślna nagroda rozdziału Emerald Jungle

ChapterId: EMERALD_JUNGLE_NINE_SEALS

| Nagroda | Ilość |
|---|---:|
| XP konta | 300 |
| XP aktywnego Great Spirit | 180 |
| Monety Odkrywców | 150 |
| Pieczęcie Planszy | 2 |
| Pamięć Ducha | 1 |

Nagroda trafia do każdego podłączonego gracza, któremu serwer zapisuje ukończenie rozdziału. XP postaci przypisywane jest do Great Spirit wybranego w chwili przyznania nagrody.

### 2.2. Idempotencja nagrody

Identyfikator transakcji powstaje deterministycznie z ChapterId i skrótu identyfikatora profilu:

CHAPTER_<ChapterId>_<ProfileHash>

W efekcie:
- powtórzenie zdarzenia ukończenia nie przyznaje nagrody drugi raz;
- ponowione żądanie może być rozpoznane przez ProcessedTransactionIds;
- nagroda i rejestr transakcji są zapisane przez istniejący profil;
- po odtworzeniu profilu brakująca nagroda ukończonego rozdziału może zostać ponowiona.

Lokalny SaveGame nadal jest prototypem jednego procesu. Produkcyjny backend musi zapisać saldo, przedmioty i klucz idempotencji w jednej transakcji bazodanowej.

## 3. Ulepszanie Great Spirit

### 3.1. Receptury startowe

Każdy Great Spirit otrzymuje własną recepturę pierwszego poziomu rozwoju:

| Great Spirit | RecipeId | Materiał | Koszt monet | Otrzymane XP |
|---|---|---|---:|---:|
| I — Korzeni | UPGRADE_ROOT_SPIRIT_01 | 2 × Żywica Korzeni | 100 | 120 |
| II — Przypływu | UPGRADE_TIDE_SPIRIT_01 | 2 × Perła Przypływu | 100 | 120 |
| III — Kamienia | UPGRADE_STONE_SPIRIT_01 | 2 × Rdzeń Kamienia | 100 | 120 |
| IV — Burzy | UPGRADE_STORM_SPIRIT_01 | 2 × Esencja Burzy | 100 | 120 |
| V — Żaru | UPGRADE_EMBER_SPIRIT_01 | 2 × Popiół Żaru | 100 | 120 |
| VI — Świtu | UPGRADE_DAWN_SPIRIT_01 | 2 × Odłamek Świtu | 100 | 120 |
| VII — Cienia | UPGRADE_SHADOW_SPIRIT_01 | 2 × Jedwab Cienia | 100 | 120 |
| VIII — Życia | UPGRADE_LIFE_SPIRIT_01 | 2 × Kwiat Życia | 100 | 120 |
| IX — Mrozu | UPGRADE_FROST_SPIRIT_01 | 2 × Kryształ Mrozu | 100 | 120 |

Receptura jest przypisana do jednego CharacterId. Materiał lub receptura innego ducha nie mogą być użyte jako zamiennik.

### 3.2. Przebieg ulepszenia

1. UI wysyła RequestGreatSpiritUpgrade(CharacterId, RecipeId).
2. PlayerController przekazuje żądanie przez Server RPC.
3. Serwer sprawdza CharacterId w rosterze.
4. Serwer odczytuje recepturę z katalogu.
5. Serwer sprawdza przypisanie receptury do wskazanego Great Spirit.
6. Serwer odrzuca ulepszenie postaci na maksymalnym poziomie.
7. Serwer buduje transakcję z ujemnymi deltas dla kosztów i dodatnim CharacterExperienceDelta.
8. ApplyProfileTransaction waliduje wszystkie zmiany na kopii stanu.
9. Jeżeli choć jeden koszt jest niedostępny, nie zmienia się ani waluta, ani ekwipunek, ani XP.
10. Po udanym zatwierdzeniu system aktualizuje atrybuty GAS, jeżeli ulepszony Great Spirit jest aktywny.

Klient nie przesyła salda, ilości materiałów, XP ani identyfikatora transakcji. Idempotency ID generuje serwer na podstawie profilu, receptury, postaci i revision progresji.

## 4. DataTable — konfiguracja w Unreal Editor

### 4.1. Chapter Rewards

Utwórz DataTable:
- folder: /Game/Progression/Data;
- nazwa: DT_ChapterRewards;
- row structure: FJumanjiChapterRewardDefinition;
- oczekiwana ścieżka: /Game/Progression/Data/DT_ChapterRewards.DT_ChapterRewards.

Kolumny:
- ChapterId;
- AccountExperienceReward;
- ActiveCharacterExperienceReward;
- CurrencyRewards;
- ItemRewards.

Wartości walut i materiałów muszą być dodatnie. W jednej tabeli nie umieszczaj dwa razy tego samego CurrencyId lub ItemId w pojedynczym wierszu. Nieznane identyfikatory zostaną odrzucone przez walidację transakcji przy przyznawaniu.

### 4.2. Spirit Upgrade Recipes

Utwórz DataTable:
- folder: /Game/Progression/Data;
- nazwa: DT_SpiritUpgradeRecipes;
- row structure: FJumanjiSpiritUpgradeRecipe;
- oczekiwana ścieżka: /Game/Progression/Data/DT_SpiritUpgradeRecipes.DT_SpiritUpgradeRecipes.

Kolumny:
- RecipeId;
- CharacterId;
- CharacterExperienceReward;
- CurrencyCosts;
- ItemCosts.

Każda receptura musi mieć:
- istniejący RecipeId;
- istniejący CharacterId;
- dodatni CharacterExperienceReward;
- co najmniej jeden koszt;
- unikalne ID walut i przedmiotów;
- dodatnie ilości w dopuszczalnych limitach.

Poprawny wiersz z DataTable nadpisuje wbudowany wiersz o tym samym ID. Brak tabeli pozostawia katalog natywny.

## 5. API runtime

### UJumanjiProgressionCatalogSubsystem
- GetCombatProgressionValues(CharacterLevel, OutValues);
- istniejące definicje walut, materiałów i progów XP.

### UJumanjiProgressionRewardSubsystem
- GetChapterRewardDefinition(ChapterId, OutDefinition);
- GetSpiritUpgradeRecipe(RecipeId, OutRecipe);
- GrantChapterCompletionReward(PlayerState, ChapterId);
- GrantMissingCompletedChapterRewards(PlayerState);
- TryUpgradeGreatSpirit(PlayerState, CharacterId, RecipeId).

### AJumanjiPlayerState
- RefreshCombatAttributesFromProgression();
- HasProcessedProfileTransaction(TransactionId);
- istniejące ApplyProfileTransaction(Transaction).

### AJumanjiPlayerController
- RequestGreatSpiritUpgrade(CharacterId, RecipeId);
- ServerRequestGreatSpiritUpgrade(CharacterId, RecipeId).

UI ma tylko zgłaszać prośbę i obserwować OnProgressionChanged. Nie może bezpośrednio modyfikować atrybutów, waluty ani ilości materiałów.

## 6. Kryteria akceptacji i testy

### Atrybuty GAS
- [ ] Nowy profil i Great Spirit na poziomie 1 mają wartości bazowe 100 HP, 100 Energy, 100 Resonance, 10 AttackPower i 5 Defense.
- [ ] Zdobycie XP i awans aktywnego Great Spirit aktualizuje jego atrybuty.
- [ ] Awans nieaktywnego Great Spirit nie zmienia atrybutów aktywnej postaci.
- [ ] Zmiana Great Spirit przywraca jego własną bazę poziomu.
- [ ] Zmiana MaxHealth/MaxEnergy zachowuje procent napełnienia zasobu.
- [ ] Tymczasowe Gameplay Effects nadal modyfikują wartości zgodnie z regułami GAS.

### Chapter rewards
- [ ] Ukończenie EMERALD_JUNGLE_NINE_SEALS przyznaje wartości z katalogu.
- [ ] Nagroda zwiększa XP konta i XP aktywnego Great Spirit.
- [ ] Nagroda jest przyznana tylko raz dla jednego profilu i rozdziału.
- [ ] Ponowione zdarzenie nie przyznaje drugi raz walut ani materiałów.
- [ ] Profil odtworzony z completed chapter bez receipt może ponowić próbę nagrody.
- [ ] Nieznany rozdział bez wiersza reward nie przyznaje pustej transakcji.

### Upgrade recipes
- [ ] Great Spirit I przyjmuje recepturę I i właściwy materiał.
- [ ] Great Spirit I odrzuca recepturę innego ducha.
- [ ] Nieznane RecipeId jest odrzucane.
- [ ] Brak monet lub materiału powoduje odrzucenie całej transakcji.
- [ ] Nieudane ulepszenie nie zużywa żadnego zasobu.
- [ ] Udane ulepszenie zużywa koszty dokładnie raz.
- [ ] Ponowienie identycznego żądania nie wykonuje drugi raz tej samej transakcji, nawet jeśli pierwsza próba już zwiększyła revision progresji.
- [ ] Ulepszenie na maksymalnym poziomie jest odrzucane.
- [ ] XP i poziom zostają zapisane i odtworzone po ponownym uruchomieniu.

### DataTables i multiplayer
- [ ] Brak DT_ChapterRewards nie blokuje inicjalizacji subsystemu.
- [ ] Poprawny wiersz DataTable nadpisuje native default.
- [ ] Niepoprawny wiersz jest ignorowany.
- [ ] Dwa profile graczy otrzymują osobne nagrody i mają oddzielne saldo.
- [ ] Klient nie może sam wywołać transakcji poza serwerową ścieżką RPC.
- [ ] Test wykonać na dwóch klientach w sesji PIE lub na lokalnym dedykowanym serwerze.

## 7. Zakres nieobjęty Stage 15

- produkcyjny backend kont i ekonomii;
- transakcyjna baza danych współdzielona między shardami;
- automatyczne mapowanie każdego zapisanego AbilityId na zatwierdzoną klasę Gameplay Ability;
- aktywowanie zapisanego loadoutu z profilu w ASC;
- zaawansowane drzewka talentów, statystyki dodatkowe i koszty kolejnych poziomów;
- nagrody indywidualne za każdy trial;
- ekran ulepszeń i finalny UMG sklep/progresja;
- APK/AAB i test urządzenia Android.

W Stage 15 statystyki poziomu są już integrowane z GAS, a nagrody i receptury mają działającą serwerową ścieżkę transakcyjną. Resolver AbilityId → UGameplayAbility pozostaje osobnym etapem.

## 8. Weryfikacja

Pliki źródłowe zostały zapisane w gałęzi funkcjonalnej i odczytane ponownie przez GitHub API. Nie wykonano kompilacji Unreal Engine 5.8 ani testu PIE w tym środowisku. Przed scaleniem należy wykonać lokalny build i wszystkie testy z tej listy.

