# JUMANJI 2.111.1 — Stage 20: Ability Presentation Runtime Integration

## Cel etapu

Połączyć manifest prezentacyjny Stage 19 z działającym runtime HUD. Pięć slotów dla każdej z dziewięciu postaci otrzymuje stabilne identyfikatory, metadane prezentacyjne i miękkie referencje do ikon, materiału cooldownu i dźwięków.

Etap 20 nie zmienia wykonywania zdolności, kosztów, cooldownów, celów ani efektów. GAS pozostaje jedynym źródłem prawdy dla aktywacji i stanu rozgrywki.

## Architektura

### UJumanjiAbilityPresentationSubsystem

Nowy GameInstanceSubsystem odczytuje skonfigurowaną DataTable i udostępnia:

- wyszukiwanie danych przez stabilny AbilityId;
- nazwę zdolności i nazwę postaci;
- miękką referencję ikony Texture2D;
- kolory AccentColor i SecondaryColor;
- miękką referencję materiału radialnego cooldownu;
- cztery miękkie referencje dźwięków UI;
- tag Gameplay Cue;
- referencyjny czas cooldownu z manifestu.

DataTable jest ładowana synchronicznie jako mały zestaw metadanych. Obiekty graficzne i audio pozostają soft references. Sam subsystem nie ładuje 45 ikon, nie odtwarza dźwięków i nie uruchamia Gameplay Cue.

Subsystem sprawdza zgodność każdego wiersza z UJumanjiAbilityCatalogSubsystem. CharacterId i AbilityId muszą odpowiadać katalogowi Stage 16. Nazwy slotów manifestu (PrimaryAttack, Active, Passive, Ultimate, Dodge) są jawnie mapowane na identyfikatory katalogu (SLOT_PRIMARY, SLOT_ACTIVE, SLOT_PASSIVE, SLOT_ULTIMATE, SLOT_DODGE). Nieprawidłowe i powtórzone wiersze są pomijane.

Jeśli DataTable nie jest skonfigurowana albo wiersz nie istnieje, HUD zachowuje metadane z klasy Gameplay Ability. Brak zasobów prezentacyjnych nie blokuje tworzenia HUD.

### FJumanjiAbilityPresentationRow

Nowa struktura DataTable odpowiada kolumnom manifestu Stage 19. CSV importowy ma dodatkową pierwszą kolumnę Name używaną jako nazwa wiersza DataTable. Właściwy AbilityId pozostaje osobną kolumną i musi być zgodny z katalogiem Stage 16.

Pliki:

- Content/UI/AbilityPresentation/AbilityPresentationManifest.csv — główny manifest produkcyjny;
- Content/UI/AbilityPresentation/AbilityPresentationDataTable.csv — importowy wariant DataTable z kolumną Name;
- Content/UI/AbilityPresentation/README.md — procedura importu i konfiguracji.

### HUDAbilityId w UJumanjiGameplayAbility

Każda klasa GA_GS_[ROMAN]_[SLOT] powinna mieć ustawione HUDAbilityId zgodne z jej wpisem w katalogu Stage 16 i odpowiednim wierszem manifestu Stage 19.

Jeżeli HUDAbilityId klasy jest ustawione, UJumanjiAbilityCatalogSubsystem odrzuca wiersz katalogu, którego AbilityId nie pasuje do identyfikatora klasy. Starsze klasy z pustym HUDAbilityId zachowują dotychczasową kompatybilność, ale nie otrzymają nadpisania z manifestu.

### FJumanjiAbilityHUDSlotState

Pięć stanów slotów przekazywanych do WBP_AbilityHUD zawiera teraz dodatkowo:

- AbilityId;
- AccentColor i SecondaryColor;
- CooldownMaterial;
- ActivatedSound, InsufficientResourceSound, CooldownSound, UnavailableSound;
- PresentationCueTag;
- CooldownSecondsReference.

Ikona z manifestu ma pierwszeństwo przed HUDIcon klasy, jeśli ścieżka wiersza jest skonfigurowana. Przy braku ścieżki używany jest HUDIcon z Gameplay Ability.

Wszystkie referencje do PNG, materiałów i Sound Wave są miękkie. WBP_AbilityHUD powinien używać Async Load Asset i własnego placeholdera, a nie wymuszać synchronicznego ładowania wszystkich zasobów przy otwieraniu interfejsu.

## Import DataTable w Unreal Engine 5.8

1. Pobierz gałąź Stage 20 i skompiluj projekt w Unreal Engine 5.8.
2. W Content Browser utwórz katalog /Game/UI/AbilityPresentation.
3. Kliknij Import i wybierz Content/UI/AbilityPresentation/AbilityPresentationDataTable.csv.
4. Wskaż typ wiersza FJumanjiAbilityPresentationRow.
5. Upewnij się, że pierwsza kolumna Name jest używana jako RowName, a AbilityId jest importowane jako osobne pole struktury.
6. Zapisz DataTable jako DT_AbilityPresentation w /Game/UI/AbilityPresentation.
7. W Config/DefaultGame.ini ustaw ścieżkę:

   `[/Script/JumanjiUE.JumanjiAbilityPresentationSubsystem]`

   `ConfiguredPresentationDataTable=/Game/UI/AbilityPresentation/DT_AbilityPresentation.DT_AbilityPresentation`

8. Otwórz klasy zdolności GA_GS_[ROMAN]_[SLOT] i ustaw HUDAbilityId identyczny z AbilityId manifestu.
9. Utwórz i zaimportuj ikony, materiał i dźwięki do lokalizacji przewidzianych przez manifest. Do czasu utworzenia plików zostaw soft references bez wymuszania ładowania.
10. Otwórz WBP_AbilityHUD i odśwież widok z OnAbilityHUDStateChanged.

## Kontrakt radialnego cooldownu

Materiał UI powinien przyjmować parametry opisane w Stage 19:

- IconTexture;
- AccentColor;
- CooldownPercent;
- DisabledAmount;
- PulseAmount.

CooldownPercent pochodzi bezpośrednio z FJumanjiAbilityHUDSlotState.Stage 18: 1 oznacza pełny pozostały cooldown, 0 oznacza gotowość. Nie twórz drugiego zegara w materiale, widgetcie ani subsystemie.

CooldownSecondsReference z manifestu służy wyłącznie jako wartość referencyjna dla projektanta. Nie może nadpisywać rzeczywistego Gameplay Effect cooldownu.

## Audio i Gameplay Cue

UJumanjiAbilityPresentationSubsystem udostępnia cztery soft references dźwięków w stanie slotu. WBP_AbilityHUD może odtworzyć właściwy krótki dźwięk przy pojedynczej zmianie stanu.

Nie odtwarzaj dźwięków przy każdym odświeżeniu HUD ani podczas samego odczytywania GetAbilitySlotStates. Nie uruchamiaj dźwięku co klatkę w trakcie cooldownu.

Tagi z manifestu są zarejestrowane w Config/DefaultGameplayTags.ini:

- GameplayCue.UI.Ability.Primary
- GameplayCue.UI.Ability.Active
- GameplayCue.UI.Ability.Passive
- GameplayCue.UI.Ability.Ultimate
- GameplayCue.UI.Ability.Dodge

Subsystem udostępnia tag, ale nie odtwarza Gameplay Cue automatycznie. Rzeczywisty Cue asset i jego logika muszą zostać utworzone i przypisane w Unreal Editor.

## Zasady autorytetu i wydajności

- GAS pozostaje źródłem prawdy dla kosztów, cooldownów, aktywacji i efektów.
- Manifest nie może przyznawać zdolności ani modyfikować profilu gracza.
- DataTable przechowuje tylko metadane prezentacyjne.
- Soft references nie są ładowane synchronicznie przez HUD.
- Brak ikony lub dźwięku nie może blokować rozgrywki.
- Passive pozostaje informacyjny i nie otrzymuje ręcznego przycisku aktywacji.
- Kolor nie może być jedynym sposobem rozróżnienia stanu.
- Sound i VFX są odtwarzane przy zdarzeniu/zmianie stanu, nie przy odczycie danych.

## Kryteria akceptacji

- [ ] DataTable używa FJumanjiAbilityPresentationRow.
- [ ] CSV importowy ma kolumnę Name i 45 unikalnych wierszy.
- [ ] Wszystkie 45 AbilityId występują w katalogu Stage 16.
- [ ] Każdy wpis ma prawidłową parę CharacterId/SlotId.
- [ ] Każda klasa Blueprint zdolności ma prawidłowy HUDAbilityId.
- [ ] Katalog odrzuca klasę z HUDAbilityId innym niż AbilityId wpisu.
- [ ] HUD dostaje ikonę, paletę i metadane z subsystemu.
- [ ] Brak DataTable lub assetów daje bezpieczny fallback do metadanych klasy.
- [ ] Brak ikony nie blokuje otwarcia HUD.
- [ ] Wartości cooldownu i kosztu nadal pochodzą z GAS.
- [ ] CooldownPercent wykorzystuje Stage 18 i nie tworzy drugiego zegara.
- [ ] Dźwięki są odtwarzane tylko po zdarzeniu/zmianie stanu.
- [ ] Passive nie ma ręcznego przycisku.
- [ ] Projekt kompiluje się w Unreal Engine 5.8.
- [ ] WBP_AbilityHUD przechodzi test PIE i test na urządzeniu Android.

## Nieujęte w tym etapie

- 45 binarnych ikon PNG;
- master material i material instance .uasset;
- cztery pliki Sound Wave;
- właściwe Gameplay Cue assets i Niagara;
- gotowy WBP_AbilityHUD .uasset;
- testy PIE, test Android, profilowanie pamięci i pakiet APK/AAB.

Te zasoby wymagają importu i konfiguracji w Unreal Editor. Etap 20 dostarcza kod runtime oraz kontrakt DataTable, nie udaje, że pliki binarne już istnieją.

## Następny etap

Stage 21 — Ability Presentation Blueprint Integration: utworzyć WBP_AbilityHUD i WBP_AbilitySlot na natywnych klasach, dodać asynchroniczne ładowanie ikon/materiałów, fallback cooldownu i feedback per slot. Zobacz Docs/STAGE_21_ABILITY_PRESENTATION_BLUEPRINT_INTEGRATION.md.
