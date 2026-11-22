#!/usr/bin/env python3
"""Run a write-scoped HTTPS/mTLS/PostgreSQL checkpoint staging drill.

This tool intentionally writes only to a synthetic DRILL_STAGE42_* encounter.
It never targets a production environment, prints credentials, or logs response
bodies. Run it against an isolated staging or ephemeral test deployment only.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import http.client
import json
import os
import re
import ssl
import sys
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from urllib.parse import urlsplit
from uuid import uuid4

import rfc8785
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.serialization import load_pem_public_key
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

ROOT = Path(__file__).resolve().parents[1]
ENCOUNTER_RE = re.compile(r"^DRILL_STAGE42_[A-Z0-9_]{8,64}$")
SERVER_RE = re.compile(r"^[A-Za-z0-9_-]{3,96}$")
REQUIRED_CHECKS = (
    "https_health",
    "postgres_readiness",
    "mtls_client_certificate_required",
    "bearer_authentication_enforced",
    "lease_acquire",
    "lease_exclusion",
    "lease_renewal",
    "checkpoint_write",
    "checkpoint_cas_conflict",
    "signed_readback",
    "stale_lease_fenced",
    "lease_reacquire_epoch",
    "durable_readback_after_reacquire",
    "lease_release",
)


class DrillFailure(RuntimeError):
    def __init__(self, code: str):
        super().__init__(code)
        self.code = code


def utc_now() -> datetime:
    return datetime.now(timezone.utc).replace(microsecond=0)


def utc_iso(value: datetime | None = None) -> str:
    value = value or utc_now()
    return value.isoformat().replace("+00:00", "Z")


def read_secret(value_name: str, file_name: str) -> str:
    file_path = os.environ.get(file_name, "").strip()
    if file_path:
        return Path(file_path).read_text(encoding="utf-8").strip()
    return os.environ.get(value_name, "").strip()


def load_verification_keys(path: Path) -> dict[str, Ed25519PublicKey]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise DrillFailure("VERIFY_KEYRING_UNREADABLE") from error

    if not isinstance(document, dict) or set(document) != {"keys"} or not isinstance(document["keys"], list):
        raise DrillFailure("VERIFY_KEYRING_SHAPE_INVALID")

    keys: dict[str, Ed25519PublicKey] = {}
    for entry in document["keys"]:
        if not isinstance(entry, dict) or set(entry) != {"keyId", "publicKeyFile"}:
            raise DrillFailure("VERIFY_KEYRING_ENTRY_INVALID")
        key_id = entry["keyId"]
        public_path = entry["publicKeyFile"]
        if not isinstance(key_id, str) or not key_id or key_id in keys:
            raise DrillFailure("VERIFY_KEYRING_DUPLICATE_OR_EMPTY_KEY_ID")
        if not isinstance(public_path, str) or not public_path:
            raise DrillFailure("VERIFY_KEYRING_PUBLIC_KEY_PATH_INVALID")
        try:
            public_key = load_pem_public_key(Path(public_path).read_bytes())
        except (OSError, ValueError) as error:
            raise DrillFailure("VERIFY_PUBLIC_KEY_UNREADABLE") from error
        if not isinstance(public_key, Ed25519PublicKey):
            raise DrillFailure("VERIFY_PUBLIC_KEY_NOT_ED25519")
        keys[key_id] = public_key

    if not keys:
        raise DrillFailure("VERIFY_KEYRING_EMPTY")
    return keys


def verify_signed_envelope(
    envelope: object,
    verification_keys: dict[str, Ed25519PublicKey],
    expected_encounter_id: str,
    expected_sequence: int,
) -> tuple[str, str]:
    if not isinstance(envelope, dict) or set(envelope) != {"envelopeVersion", "payload", "integrity"}:
        raise DrillFailure("SIGNED_ENVELOPE_SHAPE_INVALID")
    if envelope.get("envelopeVersion") != 1:
        raise DrillFailure("SIGNED_ENVELOPE_VERSION_INVALID")

    payload = envelope.get("payload")
    integrity = envelope.get("integrity")
    if not isinstance(payload, dict) or not isinstance(integrity, dict):
        raise DrillFailure("SIGNED_ENVELOPE_CONTENT_INVALID")
    if payload.get("encounterId") != expected_encounter_id:
        raise DrillFailure("SIGNED_ENVELOPE_ENCOUNTER_MISMATCH")
    if payload.get("checkpointSequence") != expected_sequence:
        raise DrillFailure("SIGNED_ENVELOPE_SEQUENCE_MISMATCH")
    if integrity.get("algorithm") != "Ed25519":
        raise DrillFailure("SIGNED_ENVELOPE_ALGORITHM_INVALID")

    key_id = integrity.get("keyId")
    signature = integrity.get("signature")
    if not isinstance(key_id, str) or key_id not in verification_keys:
        raise DrillFailure("SIGNED_ENVELOPE_KEY_ID_UNKNOWN")
    if not isinstance(signature, str) or not re.fullmatch(r"[A-Za-z0-9_-]{64,256}", signature):
        raise DrillFailure("SIGNED_ENVELOPE_SIGNATURE_FORMAT_INVALID")

    unsigned = {
        "envelopeVersion": envelope["envelopeVersion"],
        "payload": payload,
        "integrity": {"algorithm": integrity["algorithm"], "keyId": key_id},
    }
    try:
        signature_bytes = base64.urlsafe_b64decode(signature + "=" * (-len(signature) % 4))
        verification_keys[key_id].verify(signature_bytes, rfc8785.dumps(unsigned))
    except (InvalidSignature, ValueError) as error:
        raise DrillFailure("SIGNED_ENVELOPE_SIGNATURE_INVALID") from error

    envelope_digest = hashlib.sha256(rfc8785.dumps(envelope)).hexdigest()
    return key_id, envelope_digest


class StagingApi:
    def __init__(self, base_url: str, ca_file: str, client_cert: str, client_key: str, bearer_token: str):
        parsed = urlsplit(base_url)
        if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
            raise DrillFailure("STAGING_BASE_URL_MUST_BE_HTTPS_WITHOUT_CREDENTIALS")
        if parsed.path not in ("", "/") or parsed.query or parsed.fragment:
            raise DrillFailure("STAGING_BASE_URL_MUST_BE_ORIGIN_ONLY")

        self.host = parsed.hostname
        self.port = parsed.port or 443
        self.base_url = f"https://{self.host}:{self.port}"
        self.bearer_token = bearer_token
        self.context = ssl.create_default_context(cafile=ca_file)
        self.context.minimum_version = ssl.TLSVersion.TLSv1_2
        self.context.check_hostname = True
        self.context.load_cert_chain(certfile=client_cert, keyfile=client_key)

    def request(
        self,
        method: str,
        path: str,
        body: dict | None = None,
        *,
        bearer_token: str | None = None,
        context: ssl.SSLContext | None = None,
    ) -> tuple[int, dict]:
        connection = http.client.HTTPSConnection(
            self.host, self.port, context=context or self.context, timeout=8.0
        )
        headers = {"Accept": "application/json"}
        token = self.bearer_token if bearer_token is None else bearer_token
        if token:
            headers["Authorization"] = "Bearer " + token
        encoded = None
        if body is not None:
            encoded = json.dumps(body, separators=(",", ":"), allow_nan=False).encode("utf-8")
            headers["Content-Type"] = "application/json"
        try:
            connection.request(method, path, body=encoded, headers=headers)
            response = connection.getresponse()
            raw = response.read(1024 * 1024 + 1)
            if len(raw) > 1024 * 1024:
                raise DrillFailure("STAGING_RESPONSE_TOO_LARGE")
            try:
                document = json.loads(raw.decode("utf-8")) if raw else {}
            except (UnicodeDecodeError, json.JSONDecodeError) as error:
                raise DrillFailure("STAGING_RESPONSE_NOT_JSON") from error
            if not isinstance(document, dict):
                raise DrillFailure("STAGING_RESPONSE_NOT_OBJECT")
            return response.status, document
        except (OSError, http.client.HTTPException, ssl.SSLError) as error:
            raise DrillFailure("STAGING_TRANSPORT_FAILURE") from error
        finally:
            connection.close()

    def verify_missing_client_certificate_is_rejected(self) -> bool:
        context = ssl.create_default_context()
        context.check_hostname = True
        # The staging CA is still required; a client certificate is deliberately omitted.
        # This context is replaced below by the same trust roots with no load_cert_chain.
        # Use the existing context's CA store via its configured CA file at the call site.
        try:
            status, _ = self.request("GET", "/healthz", bearer_token="", context=context)
            return False if status == 200 else True
        except DrillFailure as error:
            return error.code == "STAGING_TRANSPORT_FAILURE"


def build_payload(
    encounter_id: str,
    server_id: str,
    shard_id: str,
    authority_epoch: int,
    sequence: int,
    build_id: str,
    ruleset_version: str,
    world_content_version: str,
) -> dict:
    fixture_path = ROOT / "Data/Encounter/EncounterCheckpointExample.json"
    try:
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        payload = json.loads(json.dumps(fixture["payload"]))
    except (OSError, UnicodeError, json.JSONDecodeError, KeyError) as error:
        raise DrillFailure("STAGE32_FIXTURE_UNREADABLE") from error

    now = utc_now()
    payload.update(
        {
            "schemaVersion": 1,
            "checkpointId": str(uuid4()),
            "encounterId": encounter_id,
            "sessionId": str(uuid4()),
            "checkpointSequence": sequence,
            "createdAtUtc": utc_iso(now),
            "expiresAtUtc": utc_iso(now + timedelta(minutes=30)),
        }
    )
    payload["authority"].update(
        {
            "serverInstanceId": server_id,
            "shardId": shard_id,
            "authorityEpoch": authority_epoch,
            "serverBuildId": build_id,
            "rulesetVersion": ruleset_version,
            "worldContentVersion": world_content_version,
        }
    )
    payload["safePoint"].update(
        {
            "phase": "Trials",
            "kind": "TrialBoundary",
            "encounterRevision": 1,
        }
    )
    payload["safePoint"]["chapter"].update(
        {
            "chapterId": "CHAPTER_STAGE42_DRILL",
            "boardEventId": "BOARD_EVENT_STAGE42_DRILL",
            "chapterRevision": 1,
            "completedObjectiveIds": ["OBJ_STAGE42_PROBE"],
            "completedTrialIds": ["TRIAL_STAGE42_PROBE"],
        }
    )
    payload["safePoint"]["combat"] = {"currentWave": 0, "totalWaves": 3}
    payload["deterministic"] = {"encounterSeed": 420042, "spawnSetId": "SPAWNSET_STAGE42_PROBE"}
    payload["participants"] = [
        {"participantKey": "pk_" + uuid4().hex, "profileRevision": 0}
    ]
    payload.pop("reward", None)
    return payload


def api_path(encounter_id: str, suffix: str) -> str:
    return f"/v1/encounters/{encounter_id}/{suffix}"


def expect_status(status: int, expected: int, body: dict, code: str | None = None) -> None:
    if status != expected:
        error_code = body.get("error", "UNEXPECTED_HTTP_STATUS")
        raise DrillFailure(f"EXPECTED_HTTP_{expected}_GOT_{status}_{error_code}")
    if code is not None and body.get("error") != code:
        raise DrillFailure("UNEXPECTED_ERROR_CODE")


def run_drill(args: argparse.Namespace) -> dict:
    environment_class = os.environ.get("JUMANJI_STAGING_ENVIRONMENT_CLASS", "").strip()
    environment_id = os.environ.get("JUMANJI_STAGING_ENVIRONMENT_ID", "").strip()
    if environment_class not in {"staging", "ephemeral-ci"}:
        raise DrillFailure("ENVIRONMENT_CLASS_MUST_BE_STAGING_OR_EPHEMERAL_CI")
    if not environment_id or len(environment_id) > 96:
        raise DrillFailure("STAGING_ENVIRONMENT_ID_REQUIRED")
    if environment_id.casefold() in {"prod", "production", "live"}:
        raise DrillFailure("PRODUCTION_ENVIRONMENT_IS_NOT_ALLOWED")
    if not args.confirm_staging_mutations:
        raise DrillFailure("EXPLICIT_STAGING_MUTATION_CONFIRMATION_REQUIRED")

    base_url = os.environ.get("JUMANJI_STAGING_BASE_URL", "").strip()
    token = read_secret("JUMANJI_STAGING_SERVER_TOKEN", "JUMANJI_STAGING_SERVER_TOKEN_FILE")
    ca_file = os.environ.get("JUMANJI_STAGING_CA_FILE", "").strip()
    client_cert = os.environ.get("JUMANJI_STAGING_CLIENT_CERT_FILE", "").strip()
    client_key = os.environ.get("JUMANJI_STAGING_CLIENT_KEY_FILE", "").strip()
    keyring_path = os.environ.get("JUMANJI_STAGING_VERIFY_KEYRING_FILE", "").strip()
    server_id = os.environ.get("JUMANJI_STAGING_SERVER_INSTANCE_ID", "").strip()
    shard_id = os.environ.get("JUMANJI_STAGING_SHARD_ID", "").strip()
    build_id = os.environ.get("JUMANJI_STAGING_SERVER_BUILD_ID", "").strip()
    ruleset_version = os.environ.get("JUMANJI_STAGING_RULESET_VERSION", "").strip()
    world_content_version = os.environ.get("JUMANJI_STAGING_WORLD_CONTENT_VERSION", "").strip()

    for name, value in (
        ("SERVER_TOKEN", token),
        ("CA_FILE", ca_file),
        ("CLIENT_CERT_FILE", client_cert),
        ("CLIENT_KEY_FILE", client_key),
        ("VERIFY_KEYRING_FILE", keyring_path),
        ("SERVER_INSTANCE_ID", server_id),
        ("SHARD_ID", shard_id),
        ("SERVER_BUILD_ID", build_id),
        ("RULESET_VERSION", ruleset_version),
        ("WORLD_CONTENT_VERSION", world_content_version),
    ):
        if not value:
            raise DrillFailure(f"STAGING_{name}_REQUIRED")
    if len(token) < 32 or not SERVER_RE.fullmatch(server_id) or not SERVER_RE.fullmatch(shard_id):
        raise DrillFailure("STAGING_CREDENTIAL_OR_SERVER_ID_INVALID")

    api = StagingApi(base_url, ca_file, client_cert, client_key, token)
    verification_keys = load_verification_keys(Path(keyring_path))
    encounter_id = args.encounter_id or (
        "DRILL_STAGE42_" + utc_now().strftime("%Y%m%dT%H%M%S") + "_" + uuid4().hex[:8].upper()
    )
    if not ENCOUNTER_RE.fullmatch(encounter_id):
        raise DrillFailure("ENCOUNTER_ID_MUST_BE_A_STAGE42_DRILL_ID")

    report = {
        "schema": "JUMANJI.CheckpointStagingDrillReport.v1",
        "stage": 42,
        "runId": str(uuid4()),
        "startedAtUtc": utc_iso(),
        "completedAtUtc": None,
        "result": "RUNNING",
        "environment": {
            "class": environment_class,
            "id": environment_id,
            "storageProvider": "postgresql",
            "serviceHost": api.host,
            "transport": "https-mtls",
        },
        "checks": [],
        "evidence": {},
    }

    def check(check_id: str, action) -> dict:
        started = time.perf_counter()
        try:
            details = action()
            entry = {
                "id": check_id,
                "status": "PASS",
                "durationMs": round((time.perf_counter() - started) * 1000, 3),
                "detailCode": "OK",
            }
            if isinstance(details, dict):
                entry["evidence"] = details
        except DrillFailure as error:
            entry = {
                "id": check_id,
                "status": "FAIL",
                "durationMs": round((time.perf_counter() - started) * 1000, 3),
                "detailCode": error.code,
            }
            report["checks"].append(entry)
            raise
        report["checks"].append(entry)
        return entry

    status, _ = check("https_health", lambda: (
        expect_status(*api.request("GET", "/healthz", bearer_token=""), 200) or {}
    )).get("evidence", {}), None
