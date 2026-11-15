# Great Spirit Ability Design — JUMANJI 2.111.1

This document defines the proposed gameplay identity of the nine Great Spirits. These are original design concepts; the supplied portrait reference establishes visual identity only.

## Shared combat rules

| Attribute | Starting value | Function |
|---|---:|---|
| Health | 100 | current survivability resource |
| MaxHealth | 100 | upper Health limit |
| Energy | 100 | active-ability resource |
| MaxEnergy | 100 | upper Energy limit |
| Resonance | 0 | ultimate charge |
| MaxResonance | 100 | ultimate threshold |
| AttackPower | 10 | offensive scaling baseline |
| Defense | 5 | defensive scaling baseline |

These are initial test values, not final balance values.

### Slot rules
- Primary Attack: repeatable core action; no mandatory resource cost.
- Active: signature ability; recommended starting cost 20 Energy and 8–12 second cooldown.
- Passive: persistent or event-triggered effect; no manual input.
- Ultimate: major ability; recommended cost 100 Resonance.
- Dodge: short evasive movement; recommended 3–5 second cooldown or a small Energy cost.

The values above are balancing proposals. Tune them with all nine characters and do not hard-code them into ability code.

## Great Spirit I — Duch Korzeni
Role: Guardian. Element: Root.
- Primary — Uderzenie Korzeni: short-range pulse that sends a line of root energy forward.
- Active — Korzenna Fala: ground wave that briefly slows targets in a narrow area and creates a small protective field for allies.
- Passive — Zew Korzeni: near a root-marked area, the spirit receives a small defensive bonus.
- Ultimate — Serce Pradawnej Kniei: a broad grove field that protects allies and restricts enemy movement.
- Dodge — Krok Pnącza: short directional shift with a fading vine afterimage.
Gameplay loop: hold a safe area, protect allies and create space for the team.

## Great Spirit II — Duch Przypływu
Role: Support. Element: Tide.
- Primary — Strumień Przypływu: medium-range water-energy pulse.
- Active — Prąd Ukojenia: sends a current through nearby allies, restoring some Health through a Gameplay Effect.
- Passive — Taniec Fal: supporting an ally gradually builds a small amount of Energy.
- Ultimate — Wielki Przypływ: wide current that restores allies and clears selected removable status effects.
- Dodge — Fala Uniku: fluid short reposition with a brief water trail.
Gameplay loop: maintain team resources, recover allies and keep the group moving.

## Great Spirit III — Duch Kamienia
Role: Vanguard. Element: Stone.
- Primary — Cios Granitu: focused stone-energy impact.
- Active — Tarcza Pradawnych: short-duration frontal protective barrier.
- Passive — Serce Skały: receiving a shield grants a small temporary resilience bonus.
- Ultimate — Góra Niezłomna: raises a stone formation that creates cover and controls the encounter area.
- Dodge — Kamienny Krok: short grounded movement with a brief defensive window.
Gameplay loop: protect a route, create cover and stabilize difficult encounters.

## Great Spirit IV — Duch Burzy
Role: Controller. Element: Storm.
- Primary — Iskra Burzy: fast electrical pulse.
- Active — Piorunujący Krąg: circular field that briefly interrupts or slows eligible targets.
- Passive — Znak Burzy: repeated ability hits build a temporary storm mark on a target.
- Ultimate — Gniew Niebios: controlled storm field with several clearly signaled pulses.
- Dodge — Błysk Burzy: quick directional shift with a visible arrival cue.
Gameplay loop: control space and create openings for coordinated team actions.

## Great Spirit V — Duch Żaru
Role: Striker. Element: Ember.
- Primary — Płomienny Impuls: short-range ember burst.
- Active — Płomień Feniksa: focused wave of fire energy with a brief follow-through effect.
- Passive — Rozżarzony Ślad: using an active ability briefly improves the next primary action.
- Ultimate — Feniks Odrodzenia: large rising ember field with a strong, readable area effect.
- Dodge — Żarliwy Skok: short leap with a fading ember trail.
Gameplay loop: chain short actions and use planned openings for high output.

## Great Spirit VI — Duch Świtu
Role: Ranger. Element: Dawn.
- Primary — Promień Świtu: precise light projectile.
- Active — Promień Przeznaczenia: focused beam that marks a target for the team.
- Passive — Oko Świtu: marked targets are easier for teammates to track.
- Ultimate — Korona Świtu: broad ray of light that reveals and marks targets within its area.
- Dodge — Świetlisty Krok: quick sidestep with a short luminous trace.
Gameplay loop: mark priority targets and support the team from a safe distance.

## Great Spirit VII — Duch Cienia
Role: Duelist. Element: Shadow.
- Primary — Cięcie Cienia: narrow shadow-energy arc.
- Active — Rozdarcie Cienia: fast reposition followed by a short-range shadow pulse.
- Passive — Echo Łowcy: a successful evade briefly improves the next active action.
- Ultimate — Wieczna Noc: controlled dim field that obscures hostile target indicators while keeping player-facing cues clear.
- Dodge — Cień Przemieszczenia: quick directional shift with a visible shadow echo.
Gameplay loop: reposition, evade and exploit short timing windows.

## Great Spirit VIII — Duch Życia
Role: Healer. Element: Life.
- Primary — Zielony Impuls: gentle life-energy pulse that can target an ally or an environmental object.
- Active — Ogród Odnowy: small healing area over time.
- Passive — Zielone Tchnienie: healing an ally slightly restores the healer's Energy.
- Ultimate — Ogród Wieczności: larger temporary recovery field for the party.
- Dodge — Zielony Zwrot: swift turn-step with a leaf-like visual trail.
Gameplay loop: position healing zones and sustain the team through longer encounters.

## Great Spirit IX — Duch Mrozu
Role: Sentinel. Element: Frost.
- Primary — Odłamek Mrozu: focused frost-energy projectile.
- Active — Lodowy Bastion: temporary barrier for the user or a nearby ally.
- Passive — Korona Zimy: maintaining a barrier gradually builds a small amount of Resonance.
- Ultimate — Zimowy Tron: clearly signaled frost field that slows eligible targets and protects allies within its inner area.
- Dodge — Lodowy Ślizg: quick glide with a short frost trail.
Gameplay loop: protect allies, control approach routes and create safe positions.

## Team composition guidance
- Guardian, Vanguard and Sentinel stabilize space.
- Support and Healer maintain team resources.
- Controller and Ranger create and exploit openings.
- Striker and Duelist convert openings into efficient actions.

No character should be mandatory for campaign completion. Every encounter needs alternatives for teams with different role combinations.

## Elemental reaction proposals — design only
Do not implement these as automatic damage multipliers until the base abilities are playable and tested.

| Combination | Working reaction | Intended gameplay effect |
|---|---|---|
| Root + Tide | Bloomwell | temporary recovery field with movement restriction around its edge |
| Stone + Storm | Shattercharge | briefly weakens a marked barrier |
| Ember + Frost | Thermal Shift | short area-control pulse |
| Dawn + Shadow | Eclipse | shared target mark visible to the party |
| Life + Tide | Renewal Current | slightly extends an ally recovery effect |
| Root + Storm | Tempest Grove | temporary control zone |
| Stone + Ember | Molten Seal | short-lived protective zone |
| Frost + Tide | Crystal Current | temporary party barrier |
| Dawn + Life | Radiant Renewal | improves the next healing effect in a limited window |

These reactions are optional extensions. Base ability activation and balance take priority.