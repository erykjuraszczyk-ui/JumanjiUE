# Great Spirit Ability Production Matrix — JUMANJI 2.111.1

## Konwencje

Wszystkie liczby są propozycjami startowymi. Nie są finalnym balansem.

- ForwardSweep: serwerowy sferyczny sweep przed postacią.
- RadialPulse: serwerowe wyszukanie celów w promieniu.
- Self: efekt na właścicielu.
- Dash: krótki ruch do przodu.
- Friendly: postacie po tej samej stronie prototypowego PvE.
- Hostile: przeciwnicy po przeciwnej stronie.
- Ujemna SetByCaller magnitude może służyć do obrażeń; dodatnia do leczenia. Konkretny efekt musi mieć zgodny modifier.
- Standardowe koszty slotów: Active = 20 Energy; Ultimate = 100 Resonance; Primary i Dodge bez obowiązkowego kosztu zasobu. Cooldowny startowe: Active = 10 s; Ultimate = 60 s; Dodge = 4 s.

## Great Spirit I — Duch Korzeni (Root / Guardian)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_I_PRIMARY | GA_GS_I_Primary | Uderzenie Korzeni | ForwardSweep / Hostile | krótki atak liniowy; Target Effect = obrażenia; niski promień, częste użycie |
| Active | ABILITY_GS_I_ACTIVE | GA_GS_I_Active | Korzenna Fala | RadialPulse / Hostile + Self Effect | obszar kontroli; efekt celu oznacza spowolnienie, Self Effect daje krótką ochronę |
| Passive | ABILITY_GS_I_PASSIVE | GA_GS_I_Passive | Zew Korzeni | Event-driven / Self | reaguje na wejście do obszaru oznaczonego Root; czasowy bonus Defense; usuwa bonus po wyjściu |
| Ultimate | ABILITY_GS_I_ULTIMATE | GA_GS_I_Ultimate | Serce Pradawnej Kniei | RadialPulse / dual effects | duża strefa; osobny efekt dla sojuszników i wrogów; potrzebny Blueprint dual-effect |
| Dodge | ABILITY_GS_I_DODGE | GA_GS_I_Dodge | Krok Pnącza | Dash / Self | krótki ruch i wizualny ślad pnączy; bazowy cooldown 4 s |

## Great Spirit II — Duch Przypływu (Tide / Support)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_II_PRIMARY | GA_GS_II_Primary | Strumień Przypływu | ForwardSweep / Hostile | średni zasięg; pocisk/strumień wodnej energii; mała wartość obrażeń |
| Active | ABILITY_GS_II_ACTIVE | GA_GS_II_Active | Prąd Ukojenia | RadialPulse / Friendly | dodatni SetByCaller do Health; efekt leczenia dla sojuszników w zasięgu |
| Passive | ABILITY_GS_II_PASSIVE | GA_GS_II_Passive | Taniec Fal | Event-driven / Self | po skutecznym wsparciu sojusznika odzyskuje niewielką ilość Energy; limit częstotliwości |
| Ultimate | ABILITY_GS_II_ULTIMATE | GA_GS_II_Ultimate | Wielki Przypływ | RadialPulse / Friendly | większy heal-over-time; dodatkowy Blueprint flow usuwa dozwolone statusy |
| Dodge | ABILITY_GS_II_DODGE | GA_GS_II_Dodge | Fala Uniku | Dash / Self | płynny reposition z wodnym cue; bazowy cooldown 4 s |

## Great Spirit III — Duch Kamienia (Stone / Vanguard)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_III_PRIMARY | GA_GS_III_Primary | Cios Granitu | ForwardSweep / Hostile | mocny pojedynczy impact; wyższa baza obrażeń, krótszy zasięg |
| Active | ABILITY_GS_III_ACTIVE | GA_GS_III_Active | Tarcza Pradawnych | Self / Self | czasowy efekt ochronny; absorpcja obrażeń wymaga osobnej implementacji tarczy |
| Passive | ABILITY_GS_III_PASSIVE | GA_GS_III_Passive | Serce Skały | Event-driven / Self | otrzymanie osłony uruchamia czasowy bonus odporności; wymaga eventu ShieldApplied |
| Ultimate | ABILITY_GS_III_ULTIMATE | GA_GS_III_Ultimate | Góra Niezłomna | Self + Blueprint actor | tworzy osłonę/cover w świecie; aktor środowiskowy i walidacja miejsca wymagają Blueprint flow |
| Dodge | ABILITY_GS_III_DODGE | GA_GS_III_Dodge | Kamienny Krok | Dash / Self | krótki, ciężki krok; opcjonalny krótki efekt defensywny |

## Great Spirit IV — Duch Burzy (Storm / Controller)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_IV_PRIMARY | GA_GS_IV_Primary | Iskra Burzy | ForwardSweep / Hostile | szybki, wąski impuls energii |
| Active | ABILITY_GS_IV_ACTIVE | GA_GS_IV_Active | Piorunujący Krąg | RadialPulse / Hostile | efekt obszarowy z tagiem kontroli; czytelny sygnał przed aktywacją |
| Passive | ABILITY_GS_IV_PASSIVE | GA_GS_IV_Passive | Znak Burzy | Event-driven / Hostile | kolejne trafienia nakładają/odświeżają mark; stosy i czas trwania w osobnym GE |
| Ultimate | ABILITY_GS_IV_ULTIMATE | GA_GS_IV_Ultimate | Gniew Niebios | RadialPulse / Hostile | kilka sygnalizowanych pulsów; do pełnej sekwencji potrzebny Blueprint/Ability Task |
| Dodge | ABILITY_GS_IV_DODGE | GA_GS_IV_Dodge | Błysk Burzy | Dash / Self | szybki dash z widocznym cue miejsca docelowego |

## Great Spirit V — Duch Żaru (Ember / Striker)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_V_PRIMARY | GA_GS_V_Primary | Płomienny Impuls | ForwardSweep / Hostile | szybki burst; umiarkowana wartość obrażeń |
| Active | ABILITY_GS_V_ACTIVE | GA_GS_V_Active | Płomień Feniksa | ForwardSweep / Hostile | wyższa magnituda i dłuższy montage; efekt follow-through w cue |
| Passive | ABILITY_GS_V_PASSIVE | GA_GS_V_Passive | Rozżarzony Ślad | Event-driven / Self | po Active wzmacnia następny Primary; wymaga eventu AbilityResolved i zużycia znacznika |
| Ultimate | ABILITY_GS_V_ULTIMATE | GA_GS_V_Ultimate | Feniks Odrodzenia | RadialPulse / Hostile | duży obszar obrażeń; kilka warstw VFX, bez zasłaniania telegraphów bossa |
| Dodge | ABILITY_GS_V_DODGE | GA_GS_V_Dodge | Żarliwy Skok | Dash / Self | krótki skok do przodu ze śladem żaru |

## Great Spirit VI — Duch Świtu (Dawn / Ranger)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_VI_PRIMARY | GA_GS_VI_Primary | Promień Świtu | ForwardSweep / Hostile | najdłuższy zasięg Primary; wąska trajektoria |
| Active | ABILITY_GS_VI_ACTIVE | GA_GS_VI_Active | Promień Przeznaczenia | ForwardSweep / Hostile | mark priority target; mark wymaga dedykowanego Gameplay Effect i obsługi UI |
| Passive | ABILITY_GS_VI_PASSIVE | GA_GS_VI_Passive | Oko Świtu | Event-driven / Self + Party | udostępnia widoczność oznaczonego celu; potrzebny party/UI marker |
| Ultimate | ABILITY_GS_VI_ULTIMATE | GA_GS_VI_Ultimate | Korona Świtu | RadialPulse / Hostile | ujawnia i znakuje cele w obszarze; efekt wizualny i replikowany marker |
| Dodge | ABILITY_GS_VI_DODGE | GA_GS_VI_Dodge | Świetlisty Krok | Dash / Self | szybki sidestep z krótkim świetlistym śladem |

## Great Spirit VII — Duch Cienia (Shadow / Duelist)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_VII_PRIMARY | GA_GS_VII_Primary | Cięcie Cienia | ForwardSweep / Hostile | wąski łuk; krótkie okno trafienia |
| Active | ABILITY_GS_VII_ACTIVE | GA_GS_VII_Active | Rozdarcie Cienia | Dash + radial follow-up | dash z impulsem po przemieszczeniu; follow-up wymaga Blueprint eventu |
| Passive | ABILITY_GS_VII_PASSIVE | GA_GS_VII_Passive | Echo Łowcy | Event-driven / Self | skuteczny dodge wzmacnia następny Active; wymaga eventu DodgeSucceeded |
| Ultimate | ABILITY_GS_VII_ULTIMATE | GA_GS_VII_Ultimate | Wieczna Noc | RadialPulse / Hostile | obszar ogranicza wskaźniki celów wroga; czytelność wskazówek gracza musi pozostać |
| Dodge | ABILITY_GS_VII_DODGE | GA_GS_VII_Dodge | Cień Przemieszczenia | Dash / Self | szybki reposition i widoczny shadow echo |

## Great Spirit VIII — Duch Życia (Life / Healer)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_VIII_PRIMARY | GA_GS_VIII_Primary | Zielony Impuls | ForwardSweep / Friendly | małe leczenie sojusznika lub interakcja z obiektem wymaga osobnego targetu interakcji |
| Active | ABILITY_GS_VIII_ACTIVE | GA_GS_VIII_Active | Ogród Odnowy | RadialPulse / Friendly | obszarowy heal-over-time; okresowy Gameplay Effect |
| Passive | ABILITY_GS_VIII_PASSIVE | GA_GS_VIII_Passive | Zielone Tchnienie | Event-driven / Self | leczenie sojusznika odzyskuje Energy; wymaga eventu HealApplied |
| Ultimate | ABILITY_GS_VIII_ULTIMATE | GA_GS_VIII_Ultimate | Ogród Wieczności | RadialPulse / Friendly | większy i dłuższy obszar regeneracji; osobny VFX/UI |
| Dodge | ABILITY_GS_VIII_DODGE | GA_GS_VIII_Dodge | Zielony Zwrot | Dash / Self | krótki ruch i liściasty cue |

## Great Spirit IX — Duch Mrozu (Frost / Sentinel)

| Slot | AbilityId | Blueprint class | Nazwa | Tryb / cel | Konfiguracja i zachowanie |
|---|---|---|---|---|---|
| Primary | ABILITY_GS_IX_PRIMARY | GA_GS_IX_Primary | Odłamek Mrozu | ForwardSweep / Hostile | precyzyjny pocisk energii mrozu; wąski sweep |
| Active | ABILITY_GS_IX_ACTIVE | GA_GS_IX_Active | Lodowy Bastion | RadialPulse / Friendly + Self | krótki efekt ochronny; pełna absorpcja wymaga systemu tarcz |
| Passive | ABILITY_GS_IX_PASSIVE | GA_GS_IX_Passive | Korona Zimy | Event-driven / Self | aktywna osłona generuje Resonance w ograniczonym tempie |
| Ultimate | ABILITY_GS_IX_ULTIMATE | GA_GS_IX_Ultimate | Zimowy Tron | RadialPulse / dual effects | kontrola przeciwników i ochrona sojuszników; osobne efekty na obie strony |
| Dodge | ABILITY_GS_IX_DODGE | GA_GS_IX_Dodge | Lodowy Ślizg | Dash / Self | krótki glide z lodowym śladem |

## Uwagi wdrożeniowe

1. Nazwy AbilityId muszą pozostać zgodne z Stage 16. Nie zmieniaj stabilnych ID po zapisaniu ich w profilach.
2. Nie aktywuj slotu Passive przez przycisk dotykowy. Pasywka ma reagować na zdarzenia lub utrzymywać kontrolowany efekt.
3. Dual-effect (różny efekt dla Friendly i Hostile) wymaga dedykowanego Blueprint flow albo dalszego rozszerzenia natywnego runtime’u. Jedna instancja TargetEffectClass nakłada ten sam efekt na wszystkich dopuszczonych kandydatów.
4. Mechaniki mark, cleanse, absorb shield, osłona świata i wieloetapowe pulsy wymagają odrębnych Gameplay Effects, eventów, actorów lub Ability Tasks. Nie są automatycznie zapewniane przez samą klasę bazową.
5. Zawsze testuj serwerową weryfikację i replikację w multiplayer PIE. Nie polegaj na lokalnym wyniku trace jako źródle prawdy.
