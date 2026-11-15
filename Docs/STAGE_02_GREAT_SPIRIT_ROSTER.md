# JUMANJI 2.111.1 — Stage 02: Great Spirit Roster

## Purpose

Implement the first playable-character catalog for the nine Great Spirits shown in the supplied reference image.

The image defines the nine-character count, Great Spirit I–IX numbering, portrait grid (I–V on the first row, VI–IX on the second), and distinct visual identities. It does not define names, gameplay roles, elements, statistics or abilities. Those are original design proposals introduced in this stage.

## The nine playable characters

All nine are selectable in the development roster. Great Spirit I is the default selection.

| # | Character ID | Working name | Element | Role | Signature ability | Passive concept |
|---:|---|---|---|---|---|---|
| I | GREAT_SPIRIT_I | Duch Korzeni | Root | Guardian | Korzenna Fala | Zew Korzeni |
| II | GREAT_SPIRIT_II | Duch Przypływu | Tide | Support | Prąd Ukojenia | Taniec Fal |
| III | GREAT_SPIRIT_III | Duch Kamienia | Stone | Vanguard | Tarcza Pradawnych | Serce Skały |
| IV | GREAT_SPIRIT_IV | Duch Burzy | Storm | Controller | Piorunujący Krąg | Znak Burzy |
| V | GREAT_SPIRIT_V | Duch Żaru | Ember | Striker | Płomień Feniksa | Rozżarzony Ślad |
| VI | GREAT_SPIRIT_VI | Duch Świtu | Dawn | Ranger | Promień Przeznaczenia | Oko Świtu |
| VII | GREAT_SPIRIT_VII | Duch Cienia | Shadow | Duelist | Rozdarcie Cienia | Echo Łowcy |
| VIII | GREAT_SPIRIT_VIII | Duch Życia | Life | Healer | Ogród Odnowy | Zielone Tchnienie |
| IX | GREAT_SPIRIT_IX | Duch Mrozu | Frost | Sentinel | Lodowy Bastion | Korona Zimy |

## Art direction derived from the reference

Visual observations, not claims about hidden lore:

- I: cool blue/cyan markings, blue focal jewel, dark leafy crown.
- II: turquoise/cyan ornamentation and geometric forehead jewel.
- III: ochre/gold and earth-brown palette, textured markings and braided details.
- IV: emerald/cyan palette and luminous green forehead motif.
- V: vivid red/orange markings and warm feathered crown.
- VI: blue/cyan face paint with ochre/orange feathers and ornaments.
- VII: deep red, ember orange and dark charcoal contrast.
- VIII: emerald/green palette and botanical detailing.
- IX: icy blue face paint, white feathered crown and gold accents.

Preserve each character's color identity, face-marking motif and head silhouette in final 3D art. The reference is a concept image, not a production-ready texture sheet or 3D model.

## Runtime implementation

### Character definition

FJumanjiCharacterDefinition contains:
- stable CharacterId and SpiritNumber;
- DisplayName, Title and Description;
- Element and Role;
- SignatureAbilityName and PassiveAbilityName;
- primary and secondary presentation colors;
- optional soft references for portrait texture, skeletal mesh and Gameplay Ability classes.

The soft references are empty in this initial catalog. Assign real art and ability assets after they exist.

### Roster subsystem

UJumanjiCharacterRosterSubsystem is a GameInstance subsystem. It builds the nine starter definitions during initialization and provides:
- GetRoster() for C++;
- GetRosterForBlueprint(OutRoster) for UMG/Blueprint;
- GetCharacterDefinition(CharacterId, OutDefinition);
- IsPlayableCharacter(CharacterId);
- GetDefaultCharacterId().

The initial list is implemented in C++ so selection can be integrated before binary Unreal Data Assets exist. A future content-authoring task can migrate the entries into Primary Data Assets while preserving stable IDs.

### Server-validated selection

AJumanjiPlayerController::RequestSelectCharacter(CharacterId) is the client-facing selection entry point.

The server checks the ID against the roster before setting AJumanjiPlayerState::SelectedCharacterId. The selected ID is replicated from PlayerState and exposed through OnSelectedCharacterChanged.

This stage stores selection for the current network session only. Account persistence, unlock progression, character spawning/mesh switching and lobby-only selection restrictions remain future work. All nine are available in the development roster; no paid random unlock mechanic is planned.

## UI integration contract

The future WBP_GreatSpiritSelection screen should:

1. Read the nine definitions from UJumanjiCharacterRosterSubsystem.
2. Show nine numbered cards in the same order as the reference.
3. Display portrait, name, role, element and ability names.
4. Mark the current character using AJumanjiPlayerState::GetSelectedCharacterId().
5. Call RequestSelectCharacter(CharacterId) after the player confirms a choice.
6. Listen to OnSelectedCharacterChanged to refresh the selection state.

The widget requests selection; it must not own gameplay authority. The server validates the requested ID.

## Asset production contract

Create and import these Unreal assets after the project compiles:

- Content/Characters/GreatSpirits/Portraits/T_GreatSpirit_I_Portrait through T_GreatSpirit_IX_Portrait;
- Content/Characters/GreatSpirits/Meshes/SK_GreatSpirit_I through SK_GreatSpirit_IX;
- a shared compatible skeleton and animation set where practical;
- optional per-character material instances and Niagara effects.

The repository text-file workflow used in this stage does not add the supplied binary reference image or generated Unreal assets. Keep the original image and subsequent art exports in Git LFS after it is enabled, and record provenance and license information.

## Acceptance criteria

- [ ] The roster subsystem returns exactly nine definitions.
- [ ] All CharacterId values are unique and stable.
- [ ] Every entry has a non-empty display name, title and description.
- [ ] Every entry has an element, role, signature and passive concept.
- [ ] Great Spirit I is the default selection.
- [ ] A valid selection updates PlayerState on the server.
- [ ] SelectedCharacterId replicates to clients.
- [ ] An unknown CharacterId is rejected by the server.
- [ ] The roster is accessible to a future UMG selection screen.

## Out of scope

- finished 3D character models and portraits;
- skeletons, rigs, animation blueprints and facial animation;
- fully implemented GAS abilities;
- progression, unlocks and persistent account storage;
- final character-selection HUD and live mesh switching.
