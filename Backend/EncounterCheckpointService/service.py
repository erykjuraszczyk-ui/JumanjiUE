from __future__ import annotations

import base64
import hashlib
import hmac
import json
import math
import os
import re
import secrets
import sqlite3
import ssl
import sys
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlsplit

import rfc8785
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import load_pem_private_key, load_pem_public_key

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


def _as_integral_number(value: object) -> int | None:
    # Unreal FJsonObject stores numeric values as doubles; accept 7.0 but never 7.25 or bool.
    if type(value) is int:
        return value
    if type(value) is float and math.isfinite(value) and value.is_integer():
        return int(value)
    return None


def _json_bytes(value: object) -> bytes:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode("utf-8")


def _decode_signature(value: str) -> bytes:
    return base64.urlsafe_b64decode(value + ("=" * (-len(value) % 4)))


class _PostgresConnectionAdapter:
    """Small DB-API compatibility layer for the existing SQL transaction code."""

    def __init__(self, connection: Any):
        self._connection = connection

    def execute(self, statement: str, parameters: tuple[Any, ...] = ()):
        sql = statement.strip()
        if sql.upper() == "BEGIN IMMEDIATE":
            sql = "BEGIN"
        if sql.startswith("INSERT OR IGNORE INTO encounter_checkpoint_state"):
            sql = sql.replace("INSERT OR IGNORE INTO", "INSERT INTO", 1)
            sql = sql.rstrip(";") + " ON CONFLICT (encounter_id) DO NOTHING"
        elif sql.startswith("INSERT OR IGNORE INTO checkpoint_schema_migrations"):
            sql = sql.replace("INSERT OR IGNORE INTO", "INSERT INTO", 1)
            sql = sql.rstrip(";") + " ON CONFLICT (version) DO NOTHING"
        if sql.startswith("SELECT * FROM encounter_checkpoint_state WHERE encounter_id = ?"):
            sql += " FOR UPDATE"
        return self._connection.execute(sql.replace("?", "%s"), parameters)

    def commit(self) -> None:
        self._connection.commit()

    def rollback(self) -> None:
        self._connection.rollback()

    def close(self) -> None:
        self._connection.close()


class EncounterCheckpointBackend:
    """Transactional checkpoint backend with SQLite development and PostgreSQL shared-store providers."""

    def __init__(
        self,
        database_path: Path | None,
        signing_key: Ed25519PrivateKey,
        key_id: str,
        *,
        database_url: str | None = None,
        verification_keyring_path: Path | None = None,
        allow_insecure_database: bool = False,
    ):
        if not key_id or len(key_id) > 128:
            raise ValueError("key_id must be a non-empty value no longer than 128 characters")
        if database_url and database_path:
            raise ValueError("Configure either database_url or database_path, not both")
        if not database_url and not database_path:
            raise ValueError("A checkpoint database URL or SQLite path is required")

        self.storage_kind = "postgresql" if database_url else "sqlite"
        self.database_url = database_url
        self.database_path = Path(database_path) if database_path else None
        self.signing_key = signing_key
        self.key_id = key_id
        self.verification_keys: dict[str, Any] = {key_id: signing_key.public_key()}

        if self.storage_kind == "sqlite":
            assert self.database_path is not None
            self.database_path.parent.mkdir(parents=True, exist_ok=True)
        else:
            parsed = urlsplit(database_url or "")
            if parsed.scheme not in {"postgres", "postgresql"}:
                raise ValueError("database_url must use PostgreSQL")
            query = parse_qs(parsed.query)
            ssl_mode = query.get("sslmode", [""])[0]
            if ssl_mode != "verify-full" and not allow_insecure_database:
                raise ValueError("PostgreSQL requires sslmode=verify-full outside explicit development/test mode")
            self._load_verification_keyring(verification_keyring_path)

        self.initialize()

    @staticmethod
    def _environment_secret(value_name: str, file_name: str, default: str = "") -> str:
        file_path = os.environ.get(file_name, "").strip()
        if file_path:
            return Path(file_path).read_text(encoding="utf-8").strip()
        return os.environ.get(value_name, default).strip()

    def _load_verification_keyring(self, keyring_path: Path | None) -> None:
        if not keyring_path:
            return
        try:
            document = json.loads(Path(keyring_path).read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            raise RuntimeError("Checkpoint verification keyring could not be read") from error
        if not isinstance(document, dict) or set(document) != {"keys"} or not isinstance(document["keys"], list):
            raise RuntimeError("Checkpoint verification keyring must contain exactly a keys array")

        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
        for entry in document["keys"]:
            if not isinstance(entry, dict) or set(entry) != {"keyId", "publicKeyFile"}:
                raise RuntimeError("Each verification key entry must contain keyId and publicKeyFile")
            key_id = entry["keyId"]
            public_path = entry["publicKeyFile"]
            if not isinstance(key_id, str) or not key_id or len(key_id) > 128 or key_id in self.verification_keys:
                raise RuntimeError("Verification key IDs must be unique non-empty values")
            if not isinstance(public_path, str) or not public_path:
                raise RuntimeError("Verification public-key file path is required")
            try:
                public_key = load_pem_public_key(Path(public_path).read_bytes())
            except (OSError, ValueError) as error:
                raise RuntimeError("Checkpoint verification public key could not be loaded") from error
            if not isinstance(public_key, Ed25519PublicKey):
                raise RuntimeError("Verification keyring accepts only Ed25519 public keys")
            self.verification_keys[key_id] = public_key

    @classmethod
    def from_environment(cls) -> tuple["EncounterCheckpointBackend", str]:
        key_path = os.environ.get("JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE", "").strip()
        key_id = os.environ.get("JUMANJI_CHECKPOINT_KEY_ID", "").strip()
        server_token = cls._environment_secret(
            "JUMANJI_CHECKPOINT_SERVER_TOKEN",
            "JUMANJI_CHECKPOINT_SERVER_TOKEN_FILE",
        )
        database_url = cls._environment_secret(
            "JUMANJI_CHECKPOINT_DATABASE_URL",
            "JUMANJI_CHECKPOINT_DATABASE_URL_FILE",
        )
        database_path = Path(os.environ.get("JUMANJI_CHECKPOINT_DB", "./checkpoint-service.sqlite3"))
        keyring_value = os.environ.get("JUMANJI_CHECKPOINT_VERIFY_KEYRING_FILE", "").strip()
        allow_insecure = os.environ.get("JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE", "").strip().lower() == "true"

        if not key_path or not key_id:
            raise RuntimeError("Signing key file and key id must be supplied through the server environment")
        if len(server_token) < 32:
            raise RuntimeError("JUMANJI_CHECKPOINT_SERVER_TOKEN must contain at least 32 characters")

        raw_key = Path(key_path).read_bytes()
        loaded = load_pem_private_key(raw_key, password=None)
        if not isinstance(loaded, Ed25519PrivateKey):
            raise RuntimeError("Configured signing key is not an Ed25519 private key")

        if database_url:
            backend = cls(
                None, loaded, key_id,
                database_url=database_url,
                verification_keyring_path=Path(keyring_value) if keyring_value else None,
                allow_insecure_database=allow_insecure,
            )
        else:
            if keyring_value:
                raise RuntimeError("Verification key rotation is configured only with the shared PostgreSQL provider")
            backend = cls(database_path, loaded, key_id)
        return backend, server_token

    def _connect(self):
        if self.storage_kind == "postgresql":
            try:
                import psycopg
                from psycopg.rows import dict_row
            except ImportError as error:
                raise RuntimeError("PostgreSQL provider requires psycopg 3") from error
            connection = psycopg.connect(self.database_url, row_factory=dict_row, autocommit=True)
            return _PostgresConnectionAdapter(connection)

        assert self.database_path is not None
        connection = sqlite3.connect(self.database_path, timeout=10.0, isolation_level=None)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA busy_timeout = 10000")
        return connection

    def initialize(self) -> None:
        connection = self._connect()
        try:
            if self.storage_kind == "sqlite":
                connection.execute("PRAGMA journal_mode = WAL")
                sequence_type = "INTEGER"
                epoch_type = "INTEGER"
                real_type = "REAL"
            else:
                sequence_type = "BIGINT"
                epoch_type = "BIGINT"
                real_type = "DOUBLE PRECISION"

            connection.execute(
                f"""
                CREATE TABLE IF NOT EXISTS encounter_checkpoint_state (
                    encounter_id TEXT PRIMARY KEY,
                    checkpoint_sequence {sequence_type} NOT NULL DEFAULT 0,
                    authority_epoch {epoch_type} NOT NULL DEFAULT 0,
                    lease_server_instance_id TEXT,
                    lease_shard_id TEXT,
                    lease_token_hash TEXT,
                    lease_expires_at {real_type} NOT NULL DEFAULT 0,
                    checkpoint_envelope_json TEXT,
                    checkpoint_expires_at {real_type}
                )
                """
            )
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS checkpoint_schema_migrations (
                    version INTEGER PRIMARY KEY,
                    applied_at_utc TEXT NOT NULL
                )
                """
            )
            connection.execute(
                "INSERT OR IGNORE INTO checkpoint_schema_migrations(version, applied_at_utc) VALUES (?, ?)",
                (1, _utc_iso(time.time())),
            )
            connection.commit()
        finally:
            connection.close()

    def check_ready(self) -> None:
        connection = self._connect()
        try:
            row = connection.execute("SELECT 1 AS ready").fetchone()
            if not row or row["ready"] != 1:
                raise RuntimeError("Checkpoint storage readiness query failed")
        finally:
            connection.close()

    def import_checkpoint_record(
        self,
        encounter_id: str,
        checkpoint_sequence: int,
        authority_epoch: int,
        envelope_json: str,
    ) -> bool:
        """Import one verified checkpoint. Active lease ownership is never copied."""
        if self.storage_kind != "postgresql":
            raise RuntimeError("Checkpoint migration destination must be PostgreSQL")
        self._validate_encounter_id(encounter_id)

        try:
            envelope = json.loads(envelope_json)
        except (TypeError, json.JSONDecodeError):
            raise CheckpointServiceError(422, "MIGRATION_ENVELOPE_INVALID", "Source checkpoint envelope is invalid JSON") from None
        errors = validate_envelope(envelope, now=datetime.now(timezone.utc))
        if errors:
            raise CheckpointServiceError(422, "MIGRATION_ENVELOPE_INVALID", "Source checkpoint failed contract validation", errors[:12])

        payload = envelope["payload"]
        integrity = envelope["integrity"]
        verification_key = self.verification_keys.get(integrity.get("keyId", ""))
        if verification_key is None or integrity.get("algorithm") != "Ed25519":
            raise CheckpointServiceError(422, "MIGRATION_SIGNING_KEY_UNKNOWN", "Source checkpoint signing key is not in the verification keyring")

        unsigned = {
            "envelopeVersion": envelope["envelopeVersion"],
            "payload": payload,
            "integrity": {
                "algorithm": integrity["algorithm"],
                "keyId": integrity["keyId"],
            },
        }
        try:
            verification_key.verify(_decode_signature(integrity["signature"]), rfc8785.dumps(unsigned))
        except (InvalidSignature, ValueError):
            raise CheckpointServiceError(422, "MIGRATION_SIGNATURE_INVALID", "Source checkpoint signature verification failed") from None

        if (
            payload.get("encounterId") != encounter_id
            or payload.get("checkpointSequence") != checkpoint_sequence
            or payload.get("authority", {}).get("authorityEpoch") != authority_epoch
            or checkpoint_sequence < 1
            or authority_epoch < 1
        ):
            raise CheckpointServiceError(422, "MIGRATION_HEAD_MISMATCH", "Source row metadata does not match the signed checkpoint payload")

        expires_at = _parse_utc_epoch(payload["expiresAtUtc"])
        stored_envelope = json.dumps(envelope, ensure_ascii=False, separators=(",", ":"), allow_nan=False)
        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            existing = connection.execute(
                "SELECT * FROM encounter_checkpoint_state WHERE encounter_id = ?",
                (encounter_id,),
            ).fetchone()
            if existing is not None:
                if (
                    int(existing["checkpoint_sequence"]) == checkpoint_sequence
                    and int(existing["authority_epoch"]) == authority_epoch
                    and existing["checkpoint_envelope_json"] == stored_envelope
                ):
                    connection.commit()
                    return False
                raise CheckpointServiceError(409, "MIGRATION_DESTINATION_CONFLICT", "Destination already contains different state for this encounter")

            connection.execute(
                """
                INSERT INTO encounter_checkpoint_state (
                    encounter_id,
                    checkpoint_sequence,
                    authority_epoch,
                    lease_server_instance_id,
                    lease_shard_id,
                    lease_token_hash,
                    lease_expires_at,
                    checkpoint_envelope_json,
                    checkpoint_expires_at
                ) VALUES (?, ?, ?, NULL, NULL, NULL, 0, ?, ?)
                """,
                (encounter_id, checkpoint_sequence, authority_epoch, stored_envelope, expires_at),
            )
            connection.commit()
            return True
        except Exception:
            connection.rollback()
            raise
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
        normalized = _as_integral_number(value)
        if normalized is None or not MIN_LEASE_SECONDS <= normalized <= MAX_LEASE_SECONDS:
            raise CheckpointServiceError(
                400,
                "INVALID_LEASE_TTL",
                f"ttlSeconds must be an integer from {MIN_LEASE_SECONDS} to {MAX_LEASE_SECONDS}",
            )
        return normalized

    @staticmethod
    def _validate_lease_fields(body: dict[str, Any]) -> tuple[str, int]:
        token = body.get("leaseToken")
        epoch = _as_integral_number(body.get("authorityEpoch"))
        if not isinstance(token, str) or len(token) < 32 or len(token) > 256:
            raise CheckpointServiceError(400, "INVALID_LEASE_TOKEN", "leaseToken has an invalid format")
        if epoch is None or epoch < 1:
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
        expected = _as_integral_number(body["expectedSequence"])
        if expected is None or expected < 0:
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
            verification_key = self.verification_keys.get(integrity.get("keyId", ""))
            if verification_key is None or integrity["algorithm"] != "Ed25519":
                raise CheckpointServiceError(422, "SIGNING_KEY_UNKNOWN", "Stored checkpoint uses an unknown verification key")
            unsigned = {
                "envelopeVersion": envelope["envelopeVersion"],
                "payload": envelope["payload"],
                "integrity": {
                    "algorithm": integrity["algorithm"],
                    "keyId": integrity["keyId"],
                },
            }
            try:
                verification_key.verify(_decode_signature(integrity["signature"]), rfc8785.dumps(unsigned))
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
        server_version = "JumanjiCheckpointService/0.2"
        sys_version = ""

        def setup(self) -> None:
            super().setup()
            self.connection.settimeout(10.0)

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
            if method == "GET" and parsed_path == "/readyz":
                try:
                    backend.check_ready()
                    self._send_json(200, {"status": "ready", "service": "encounter-checkpoint"})
                except Exception:
                    self._send_json(503, {"status": "not_ready", "service": "encounter-checkpoint"})
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
            except Exception as error:
                if error.__class__.__module__.startswith("psycopg"):
                    self._send_json(503, {"error": "BACKEND_UNAVAILABLE", "message": "Checkpoint store is temporarily unavailable"})
                else:
                    self._send_json(500, {"error": "INTERNAL_ERROR", "message": "Request could not be completed"})

        def do_GET(self) -> None:
            self._handle("GET")

        def do_POST(self) -> None:
            self._handle("POST")

        def do_PUT(self) -> None:
            self._handle("PUT")

    return CheckpointRequestHandler


class BoundedThreadingHTTPServer(ThreadingHTTPServer):
    """Bound concurrent request threads so overload fails closed with HTTP 503."""

    daemon_threads = True
    allow_reuse_address = True
    request_queue_size = 128

    def __init__(self, server_address, handler_class, max_concurrency: int):
        if not 1 <= max_concurrency <= 1024:
            raise ValueError("max_concurrency must be between 1 and 1024")
        self._request_slots = threading.BoundedSemaphore(max_concurrency)
        super().__init__(server_address, handler_class)

    def process_request(self, request, client_address):
        if not self._request_slots.acquire(blocking=False):
            try:
                request.sendall(
                    b"HTTP/1.1 503 Service Unavailable\\r\\n"
                    b"Connection: close\\r\\n"
                    b"Content-Length: 0\\r\\n\\r\\n"
                )
            except OSError:
                pass
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except Exception:
            self._request_slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self._request_slots.release()


def _load_tls_context() -> tuple[ssl.SSLContext | None, bool]:
    cert_path = os.environ.get("JUMANJI_CHECKPOINT_TLS_CERT_FILE", "").strip()
    key_path = os.environ.get("JUMANJI_CHECKPOINT_TLS_KEY_FILE", "").strip()
    client_ca_path = os.environ.get("JUMANJI_CHECKPOINT_MTLS_CA_FILE", "").strip()
    mtls_required = os.environ.get("JUMANJI_CHECKPOINT_MTLS_REQUIRED", "").strip().lower() == "true"

    if bool(cert_path) != bool(key_path):
        raise RuntimeError("Both TLS certificate and private-key file are required")
    if mtls_required and (not cert_path or not client_ca_path):
        raise RuntimeError("mTLS requires a server certificate, private key and trusted client CA")
    if not cert_path:
        return None, False

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(certfile=cert_path, keyfile=key_path)
    if client_ca_path:
        context.load_verify_locations(cafile=client_ca_path)
        context.verify_mode = ssl.CERT_REQUIRED if mtls_required else ssl.CERT_OPTIONAL
    return context, mtls_required


def main() -> int:
    try:
        backend, server_token = EncounterCheckpointBackend.from_environment()
        host = os.environ.get("JUMANJI_CHECKPOINT_BIND_HOST", "127.0.0.1").strip()
        port = int(os.environ.get("JUMANJI_CHECKPOINT_BIND_PORT", "8787"))
        max_concurrency = int(os.environ.get("JUMANJI_CHECKPOINT_MAX_CONCURRENCY", "64"))
        tls_context, mtls_required = _load_tls_context()
        is_loopback = host in {"127.0.0.1", "::1", "localhost"}
        if not is_loopback and (tls_context is None or not mtls_required):
            raise RuntimeError("Non-loopback bind requires direct TLS with mandatory client-certificate verification")
        server = BoundedThreadingHTTPServer(
            (host, port),
            make_handler(backend, server_token),
            max_concurrency,
        )
        if tls_context:
            server.socket = tls_context.wrap_socket(server.socket, server_side=True)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Checkpoint service configuration error: {error}", file=sys.stderr)
        return 2

    print(f"Encounter checkpoint service listening on {host}:{port}; storage={backend.storage_kind}; max_concurrency={max_concurrency}")
    print("Readiness is available at /readyz; /v1 routes require bearer authentication and non-loopback binds require mTLS.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
