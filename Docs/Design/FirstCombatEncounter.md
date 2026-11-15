# First Combat Encounter — JUMANJI 2.111.1

## Encounter: Strażnik Planszy

**Type:** first region boss prototype.
**Region:** Szmaragdowa Knieja.
**Role:** guardian of the first Living Board marker.
**Base class:** AJumanjiBossCharacter.
**Prototype defaults:** native boss attribute initialization and basic melee ability are assigned in C++; custom Blueprint assets may override them.

## Intended fight structure

### Phase 1 — Straż Pradawnych (100–66%)
- Boss uses a slow frontal, clearly signaled short-range strike (native prototype ability; replace with a custom phase-aware Blueprint ability for production).
- AI pursues the player and requests its configured attack when in range, paced by a configurable 1.0-second prototype interval.
- The attack has a visible wind-up and a cooldown.
- Player learns to dodge and use one primary ability.

### Phase 2 — Przebudzenie Kniei (66–33%)
- BP_OnBossPhaseChanged receives phase 2.
- Blueprint may add a short root-field attack and a second attack cue.
- Boss movement speed and timing can change, but the hit signals must remain readable.
- The phase tag State.Boss.Phase.Two is available for phase-aware abilities.

### Phase 3 — Serce Planszy (33–0%)
- BP_OnBossPhaseChanged receives phase 3.
- Blueprint may add an area sweep or a short sequence with safe positioning zones.
- Do not increase damage without a clear wind-up and an escape route.
- State.Boss.Phase.Three is available for phase-aware abilities.

## Victory and rewards
- Server detects boss Health <= 0.
- Boss enters defeated state and replicates it.
- Reward grant must be performed by a server-authoritative reward service or server-side encounter script.
- Do not award inventory items directly from a client-side Blueprint event.

## Accessibility and mobile readability
- Use clear telegraphs and avoid full-screen particle coverage.
- Provide equivalent visual cues for important attack audio.
- Allow effect intensity reduction.
- Keep the encounter readable at 30 FPS on the lower supported device tier.

## Not included yet
- scripted boss attack ability assets;
- boss model, rig, animation montages and Niagara effects;
- encounter arena map and props;
- server-authoritative reward table implementation;
- final balance and multiplayer playtest.