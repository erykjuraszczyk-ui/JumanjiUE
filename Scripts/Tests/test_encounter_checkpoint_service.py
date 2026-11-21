from __future__ import annotations

import copy
import json
import sys
import tempfile
import threading
import unittest
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError
import os

from urllib.request import Request, urlopen
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from Backend.EncounterCheckpointService.service import (
    CheckpointServiceError,
    EncounterCheckpointBackend,
    make_handler,
)


ENCOUNTER_ID = "ENCOUNTER_EMERALD_JUNGLE_CITADEL"
SERVER_ID = "srv-test-eu-01"
SHARD_ID = "eu-test-01"


class EncounterCheckpointBackendTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.key = Ed25519PrivateKey.generate()
        self.backend = EncounterCheckpointBackend(
            Path(self.temp_dir.name) / "checkpoints.sqlite3",
            self.key,
            "UNIT-TEST-KEY-01",
        )
        self.identity = {"serverInstanceId": SERVER_ID, "shardId": SHARD_ID}
        self.lease_request = {**self.identity, "ttlSeconds": 45}
        self.lease = self.backend.acquire_lease(ENCOUNTER_ID, self.lease_request)

    def make_payload(self, sequence: int = 1) -> dict:
        fixture_path = ROOT / "Data/Encounter/EncounterCheckpointExample.json"
        payload = copy.deepcopy(json.loads(fixture_path.read_text(encoding="utf-8"))["payload"])
        now = datetime.now(timezone.utc).replace(microsecond=0)
        payload["checkpointId"] = str(uuid4())
        payload["sessionId"] = str(uuid4())
        payload["checkpointSequence"] = sequence
        payload["createdAtUtc"] = now.isoformat().replace("+00:00", "Z")
        payload["expiresAtUtc"] = (now + timedelta(minutes=20)).isoformat().replace("+00:00", "Z")
        payload["authority"]["serverInstanceId"] = SERVER_ID
        payload["authority"]["shardId"] = SHARD_ID
        payload["authority"]["authorityEpoch"] = self.lease["authorityEpoch"]
        return payload

    def make_write_request(self, payload: dict, expected_sequence: int = 0) -> dict:
        return {
            **self.identity,
            "leaseToken": self.lease["leaseToken"],
            "authorityEpoch": self.lease["authorityEpoch"],
            "expectedSequence": expected_sequence,
            "payload": payload,
        }

    def make_read_request(self) -> dict:
        return {
            **self.identity,
            "leaseToken": self.lease["leaseToken"],
            "authorityEpoch": self.lease["authorityEpoch"],
        }

    def test_checkpoint_is_signed_and_verified_on_read(self) -> None:
        payload = self.make_payload()
        written = self.backend.write_checkpoint(ENCOUNTER_ID, self.make_write_request(payload))
        self.assertEqual(written["checkpointSequence"], 1)
        self.assertEqual(written["authorityEpoch"], self.lease["authorityEpoch"])

        result = self.backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request())
        self.assertEqual(result["envelope"]["payload"]["checkpointId"], payload["checkpointId"])
        self.assertEqual(result["envelope"]["integrity"]["algorithm"], "Ed25519")
        self.assertEqual(result["envelope"]["integrity"]["keyId"], "UNIT-TEST-KEY-01")

    def test_compare_and_swap_rejects_stale_sequence(self) -> None:
        self.backend.write_checkpoint(ENCOUNTER_ID, self.make_write_request(self.make_payload(), 0))
        stale_payload = self.make_payload(sequence=1)
        with self.assertRaises(CheckpointServiceError) as error:
            self.backend.write_checkpoint(ENCOUNTER_ID, self.make_write_request(stale_payload, 0))
        self.assertEqual(error.exception.code, "SEQUENCE_CONFLICT")

    def test_payload_cannot_claim_another_fencing_epoch(self) -> None:
        payload = self.make_payload()
        payload["authority"]["authorityEpoch"] += 1
        with self.assertRaises(CheckpointServiceError) as error:
            self.backend.write_checkpoint(ENCOUNTER_ID, self.make_write_request(payload))
        self.assertEqual(error.exception.code, "AUTHORITY_MISMATCH")

    def test_reacquire_increments_epoch_and_fences_old_server(self) -> None:
        old_lease = self.lease
        self.backend.release_lease(
            ENCOUNTER_ID,
            {**self.identity, "leaseToken": old_lease["leaseToken"], "authorityEpoch": old_lease["authorityEpoch"]},
        )
        new_lease = self.backend.acquire_lease(ENCOUNTER_ID, self.lease_request)
        self.assertEqual(new_lease["authorityEpoch"], old_lease["authorityEpoch"] + 1)

        old_request = {
            **self.identity,
            "leaseToken": old_lease["leaseToken"],
            "authorityEpoch": old_lease["authorityEpoch"],
            "expectedSequence": 0,
            "payload": self.make_payload(),
        }
        with self.assertRaises(CheckpointServiceError) as error:
            self.backend.write_checkpoint(ENCOUNTER_ID, old_request)
        self.assertEqual(error.exception.code, "LEASE_INVALID")

    def test_only_one_concurrent_server_can_acquire_lease(self) -> None:
        other_backend = EncounterCheckpointBackend(
            Path(self.temp_dir.name) / "checkpoints.sqlite3",
            self.key,
            "UNIT-TEST-KEY-01",
        )
        barrier = threading.Barrier(2)

        def attempt(service: EncounterCheckpointBackend, server_id: str) -> str:
            barrier.wait()
            try:
                service.acquire_lease(
                    "ENCOUNTER_CONCURRENCY_TEST",
                    {"serverInstanceId": server_id, "shardId": SHARD_ID, "ttlSeconds": 45},
                )
                return "ACQUIRED"
            except CheckpointServiceError as error:
                return error.code

        with ThreadPoolExecutor(max_workers=2) as pool:
            outcomes = list(pool.map(
                lambda item: attempt(item[0], item[1]),
                [(self.backend, "srv-alpha-01"), (other_backend, "srv-beta-01")],
            ))
        self.assertEqual(outcomes.count("ACQUIRED"), 1)
        self.assertEqual(outcomes.count("LEASE_HELD"), 1)

    def test_modified_stored_payload_fails_signature_verification(self) -> None:
        payload = self.make_payload()
        self.backend.write_checkpoint(ENCOUNTER_ID, self.make_write_request(payload))

        import sqlite3
        connection = sqlite3.connect(self.backend.database_path)
        stored = json.loads(connection.execute(
            "SELECT checkpoint_envelope_json FROM encounter_checkpoint_state WHERE encounter_id = ?",
            (ENCOUNTER_ID,),
        ).fetchone()[0])
        stored["payload"]["deterministic"]["encounterSeed"] += 1
        connection.execute(
            "UPDATE encounter_checkpoint_state SET checkpoint_envelope_json = ? WHERE encounter_id = ?",
            (json.dumps(stored), ENCOUNTER_ID),
        )
        connection.commit()
        connection.close()

        with self.assertRaises(CheckpointServiceError) as error:
            self.backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request())
        self.assertEqual(error.exception.code, "SIGNATURE_INVALID")

    def test_http_api_rejects_missing_server_authentication(self) -> None:
        handler = make_handler(self.backend, "unit-test-server-token-with-at-least-32-characters")
        server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(thread.join)
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)

        url = f"http://127.0.0.1:{server.server_port}/v1/encounters/ENCOUNTER_HTTP_TEST/leases"
        request = Request(
            url,
            data=json.dumps({**self.identity, "ttlSeconds": 30}).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with self.assertRaises(HTTPError) as error:
            urlopen(request, timeout=3)
        self.assertEqual(error.exception.code, 401)

    def test_signing_key_rotation_keeps_old_checkpoint_readable(self) -> None:
        from cryptography.hazmat.primitives import serialization

        original_payload = self.make_payload()
        self.backend.write_checkpoint(
            ENCOUNTER_ID,
            self.make_write_request(original_payload, 0),
        )

        replacement_key = Ed25519PrivateKey.generate()
        old_public_path = Path(self.temp_dir.name) / "old-public-key.pem"
        old_public_path.write_bytes(
            self.key.public_key().public_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PublicFormat.SubjectPublicKeyInfo,
            )
        )
        keyring_path = Path(self.temp_dir.name) / "verify-keyring.json"
        keyring_path.write_text(
            json.dumps({
                "keys": [{
                    "keyId": "UNIT-TEST-KEY-01",
                    "publicKeyFile": str(old_public_path),
                }]
            }),
            encoding="utf-8",
        )

        rotated_backend = EncounterCheckpointBackend(
            self.backend.database_path,
            replacement_key,
            "UNIT-TEST-KEY-02",
            verification_keyring_path=keyring_path,
        )
        old_read = rotated_backend.read_checkpoint(
            ENCOUNTER_ID,
            self.make_read_request(),
        )
        self.assertEqual(old_read["envelope"]["integrity"]["keyId"], "UNIT-TEST-KEY-01")

        next_payload = self.make_payload(sequence=2)
        rotated_backend.write_checkpoint(
            ENCOUNTER_ID,
            self.make_write_request(next_payload, expected_sequence=1),
        )
        new_read = rotated_backend.read_checkpoint(
            ENCOUNTER_ID,
            self.make_read_request(),
        )
        self.assertEqual(new_read["envelope"]["integrity"]["keyId"], "UNIT-TEST-KEY-02")

    def test_storage_readiness_query_succeeds(self) -> None:
        self.backend.check_ready()

    def test_integral_json_numbers_from_unreal_are_accepted(self) -> None:
        acquired = self.backend.acquire_lease(
            "ENCOUNTER_JSON_NUMBER_TTL",
            {**self.identity, "ttlSeconds": 45.0},
        )
        self.assertGreaterEqual(acquired["authorityEpoch"], 1)

        write_request = self.make_write_request(self.make_payload())
        write_request["authorityEpoch"] = float(self.lease["authorityEpoch"])
        write_request["expectedSequence"] = 0.0
        written = self.backend.write_checkpoint(ENCOUNTER_ID, write_request)
        self.assertEqual(written["checkpointSequence"], 1)

        renew_request = {
            **self.identity,
            "leaseToken": self.lease["leaseToken"],
            "authorityEpoch": float(self.lease["authorityEpoch"]),
            "ttlSeconds": 45.0,
        }
        renewed = self.backend.renew_lease(ENCOUNTER_ID, renew_request)
        self.assertEqual(renewed["authorityEpoch"], self.lease["authorityEpoch"])

    def test_fractional_json_numbers_are_rejected_for_integer_fields(self) -> None:
        with self.assertRaises(CheckpointServiceError) as ttl_error:
            self.backend.acquire_lease(
                "ENCOUNTER_FRACTIONAL_TTL",
                {**self.identity, "ttlSeconds": 45.5},
            )
        self.assertEqual(ttl_error.exception.code, "INVALID_LEASE_TTL")

        request = self.make_read_request()
        request["authorityEpoch"] = float(self.lease["authorityEpoch"]) + 0.5
        with self.assertRaises(CheckpointServiceError) as epoch_error:
            self.backend.read_checkpoint(ENCOUNTER_ID, request)
        self.assertEqual(epoch_error.exception.code, "INVALID_AUTHORITY_EPOCH")

    def test_lease_ttl_is_bounded(self) -> None:
        with self.assertRaises(CheckpointServiceError) as error:
            self.backend.acquire_lease(
                "ENCOUNTER_INVALID_TTL",
                {**self.identity, "ttlSeconds": 1000},
            )
        self.assertEqual(error.exception.code, "INVALID_LEASE_TTL")



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
                self.make_request(self.make_payload(2), 0),
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


if __name__ == "__main__":
    unittest.main()
