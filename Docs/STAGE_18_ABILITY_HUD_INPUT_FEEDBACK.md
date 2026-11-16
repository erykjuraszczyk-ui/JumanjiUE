# JUMANJI 2.111.1 — Stage 18: Ability HUD & Input Feedback

## Cel etapu

Dodać natywną bazę mobilnego HUD dla pięciu slotów Great Spirit oraz wspólny przepływ informacji zwrotnej dla wejścia dotykowego i sprzętowego. HUD prezentuje:
- Primary, Active, Passive, Ultimate i Dodge;
- nazwę, opis, ikonę oraz metadane zasobu przypisane do klasy zdolności;
- Energy i Resonance wraz z wartościami maksymalnymi i procentem paska;
- pozostały czas cooldownu, całkowity czas oraz wartość do radialnego wskaźnika odnowienia;
- informację o braku zasobu, aktywnym cooldownie lub niedostępnej zdolności;
- lokalny znacznik fokusu wybranego aktora posiadającego Ability System Component.

To jest warstwa UI i wejścia. Nie przenosi do widgetu kosztu, cooldownu, targetowania ani autorytetu aplikowania efektów. GAS pozostaje źródłem prawdy.

## 1. Nowy widget runtime

Pliki:
- Source/JumanjiUE/Public/JumanjiAbilityHUDWidget.h
- Source/JumanjiUE/Private/JumanjiAbilityHUDWidget.cpp

UJumanjiAbilityHUDWidget jest abstrakcyjną klasą bazową UUserWidget. Twórca projektu powinien utworzyć WBP_AbilityHUD jako Widget Blueprint pochodny.

### Dane dla Blueprint

GetAbilitySlotStates() zwraca zawsze pięć wpisów w stałej kolejności:
1. PrimaryAttack
2. Active
3. Passive
4. Ultimate
5. Dodge

Każdy FJumanjiAbilityHUDSlotState zawiera:
- Slot, DisplayName i Description;
- InputTag i soft reference do Icon;
- ResourceType, RequiredResource i CurrentResource;
- CooldownRemaining, CooldownDuration i CooldownPercent;
- bIsGranted, bIsPassive, bIsOnCooldown, bHasRequiredResource i bCanActivate.

Brak przyznanej zdolności daje pusty/nieprzyznany slot, a nie fałszywą możliwość aktywacji.

GetResourceState() zwraca Energy, MaxEnergy, EnergyPercent, Resonance, MaxResonance i ResonancePercent. Wartości są pobierane bezpośrednio z atrybutów ASC.

### Odświeżanie

Widget:
- binduje się do delegate zmian atrybutów Energy i Resonance;
- próbuje ponownie znaleźć PlayerState/ASC, jeśli nie są gotowe w chwili NativeConstruct;
- odświeża czas cooldownu w krótkim interwale przez NativeTick;
- usuwa delegate z ASC w NativeDestruct.

Blueprint implementuje OnAbilityHUDStateChanged i odrysowuje paski zasobów, sloty, nakładkę cooldownu i stany przycisków.

## 2. Metadane HUD na klasie zdolności

UJumanjiGameplayAbility ma teraz edytowalne pola prezentacji:
- HUDDisplayName;
- HUDDescription;
- HUDIcon;
- HUDResourceType: None, Energy lub Resonance;
- HUDResourceCost.

Te pola opisują prezentację, nie wykonują kosztu. HUDResourceType i HUDResourceCost muszą odzwierciedlać rzeczywiście skonfigurowany Gameplay Effect. Nie wpisuj tutaj wartości odmiennej od GE.

Dla slotów bez kosztu zasobu ustaw ResourceType = None i HUDResourceCost = 0. Dla Active i Ultimate ustaw typ oraz wartość zgodną z odpowiednim GE w konkretnym Blueprint. Nie zakładaj, że wszystkie zdolności w danym slocie mają identyczny koszt.

Ikona jest soft reference. WBP_AbilityHUD może ładować ją asynchronicznie i pokazać placeholder, jeśli asset nie został przypisany.

## 3. Wejście dotykowe

PlayerController tworzy AbilityHUDWidgetClass lokalnego gracza w BeginPlay. W BP_JumanjiPlayerController ustaw:
- AbilityHUDWidgetClass = WBP_AbilityHUD;
- AdventureHUDWidgetClass pozostaw oddzielnym widgetem rozdziału/eksploracji.

WBP_AbilityHUD:
- przycisk Primary wywołuje ActivateAbilitySlot(PrimaryAttack);
- przycisk Active wywołuje ActivateAbilitySlot(Active);
- przycisk Ultimate wywołuje ActivateAbilitySlot(Ultimate);
- przycisk Dodge wywołuje ActivateAbilitySlot(Dodge);
- Passive jest wskaźnikiem stanu, nie przyciskiem ręcznej aktywacji.

ActivateAbilitySlot odczytuje przyznaną zdolność i jej InputTag, a następnie korzysta z istniejącego AJumanjiCharacter::ActivateAbilityByInputTag. Dzięki temu UI dotykowe używa tego samego wejścia co istniejące akcje sprzętowe.

Gdy GAS odrzuci próbę, widget otrzymuje OnAbilityInputFeedback:
- Activated;
- InsufficientResource;
- Cooldown;
- Unavailable.

Komunikat jest wskazówką UI. Samo wyświetlenie komunikatu nie zmienia atrybutów ani statusu zdolności. Koszt i cooldown są nadal weryfikowane przez GAS.

## 4. Wskaźnik cooldownu radialnego

Blueprint używa:
- CooldownRemaining;
- CooldownDuration;
- CooldownPercent.

CooldownPercent jest ograniczony do zakresu 0–1 i oznacza część czasu, która pozostała. Dla radialnego wskaźnika ustaw właściwy kierunek animacji zgodny z projektem UI. Jeżeli zdolność nie ma aktywnego cooldownu, wartości czasu wynoszą 0.

Dane cooldownu pochodzą z funkcji Gameplay Ability GetCooldownTimeRemainingAndDuration dla przyznanego speca. Nie używaj ręcznego odliczania w widgetach jako źródła prawdy.

## 5. Lokalny fokus celu

SetTargetFocus(AActor*) przyjmuje lokalny aktor UI tylko wtedy, gdy:
- aktor jest poprawny i nie jest własną postacią;
- znajduje się w MaxTargetFocusDistance;
- implementuje IAbilitySystemInterface i posiada ASC.

OnAbilityTargetFocusChanged przekazuje zmianę do Blueprint. Można wykorzystać to do nazwy celu, ramki HUD lub prostego wskaźnika.

Fokus jest wyłącznie wskazówką prezentacyjną klienta. Nie jest serwerowym target data, nie omija walidacji z Stage 17 i nie może być używany jako dowód, że konkretny aktor powinien otrzymać efekt. Każda zdolność nadal realizuje własną walidację w GAS.

Blueprint może podać aktora wybranego przez własny trace/target selector. Ten etap nie dodaje automatycznego aim-assist ani globalnego wyboru aktorów.

## 6. Unreal Editor — konfiguracja

1. Skompiluj moduł C++ w Unreal Engine 5.8.
2. Otwórz klasy Blueprint dziedziczące po UJumanjiConfiguredEffectAbility.
3. Ustaw HUDDisplayName i HUDDescription dla każdej aktywnej zdolności.
4. Przypisz HUDIcon lub przygotuj placeholder.
5. Ustaw HUDResourceType i HUDResourceCost dokładnie zgodnie z kosztem Gameplay Effect; dla zdolności bez kosztu użyj None/0.
6. Utwórz WBP_AbilityHUD jako Widget Blueprint dziecko UJumanjiAbilityHUDWidget.
7. Dodaj pięć kart/slotów, dwa paski zasobów, feedback text i lokalny panel fokusu celu.
8. Zwiąż widok z OnAbilityHUDStateChanged, OnAbilityInputFeedback i OnAbilityTargetFocusChanged.
9. Do czterech przycisków ręcznych podłącz ActivateAbilitySlot z odpowiednim enumem. Nie przypinaj przycisku do Passive.
10. W BP_JumanjiPlayerController ustaw AbilityHUDWidgetClass na WBP_AbilityHUD.
11. Przetestuj bez ikon, aby sprawdzić fallback, następnie przypisz ikony i sprawdź ich ładowanie.
12. Użyj ekranów o różnych proporcjach, aby upewnić się, że przyciski nie zasłaniają ruchu i widoku gry.

## 7. Reguły UX mobilnego

- Primary ma być łatwo dostępny, ale nie może zasłaniać sterowania ruchem.
- Active, Ultimate i Dodge powinny mieć wyraźne, rozróżnialne stany.
- Passive pokazuje opis/stan, ale nie wygląda jak przycisk do naciśnięcia.
- Brak zasobu ma być czytelny bez migania całego HUD.
- Cooldown radial powinien pokazywać czas pozostały, nie tylko blokować przycisk.
- Podczas aktywacji pokazuj krótki feedback, nie długie komunikaty.
- Fokus celu ma mieć wyraźny stan wyczyszczenia i nie może zasłaniać głównego pola widzenia.
- Wszystkie stany powinny mieć tekst/ikonę/kształt, nie tylko kolor.

## 8. Kryteria akceptacji

- [ ] Projekt kompiluje się w Unreal Engine 5.8 bez błędów UHT/C++.
- [ ] PlayerController tworzy WBP_AbilityHUD tylko dla lokalnego gracza.
- [ ] HUD zawsze zwraca pięć slotów w ustalonej kolejności.
- [ ] Brak przyznanej zdolności nie pokazuje aktywnego przycisku.
- [ ] Passive nie może być ręcznie aktywowany z HUD.
- [ ] Energy i Resonance aktualizują paski po zmianie wartości atrybutów.
- [ ] Procenty zasobów są bezpieczne dla MaxValue = 0.
- [ ] CooldownRemaining i CooldownDuration odzwierciedlają aktywny cooldown GAS.
- [ ] CooldownPercent mieści się w zakresie 0–1.
- [ ] HUD odróżnia brak zasobu, cooldown i ogólną niedostępność.
- [ ] Aktywacja przez dotyk używa InputTag tego samego slotu co wejście sprzętowe.
- [ ] UI nie zmienia Energy/Resonance i nie nakłada Gameplay Effects.
- [ ] Po usunięciu widgetu delegaty atrybutów są odpinane.
- [ ] Fokus celu odrzuca aktora bez ASC, własną postać i aktora poza limitem odległości.
- [ ] Fokus celu pozostaje lokalną informacją UI i nie zastępuje walidacji serwera.
- [ ] Test na telefonie potwierdza czytelność, wygodę dotyku i brak konfliktu z nawigacją.

## 9. Nie obejmuje

- Gotowych Widget Blueprint .uasset, ikon, animacji UI, materiałów i dźwięków.
- Automatycznego wyboru celu/aim-assist; Blueprint dostarcza aktora do SetTargetFocus.
- Przenoszenia logiki kosztów/cooldownów do UI.
- Nowych zasad atakowania, obrażeń lub targetowania gameplay.
- Kompilacji UE 5.8, testu PIE, testu dwóch klientów i APK/AAB w tym środowisku.

## Weryfikacja

Po zapisaniu zmian wykonaj statyczne kontrole źródeł i porównaj gałąź z main. Kompilacja UHT/C++ oraz test dotykowy wymagają lokalnej instalacji Unreal Engine 5.8 i właściwych assetów.

## Następny etap

Stage 19 — Ability Presentation Asset Pack: spójny zestaw ikon, UI materials, animowanych cooldown overlays, feedback audio/VFX cues i konfiguracja dziewięciu zestawów Great Spirit w Unreal Editor.
