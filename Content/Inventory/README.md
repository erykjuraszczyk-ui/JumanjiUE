# Inventory & Progression content

Stage 14 introduces a source-controlled prototype catalog in
UJumanjiProgressionCatalogSubsystem.

## Currency IDs

- CURRENCY_EXPLORER_COINS — Monety Odkrywców
- CURRENCY_BOARD_SEALS — Pieczęcie Planszy
- CURRENCY_ECHO_CRYSTALS — Kryształy Echa

## Upgrade material IDs

- MAT_ROOT_RESIN
- MAT_TIDE_PEARL
- MAT_STONE_CORE
- MAT_STORM_ESSENCE
- MAT_EMBER_ASH
- MAT_DAWN_SHARD
- MAT_SHADOW_SILK
- MAT_LIFE_BLOOM
- MAT_FROST_CRYSTAL
- MAT_SPIRIT_MEMORY

All current material stacks have a maximum quantity of 9999.

## Ability slot IDs

- SLOT_PRIMARY
- SLOT_ACTIVE
- SLOT_PASSIVE
- SLOT_ULTIMATE
- SLOT_DODGE

The profile stores stable AbilityId values by character and slot. This stage does not resolve those values to runtime Gameplay Ability classes; that integration belongs to Stage 15.

## Reward transactions

Trusted server gameplay code should create a stable FJumanjiProfileTransaction and call
AJumanjiPlayerState::ApplyProfileTransaction.

Do not call this from untrusted client-provided reward data. The method validates the entire transaction on copies and commits only if every currency, item and XP change is valid. Duplicate TransactionId values are rejected.

## Asset workflow

The current catalog is implemented in C++ and needs no DataTable asset to run. Later, designers may migrate the catalog definitions to validated DataTables or Primary Data Assets while preserving IDs.

No inventory icon textures, item meshes, shop widgets or reward Widget Blueprints are included yet.
