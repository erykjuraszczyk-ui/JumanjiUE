# Progression content assets

The runtime progression catalog provides validated starter currencies and materials. Stage 15 adds an optional DataTable authoring path for chapter rewards and Great Spirit upgrade recipes.

## Create the folders and tables in Unreal Editor

Create:
- /Game/Progression/Data

Then create these DataTables:

| Asset | Row structure | Required asset path |
|---|---|---|
| DT_ChapterRewards | FJumanjiChapterRewardDefinition | /Game/Progression/Data/DT_ChapterRewards.DT_ChapterRewards |
| DT_SpiritUpgradeRecipes | FJumanjiSpiritUpgradeRecipe | /Game/Progression/Data/DT_SpiritUpgradeRecipes.DT_SpiritUpgradeRecipes |
| DT_TraversalRewards | FJumanjiTraversalRewardDefinition | /Game/Progression/Data/DT_TraversalRewards.DT_TraversalRewards |

The reward subsystem loads these paths during GameInstance subsystem initialization. If an asset is absent, the built-in validated starter catalog remains active.

## Chapter reward rows

Set the row's ChapterId and provide optional:
- AccountExperienceReward;
- ActiveCharacterExperienceReward;
- CurrencyRewards (positive deltas only);
- ItemRewards (positive deltas only).

The built-in sample reward is for EMERALD_JUNGLE_NINE_SEALS:
- 300 account XP;
- 180 XP for the currently selected Great Spirit;
- 150 CURRENCY_EXPLORER_COINS;
- 2 CURRENCY_BOARD_SEALS;
- 1 MAT_SPIRIT_MEMORY.

## Great Spirit upgrade recipe rows

Each row must identify:
- RecipeId;
- CharacterId;
- CharacterExperienceReward;
- positive CurrencyCosts;
- positive ItemCosts.

The recipe CharacterId must match the Great Spirit sent in the upgrade request. The server converts costs into negative profile transaction deltas.

Starter recipes are available for all nine spirits. Each costs 100 Explorer Coins and two element-specific materials and grants 120 character XP.

## Runtime authority

- Chapter completion is detected by the server-side GameState.
- Rewards are applied through UJumanjiProgressionRewardSubsystem and AJumanjiPlayerState::ApplyProfileTransaction.
- The PlayerState transaction ledger prevents the same chapter reward from being applied twice.
- Upgrade requests enter through AJumanjiPlayerController::RequestGreatSpiritUpgrade and a server RPC.
- UI only requests an upgrade and reads OnProgressionChanged. Never set wallet or inventory values from a widget.

## Notes

DataTables are optional overrides. Invalid rows are ignored and the native default rows remain in effect for those IDs. The local SaveGame profile is a single-server prototype, not a production cross-shard MMO economy.

See Docs/STAGE_15_PROGRESSION_GAMEPLAY_REWARDS.md for the full formula, sample values, validation rules and acceptance tests.


## Stage 26 — Traversal rewards

Import Data/Traversal/EmeraldJungle_TraversalRewards.csv as DT_TraversalRewards using FJumanjiTraversalRewardDefinition.

Row fields:
- TraversalId — must match the stable TraversalId on the placed AJumanjiTraversalLink;
- CurrencyId — currently allowlisted to CURRENCY_EXPLORER_COINS;
- CurrencyAmount — 1..5000;
- ItemId — optional; if supplied it must be a known material catalog ID;
- ItemAmount — zero when ItemId is empty, otherwise 1..9999.

The native fallback provides one reward for each of the 12 Stage 25 links. DT_TraversalRewards overrides a matching ID; missing table keeps the native catalog.

The server calls GrantTraversalCompletionReward only after FinishTraversal confirms a completed traversal. The receipt key is stable for profile + TraversalId, so repeating the link does not pay again. Cancelled/blocked/interrupted traversal has no reward.

Run:
    python Scripts/validate_emerald_jungle_traversal_rewards.py
    python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_traversal_rewards.py" -v

The CSV validator is not a substitute for DataTable import verification or a multiplayer PIE test. See Docs/STAGE_26_TRAVERSAL_QA_LIFT_RECOVERY_REWARDS.md.
