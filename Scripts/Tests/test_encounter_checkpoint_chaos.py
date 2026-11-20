from __future__ import annotations

import copy
import json
import socket
import sqlite3
import tempfile
import threading
import unittest
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import URLError
from urllib.request import Request, urlopen
from uuid import uuid4
from unittest.mock import patch

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

import Backend.EncounterCheckpointService.service as service_module
from Backend.EncounterCheckpointService.service import (
    CheckpointServiceError,
    EncounterCheckpointBackend,
    make_handler,
)


ENCOUNTER_ID = "ENCOUNTER_EMERALD_JUNGLE_CITADEL"
SERVER_ID = "srv-chaos-eu-01"
SHARD_ID = "eu-chaos-01"
SERVER_TOKEN = "chaos-test-token-with-more-than-32-characters"


class EncounterCheckpointChaosTests(unittest.TestCase):
    """Failure-injection tests for the real single-host checkpoint backend.

    These tests exercise the Python service and SQLite store. They do not
    simulate Unreal actors, compile C++, or replace multiplayer PIE testing.
    """

    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.database_path = Path(self.temp_dir.name) / "checkpoint-chaos.sqlite3"
        self.signing_key = Ed25519PrivateKey.generate()
        self.backend = EncounterCheckpointBackend(
            self.database_path,
            self.signing_key,
            "CHAOS-TEST-KEY-01",
        )
        self.identity = {"serverInstanceId": SERVER_ID, "shardId": SHARD_ID}
        self.lease_request = {**self.identity, "ttlSeconds": 45}
        self.lease = self.backend.acquire_lease(ENCOUNTER_ID, self.lease_request)

    def make_payload(self, sequence: int = 1) -> dict:
        fixture_path = Path(__file__).resolve().parents[2] / "Data/Encounter/EncounterCheckpointExample.json"
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

    def make_read_request(self, lease: dict | None = None) -> dict:
        active_lease = lease or self.lease
        return {
            **self.identity,
            "leaseToken": active_lease["leaseToken"],
            "authorityEpoch": active_lease["authorityEpoch"],
        }

    def start_http_server(self, backend: EncounterCheckpointBackend, drop_write_ack: bool = False):
        base_handler = make_handler(backend, SERVER_TOKEN)
        handler = base_handler
        if drop_write_ack:
            class DropWriteAckHandler(base_handler):
                def _send_json(self, status: int, body: dict) -> None:
                    if status == 201 and "checkpointSequence" in body:
                        # The service has committed the transaction. Close the
                        # socket without sending the successful response.
                        self.close_connection = True
                        try:
                            self.connection.shutdown(socket.SHUT_RDWR)
                        except OSError:
                            pass
                        return
                    super()._send_json(status, body)

            handler = DropWriteAckHandler

        server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        server.daemon_threads = True
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        return server, thread, f"http://127.0.0.1:{server.server_port}"

    @staticmethod
    def stop_http_server(server: ThreadingHTTPServer, thread: threading.Thread) -> None:
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)

    def test_lost_write_ack_is_reconciled_without_duplicate_sequence(self) -> None:
        server, thread, base_url = self.start_http_server(self.backend, drop_write_ack=True)
        try:
            payload = self.make_payload(sequence=1)
            request_body = self.make_write_request(payload, expected_sequence=0)
            request = Request(
                f"{base_url}/v1/encounters/{ENCOUNTER_ID}/checkpoint",
                data=json.dumps(request_body).encode("utf-8"),
                headers={
                    "Authorization": f"Bearer {SERVER_TOKEN}",
                    "Content-Type": "application/json",
                },
                method="PUT",
            )

            # Fault injection: backend commits, then the handler drops the
            # response. The client cannot know whether the write committed.
            with self.assertRaises((OSError, URLError)):
                urlopen(request, timeout=3)

            stored = self.backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request())
            self.assertEqual(stored["checkpointSequence"], 1)
            self.assertEqual(stored["envelope"]["payload"]["checkpointId"], payload["checkpointId"])

            # A blind retry with the old expected sequence is rejected. The
            # caller must reconcile the authoritative head before any new write.
            with self.assertRaises(CheckpointServiceError) as error:
                self.backend.write_checkpoint(
                    ENCOUNTER_ID,
                    self.make_write_request(self.make_payload(sequence=1), expected_sequence=0),
                )
            self.assertEqual(error.exception.code, "SEQUENCE_CONFLICT")
            confirmed = self.backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request())
            self.assertEqual(confirmed["checkpointSequence"], 1)
        finally:
            self.stop_http_server(server, thread)

    def test_backend_restart_preserves_checkpoint_and_fences_old_owner(self) -> None:
        payload = self.make_payload()
        self.backend.write_checkpoint(ENCOUNTER_ID, self.make_write_request(payload))

        # Recreate the service object against the same durable SQLite file and
        # key, as a process restart would. The old lease remains authoritative
        # until expiry; restart alone must not steal it.
        restarted_backend = EncounterCheckpointBackend(
            self.database_path,
            self.signing_key,
            "CHAOS-TEST-KEY-01",
        )
        with self.assertRaises(CheckpointServiceError) as held:
            restarted_backend.acquire_lease(ENCOUNTER_ID, self.lease_request)
        self.assertEqual(held.exception.code, "LEASE_HELD")

        current_time = service_module.time.time()
        with patch.object(service_module.time, "time", return_value=current_time + 46):
            new_lease = restarted_backend.acquire_lease(ENCOUNTER_ID, self.lease_request)
            self.assertEqual(new_lease["authorityEpoch"], self.lease["authorityEpoch"] + 1)

            with self.assertRaises(CheckpointServiceError) as stale:
                restarted_backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request(self.lease))
            self.assertEqual(stale.exception.code, "LEASE_INVALID")

            restored = restarted_backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request(new_lease))
            self.assertEqual(restored["checkpointSequence"], 1)
            self.assertEqual(restored["envelope"]["payload"]["checkpointId"], payload["checkpointId"])

    def test_sqlite_abort_keeps_checkpoint_head_unchanged(self) -> None:
        connection = sqlite3.connect(self.database_path)
        try:
            connection.execute(
                """
                CREATE TRIGGER inject_checkpoint_write_failure
                BEFORE UPDATE OF checkpoint_envelope_json ON encounter_checkpoint_state
                BEGIN
                    SELECT RAISE(ABORT, 'injected checkpoint write failure');
                END;
                """
            )
            connection.commit()
        finally:
            connection.close()

        with self.assertRaises(sqlite3.IntegrityError):
            self.backend.write_checkpoint(
                ENCOUNTER_ID,
                self.make_write_request(self.make_payload(), expected_sequence=0),
            )

        connection = sqlite3.connect(self.database_path)
        try:
            row = connection.execute(
                "SELECT checkpoint_sequence, checkpoint_envelope_json "
                "FROM encounter_checkpoint_state WHERE encounter_id = ?",
                (ENCOUNTER_ID,),
            ).fetchone()
        finally:
            connection.close()

        self.assertEqual(row[0], 0)
        self.assertIsNone(row[1])

    def test_concurrent_same_sequence_writes_have_one_winner(self) -> None:
        competing_backend = EncounterCheckpointBackend(
            self.database_path,
            self.signing_key,
            "CHAOS-TEST-KEY-01",
        )
        barrier = threading.Barrier(2)

        requests = [
            (self.backend, self.make_write_request(self.make_payload(sequence=1), 0)),
            (competing_backend, self.make_write_request(self.make_payload(sequence=1), 0)),
        ]

        def attempt(item: tuple[EncounterCheckpointBackend, dict]) -> str:
            backend, body = item
            barrier.wait(timeout=5)
            try:
                backend.write_checkpoint(ENCOUNTER_ID, body)
                return "COMMITTED"
            except CheckpointServiceError as error:
                return error.code

        with ThreadPoolExecutor(max_workers=2) as pool:
            outcomes = list(pool.map(attempt, requests))

        self.assertEqual(outcomes.count("COMMITTED"), 1)
        self.assertEqual(outcomes.count("SEQUENCE_CONFLICT"), 1)
        result = self.backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request())
        self.assertEqual(result["checkpointSequence"], 1)

    def test_signature_tamper_fails_closed(self) -> None:
        self.backend.write_checkpoint(
            ENCOUNTER_ID,
            self.make_write_request(self.make_payload()),
        )

        connection = sqlite3.connect(self.database_path)
        try:
            stored_json = connection.execute(
                "SELECT checkpoint_envelope_json FROM encounter_checkpoint_state WHERE encounter_id = ?",
                (ENCOUNTER_ID,),
            ).fetchone()[0]
            envelope = json.loads(stored_json)
            envelope["payload"]["deterministic"]["encounterSeed"] += 1
            connection.execute(
                "UPDATE encounter_checkpoint_state SET checkpoint_envelope_json = ? WHERE encounter_id = ?",
                (json.dumps(envelope), ENCOUNTER_ID),
            )
            connection.commit()
        finally:
            connection.close()

        with self.assertRaises(CheckpointServiceError) as error:
            self.backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request())
        self.assertEqual(error.exception.code, "SIGNATURE_INVALID")

    def test_expired_checkpoint_fails_closed(self) -> None:
        self.backend.write_checkpoint(
            ENCOUNTER_ID,
            self.make_write_request(self.make_payload()),
        )

        connection = sqlite3.connect(self.database_path)
        try:
            connection.execute(
                "UPDATE encounter_checkpoint_state SET checkpoint_expires_at = ? WHERE encounter_id = ?",
                (service_module.time.time() - 1, ENCOUNTER_ID),
            )
            connection.commit()
        finally:
            connection.close()

        with self.assertRaises(CheckpointServiceError) as error:
            self.backend.read_checkpoint(ENCOUNTER_ID, self.make_read_request())
        self.assertEqual(error.exception.code, "CHECKPOINT_EXPIRED")

    def test_transport_outage_is_reported_as_unavailable(self) -> None:
        server, thread, base_url = self.start_http_server(self.backend)
        self.stop_http_server(server, thread)

        request = Request(f"{base_url}/healthz", method="GET")
        with self.assertRaises(URLError):
            urlopen(request, timeout=2)


if __name__ == "__main__":
    unittest.main()
