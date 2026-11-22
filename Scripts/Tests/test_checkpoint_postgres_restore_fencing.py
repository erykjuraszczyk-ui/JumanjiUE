from __future__ import annotations

import copy
import json
import os
import sys
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from uuid import uuid4

import psycopg
from psycopg.rows import dict_row
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from Backend.EncounterCheckpointService.service import CheckpointServiceError, EncounterCheckpointBackend
from Scripts.fence_checkpoint_leases_after_restore import FenceError, fence_restored_target


@unittest.skipUnless(
    os.environ.get("JUMANJI_TEST_POSTGRES_URL"),
    "Set JUMANJI_TEST_POSTGRES_URL to run PostgreSQL restore-fencing integration tests",
)
class PostgreSQLRestoreFencingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.database_url = os.environ["JUMANJI_TEST_POSTGRES_URL"]
        self.server_id = "srv-stage42-test"
        self.shard_id = "eu-stage42-test"
        self.encounter_id = f"ENCOUNTER_DR42_{uuid4().hex[:14].upper()}"
        self.key = Ed25519PrivateKey.generate()
        self.backend = EncounterCheckpointBackend(
            None,
            self.key,
            "STAGE42-RESTORE-TEST-KEY",
            database_url=self.database_url,
            allow_insecure_database=os.environ.get("JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE") == "true",
        )
        self.lease = self.backend.acquire_lease(
            self.encounter_id,
            {
                "serverInstanceId": self.server_id,
                "shardId": self.shard_id,
                "ttlSeconds": 90,
            },
        )
        fixture_path = ROOT / "Data/Encounter/EncounterCheckpointExample.json"
        payload = copy.deepcopy(json.loads(fixture_path.read_text(encoding="utf-8"))["payload"])
        now = datetime.now(timezone.utc).replace(microsecond=0)
        payload["checkpointId"] = str(uuid4())
        payload["sessionId"] = str(uuid4())
        payload["encounterId"] = self.encounter_id
        payload["checkpointSequence"] = 1
        payload["createdAtUtc"] = now.isoformat().replace("+00:00", "Z")
        payload["expiresAtUtc"] = (now + timedelta(minutes=20)).isoformat().replace("+00:00", "Z")
        payload["authority"]["serverInstanceId"] = self.server_id
        payload["authority"]["shardId"] = self.shard_id
        payload["authority"]["authorityEpoch"] = self.lease["authorityEpoch"]
        self.payload = payload
        self.write = self.backend.write_checkpoint(
            self.encounter_id,
            {
                "serverInstanceId": self.server_id,
                "shardId": self.shard_id,
                "leaseToken": self.lease["leaseToken"],
                "authorityEpoch": self.lease["authorityEpoch"],
                "expectedSequence": 0,
                "payload": payload,
            },
        )

    def _connection(self):
        return psycopg.connect(self.database_url, autocommit=True, row_factory=dict_row)

    def _read(self, lease: dict):
        return self.backend.read_checkpoint(
            self.encounter_id,
            {
                "serverInstanceId": self.server_id,
                "shardId": self.shard_id,
                "leaseToken": lease["leaseToken"],
                "authorityEpoch": lease["authorityEpoch"],
            },
        )

    def test_dry_run_does_not_change_lease_or_checkpoint(self) -> None:
        with self._connection() as connection:
            report = fence_restored_target(connection, apply=False)

        self.assertEqual(report["mode"], "dry-run")
        self.assertEqual(report["rowsFenced"], 0)
        self.assertGreaterEqual(report["rowCount"], 1)

        stored = self._read(self.lease)
        self.assertEqual(stored["checkpointSequence"], 1)
        self.assertEqual(stored["envelope"]["payload"]["checkpointId"], self.payload["checkpointId"])

    def test_apply_invalidates_old_lease_and_preserves_signed_checkpoint(self) -> None:
        old_epoch = self.lease["authorityEpoch"]

        with self._connection() as connection:
            report = fence_restored_target(connection, apply=True)

        self.assertEqual(report["mode"], "apply")
        self.assertEqual(report["rowsFenced"], report["rowCount"])
        self.assertGreaterEqual(report["leaseOwnerRowsInvalidated"], 1)
        self.assertEqual(report["checkpointRowsPreserved"], report["rowCount"])

        with self.assertRaises(CheckpointServiceError) as stale:
            self._read(self.lease)
        self.assertEqual(stale.exception.code, "LEASE_INVALID")

        new_lease = self.backend.acquire_lease(
            self.encounter_id,
            {
                "serverInstanceId": self.server_id,
                "shardId": self.shard_id,
                "ttlSeconds": 90,
            },
        )
        self.assertEqual(new_lease["authorityEpoch"], old_epoch + 2)

        restored = self._read(new_lease)
        self.assertEqual(restored["checkpointSequence"], 1)
        self.assertEqual(restored["envelope"]["payload"]["checkpointId"], self.payload["checkpointId"])
        self.assertEqual(restored["envelope"]["integrity"]["keyId"], "STAGE42-RESTORE-TEST-KEY")

        self.backend.release_lease(
            self.encounter_id,
            {
                "serverInstanceId": self.server_id,
                "shardId": self.shard_id,
                "leaseToken": new_lease["leaseToken"],
                "authorityEpoch": new_lease["authorityEpoch"],
            },
        )

    def test_authority_epoch_overflow_aborts_without_partial_fencing(self) -> None:
        with self._connection() as connection:
            connection.execute(
                "UPDATE encounter_checkpoint_state SET authority_epoch = %s WHERE encounter_id = %s",
                (9_223_372_036_854_775_807, self.encounter_id),
            )
            try:
                with self.assertRaises(FenceError) as overflow:
                    fence_restored_target(connection, apply=True)
                self.assertEqual(str(overflow.exception), "AUTHORITY_EPOCH_OVERFLOW")
                row = connection.execute(
                    "SELECT authority_epoch, checkpoint_sequence FROM encounter_checkpoint_state WHERE encounter_id = %s",
                    (self.encounter_id,),
                ).fetchone()
                self.assertEqual(row["authority_epoch"], 9_223_372_036_854_775_807)
                self.assertEqual(row["checkpoint_sequence"], 1)
            finally:
                connection.execute(
                    "DELETE FROM encounter_checkpoint_state WHERE encounter_id = %s",
                    (self.encounter_id,),
                )


if __name__ == "__main__":
    unittest.main()
