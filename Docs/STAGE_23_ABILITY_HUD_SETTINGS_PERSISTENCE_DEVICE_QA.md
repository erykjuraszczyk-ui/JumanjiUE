# JUMANJI 2.111.1 — Stage 23: Ability HUD Settings Persistence & Device QA

## Cel

Podłączyć preferencje HUD z Etapu 22 do natywnego lokalnego systemu ustawień Unreal Engine oraz przygotować powtarzalny zestaw QA dla różnych orientacji, DPI, safe area i rozmiarów dotykowych.

## Decyzja architektoniczna: GameUserSettings zamiast profilu postaci

Repozytorium posiada `UJumanjiPlayerProfileSubsystem`, który zapisuje dane rozgrywki w profilu obsługiwanym przez warstwę serwerową prototypu. Nie używamy go do skalowania HUD-u, kontrastu ani Reduced Motion.

Nowa klasa `UJumanjiGameUserSettings` dziedziczy po `UGameUserSettings` i zapisuje cztery lokalne preferencje w pliku `GameUserSettings.ini`:
- `HUDScale` — 0.75–1.35;
- `TouchTargetScale` — 0.85–1.50;
- `HighContrastMode`;
- `ReduceMotion`.

Preferencje są właściwością urządzenia/lokalnego użytkownika, nie PlayerState i nie są replikowane. Nie są też synchronizowane między kontami ani urządzeniami.

## Konfiguracja klasy w UE 5.8

W `Config/DefaultEngine.ini`, sekcja `[/Script/Engine.Engine]` wskazuje:

```ini
GameUserSettingsClassName=/Script/JumanjiUE.JumanjiGameUserSettings
```

Po zmianie klasy ustawień uruchom ponownie edytor i aplikację. Upewnij się w logu/runtime, że `GEngine->GetGameUserSettings()` zwraca `UJumanjiGameUserSettings`.

Klasa dostarcza:
- `GetAbilityHUDPreferences()`;
- `SetAbilityHUDPreferences(Preferences)` — walidacja, clamp i `SaveSettings()`;
- `ResetAbilityHUDPreferences()` — reset wyłącznie czterech preferencji HUD;
- `ValidateSettings()` — walidacja danych wczytanych z pliku.

Nie resetuj ustawień grafiki ani dźwięku podczas resetowania preferencji HUD.

## Automatyczne zastosowanie przy starcie

`AJumanjiPlayerController::BeginPlay` tworzy HUD tylko dla kontrolera lokalnego. Po utworzeniu `ActiveAbilityHUDWidget` odczytuje `UJumanjiGameUserSettings` i przekazuje cztery wartości do:

```cpp
ActiveAbilityHUDWidget->ApplyMobileAccessibilitySettings(
    Preferences.HUDScale,
    Preferences.TouchTargetScale,
    Preferences.bHighContrastMode,
    Preferences.bReduceMotion
);
```

Jeśli klasa niestandardowa nie jest aktywna, zostają wartości domyślne widgetu. Brak ustawień nie blokuje tworzenia HUD-u.

## Ekran ustawień WBP_AbilityHUDSettings

Utwórz Widget Blueprint:
- Asset: `/Game/UI/AbilityHUD/WBP_AbilityHUDSettings`;
- Parent Class: `UJumanjiAbilityHUDSettingsWidget`.

Przykładowe elementy UI:
- `HUDScaleSlider` — min 0.75, max 1.35, step 0.05;
- `TouchTargetScaleSlider` — min 0.85, max 1.50, step 0.05;
- `HighContrastToggle`;
- `ReduceMotionToggle`;
- `ApplyButton`;
- `ResetHUDButton`;
- `BackButton`.

Kontrakt Blueprint:
1. Przy otwarciu wywołaj `GetSavedPreferences` i ustaw cztery kontrolki.
2. Zmiany suwaków przechowuj tymczasowo w lokalnym ViewModel/zmiennych widgetu.
3. `ApplyButton` tworzy `FJumanjiAbilityHUDPreferences` i wywołuje `ApplyAndSavePreferences`.
4. Po sukcesie event `OnPreferencesSaved` otrzymuje wartości po walidacji.
5. `ResetHUDButton` wywołuje `ResetAndSavePreferences`.
6. Nie wywołuj zapisu na każdą zmianę suwaka; zapisuj po zatwierdzeniu, aby uniknąć nadmiarowych operacji I/O.

Po zapisaniu ustawień aktualizuj istniejący Ability HUD:
- przez referencję do HUD i `ApplyMobileAccessibilitySettings`, albo
- przez jawny event/delegat połączony z PlayerController.

Widget ustawień nie ma samodzielnie zmieniać atrybutów, GAS, PlayerState ani profilu konta.

## Testy automatyczne i manualne

`Data/UI/AbilityHUD_DeviceQAMatrix.csv` zawiera 14 przypadków dla:
- landscape i portrait;
- 720p, 1080p i proporcji telefonów;
- cutoutów, zaokrąglonych rogów i systemowego paska gestów;
- minimalnego i maksymalnego skalowania;
- high contrast i Reduced Motion;
- restartu aplikacji i zmiany orientacji.

Statusy zaczynają się od `NOT_RUN`. Nie zmieniaj ich na PASS bez wyniku rzeczywistego testu na urządzeniu.

Walidator struktury:
```bash
python Scripts/validate_ability_hud_device_qa.py
python -m unittest discover -s Scripts/Tests -p "test_validate_ability_hud_device_qa.py"
```

Workflow `.github/workflows/ability-hud-device-qa.yml` uruchamia walidator i testy regresyjne przy zmianach macierzy lub skryptów.

Ważne: automatyczny workflow waliduje kompletność i poprawność danych testowych. Nie uruchamia Unreal Engine ani nie potwierdza dotyku, DPI, SafeZone, FPS lub działania APK.

## Procedura testu rzeczywistego urządzenia

1. Zbuduj Development APK z UE 5.8 i zainstaluj na urządzeniu.
2. Otwórz Settings i ustaw wartości skrajne, a następnie wartości domyślne.
3. Wciśnij Apply, zamknij i uruchom aplikację ponownie.
4. Potwierdź, że HUD odtwarza zapisane wartości.
5. Użyj Reset HUD i sprawdź, że ustawienia grafiki/dźwięku nie zostały zmienione.
6. Obróć urządzenie w obsługiwanym zakresie orientacji.
7. Sprawdź SafeZone na wycięciach, zaokrągleniach i pasku gestów.
8. Sprawdź hit-testy slotów przy TouchTargetScale 1.50; nie może być nakładania na joystick ani innych slotów.
9. Włącz High Contrast i Reduced Motion.
10. Zmierz FPS, czas Game Thread/Render Thread, input latency i pamięć w Development oraz docelowym Shipping build.
11. Zapisz Device Model, Android Version, UE Build, Viewport, DPI Scale, wyniki i logi w raporcie testowym.

## Kryteria akceptacji

- [ ] UHT i kompilacja UE 5.8 przechodzą.
- [ ] Silnik aktywuje niestandardową klasę GameUserSettings.
- [ ] Preferencje HUD zapisują się do lokalnego GameUserSettings.ini.
- [ ] Ponowne uruchomienie aplikacji przywraca zapisane wartości.
- [ ] Reset HUD nie zmienia ustawień grafiki, dźwięku ani danych profilu gry.
- [ ] Nieprawidłowe wartości są ograniczane do bezpiecznych zakresów.
- [ ] PlayerController stosuje zapisane wartości po utworzeniu lokalnego HUD.
- [ ] WBP_AbilityHUDSettings odczytuje, zapisuje i resetuje preferencje.
- [ ] Ustawienia są lokalne, nie są replikowane i nie zapisują się do PlayerState.
- [ ] 14 scenariuszy macierzy jest wykonanych i udokumentowanych na urządzeniach.
- [ ] Zmiana orientacji nie resetuje GAS, wybranego ducha ani stanu gry.
- [ ] SafeZone/DPI/hit-test i wydajność są zaakceptowane na urządzeniach docelowych.

## Nie obejmuje

- Binarnego `.uasset` ekranu ustawień — musi zostać utworzony w Unreal Editor.
- Faktycznego builda UE 5.8, PIE, Development/Shipping APK lub testu na telefonie w tym środowisku.
- Synchronizacji preferencji UI z kontem/chmurą.
- Platformowych wibracji — adapter Android haptics pozostaje odrębnym zadaniem.

## Następny etap

Stage 24 — HUD Settings Screen Blueprint & Live Preview: zbudować pełny WBP_AbilityHUDSettings, podłączyć suwaki/przełączniki, Apply/Reset/Back i natychmiastowy podgląd preferencji podczas konfiguracji.
