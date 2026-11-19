from __future__ import annotations

import base64
import hashlib
import hmac
import json
import os
import re
import secrets
import sqlite3
import sys
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit

import rfc8785
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import load_pem_private_key

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from Scripts.validate_encounter_checkpoint_contract import (  # noqa: E402
    validate_envelope,
    validate_payload,
)

ENCOUNTER_ID = re.compile(r"^[A-Z][A-Z0-9_]{2,95}$")
SERVER_ID = re.compile(r"^[A-Za-z0-9_-]{3,96}$")
MAX_BODY_BYTES = 1024 * 1024
MIN_LEASE_SECONDS = 15
MAX_LEASE_SECONDS = 120


class CheckpointServiceError(Exception):
    def __init__(self, status: int, code: str, message: str, details: list[str] | None = None):
        super().__init__(message)
        self.status = status
        self.code = code
        self.message = message
        self.details = details or []


def _utc_iso(timestamp: float) -> str:
    return datetime.fromtimestamp(timestamp, timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def _parse_utc_epoch(value: str) -> float:
    return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()


def _json_bytes(value: object) -> bytes:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode("utf-8")


def _decode_signature(value: str) -> bytes:
    return base64.urlsafe_b64decode(value + ("=" * (-len(value) % 4)))


class EncounterCheckpointBackend:
    """Single-host prototype. SQLite transactions provide CAS/lease atomicity on this host."""

    def __init__(self, database_path: Path, signing_key: Ed25519PrivateKey, key_id: str):
        if not key_id or len(key_id) > 128:
            raise ValueError("key_id must be a non-empty value no longer than 128 characters")
        self.database_path = Path(database_path)
        self.database_path.parent.mkdir(parents=True, exist_ok=True)
        self.signing_key = signing_key
        self.verification_key = signing_key.public_key()
        self.key_id = key_id
        self.initialize()

    @classmethod
    def from_environment(cls) -> tuple["EncounterCheckpointBackend", str]:
        key_path = os.environ.get("JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE", "").strip()
        key_id = os.environ.get("JUMANJI_CHECKPOINT_KEY_ID", "").strip()
        server_token = os.environ.get("JUMANJI_CHECKPOINT_SERVER_TOKEN", "")
        database_path = Path(os.environ.get("JUMANJI_CHECKPOINT_DB", "./checkpoint-service.sqlite3"))

        if not key_path or not key_id:
            raise RuntimeError("Signing key file and key id must be supplied through the server environment")
        if len(server_token) < 32:
            raise RuntimeError("JUMANJI_CHECKPOINT_SERVER_TOKEN must contain at least 32 characters")

        raw_key = Path(key_path).read_bytes()
        loaded = load_pem_private_key(raw_key, password=None)
        if not isinstance(loaded, Ed25519PrivateKey):
            raise RuntimeError("Configured signing key is not an Ed25519 private key")

        return cls(database_path, loaded, key_id), server_token

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self.database_path, timeout=10.0, isolation_level=None)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA busy_timeout = 10000")
        return connection

    def initialize(self) -> None:
        connection = self._connect()
        try:
            connection.execute("PRAGMA journal_mode = WAL")
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS encounter_checkpoint_state (
                    encounter_id TEXT PRIMARY KEY,
                    checkpoint_sequence INTEGER NOT NULL DEFAULT 0,
                    authority_epoch INTEGER NOT NULL DEFAULT 0,
                    lease_server_instance_id TEXT,
                    lease_shard_id TEXT,
                    lease_token_hash TEXT,
                    lease_expires_at REAL NOT NULL DEFAULT 0,
                    checkpoint_envelope_json TEXT,
                    checkpoint_expires_at REAL
                )
                """
            )
        finally:
            connection.close()

    @staticmethod
    def _exact_fields(body: object, expected: set[str]) -> dict[str, Any]:
        if not isinstance(body, dict) or set(body) != expected:
            raise CheckpointServiceError(400, "INVALID_REQUEST", "Request body has missing or unsupported fields")
        return body

    @staticmethod
    def _validate_encounter_id(encounter_id: str) -> None:
        if not isinstance(encounter_id, str) or not ENCOUNTER_ID.fullmatch(encounter_id):
            raise CheckpointServiceError(400, "INVALID_ENCOUNTER_ID", "EncounterId is not a stable content ID")

    @staticmethod
    def _validate_server_identity(server_instance_id: object, shard_id: object) -> tuple[str, str]:
        if not isinstance(server_instance_id, str) or not SERVER_ID.fullmatch(server_instance_id):
            raise CheckpointServiceError(400, "INVALID_SERVER_ID", "serverInstanceId has an invalid format")
        if not isinstance(shard_id, str) or not SERVER_ID.fullmatch(shard_id):
            raise CheckpointServiceError(400, "INVALID_SHARD_ID", "shardId has an invalid format")
        return server_instance_id, shard_id

    @staticmethod
    def _validate_ttl(value: object) -> int:
        if type(value) is not int or not MIN_LEASE_SECONDS <= value <= MAX_LEASE_SECONDS:
            raise CheckpointServiceError(
                400,
                "INVALID_LEASE_TTL",
                f"ttlSeconds must be between {MIN_LEASE_SECONDS} and {MAX_LEASE_SECONDS}",
            )
        return value

    @staticmethod
    def _validate_lease_fields(body: dict[str, Any]) -> tuple[str, int]:
        token = body.get("leaseToken")
        epoch = body.get("authorityEpoch")
        if not isinstance(token, str) or len(token) < 32 or len(token) > 256:
            raise CheckpointServiceError(400, "INVALID_LEASE_TOKEN", "leaseToken has an invalid format")
        if type(epoch) is not int or epoch < 1:
            raise CheckpointServiceError(400, "INVALID_AUTHORITY_EPOCH", "authorityEpoch must be a positive integer")
        return token, epoch

    @staticmethod
    def _token_hash(token: str) -> str:
        return hashlib.sha256(token.encode("utf-8")).hexdigest()

    def _ensure_row(self, connection: sqlite3.Connection, encounter_id: str) -> sqlite3.Row:
        connection.execute(
            "INSERT OR IGNORE INTO encounter_checkpoint_state(encounter_id) VALUES (?)",
            (encounter_id,),
        )
        row = connection.execute(
            "SELECT * FROM encounter_checkpoint_state WHERE encounter_id = ?",
            (encounter_id,),
        ).fetchone()
        if row is None:
            raise CheckpointServiceError(500, "STORE_ERROR", "Checkpoint state could not be initialized")
        return row

    def _verify_lease(
        self,
        row: sqlite3.Row | None,
        server_instance_id: str,
        shard_id: str,
        lease_token: str,
        authority_epoch: int,
        now: float,
    ) -> None:
        if row is None:
            raise CheckpointServiceError(409, "LEASE_INVALID", "No active lease exists for this encounter")
        stored_hash = row["lease_token_hash"] or ""
        supplied_hash = self._token_hash(lease_token)
        if (
            not hmac.compare_digest(stored_hash, supplied_hash)
            or row["lease_server_instance_id"] != server_instance_id
            or row["lease_shard_id"] != shard_id
            or row["authority_epoch"] != authority_epoch
            or row["lease_expires_at"] <= now
        ):
            raise CheckpointServiceError(409, "LEASE_INVALID", "Lease is expired, stale or owned by another server")

    def acquire_lease(self, encounter_id: str, request: object) -> dict[str, Any]:
        self._validate_encounter_id(encounter_id)
        body = self._exact_fields(request, {"serverInstanceId", "shardId", "ttlSeconds"})
        server_id, shard_id = self._validate_server_identity(body["serverInstanceId"], body["shardId"])
        ttl = self._validate_ttl(body["ttlSeconds"])
        now = time.time()
        token = secrets.token_urlsafe(32)

        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = self._ensure_row(connection, encounter_id)
            if row["lease_token_hash"] and row["lease_expires_at"] > now:
                raise CheckpointServiceError(409, "LEASE_HELD", "Another server currently owns the recovery lease")

            next_epoch = int(row["authority_epoch"]) + 1
            expires = now + ttl
            connection.execute(
                """
                UPDATE encounter_checkpoint_state
                SET authority_epoch = ?,
                    lease_server_instance_id = ?,
                    lease_shard_id = ?,
                    lease_token_hash = ?,
                    lease_expires_at = ?
                WHERE encounter_id = ?
                """,
                (next_epoch, server_id, shard_id, self._token_hash(token), expires, encounter_id),
            )
            connection.commit()
            return {
                "encounterId": encounter_id,
                "leaseToken": token,
                "authorityEpoch": next_epoch,
                "leaseExpiresAtUtc": _utc_iso(expires),
            }
        except Exception:
            connection.rollback()
            raise
        finally:
            connection.close()

    def renew_lease(self, encounter_id: str, request: object) -> dict[str, Any]:
        self._validate_encounter_id(encounter_id)
        body = self._exact_fields(
            request,
            {"serverInstanceId", "shardId", "leaseToken", "authorityEpoch", "ttlSeconds"},
        )
        server_id, shard_id = self._validate_server_identity(body["serverInstanceId"], body["shardId"])
        token, epoch = self._validate_lease_fields(body)
        ttl = self._validate_ttl(body["ttlSeconds"])
        now = time.time()

        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT * FROM encounter_checkpoint_state WHERE encounter_id = ?",
                (encounter_id,),
            ).fetchone()
            self._verify_lease(row, server_id, shard_id, token, epoch, now)
            expires = now + ttl
            connection.execute(
                "UPDATE encounter_checkpoint_state SET lease_expires_at = ? WHERE encounter_id = ?",
                (expires, encounter_id),
            )
            connection.commit()
            return {"encounterId": encounter_id, "authorityEpoch": epoch, "leaseExpiresAtUtc": _utc_iso(expires)}
        except Exception:
            connection.rollback()
            raise
        finally:
            connection.close()

    def release_lease(self, encounter_id: str, request: object) -> dict[str, Any]:
        self._validate_encounter_id(encounter_id)
        body = self._exact_fields(
            request,
            {"serverInstanceId", "shardId", "leaseToken", "authorityEpoch"},
        )
        server_id, shard_id = self._validate_server_identity(body["serverInstanceId"], body["shardId"])
        token, epoch = self._validate_lease_fields(body)
        now = time.time()

        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT * FROM encounter_checkpoint_state WHERE encounter_id = ?",
                (encounter_id,),
            ).fetchone()
            self._verify_lease(row, server_id, shard_id, token, epoch, now)
            connection.execute(
                """
                UPDATE encounter_checkpoint_state
                SET lease_server_instance_id = NULL,
                    lease_shard_id = NULL,
                    lease_token_hash = NULL,
                    lease_expires_at = 0
                WHERE encounter_id = ?
                """,
                (encounter_id,),
            )
            connection.commit()
            return {"encounterId": encounter_id, "authorityEpoch": epoch, "released": True}
        except Exception:
            connection.rollback()
            raise
        finally:
            connection.close()

    def write_checkpoint(self, encounter_id: str, request: object) -> dict[str, Any]:
        self._validate_encounter_id(encounter_id)
        body = self._exact_fields(
            request,
            {"serverInstanceId", "shardId", "leaseToken", "authorityEpoch", "expectedSequence", "payload"},
        )
        server_id, shard_id = self._validate_server_identity(body["serverInstanceId"], body["shardId"])
        token, epoch = self._validate_lease_fields(body)
        expected = body["expectedSequence"]
        if type(expected) is not int or expected < 0:
            raise CheckpointServiceError(400, "INVALID_SEQUENCE", "expectedSequence must be a non-negative integer")

        payload = body["payload"]
        now_dt = datetime.now(timezone.utc)
        payload_errors = validate_payload(payload, now=now_dt)
        if payload_errors:
            raise CheckpointServiceError(422, "CHECKPOINT_INVALID", "Checkpoint payload failed contract validation", payload_errors[:12])
        if payload["encounterId"] != encounter_id:
            raise CheckpointServiceError(422, "ENCOUNTER_MISMATCH", "Payload EncounterId does not match the route")
        if payload["checkpointSequence"] != expected + 1:
            raise CheckpointServiceError(409, "SEQUENCE_MISMATCH", "Payload sequence must equal expectedSequence + 1")

        authority = payload["authority"]
        if (
            authority["serverInstanceId"] != server_id
            or authority["shardId"] != shard_id
            or authority["authorityEpoch"] != epoch
        ):
            raise CheckpointServiceError(422, "AUTHORITY_MISMATCH", "Payload authority does not match the active lease")

        created = datetime.fromisoformat(payload["createdAtUtc"].replace("Z", "+00:00"))
        if created.timestamp() > now_dt.timestamp() + 300:
            raise CheckpointServiceError(422, "FUTURE_CHECKPOINT", "Checkpoint creation time is too far in the future")

        unsigned_envelope: dict[str, Any] = {
            "envelopeVersion": 1,
            "payload": payload,
            "integrity": {"algorithm": "Ed25519", "keyId": self.key_id},
        }
        signature = self.signing_key.sign(rfc8785.dumps(unsigned_envelope))
        envelope = {
            **unsigned_envelope,
            "integrity": {
                "algorithm": "Ed25519",
                "keyId": self.key_id,
                "signature": base64.urlsafe_b64encode(signature).decode("ascii").rstrip("="),
            },
        }
        envelope_errors = validate_envelope(envelope, now=now_dt)
        if envelope_errors:
            raise CheckpointServiceError(422, "SIGNED_ENVELOPE_INVALID", "Signed envelope failed contract validation", envelope_errors[:12])

        now = time.time()
        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT * FROM encounter_checkpoint_state WHERE encounter_id = ?",
                (encounter_id,),
            ).fetchone()
            self._verify_lease(row, server_id, shard_id, token, epoch, now)
            current_sequence = int(row["checkpoint_sequence"])
            if current_sequence != expected:
                raise CheckpointServiceError(409, "SEQUENCE_CONFLICT", "Checkpoint sequence changed; reload before writing")

            expires_epoch = _parse_utc_epoch(payload["expiresAtUtc"])
            changed = connection.execute(
                """
                UPDATE encounter_checkpoint_state
                SET checkpoint_sequence = ?,
                    checkpoint_envelope_json = ?,
                    checkpoint_expires_at = ?
                WHERE encounter_id = ?
                  AND checkpoint_sequence = ?
                  AND authority_epoch = ?
                  AND lease_token_hash = ?
                """,
                (
                    payload["checkpointSequence"],
                    json.dumps(envelope, ensure_ascii=False, separators=(",", ":"), allow_nan=False),
                    expires_epoch,
                    encounter_id,
                    expected,
                    epoch,
                    self._token_hash(token),
                ),
            ).rowcount
            if changed != 1:
                raise CheckpointServiceError(409, "SEQUENCE_CONFLICT", "Compare-and-swap rejected the checkpoint write")
            connection.commit()
            return {
                "encounterId": encounter_id,
                "checkpointId": payload["checkpointId"],
                "checkpointSequence": payload["checkpointSequence"],
                "authorityEpoch": epoch,
                "integrity": envelope["integrity"],
            }
        except Exception:
            connection.rollback()
            raise
        finally:
            connection.close()

    def read_checkpoint(self, encounter_id: str, request: object) -> dict[str, Any]:
        self._validate_encounter_id(encounter_id)
        body = self._exact_fields(
            request,
            {"serverInstanceId", "shardId", "leaseToken", "authorityEpoch"},
        )
        server_id, shard_id = self._validate_server_identity(body["serverInstanceId"], body["shardId"])
        token, epoch = self._validate_lease_fields(body)
        now = time.time()

        connection = self._connect()
        try:
            connection.execute("BEGIN")
            row = connection.execute(
                "SELECT * FROM encounter_checkpoint_state WHERE encounter_id = ?",
                (encounter_id,),
            ).fetchone()
            self._verify_lease(row, server_id, shard_id, token, epoch, now)
            if not row["checkpoint_envelope_json"]:
                raise CheckpointServiceError(404, "CHECKPOINT_NOT_FOUND", "No checkpoint has been stored for this encounter")
            if row["checkpoint_expires_at"] is None or row["checkpoint_expires_at"] <= now:
                raise CheckpointServiceError(410, "CHECKPOINT_EXPIRED", "The stored checkpoint has expired")

            envelope = json.loads(row["checkpoint_envelope_json"])
            errors = validate_envelope(envelope, now=datetime.now(timezone.utc))
            if errors:
                raise CheckpointServiceError(422, "CHECKPOINT_CORRUPT", "Stored checkpoint failed contract validation", errors[:12])
            integrity = envelope["integrity"]
            if integrity["keyId"] != self.key_id or integrity["algorithm"] != "Ed25519":
                raise CheckpointServiceError(422, "SIGNING_KEY_UNKNOWN", "Stored checkpoint uses an unknown signing key")
            unsigned = {
                "envelopeVersion": envelope["envelopeVersion"],
                "payload": envelope["payload"],
                "integrity": {
                    "algorithm": integrity["algorithm"],
                    "keyId": integrity["keyId"],
                },
            }
            try:
                self.verification_key.verify(_decode_signature(integrity["signature"]), rfc8785.dumps(unsigned))
            except (InvalidSignature, ValueError):
                raise CheckpointServiceError(422, "SIGNATURE_INVALID", "Stored checkpoint signature verification failed") from None

            connection.commit()
            return {
                "encounterId": encounter_id,
                "checkpointSequence": row["checkpoint_sequence"],
                "authorityEpoch": row["authority_epoch"],
                "envelope": envelope,
            }
        except Exception:
            connection.rollback()
            raise
        finally:
            connection.close()


def make_handler(backend: EncounterCheckpointBackend, server_token: str):
    class CheckpointRequestHandler(BaseHTTPRequestHandler):
        server_version = "JumanjiCheckpointService/0.1"
        sys_version = ""

        def log_message(self, format_string: str, *args: object) -> None:
            # Deliberately avoid logging request bodies, credentials or lease tokens.
            super().log_message("%s", format_string % args)

        def _send_json(self, status: int, body: dict[str, Any]) -> None:
            encoded = _json_bytes(body)
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(encoded)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(encoded)

        def _authorized(self) -> bool:
            supplied = self.headers.get("Authorization", "")
            expected = "Bearer " + server_token
            if not hmac.compare_digest(supplied, expected):
                self._send_json(401, {"error": "UNAUTHORIZED", "message": "Server authentication failed"})
                return False
            return True

        def _read_json(self) -> dict[str, Any]:
            raw_length = self.headers.get("Content-Length", "")
            try:
                length = int(raw_length)
            except ValueError:
                raise CheckpointServiceError(400, "INVALID_CONTENT_LENGTH", "Content-Length is required") from None
            if length < 2 or length > MAX_BODY_BYTES:
                raise CheckpointServiceError(413, "BODY_SIZE_INVALID", "Request body is empty or exceeds the size limit")
            raw = self.rfile.read(length)
            try:
                value = json.loads(raw.decode("utf-8"), parse_constant=lambda item: (_ for _ in ()).throw(ValueError(item)))
            except (UnicodeDecodeError, json.JSONDecodeError, ValueError):
                raise CheckpointServiceError(400, "INVALID_JSON", "Request body is not valid UTF-8 JSON") from None
            if not isinstance(value, dict):
                raise CheckpointServiceError(400, "INVALID_JSON_OBJECT", "Request body must be a JSON object")
            return value

        def _handle(self, method: str) -> None:
            parsed_path = urlsplit(self.path).path
            if method == "GET" and parsed_path == "/healthz":
                self._send_json(200, {"status": "ok", "service": "encounter-checkpoint"})
                return
            if not self._authorized():
                return

            match = re.fullmatch(
                r"/v1/encounters/([A-Z][A-Z0-9_]{2,95})/(leases|leases/renew|leases/release|checkpoint/read|checkpoint)",
                parsed_path,
            )
            if not match:
                self._send_json(404, {"error": "NOT_FOUND", "message": "Route not found"})
                return

            encounter_id, operation = match.groups()
            try:
                body = self._read_json()
                if method == "POST" and operation == "leases":
                    result = backend.acquire_lease(encounter_id, body)
                    self._send_json(201, result)
                elif method == "POST" and operation == "leases/renew":
                    self._send_json(200, backend.renew_lease(encounter_id, body))
                elif method == "POST" and operation == "leases/release":
                    self._send_json(200, backend.release_lease(encounter_id, body))
                elif method == "POST" and operation == "checkpoint/read":
                    self._send_json(200, backend.read_checkpoint(encounter_id, body))
                elif method == "PUT" and operation == "checkpoint":
                    self._send_json(201, backend.write_checkpoint(encounter_id, body))
                else:
                    self._send_json(405, {"error": "METHOD_NOT_ALLOWED", "message": "Method is not allowed for this route"})
            except CheckpointServiceError as error:
                response: dict[str, Any] = {"error": error.code, "message": error.message}
                if error.details:
                    response["details"] = error.details
                self._send_json(error.status, response)
            except (sqlite3.Error, OSError):
                self._send_json(503, {"error": "BACKEND_UNAVAILABLE", "message": "Checkpoint store is temporarily unavailable"})
            except Exception:
                self._send_json(500, {"error": "INTERNAL_ERROR", "message": "Request could not be completed"})

        def do_GET(self) -> None:
            self._handle("GET")

        def do_POST(self) -> None:
            self._handle("POST")

        def do_PUT(self) -> None:
            self._handle("PUT")

    return CheckpointRequestHandler


def main() -> int:
    try:
        backend, server_token = EncounterCheckpointBackend.from_environment()
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Checkpoint service configuration error: {error}", file=sys.stderr)
        return 2

    host = os.environ.get("JUMANJI_CHECKPOINT_BIND_HOST", "127.0.0.1")
    port = int(os.environ.get("JUMANJI_CHECKPOINT_BIND_PORT", "8787"))
    server = ThreadingHTTPServer((host, port), make_handler(backend, server_token))
    print(f"Encounter checkpoint prototype listening on {host}:{port}")
    print("Expose only behind a private service network and an mTLS-terminating reverse proxy.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
