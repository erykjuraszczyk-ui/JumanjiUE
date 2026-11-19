from __future__ import annotations

import copy
import json
import sys
import unittest
from datetime import datetime, timezone
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_encounter_checkpoint_contract as MODULE


ROOT = Path(__file__).resolve().parents[2]


class EncounterCheckpointContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.example = json.loads((ROOT / MODULE.EXAMPLE_PATH).read_text(encoding="utf-8"))
        cls.schema = json.loads((ROOT / MODULE.SCHEMA_PATH).read_text(encoding="utf-8"))

    def test_schema_declares_strict_draft_2020_12_contract(self) -> None:
        self.assertEqual(MODULE.validate_schema(self.schema), [])

    def test_example_fixture_is_valid(self) -> None:
        self.assertEqual(MODULE.validate_envelope(self.example), [])

    def test_unsupported_schema_version_is_rejected(self) -> None:
        candidate = copy.deepcopy(self.example)
        candidate["payload"]["schemaVersion"] = 99
        self.assertTrue(any("schemaVersion" in error for error in MODULE.validate_envelope(candidate)))

    def test_phase_and_safe_point_must_match(self) -> None:
        candidate = copy.deepcopy(self.example)
        candidate["payload"]["safePoint"]["kind"] = "WaveBoundary"
        self.assertTrue(any("does not match phase Boss" in error for error in MODULE.validate_envelope(candidate)))

    def test_boss_checkpoint_requires_logical_boss_state(self) -> None:
        candidate = copy.deepcopy(self.example)
        del candidate["payload"]["safePoint"]["combat"]["boss"]
        self.assertTrue(any("requires a bounded logical boss snapshot" in error for error in MODULE.validate_envelope(candidate)))

    def test_reward_phase_requires_stable_transaction(self) -> None:
        candidate = copy.deepcopy(self.example)
        safe = candidate["payload"]["safePoint"]
        safe["phase"] = "Reward"
        safe["kind"] = "RewardPending"
        safe["combat"].pop("boss", None)
        candidate["payload"].pop("reward", None)
        self.assertTrue(any("requires rewardDefinitionId" in error for error in MODULE.validate_envelope(candidate)))

    def test_duplicate_participant_keys_are_rejected(self) -> None:
        candidate = copy.deepcopy(self.example)
        candidate["payload"]["participants"][1]["participantKey"] = candidate["payload"]["participants"][0]["participantKey"]
        self.assertTrue(any("duplicate participantKey" in error for error in MODULE.validate_envelope(candidate)))

    def test_actor_or_raw_profile_fields_are_rejected(self) -> None:
        candidate = copy.deepcopy(self.example)
        candidate["payload"]["runtimeActorRef"] = "NetGUID:123"
        errors = MODULE.validate_envelope(candidate)
        self.assertTrue(any("unsupported fields" in error or "forbidden field" in error for error in errors))

        candidate = copy.deepcopy(self.example)
        candidate["payload"]["profileId"] = "raw-profile-id"
        errors = MODULE.validate_envelope(candidate)
        self.assertTrue(any("forbidden field" in error for error in errors))

    def test_expiry_cannot_exceed_24_hours_or_be_in_the_past(self) -> None:
        candidate = copy.deepcopy(self.example)
        candidate["payload"]["expiresAtUtc"] = "2026-01-03T12:00:00Z"
        self.assertTrue(any("no longer than 24 hours" in error for error in MODULE.validate_envelope(candidate)))

        now = datetime(2026, 1, 2, tzinfo=timezone.utc)
        self.assertTrue(any("has expired" in error for error in MODULE.validate_envelope(self.example, now=now)))

    def test_recovery_matrix_contains_required_cases(self) -> None:
        self.assertEqual(MODULE.validate_matrix(ROOT / MODULE.MATRIX_PATH), [])


if __name__ == "__main__":
    unittest.main()
