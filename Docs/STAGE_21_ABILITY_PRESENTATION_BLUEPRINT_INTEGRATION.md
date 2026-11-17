# JUMANJI 2.111.1 — Stage 21: Ability Presentation Blueprint Integration

## Cel

Przekształcić dane Stage 20 w konfigurowalny, mobilny HUD zdolności. Etap dodaje natywny, wielokrotnego użytku widget pojedynczego slotu oraz wiąże pięć slotów z nadrzędnym UJumanjiAbilityHUDWidget.

Nie powstają binarne pliki .uasset. Kod udostępnia stabilny kontrakt dla Widget Blueprintów, które trzeba utworzyć i zapisać w Unreal Editor 5.8.

## Nowe klasy

### UJumanjiAbilityHUDSlotWidget

Pliki:
- Source/JumanjiUE/Public/JumanjiAbilityHUDSlotWidget.h
- Source/JumanjiUE/Private/JumanjiAbilityHUDSlotWidget.cpp

Klasa bazowa dla WBP_AbilitySlot. Odpowiada za:
- wyświetlanie nazwy zdolności i kosztu zasobu;
- wyświetlanie pozostałego czasu cooldownu;
- stan niedostępny, pasywny, cooldown i brak zasobu;
- asynchroniczne ładowanie ikony Texture2D;
- asynchroniczne ładowanie materiału radialnego i utworzenie dynamicznej instancji;
- przekazywanie CooldownPercent, AccentColor i DisabledAmount do materiału;
- pasek cooldownu jako fallback, gdy materiał nie został przypisany;
- odtwarzanie dźwięku i animacji tylko po jawnie wywołanym PlayFeedback;
- emisję OnSlotPressed po dotknięciu/przyciśnięciu przycisku.

Wszystkie asynchroniczne callbacki sprawdzają ważność widgetu i aktualną ścieżkę żądanego assetu. Zmiana ikony, materiału lub dźwięku nie może spowodować użycia nieaktualnego callbacku.

### Pięć punktów BindWidgetOptional w UJumanjiAbilityHUDWidget

Rodzic udostępnia następujące opcjonalne pola:
- PrimarySlotWidget;
- ActiveSlotWidget;
- PassiveSlotWidget;
- UltimateSlotWidget;
- DodgeSlotWidget.

Każde pole powinno wskazywać osobną instancję WBP_AbilitySlot w hierarchii WBP_AbilityHUD. Jeżeli slot nie został umieszczony w hierarchii, pozostałe sloty i zdolności nadal działają.

Rodzic:
- subskrybuje OnSlotPressed każdego slotu;
- kieruje dotyk do ActivateAbilitySlot;
- rozsyła snapshot stanu do odpowiedniego slotu;
- odpina wszystkie delegaty w NativeDestruct;
- emituje OnAbilitySlotFeedback z konkretnym slotem, typem feedbacku i tekstem;
- odtwarza odpowiedni dźwięk w slocie, jeżeli asset został skonfigurowany.

GAS pozostaje jedynym źródłem prawdy dla aktywacji, kosztu i cooldownu.

## Struktura WBP_AbilitySlot

Utwórz Widget Blueprint dziedziczący po UJumanjiAbilityHUDSlotWidget.

Nazwy elementów muszą odpowiadać polom BindWidgetOptional:

| Nazwa w Hierarchy | Typ | Przeznaczenie |
|---|---|---|
| SlotButton | Button | Dotykowy przycisk slotu |
| AbilityIcon | Image | Ikona zdolności lub placeholder |
| CooldownOverlay | Image | Materiał radialnego cooldownu |
| CooldownFallbackBar | Progress Bar | Awaryjny cooldown bez materiału |
| AbilityNameText | Text Block | Nazwa zdolności |
| CooldownText | Text Block | Pozostałe sekundy |
| ResourceCostText | Text Block | Koszt Energy/Resonance |
| SlotStateText | Text Block | Pasywna, niedostępna, brak zasobu itd. |
| ActivationPulseAnimation | Widget Animation | Krótki sygnał udanej aktywacji |

Pola są opcjonalne po stronie C++; brak któregoś nie powinien zatrzymać gry. Dla pełnego interfejsu należy jednak dodać wszystkie elementy.

### Hierarchia zalecana

- SlotRoot (Size Box)
  - SlotButton
    - SlotFrame (Border/Overlay)
      - AbilityIcon (Image)
      - CooldownOverlay (Image)
      - CooldownFallbackBar (Progress Bar)
      - AbilityNameText (Text Block)
      - CooldownText (Text Block)
      - ResourceCostText (Text Block)
      - SlotStateText (Text Block)

Ustaw domyślną teksturę AbilityIcon jako neutralny placeholder. Asynchroniczny loader zastąpi ją ikoną z manifestu, gdy asset będzie dostępny.

### Materiał radialny

Master Material Stage 19 musi być w domenie User Interface i obsługiwać:
- CooldownPercent (Scalar 0–1; 1 = pełny pozostały cooldown);
- AccentColor (Vector);
- DisabledAmount (Scalar 0–1).

Widget tworzy UMaterialInstanceDynamic i aktualizuje parametry. Jeżeli materiał nie jest załadowany, używa CooldownFallbackBar. Nie twórz dodatkowego timera.

## Struktura WBP_AbilityHUD

Utwórz Widget Blueprint dziedziczący po UJumanjiAbilityHUDWidget.

W Hierarchy dodaj pięć instancji WBP_AbilitySlot i nazwij je dokładnie:
- PrimarySlotWidget;
- ActiveSlotWidget;
- PassiveSlotWidget;
- UltimateSlotWidget;
- DodgeSlotWidget.

Dodatkowe elementy nadrzędnego HUD:
- EnergyBar + EnergyValueText;
- ResonanceBar + ResonanceValueText;
- FeedbackText;
- opcjonalny panel lokalnego fokusu celu.

W zdarzeniu OnAbilityHUDStateChanged:
1. Odczytaj pięć slotów i dopasuj je według enumu Slot.
2. Zaktualizuj EnergyBar z EnergyPercent.
3. Zaktualizuj ResonanceBar z ResonancePercent.
4. Aktualizuj liczby tylko wtedy, gdy wartość się zmieniła.
5. Nie twórz własnego odliczania cooldownu; slot otrzymuje aktualny snapshot z C++.

OnAbilityInputFeedback wyświetla krótki komunikat. OnAbilityTargetFocusChanged aktualizuje opcjonalny lokalny marker celu.

## Layout mobilny — propozycja

Projektuj dla landscape i skalowania DPI:
- Primary: największy i najłatwiej dostępny przycisk w prawym dolnym obszarze.
- Active i Dodge: mniejsze przyciski blisko Primary, ale poza obszarem sterowania ruchem.
- Ultimate: osobny, wyraźny przycisk w górnym rzędzie grupy.
- Passive: karta informacyjna, bez przycisku i bez affordance dotykowego.
- Energy/Resonance: krótkie paski w pobliżu grupy zdolności, bez zasłaniania celownika/centrum ekranu.

Punkty startowe do testów na telefonie:
- Primary: 88–104 dp;
- Active, Ultimate, Dodge: 64–80 dp;
- odstępy: minimum 8 dp;
- tekst stanu: czytelny na ekranie 6–7 cali;
- safe area: uwzględnij wycięcia, zaokrąglenia i systemowe gesty Android.

To są wartości startowe UX, a nie wymuszone rozmiary C++.

## Konfiguracja w Unreal Editor 5.8

1. Pobierz branch Stage 21.
2. Wygeneruj pliki projektu i skompiluj moduł JumanjiUE.
3. W Content Browser utwórz /Game/UI/AbilityHUD/Slots.
4. Utwórz WBP_AbilitySlot jako dziecko UJumanjiAbilityHUDSlotWidget.
5. Dodaj elementy Hierarchy o nazwach z tabeli i przypisz placeholder do AbilityIcon.
6. Utwórz /Game/UI/AbilityHUD/WBP_AbilityHUD jako dziecko UJumanjiAbilityHUDWidget.
7. Umieść pięć instancji WBP_AbilitySlot i nazwij je dokładnie jak pola BindWidgetOptional.
8. Dodaj EnergyBar, ResonanceBar i FeedbackText.
9. Zapisz widgety i otwórz BP_JumanjiPlayerController.
10. Ustaw AbilityHUDWidgetClass = WBP_AbilityHUD.
11. Upewnij się, że Stage 20 DataTable jest zaimportowana i przypisana w DefaultGame.ini.
12. Ustaw HUDAbilityId dla klas zdolności zgodnie z katalogiem.
13. Dodaj prawdziwe ikony, MI_UI_AbilityCooldown_Radial i Sound Wave zgodnie z manifestem.
14. Przetestuj PIE z brakującymi assetami, następnie z pełnym zestawem zasobów.
15. Przetestuj dotyk na urządzeniu Android w docelowej orientacji.

## Audio feedback

WBP_AbilityHUD może nadal implementować OnAbilityInputFeedback dla globalnego tekstu/komunikatu. Dodatkowy OnAbilitySlotFeedback dostarcza konkretny slot, typ feedbacku i komunikat. Rodzic automatycznie przekazuje typ feedbacku do UJumanjiAbilityHUDSlotWidget::PlayFeedback.

Dźwięk jest odtwarzany tylko po jawnym zdarzeniu feedbacku. Samo odświeżanie slotu co 0,1 s nie odtwarza dźwięków. Brak Sound Wave jest bezpieczny i nie blokuje aktywacji zdolności.

## Kryteria akceptacji

- [ ] Projekt przechodzi UHT i kompilację UE 5.8.
- [ ] WBP_AbilitySlot dziedziczy po UJumanjiAbilityHUDSlotWidget.
- [ ] WBP_AbilityHUD dziedziczy po UJumanjiAbilityHUDWidget.
- [ ] Pięć nazw BindWidgetOptional jest zgodnych z Hierarchy.
- [ ] Sloty odbierają stan zgodnie z enumem, nie po kolejności przypadkowej.
- [ ] Dotknięcie Primary/Active/Ultimate/Dodge uruchamia istniejący ActivateAbilitySlot.
- [ ] Passive nie może być klikany ani ręcznie aktywowany.
- [ ] Ikony i materiały są ładowane asynchronicznie.
- [ ] Placeholder pozostaje widoczny do załadowania ikony.
- [ ] Cooldown radial używa rzeczywistego CooldownPercent z GAS.
- [ ] Fallback bar działa bez materiału radialnego.
- [ ] Brak zasobu, cooldown i niedostępność są rozróżnialne tekstem/symbolem.
- [ ] Audio odtwarza się tylko po feedback event.
- [ ] Anulowane lub spóźnione callbacki nie zmieniają nowszego stanu slotu.
- [ ] Delegaty są odpinane przy usuwaniu widgetu.
- [ ] HUD nie modyfikuje atrybutów GAS.
- [ ] Test PIE oraz test Android potwierdzają układ, dotyk, czytelność i wydajność.

## Nie obejmuje

- Binarne pliki WBP_AbilityHUD.uasset i WBP_AbilitySlot.uasset.
- Wygenerowane 45 ikon PNG, placeholder PNG, materiał .uasset i Sound Wave.
- Finalne animacje UI, Gameplay Cue i Niagara.
- Kompilacja UE 5.8, test PIE lub test Android wykonany w tym środowisku.
- APK/AAB.

## Następny etap

Stage 22 — Mobile HUD Polish & Accessibility: profile landscape/portrait, safe areas, skalowanie DPI, ustawienia wielkości przycisków, kontrast, haptics i profilowanie UI na Androidzie.
