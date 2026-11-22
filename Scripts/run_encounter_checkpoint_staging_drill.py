#!/usr/bin/env python3
"""Run a write-scoped HTTPS/mTLS/PostgreSQL checkpoint staging drill.

Only synthetic DRILL_STAGE42_* encounter IDs are accepted. The tool never
targets a production environment, prints credentials, or logs response bodies.
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
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
from cryptography.hazmat.primitives.serialization import load_pem_public_key

ROOT = Path(__file__).resolve().parents[1]
ENCOUNTER_RE = re.compile(r"^DRILL_STAGE42_[A-Z0-9_]{8,64}$")
SERVER_RE = re.compile(r"^[A-Za-z0-9_-]{3,96}$")
REQUIRED_CHECKS = (
    "https_health", "postgres_readiness", "mtls_client_certificate_required",
    "bearer_authentication_enforced", "lease_acquire", "lease_exclusion",
    "lease_renewal", "preexisting_checkpoint_probe", "checkpoint_write",
    "checkpoint_cas_conflict", "signed_readback", "lease_release",
    "lease_reacquire_epoch", "stale_lease_fenced",
    "durable_readback_after_reacquire", "final_lease_release",
)


class DrillFailure(RuntimeError):
    def __init__(self, code: str):
        super().__init__(code)
        self.code = code


def utc_now() -> datetime:
    return datetime.now(timezone.utc).replace(microsecond=0)


def utc_iso(value: datetime | None = None) -> str:
    return (value or utc_now()).isoformat().replace("+00:00", "Z")


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
        key_id, public_path = entry["keyId"], entry["publicKeyFile"]
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
    payload, integrity = envelope.get("payload"), envelope.get("integrity")
    if not isinstance(payload, dict) or not isinstance(integrity, dict):
        raise DrillFailure("SIGNED_ENVELOPE_CONTENT_INVALID")
    if payload.get("encounterId") != expected_encounter_id:
        raise DrillFailure("SIGNED_ENVELOPE_ENCOUNTER_MISMATCH")
    if payload.get("checkpointSequence") != expected_sequence:
        raise DrillFailure("SIGNED_ENVELOPE_SEQUENCE_MISMATCH")
    if integrity.get("algorithm") != "Ed25519":
        raise DrillFailure("SIGNED_ENVELOPE_ALGORITHM_INVALID")

    key_id, signature = integrity.get("keyId"), integrity.get("signature")
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
    return key_id, hashlib.sha256(rfc8785.dumps(envelope)).hexdigest()


class StagingApi:
    def __init__(self, base_url: str, ca_file: str, client_cert: str, client_key: str, bearer_token: str):
        parsed = urlsplit(base_url)
        if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
            raise DrillFailure("STAGING_BASE_URL_MUST_BE_HTTPS_WITHOUT_CREDENTIALS")
        if parsed.path not in ("", "/") or parsed.query or parsed.fragment:
            raise DrillFailure("STAGING_BASE_URL_MUST_BE_ORIGIN_ONLY")
        self.host = parsed.hostname
        self.port = parsed.port or 443
        self.bearer_token = bearer_token
        self.ca_file = ca_file
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
        except DrillFailure:
            raise
        except (OSError, http.client.HTTPException, ssl.SSLError) as error:
            raise DrillFailure("STAGING_TRANSPORT_FAILURE") from error
        finally:
            connection.close()

    def request_without_client_certificate(self) -> tuple[int, dict]:
        context = ssl.create_default_context(cafile=self.ca_file)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.check_hostname = True
        return self.request("GET", "/healthz", bearer_token="", context=context)


def expect_status(status: int, expected: int, body: dict, code: str | None = None) -> None:
    if status != expected:
        error_code = body.get("error", "UNEXPECTED_HTTP_STATUS")
        raise DrillFailure(f"EXPECTED_HTTP_{expected}_GOT_{status}_{error_code}")
    if code is not None and body.get("error") != code:
        raise DrillFailure("UNEXPECTED_ERROR_CODE")


def api_path(encounter_id: str, suffix: str) -> str:
    return f"/v1/encounters/{encounter_id}/{suffix}"


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
        {"phase": "Trials", "kind": "TrialBoundary", "encounterRevision": 1}
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


def make_report() -> dict:
    parsed = urlsplit(os.environ.get("JUMANJI_STAGING_BASE_URL", ""))
    return {
        "schema": "JUMANJI.CheckpointStagingDrillReport.v1",
        "stage": 42,
        "runId": str(uuid4()),
        "startedAtUtc": utc_iso(),
        "completedAtUtc": None,
        "result": "RUNNING",
        "environment": {
            "class": os.environ.get("JUMANJI_STAGING_ENVIRONMENT_CLASS", "unknown").strip() or "unknown",
            "id": os.environ.get("JUMANJI_STAGING_ENVIRONMENT_ID", "unknown").strip() or "unknown",
            "storageProvider": "postgresql",
            "serviceHost": parsed.hostname or "unknown",
            "transport": "https-mtls",
        },
        "checks": [],
        "evidence": {},
        "failureCode": None,
    }


def run_drill(args: argparse.Namespace, report: dict) -> dict:
    lease_token = ""
    lease_epoch = 0
    encounter_id = ""
    active_owner = ""
    release_required = False
    api: StagingApi | None = None

    def record(check_id: str, action) -> dict:
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

    try:
        env_class = report["environment"]["class"]
        env_id = report["environment"]["id"]
        if env_class not in {"staging", "ephemeral-ci"}:
            raise DrillFailure("ENVIRONMENT_CLASS_MUST_BE_STAGING_OR_EPHEMERAL_CI")
        if not env_id or len(env_id) > 96:
            raise DrillFailure("STAGING_ENVIRONMENT_ID_REQUIRED")
        if env_id.casefold() in {"prod", "production", "live"}:
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
        ruleset = os.environ.get("JUMANJI_STAGING_RULESET_VERSION", "").strip()
        content_version = os.environ.get("JUMANJI_STAGING_WORLD_CONTENT_VERSION", "").strip()
        required = (
            token, ca_file, client_cert, client_key, keyring_path,
            server_id, shard_id, build_id, ruleset, content_version,
        )
        if any(not value for value in required):
            raise DrillFailure("STAGING_CONFIGURATION_INCOMPLETE")
        if len(token) < 32 or not SERVER_RE.fullmatch(server_id) or not SERVER_RE.fullmatch(shard_id):
            raise DrillFailure("STAGING_CREDENTIAL_OR_ID_INVALID")
        if any(len(value) > 128 for value in (build_id, ruleset, content_version)):
            raise DrillFailure("STAGING_VERSION_VALUE_TOO_LONG")

        api = StagingApi(base_url, ca_file, client_cert, client_key, token)
        verification_keys = load_verification_keys(Path(keyring_path))
        encounter_id = args.encounter_id or (
            "DRILL_STAGE42_" + utc_now().strftime("%Y%m%dT%H%M%S") + "_" + uuid4().hex[:8].upper()
        )
        if not ENCOUNTER_RE.fullmatch(encounter_id):
            raise DrillFailure("ENCOUNTER_ID_MUST_BE_A_STAGE42_DRILL_ID")

        def health():
            status, body = api.request("GET", "/healthz", bearer_token="")
            expect_status(status, 200, body)
            return {"service": "healthy"}

        record("https_health", health)

        def readiness():
            status, body = api.request("GET", "/readyz", bearer_token="")
            expect_status(status, 200, body)
            if body.get("status") != "ready":
                raise DrillFailure("POSTGRES_STORAGE_NOT_READY")
            return {"storageProvider": "postgresql", "ready": True}

        record("postgres_readiness", readiness)

        def mtls():
            try:
                status, _ = api.request_without_client_certificate()
            except DrillFailure as error:
                if error.code == "STAGING_TRANSPORT_FAILURE":
                    return {"clientCertificateRequired": True}
                raise
            if status == 200:
                raise DrillFailure("MTLS_ACCEPTED_CLIENT_WITHOUT_CERTIFICATE")
            raise DrillFailure("MTLS_NO_CERTIFICATE_DID_NOT_FAIL_HANDSHAKE")

        record("mtls_client_certificate_required", mtls)

        def bearer():
            status, body = api.request(
                "POST",
                api_path(encounter_id, "leases"),
                {"serverInstanceId": server_id, "shardId": shard_id, "ttlSeconds": 45},
                bearer_token="stage42-deliberately-invalid-bearer-token",
            )
            expect_status(status, 401, body)
            return {"unauthorizedRequestRejected": True}

        record("bearer_authentication_enforced", bearer)

        def acquire():
            nonlocal lease_token, lease_epoch, active_owner, release_required
            status, body = api.request(
                "POST",
                api_path(encounter_id, "leases"),
                {"serverInstanceId": server_id, "shardId": shard_id, "ttlSeconds": 90},
            )
            expect_status(status, 201, body)
            lease_token, lease_epoch = body.get("leaseToken", ""), body.get("authorityEpoch", 0)
            if not isinstance(lease_token, str) or len(lease_token) < 32:
                raise DrillFailure("LEASE_TOKEN_RESPONSE_INVALID")
            if not isinstance(lease_epoch, int) or lease_epoch < 1:
                raise DrillFailure("LEASE_EPOCH_RESPONSE_INVALID")
            active_owner = server_id
            release_required = True
            report["evidence"]["firstAuthorityEpoch"] = lease_epoch
            return {"authorityEpoch": lease_epoch}

        record("lease_acquire", acquire)

        def exclusion():
            status, body = api.request(
                "POST",
                api_path(encounter_id, "leases"),
                {"serverInstanceId": "stage42-contender", "shardId": shard_id, "ttlSeconds": 45},
            )
            expect_status(status, 409, body, "LEASE_HELD")
            return {"secondOwnerRejected": True}

        record("lease_exclusion", exclusion)

        def renew():
            status, body = api.request(
                "POST",
                api_path(encounter_id, "leases/renew"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": lease_token, "authorityEpoch": lease_epoch, "ttlSeconds": 90,
                },
            )
            expect_status(status, 200, body)
            if body.get("authorityEpoch") != lease_epoch:
                raise DrillFailure("LEASE_RENEWAL_CHANGED_EPOCH")
            return {"authorityEpochUnchanged": True}

        record("lease_renewal", renew)

        prior_sequence = 0
        def read_prior():
            nonlocal prior_sequence
            status, body = api.request(
                "POST",
                api_path(encounter_id, "checkpoint/read"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": lease_token, "authorityEpoch": lease_epoch,
                },
            )
            if status == 404 and body.get("error") == "CHECKPOINT_NOT_FOUND":
                prior_sequence = 0
                return {"priorCheckpointFound": False}
            expect_status(status, 200, body)
            prior_sequence = body.get("checkpointSequence", 0)
            if not isinstance(prior_sequence, int) or prior_sequence < 1:
                raise DrillFailure("PRIOR_CHECKPOINT_SEQUENCE_INVALID")
            verify_signed_envelope(body.get("envelope"), verification_keys, encounter_id, prior_sequence)
            return {"priorCheckpointFound": True, "priorSequence": prior_sequence}

        record("preexisting_checkpoint_probe", read_prior)

        payload = build_payload(
            encounter_id, server_id, shard_id, lease_epoch, prior_sequence + 1,
            build_id, ruleset, content_version,
        )
        payload_digest = hashlib.sha256(rfc8785.dumps(payload)).hexdigest()

        def write():
            status, body = api.request(
                "PUT",
                api_path(encounter_id, "checkpoint"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": lease_token, "authorityEpoch": lease_epoch,
                    "expectedSequence": prior_sequence, "payload": payload,
                },
            )
            expect_status(status, 201, body)
            if body.get("checkpointSequence") != prior_sequence + 1:
                raise DrillFailure("WRITE_SEQUENCE_RESPONSE_MISMATCH")
            report["evidence"].update(
                {
                    "encounterId": encounter_id,
                    "priorSequence": prior_sequence,
                    "writtenSequence": prior_sequence + 1,
                    "payloadSha256": payload_digest,
                }
            )
            return {"checkpointSequence": prior_sequence + 1}

        record("checkpoint_write", write)

        def cas_conflict():
            status, body = api.request(
                "PUT",
                api_path(encounter_id, "checkpoint"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": lease_token, "authorityEpoch": lease_epoch,
                    "expectedSequence": prior_sequence, "payload": payload,
                },
            )
            expect_status(status, 409, body, "SEQUENCE_CONFLICT")
            return {"staleExpectedSequenceRejected": True}

        record("checkpoint_cas_conflict", cas_conflict)

        signing_key_id = ""
        envelope_digest = ""
        def signed_readback():
            nonlocal signing_key_id, envelope_digest
            status, body = api.request(
                "POST",
                api_path(encounter_id, "checkpoint/read"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": lease_token, "authorityEpoch": lease_epoch,
                },
            )
            expect_status(status, 200, body)
            if body.get("authorityEpoch") != lease_epoch:
                raise DrillFailure("READ_AUTHORITY_EPOCH_MISMATCH")
            signing_key_id, envelope_digest = verify_signed_envelope(
                body.get("envelope"), verification_keys, encounter_id, prior_sequence + 1
            )
            return {"signatureVerified": True, "keyId": signing_key_id}

        record("signed_readback", signed_readback)

        def release_first():
            nonlocal release_required
            status, body = api.request(
                "POST",
                api_path(encounter_id, "leases/release"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": lease_token, "authorityEpoch": lease_epoch,
                },
            )
            expect_status(status, 200, body)
            release_required = False
            return {"released": body.get("released") is True}

        record("lease_release", release_first)

        old_token, old_epoch = lease_token, lease_epoch

        def reacquire():
            nonlocal lease_token, lease_epoch, release_required
            status, body = api.request(
                "POST",
                api_path(encounter_id, "leases"),
                {"serverInstanceId": server_id, "shardId": shard_id, "ttlSeconds": 90},
            )
            expect_status(status, 201, body)
            new_token, new_epoch = body.get("leaseToken", ""), body.get("authorityEpoch", 0)
            if not isinstance(new_token, str) or len(new_token) < 32:
                raise DrillFailure("REACQUIRED_LEASE_TOKEN_INVALID")
            if new_epoch != old_epoch + 1:
                raise DrillFailure("REACQUIRED_EPOCH_NOT_MONOTONIC")
            lease_token, lease_epoch = new_token, new_epoch
            release_required = True
            report["evidence"]["reacquiredAuthorityEpoch"] = new_epoch
            return {"authorityEpochAdvancedByOne": True}

        record("lease_reacquire_epoch", reacquire)

        def stale_lease():
            status, body = api.request(
                "POST",
                api_path(encounter_id, "checkpoint/read"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": old_token, "authorityEpoch": old_epoch,
                },
            )
            expect_status(status, 409, body, "LEASE_INVALID")
            return {"oldLeaseFenced": True}

        record("stale_lease_fenced", stale_lease)

        def durable_readback():
            status, body = api.request(
                "POST",
                api_path(encounter_id, "checkpoint/read"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": lease_token, "authorityEpoch": lease_epoch,
                },
            )
            expect_status(status, 200, body)
            if body.get("checkpointSequence") != prior_sequence + 1:
                raise DrillFailure("DURABLE_SEQUENCE_CHANGED")
            key_after, digest_after = verify_signed_envelope(
                body.get("envelope"), verification_keys, encounter_id, prior_sequence + 1
            )
            if key_after != signing_key_id or digest_after != envelope_digest:
                raise DrillFailure("DURABLE_ENVELOPE_CHANGED")
            report["evidence"]["signingKeyId"] = key_after
            report["evidence"]["envelopeSha256"] = digest_after
            return {"signedEnvelopeSurvivedLeaseReacquire": True}

        record("durable_readback_after_reacquire", durable_readback)

        def final_release():
            nonlocal release_required
            status, body = api.request(
                "POST",
                api_path(encounter_id, "leases/release"),
                {
                    "serverInstanceId": server_id, "shardId": shard_id,
                    "leaseToken": lease_token, "authorityEpoch": lease_epoch,
                },
            )
            expect_status(status, 200, body)
            release_required = False
            return {"released": body.get("released") is True}

        record("final_lease_release", final_release)
        report["result"] = "PASS"
    except DrillFailure as error:
        report["result"] = "FAIL"
        report["failureCode"] = error.code
    finally:
        if release_required and api is not None and encounter_id and lease_token:
            try:
                api.request(
                    "POST",
                    api_path(encounter_id, "leases/release"),
                    {
                        "serverInstanceId": active_owner,
                        "shardId": os.environ.get("JUMANJI_STAGING_SHARD_ID", "").strip(),
                        "leaseToken": lease_token,
                        "authorityEpoch": lease_epoch,
                    },
                )
                report["evidence"]["cleanupReleaseAttempted"] = True
            except DrillFailure:
                report["evidence"]["cleanupReleaseAttempted"] = False

    report["completedAtUtc"] = utc_iso()
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encounter-id", help="Reuse a DRILL_STAGE42_* ID after external failover/PITR restore.")
    parser.add_argument("--confirm-staging-mutations", action="store_true", help="Explicitly permit synthetic staging writes.")
    parser.add_argument("--report", type=Path, default=Path("stage42-staging-drill-report.json"))
    args = parser.parse_args()

    report = make_report()
    result = run_drill(args, report)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"Stage 42 staging drill: {result['result']}")
    print(f"Report: {args.report}")
    if result.get("failureCode"):
        print(f"Failure code: {result['failureCode']}", file=sys.stderr)
        return 1
    print(f"Checks passed: {len(result['checks'])}/{len(REQUIRED_CHECKS)}")
    print("The report excludes credentials, lease tokens, participant keys and response bodies.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
