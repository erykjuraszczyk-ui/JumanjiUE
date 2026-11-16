# JUMANJI 2.111.1 — Stage 19: Ability Presentation Asset Pack

## Cel etapu

Przygotować spójny system prezentacji pięciu slotów zdolności dla dziewięciu Great Spirits: 45 ikon, wspólny materiał radialnego cooldownu, stany wizualne, krótkie dźwięki feedbacku, cue prezentacyjne oraz jednoznaczną konfigurację klas Gameplay Ability.

Etap 19 jest kontynuacją Stage 18. Nie zmienia reguł GAS, kosztów, cooldownów ani walidacji serwera. Widget pozostaje warstwą prezentacji.

## Zakres i status

Repozytorium otrzymuje:
- manifest 45 slotów oparty na stabilnych AbilityId z Stage 16 i nazwach z GreatSpiritAbilityProductionMatrix;
- ścieżki docelowe ikon, materiału radialnego, dźwięków i Gameplay Cue;
- dziewięć palet akcentowych jako propozycje art-direction;
- specyfikację ikon, materiałów, stanów HUD, audio i VFX;
- checklistę importu i przypisania zasobów w Unreal Editor.

Manifest jest specyfikacją assetów. Nie oznacza, że binarne pliki .uasset/.png/.wav/.uasset Niagara zostały wygenerowane lub zaimportowane. Te zasoby należy utworzyć w Unreal Editor lub przygotować w zewnętrznym pipeline graficzno-audio.

## 1. Źródło prawdy i identyfikatory

Plik: Content/UI/AbilityPresentation/AbilityPresentationManifest.csv

- Każdy wiersz ma stabilny AbilityId zgodny z katalogiem Stage 16.
- CharacterId i SlotId nie mogą być zmieniane po zapisaniu loadoutów w profilu.
- Kolejność pięciu slotów: PrimaryAttack, Active, Passive, Ultimate, Dodge.
- Passive jest wyłącznie informacyjny; nie twórz ręcznego przycisku aktywacji.
- CooldownSeconds w manifeście są wartościami referencyjnymi do konfiguracji assetów. Rzeczywisty cooldown nadal pochodzi z Gameplay Effect/Gameplay Ability, a nie z CSV ani widgetu.
- Ścieżki /Game/... są docelowymi nazwami assetów, nie potwierdzeniem, że asset już istnieje.

## 2. Ikony — 45 pozycji

Docelowy katalog: /Game/UI/AbilityPresentation/Icons/

Każda ikona:
- format źródłowy: PNG z kanałem alpha lub wektorowy master rasteryzowany przed importem;
- rozmiar docelowy: 256×256 px (wariant 128×128 px można wygenerować z mastera);
- kompozycja centralna, bez tekstu, cyfr i liter;
- bezpieczny margines minimum 12% od krawędzi;
- jedna czytelna sylwetka/motyw, bez drobnych detali ginących na ekranie telefonu;
- tło przezroczyste; bez wbudowanego pierścienia cooldownu;
- eksport sRGB, bez niepotrzebnego mip-bluru dla małych ikon UI.

Nie używaj jednej ikony dla różnych postaci tylko dlatego, że mają ten sam slot. Motyw każdej ikony opisuje kolumna IconMotif w manifeście.

## 3. Palety Great Spirit

Kolory AccentHex i SecondaryHex w manifeście są propozycjami kierunku artystycznego, a nie próbkami pobranymi z referencyjnego obrazu.

| Great Spirit | Element | Accent | Secondary |
|---|---|---|---|
| I — Duch Korzeni | Root | #4D9B62 | #B8D88A |
| II — Duch Przypływu | Tide | #278FB8 | #9BE1E8 |
| III — Duch Kamienia | Stone | #9A8064 | #D8C6A5 |
| IV — Duch Burzy | Storm | #8B76D8 | #D3C5FF |
| V — Duch Żaru | Ember | #D66A36 | #FFC16A |
| VI — Duch Świtu | Dawn | #D6A83F | #FFF0A6 |
| VII — Duch Cienia | Shadow | #655B83 | #B9A8D8 |
| VIII — Duch Życia | Life | #4C9B76 | #A9DDB8 |
| IX — Duch Mrozu | Frost | #68A9CF | #D7F2FF |

Kolory nie mogą być jedynym nośnikiem stanu. Każdy stan ma też zmianę kształtu, ikonę, tekst lub wzór.

## 4. Materiał radialnego cooldownu

Docelowy asset:
- /Game/UI/AbilityPresentation/Materials/M_UI_AbilityCooldown_Radial — master material;
- /Game/UI/AbilityPresentation/Materials/MI_UI_AbilityCooldown_Radial — instancja używana przez WBP_AbilityHUD.

Ustawienia master:
- Material Domain: User Interface;
- Blend Mode: Translucent;
- parametry: IconTexture (Texture Object), AccentColor (Vector), CooldownPercent (Scalar 0–1), DisabledAmount (Scalar 0–1), PulseAmount (Scalar 0–1);
- cooldown rysowany jako radialna maska/overlay, bez modyfikowania źródłowej tekstury ikony;
- brak ciężkich pętli i kosztownych efektów per-pixel.

Semantyka CooldownPercent musi odpowiadać Stage 18: 1 = pełny pozostały cooldown, 0 = gotowość. WBP pobiera wartość z FJumanjiAbilityHUDSlotState. Nie uruchamiaj drugiego, niezależnego zegara cooldownu.

Stany:
- Ready: pełna czytelność ikony, brak maski;
- OnCooldown: radialna maska zgodna z CooldownPercent;
- InsufficientResource: delikatny desaturation + osobny symbol zasobu;
- Unavailable/NotGranted: neutralny placeholder i etykieta;
- Passive: stan informacyjny, bez affordance przycisku;
- Pressed/Activated: krótki scale/opacity feedback, bez pełnoekranowego flasha.

## 5. Audio i cue prezentacyjne

Docelowe krótkie UI Sound Wave:
- S_UI_Ability_Activated;
- S_UI_Ability_InsufficientResource;
- S_UI_Ability_Cooldown;
- S_UI_Ability_Unavailable.

Zasady:
- krótki czas, bez długich pętli;
- brak głośnych, nagłych transjentów;
- osobne suwaki UI SFX i Master;
- cooldown nie powinien odtwarzać dźwięku co klatkę ani przy każdym odświeżeniu HUD;
- feedback odtwarzaj tylko przy zmianie stanu/zdarzeniu, nie przy samym odczycie danych.

Gameplay Cue tags z manifestu są punktami integracji dla klas zdolności. Przypisz je do rzeczywistych Gameplay Cue assets dopiero po ich utworzeniu. Dla pasywek cue może być zdarzeniem stanu, nie powtarzanym efektem ciągłym.

## 6. Niagara / VFX

VFX zdolności w świecie gry pozostają przypisane do Stage 17/Blueprint ability assets. Etap 19 definiuje jedynie reguły prezentacji:
- krótki, czytelny sygnał aktywacji;
- czytelny telegraph przed większym efektem;
- nie zasłaniaj pola widzenia i wskazówek bossa;
- ogranicz emisję cząstek i overdraw na urządzeniach mobilnych;
- nie odtwarzaj efektu świata z lokalnego widgetu jako substytutu serwerowego Gameplay Cue.

Dla HUD używaj lekkiego materiału UI i krótkiej animacji UMG; Niagara nie jest wymagane do samego cooldown overlay.

## 7. Przypisanie do 45 klas

Dla każdego wiersza manifestu:
1. Otwórz odpowiednią klasę GA_GS_[ROMAN]_[SLOT].
2. Ustaw HUDDisplayName zgodnie z AbilityNamePL.
3. Ustaw HUDIcon na IconAssetPath.
4. Ustaw HUDResourceType/HUDResourceCost zgodnie z rzeczywistym Gameplay Effect.
5. Ustaw klasę/Gameplay Cue zgodnie z konfiguracją Stage 17 i manifestem.
6. Sprawdź, czy AbilitySlot i InputTag pasują do SlotId.
7. Nie ustawiaj ręcznego wejścia dla Passive.
8. Uruchom WBP_AbilityHUD i sprawdź stany gotowości, cooldownu, braku zasobu i braku przyznanej zdolności.

Standardy referencyjne z produkcyjnej matrycy:
- Active: 20 Energy, bazowy cooldown 10 s;
- Ultimate: 100 Resonance, bazowy cooldown 60 s;
- Dodge: bazowy cooldown 4 s;
- Primary i Passive: brak wspólnego cooldownu/kosztu narzuconego przez ten manifest.

Są to wartości startowe/specyfikacyjne. Rzeczywiste wartości muszą być zgodne z konkretnym Gameplay Effect danej zdolności.

## 8. Mobile performance i dostępność

- ikony 256 px; unikaj niepotrzebnych wariantów 4K;
- używaj soft references i placeholderów, jeśli ikona nie została załadowana;
- nie ładuj synchronicznie całego zestawu 45 ikon podczas otwierania HUD;
- ogranicz animacje do krótkich przejść i nie animuj wszystkich slotów bez potrzeby;
- kontrast tekstu i symboli sprawdzaj na jasnym i ciemnym tle;
- stan disabled/cooldown rozróżniaj także kształtem/tekstem;
- testuj w orientacji docelowej oraz na niskiej rozdzielczości Android.

## 9. Kryteria akceptacji

- [ ] Manifest zawiera dokładnie 45 unikalnych AbilityId.
- [ ] Wszystkie 9 Great Spirits mają po pięć slotów.
- [ ] Wszystkie AbilityId zgadzają się z katalogiem Stage 16.
- [ ] Każdy wiersz ma unikalną ścieżkę ikony i czytelny motyw.
- [ ] Paleta każdej postaci jest spójna we wszystkich pięciu slotach.
- [ ] Master Material ma parametry zgodne z dokumentacją.
- [ ] CooldownPercent jest zasilany z danych Stage 18, bez osobnego zegara.
- [ ] Feedback audio jest odtwarzany tylko przy zmianie stanu.
- [ ] Passive nie ma przycisku ręcznej aktywacji.
- [ ] Brak zasobu, cooldown i unavailable są rozróżnialne bez polegania wyłącznie na kolorze.
- [ ] Soft references i placeholdery nie powodują blokowania otwarcia HUD.
- [ ] UI i efekty są czytelne na urządzeniu Android w docelowej rozdzielczości.
- [ ] Projekt kompiluje się w Unreal Engine 5.8 i przechodzi test PIE.

## 10. Co pozostaje do wykonania w Unreal Editor

Ten PR zawiera specyfikację i manifest. Nie zawiera binarnych ikon PNG, materiałów .uasset, Sound Wave, Sound Cue, Niagara ani Widget Blueprint. Trzeba je utworzyć/importować w Unreal Editor, przypisać do klas zdolności i wykonać testy urządzenia.

## Weryfikacja repozytorium

W PR należy sprawdzić:
- 45 rekordów manifestu i 45 unikalnych AbilityId;
- 9 × 5 kompletnego pokrycia slotów;
- spójność nazw AbilityId z Stage 16;
- brak zmiany reguł GAS i Stage 18;
- brak znaczników konfliktu w plikach tekstowych.

Nie deklaruj kompilacji UE 5.8, importu assetów ani testu Android bez rzeczywistego wykonania w Unreal Editor.
