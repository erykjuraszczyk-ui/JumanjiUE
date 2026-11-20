# JUMANJI 2.111.1 — Stage 22: Mobile HUD Polish & Accessibility

## Cel i zakres

Dopracować pięcioslotowy Ability HUD z Etapu 21 pod kątem telefonów i tabletów Android. Etap dodaje lokalne ustawienia skalowania, większych obszarów dotykowych, kontrastu i ograniczenia animacji. Definiuje też prawidłową obsługę Safe Area oraz dwa responsywne układy UMG.

To osobny tor prac UI. Repozytorium ma również Stage 22 dotyczący walidacji greyboxu Emerald Jungle; numer etapu w tym dokumencie jest opisany jako **Ability HUD track**, aby nie zastępować prac World track.

## Zmiany natywnego C++

### UJumanjiAbilityHUDWidget

Nowe API:

```cpp
ApplyMobileAccessibilitySettings(
    float InHUDScale,
    float InTouchTargetScale,
    bool bInHighContrastMode,
    bool bInReduceMotion
);
```

Zakres wartości:
- HUDScale: 0.75–1.35;
- TouchTargetScale: 0.85–1.50;
- HighContrast: włącza jasne kolory tekstu semantycznego w slotach;
- ReduceMotion: pomija animację ActivationPulseAnimation.

Wartości niepoprawne lub nieskończone są zastępowane neutralną wartością 1.0 przed ograniczeniem zakresu. Ustawienia są lokalne i nie zmieniają GAS, atrybutów ani stanu multiplayer.

### UJumanjiAbilityHUDSlotWidget

Nowe API:

```cpp
ApplyAccessibilityOptions(float TouchTargetScale, bool bHighContrast, bool bReduceMotion);
```

Slot skaluje kontrolkę SlotButton przez transform UMG, dzięki czemu powiększa się zarówno prezentacja, jak i obszar hit-test. Wysoki kontrast zmienia kolor tekstów, a tryb ograniczenia ruchu blokuje pulse po aktywacji. Domyślne kolory tekstu są przechwytywane z Widget Blueprint i przywracane po wyłączeniu wysokiego kontrastu.

Brak opcjonalnego widgetu nadal nie blokuje całego HUD-u. Pasywny slot pozostaje nieinteraktywny.

## Safe Area i wycięcia ekranu — konfiguracja UMG

W `WBP_AbilityHUD`:
1. Utwórz nadrzędny `SafeZone` jako pierwszy element pod Canvas/Root.
2. Umieść całą zawartość HUD wewnątrz SafeZone, nie tylko grupę przycisków.
3. Nie dodawaj ręcznie stałego marginesu „pod notch”; SafeZone ma reagować na aktualne insety urządzenia.
4. Sprawdź zaokrąglone narożniki, wycięcia kamery, pasek gestów oraz tryb immersive.
5. Utrzymaj minimapę, komunikaty i zasoby w obrębie bezpiecznego obszaru. Nie przesuwaj w ten sposób centralnego świata 3D lub celownika.

SafeZone w hierarchii Widget Blueprint musi zostać utworzony w Unreal Editor; sam kod C++ nie tworzy binarnego `.uasset`.

## Landscape i portrait

Zalecany jest jeden `WBP_AbilityHUD` z responsywnym układem, nie dwa niezależne źródła logiki.

### Landscape
- Primary w prawym dolnym obszarze, z największym targetem.
- Active, Ultimate i Dodge w sąsiednim łuku, ale poza joystickiem ruchu.
- Passive jako karta informacyjna, bez przycisku.
- Energy/Resonance nad albo po wewnętrznej stronie grupy zdolności.
- Nie zasłaniaj środka ekranu, interakcji środowiskowych ani telegraphów.

### Portrait
- Grupa zdolności w dolnym pasie, nad systemowym obszarem gestów.
- Primary ma największą powierzchnię; pozostałe kontrolki zachowują odstępy i osobne hit-testy.
- Energy/Resonance układają się pionowo lub jako krótkie paski nad slotami.
- Passive przechodzi do kompaktowej karty/ikony informacyjnej.
- Komunikaty i długie nazwy przenoszą się do górnej części bez nakładania na wycięcie.

Do przełączania wariantów użyj `WidgetSwitcher` albo responsywnych gałęzi opartego na rozmiarze kontenera Canvas/SizeBox. Nie opieraj logiki na stałej rozdzielczości konkretnego telefonu. Ustaw obsługę orientacji w Android Project Settings zgodnie z docelowym UX; obrót nie może resetować GAS ani slotów.

## DPI i rozmiary dotykowe

- Użyj krzywej DPI w Project Settings → User Interface → DPI Scaling.
- UMG pracuje w jednostkach logicznych, a nie w surowych pikselach urządzenia.
- Punkt startowy: co najmniej 48 jednostek logicznych dla zwykłych kontrolek dotykowych; Primary powinien być większy.
- Rozmiary i odstępy sprawdź po zastosowaniu rzeczywistej krzywej DPI oraz SafeZone na docelowym ekranie.
- Przy większym TouchTargetScale sprawdź kolizję hit-testów między slotami. Nie pozwól, aby rosnące przyciski nachodziły na joystick lub na siebie.
- Testuj skalę HUD 0.75, 1.0 i 1.35 oraz target 0.85, 1.0 i 1.5.

Wartości C++ są ograniczane do bezpiecznych zakresów, ale nie zastępują walidacji finalnego layoutu UMG.

## Kontrast i czytelność

- Kolor nie może być jedynym sposobem komunikowania stanu: używaj również tekstu/ikony dla cooldownu, braku zasobu, pasywności i niedostępności.
- W trybie High Contrast tekst nazwy i cooldownu jest biały, koszt/stany — jasny żółty.
- Dodaj ciemny, nieprzezroczysty lub odpowiednio przyciemniony panel pod tekstem. Kolor tekstu bez tła nie gwarantuje wymaganego kontrastu.
- Nie zmniejszaj kluczowego tekstu do rozmiaru nieczytelnego na telefonie.
- Sprawdź UI przy niskiej jasności i na jasnym tle świata.

## Reduced Motion i haptics

Reduced Motion pomija pulse animacji aktywacji, ale zachowuje czytelny tekstowy/kolorystyczny feedback i wszystkie zdarzenia gameplay.

Haptics pozostaje opcjonalnym adapterem platformowym, ponieważ projekt nie ma jeszcze wskazanej, zaimportowanej wtyczki Android Haptics. WBP może nasłuchiwać `OnAbilitySlotFeedback` i wywołać adapter tylko wtedy, gdy:
- użytkownik włączył haptics;
- zdarzenie jest istotne (np. zatwierdzona aktywacja lub zakończenie interakcji);
- urządzenie i aktualna platforma wspierają adapter.

Nie wywołuj wibracji przy każdym odświeżeniu HUD, każdej klatce ani przy cyklicznym ticku cooldownu. Nie ma tu implementacji JNI ani gwarancji wibracji na każdym Androidzie.

## Zapis ustawień

Nowe API C++ stosuje ustawienia w działającym HUD, ale samo nie zapisuje ich do profilu. Ekran ustawień powinien przechowywać cztery wartości w istniejącej warstwie ustawień użytkownika/SaveGame i wywoływać API po odtworzeniu profilu oraz po zmianie opcji. Nie zapisuj preferencji UI w PlayerState jako stanu współdzielonego przez multiplayer.

## Profilowanie Android

Przeprowadź testy na urządzeniu referencyjnym i urządzeniu o niższej wydajności:
- sprawdź czas Game Thread i Render Thread oraz koszt Slate/UMG;
- zweryfikuj brak alokacji/ładowania assetów przy każdym ticku;
- potwierdź, że dźwięk nie odtwarza się na cyklicznym refreshu;
- sprawdź pamięć i liczbę widgetów przy zmianie orientacji;
- zmierz input latency oraz sprawdź brak nakładających się hit-testów;
- profiluj Development/Shipping build z docelową rozdzielczością i skalowaniem.

Nie traktuj samego PIE na PC jako potwierdzenia jakości Android.

## Plan testów i kryteria akceptacji

- [ ] UHT i kompilacja UE 5.8 przechodzą bez błędów.
- [ ] `ApplyMobileAccessibilitySettings` ogranicza HUDScale do 0.75–1.35.
- [ ] `ApplyMobileAccessibilitySettings` ogranicza TouchTargetScale do 0.85–1.50.
- [ ] Wszystkie dostępne sloty otrzymują te same ustawienia lokalne.
- [ ] Zmiana ustawień w czasie działania aktualizuje HUD bez restartu rozgrywki.
- [ ] Wysoki kontrast przywraca oryginalne kolory po wyłączeniu.
- [ ] Reduced Motion blokuje pulse, ale nie blokuje aktywacji ani feedback tekstowego.
- [ ] Passive nadal nie jest przyciskiem.
- [ ] SafeZone chroni wszystkie elementy UI przed cutoutami i gestami systemowymi.
- [ ] Landscape i portrait nie zasłaniają obszaru rozgrywki ani innych kontrolek.
- [ ] Powiększone sloty nie mają nakładających się obszarów dotyku.
- [ ] Wszystkie stany są rozpoznawalne bez polegania wyłącznie na kolorze.
- [ ] Haptics jest wyłączalne i uruchamia się wyłącznie przez adapter platformowy po jawnej decyzji użytkownika.
- [ ] Test Android potwierdza czytelność, dotyk, obrót, SafeZone i stabilność FPS.

## Nie obejmuje

- Gotowych binarnych `.uasset` dla HUD ani ustawień.
- Automatycznego zapisu preferencji — wymaga podłączenia do ekranu/warstwy SaveGame.
- Wtyczki JNI/Android Haptics lub gwarantowanej wibracji.
- Wykonanego profilowania urządzenia, testu PIE, kompilacji UE 5.8 ani APK/AAB w tym środowisku.

## Następny etap

Stage 23 — Ability HUD Settings Persistence & Device QA: podłączyć ekran ustawień do trwałego zapisu, wykonać testy rzeczywistych breakpointów DPI/orientacji i zamknąć listę regresji na urządzeniach Android.
