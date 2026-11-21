from __future__ import annotations

import copy
import json
import os
import threading
import unittest
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone
from pathlib import Path
from uuid import uuid4

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization

from Backend.EncounterCheckpointService.service import CheckpointServiceError, EncounterCheckpointBackend
from Scripts.migrate_encounter_checkpoint_store import migrate_sqlite_store

ROOT = Path(__file__).resolve().parents[2]
SERVER_ID = "srv-test-eu-01"
SHARD_ID = "eu-test-01"


@unittest.skipUnless(
    os.environ.get("JUMANJI_TEST_POSTGRES_URL"),
    "Set JUMANJI_TEST_POSTGRES_URL to run PostgreSQL provider integration tests",
)
class PostgreSQLCheckpointProviderTests(unittest.TestCase):
    def setUp(self) -> None:
        self.key = Ed25519PrivateKey.generate()
        self.database_url = os.environ["JUMANJI_TEST_POSTGRES_URL"]
        self.backend = EncounterCheckpointBackend(
            None,
            self.key,
            "POSTGRES-TEST-KEY-01",
            database_url=self.database_url,
            allow_insecure_database=os.environ.get("JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE") == "true",
        )
        self.identity = {"serverInstanceId": SERVER_ID, "shardId": SHARD_ID}
        self.encounter_id = f"ENCOUNTER_PG_{uuid4().hex[:12].upper()}"
        self.lease = self.backend.acquire_lease(
            self.encounter_id,
            {**self.identity, "ttlSeconds": 45},
        )

    def make_payload(self, sequence: int) -> dict:
        fixture_path = ROOT / "Data/Encounter/EncounterCheckpointExample.json"
        payload = copy.deepcopy(json.loads(fixture_path.read_text(encoding="utf-8"))["payload"])
        now = datetime.now(timezone.utc).replace(microsecond=0)
        payload["checkpointId"] = str(uuid4())
        payload["sessionId"] = str(uuid4())
        payload["encounterId"] = self.encounter_id
        payload["checkpointSequence"] = sequence
        payload["createdAtUtc"] = now.isoformat().replace("+00:00", "Z")
        payload["expiresAtUtc"] = (now + timedelta(minutes=20)).isoformat().replace("+00:00", "Z")
        payload["authority"]["serverInstanceId"] = SERVER_ID
        payload["authority"]["shardId"] = SHARD_ID
        payload["authority"]["authorityEpoch"] = self.lease["authorityEpoch"]
        return payload

    def make_request(self, payload: dict, expected_sequence: int) -> dict:
        return {
            **self.identity,
            "leaseToken": self.lease["leaseToken"],
            "authorityEpoch": self.lease["authorityEpoch"],
            "expectedSequence": expected_sequence,
            "payload": payload,
        }

    def test_postgres_persists_signed_checkpoint_and_cas(self) -> None:
        payload = self.make_payload(1)
        receipt = self.backend.write_checkpoint(
            self.encounter_id,
            self.make_request(payload, 0),
        )
        self.assertEqual(receipt["checkpointSequence"], 1)
        response = self.backend.read_checkpoint(
            self.encounter_id,
            {
                **self.identity,
                "leaseToken": self.lease["leaseToken"],
                "authorityEpoch": self.lease["authorityEpoch"],
            },
        )
        self.assertEqual(response["envelope"]["payload"]["checkpointId"], payload["checkpointId"])

        with self.assertRaises(CheckpointServiceError) as conflict:
            self.backend.write_checkpoint(
                self.encounter_id,
                self.make_request(self.make_payload(1), 0),
            )
        self.assertEqual(conflict.exception.code, "SEQUENCE_CONFLICT")

    def test_postgres_fences_old_lease_after_reacquire(self) -> None:
        old = self.lease
        self.backend.release_lease(
            self.encounter_id,
            {
                **self.identity,
                "leaseToken": old["leaseToken"],
                "authorityEpoch": old["authorityEpoch"],
            },
        )
        newer = self.backend.acquire_lease(
            self.encounter_id,
            {**self.identity, "ttlSeconds": 45},
        )
        self.assertEqual(newer["authorityEpoch"], old["authorityEpoch"] + 1)
        with self.assertRaises(CheckpointServiceError) as stale:
            self.backend.read_checkpoint(
                self.encounter_id,
                {
                    **self.identity,
                    "leaseToken": old["leaseToken"],
                    "authorityEpoch": old["authorityEpoch"],
                },
            )
        self.assertEqual(stale.exception.code, "LEASE_INVALID")

    def test_sqlite_migration_carries_signed_checkpoint_but_not_lease(self) -> None:
        import tempfile

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            old_key = Ed25519PrivateKey.generate()
            source_path = root / "legacy.sqlite3"
            source = EncounterCheckpointBackend(source_path, old_key, "LEGACY-KEY-01")
            migration_encounter = f"ENCOUNTER_MIG_{uuid4().hex[:10].upper()}"
            old_identity = {"serverInstanceId": SERVER_ID, "shardId": SHARD_ID}
            old_lease = source.acquire_lease(
                migration_encounter,
                {**old_identity, "ttlSeconds": 45},
            )
            fixture = json.loads(
                (ROOT / "Data/Encounter/EncounterCheckpointExample.json").read_text(encoding="utf-8")
            )["payload"]
            payload = copy.deepcopy(fixture)
            now = datetime.now(timezone.utc).replace(microsecond=0)
            payload["encounterId"] = migration_encounter
            payload["checkpointId"] = str(uuid4())
            payload["sessionId"] = str(uuid4())
            payload["checkpointSequence"] = 1
            payload["createdAtUtc"] = now.isoformat().replace("+00:00", "Z")
            payload["expiresAtUtc"] = (now + timedelta(minutes=20)).isoformat().replace("+00:00", "Z")
            payload["authority"]["serverInstanceId"] = SERVER_ID
            payload["authority"]["shardId"] = SHARD_ID
            payload["authority"]["authorityEpoch"] = old_lease["authorityEpoch"]
            source.write_checkpoint(
                migration_encounter,
                {
                    **old_identity,
                    "leaseToken": old_lease["leaseToken"],
                    "authorityEpoch": old_lease["authorityEpoch"],
                    "expectedSequence": 0,
                    "payload": payload,
                },
            )

            old_public_path = root / "legacy-public-key.pem"
            old_public_path.write_bytes(
                old_key.public_key().public_bytes(
                    encoding=serialization.Encoding.PEM,
                    format=serialization.PublicFormat.SubjectPublicKeyInfo,
                )
            )
            keyring_path = root / "keyring.json"
            keyring_path.write_text(
                json.dumps({
                    "keys": [{
                        "keyId": "LEGACY-KEY-01",
                        "publicKeyFile": str(old_public_path),
                    }]
                }),
                encoding="utf-8",
            )
            target = EncounterCheckpointBackend(
                None,
                Ed25519PrivateKey.generate(),
                "POSTGRES-MIGRATED-KEY",
                database_url=self.database_url,
                verification_keyring_path=keyring_path,
                allow_insecure_database=os.environ.get("JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE") == "true",
            )

            report = migrate_sqlite_store(source_path, target)
            self.assertEqual(report["imported"], 1)
            self.assertEqual(report["activeLeasesTransferred"], 0)

            new_lease = target.acquire_lease(
                migration_encounter,
                {**self.identity, "ttlSeconds": 45},
            )
            self.assertEqual(new_lease["authorityEpoch"], old_lease["authorityEpoch"] + 1)
            restored = target.read_checkpoint(
                migration_encounter,
                {
                    **self.identity,
                    "leaseToken": new_lease["leaseToken"],
                    "authorityEpoch": new_lease["authorityEpoch"],
                },
            )
            self.assertEqual(restored["envelope"]["integrity"]["keyId"], "LEGACY-KEY-01")
            self.assertEqual(restored["envelope"]["payload"]["checkpointId"], payload["checkpointId"])

    def test_postgres_concurrent_acquire_has_one_owner(self) -> None:
        other = EncounterCheckpointBackend(
            None,
            self.key,
            "POSTGRES-TEST-KEY-01",
            database_url=self.database_url,
            allow_insecure_database=os.environ.get("JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE") == "true",
        )
        target = f"ENCOUNTER_PG_RACE_{uuid4().hex[:10].upper()}"
        barrier = threading.Barrier(2)

        def acquire(backend: EncounterCheckpointBackend, server_id: str) -> str:
            barrier.wait()
            try:
                backend.acquire_lease(
                    target,
                    {"serverInstanceId": server_id, "shardId": SHARD_ID, "ttlSeconds": 45},
                )
                return "ACQUIRED"
            except CheckpointServiceError as error:
                return error.code

        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(
                lambda entry: acquire(entry[0], entry[1]),
                [(self.backend, "srv-pg-alpha"), (other, "srv-pg-beta")],
            ))
        self.assertEqual(results.count("ACQUIRED"), 1)
        self.assertEqual(results.count("LEASE_HELD"), 1)


if __name__ == "__main__":
    unittest.main()
