#!/usr/bin/env python3
"""Static governance checks for Stage 41 checkpoint production hardening.

This does not establish production readiness, run a staging deployment, prove HA,
execute a backup restore drill, compile Unreal or approve any SLO.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_PATHS = (
    "Backend/EncounterCheckpointService/service.py",
    "Backend/EncounterCheckpointService/requirements.txt",
    "Backend/EncounterCheckpointService/Dockerfile",
    "Backend/EncounterCheckpointService/compose.production.yml",
    "Backend/EncounterCheckpointService/README.md",
    "Docs/STAGE_41_PRODUCTION_CHECKPOINT_STORE_DEPLOYMENT_HARDENING.md",
    "Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json",
    "Data/Encounter/EncounterCheckpointVerificationKeyringTemplate.json",
    "Scripts/migrate_encounter_checkpoint_store.py",
    "Scripts/Tests/test_encounter_checkpoint_service.py",
    "Scripts/Tests/test_encounter_checkpoint_postgres.py",
    "Scripts/validate_encounter_checkpoint_backend.py",
)
REQUIRED_PROVIDER_MARKERS = (
    "class _PostgresConnectionAdapter",
    "psycopg.connect(",
    "ssl_mode != \"verify-full\"",
    "FOR UPDATE",
    "ON CONFLICT (encounter_id) DO NOTHING",
    "checkpoint_schema_migrations",
    "self.verification_keys.get(",
    "def validate_import_envelope(",
    "def import_checkpoint_record(",
    "JUMANJI_CHECKPOINT_MTLS_REQUIRED",
    "ssl.CERT_REQUIRED",
    "class BoundedThreadingHTTPServer",
    'parsed_path == "/readyz"',
    "JUMANJI_CHECKPOINT_MAX_CONCURRENCY",
)
REQUIRED_COMPOSE_ENV = (
    "JUMANJI_CHECKPOINT_DATABASE_URL_FILE",
    "JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE",
    "JUMANJI_CHECKPOINT_SERVER_TOKEN_FILE",
    "JUMANJI_CHECKPOINT_VERIFY_KEYRING_FILE",
    "JUMANJI_CHECKPOINT_MTLS_REQUIRED: \"true\"",
)
REQUIRED_MIGRATION_MARKERS = (
    "mode=ro",
    "dry_run",
    "validate_import_envelope",
    "import_checkpoint_record",
    '"activeLeasesTransferred": 0',
)
REQUIRED_TEST_MARKERS = (
    "test_signing_key_rotation_keeps_old_checkpoint_readable",
    "test_storage_readiness_query_succeeds",
)


def _load_json(path: Path, errors: list[str], label: str) -> dict[str, Any] | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        errors.append(f"{label}: cannot parse JSON: {error}")
        return None
    if not isinstance(value, dict):
        errors.append(f"{label}: root must be an object")
        return None
    return value


def validate_manifest(manifest: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if manifest.get("schema") != "JUMANJI.CheckpointProductionHardening.v1":
        errors.append("manifest schema must be JUMANJI.CheckpointProductionHardening.v1")
    if manifest.get("stage") != 41:
        errors.append("manifest stage must be 41")
    if manifest.get("productionReady") is not False:
        errors.append("productionReady must remain false until staging and operational evidence exist")
    if manifest.get("highAvailabilityApproved") is not False:
        errors.append("highAvailabilityApproved must remain false")
    if manifest.get("sloStatus") != "UNBASELINED":
        errors.append("Stage 40 SLO governance must remain UNBASELINED")

    providers = manifest.get("providers")
    if not isinstance(providers, dict):
        errors.append("providers must be an object")
    else:
        sqlite = providers.get("sqlite", {})
        postgres = providers.get("postgresql", {})
        if sqlite.get("purpose") != "local development, unit tests, single-host prototype only":
            errors.append("SQLite must remain limited to local development and single-host prototype use")
        if sqlite.get("multiHostSafe") is not False:
            errors.append("SQLite multiHostSafe must remain false")
        if postgres.get("driver") != "psycopg3" or postgres.get("rowLocking") != "SELECT FOR UPDATE":
            errors.append("PostgreSQL provider must identify psycopg3 and row locking")
        if postgres.get("requiresSslModeVerifyFull") is not True:
            errors.append("PostgreSQL must require sslmode=verify-full outside explicit test/development override")
        if postgres.get("connectionPoolImplemented") is not False:
            errors.append("manifest must disclose connection pooling is not implemented")

    transport = manifest.get("transport")
    if not isinstance(transport, dict):
        errors.append("transport must be an object")
    else:
        if transport.get("tlsMinimumVersion") != "TLSv1.2":
            errors.append("TLS minimum must be TLSv1.2")
        if transport.get("nonLoopbackBindRequiresClientCertificate") is not True:
            errors.append("non-loopback bind must require client certificates")
        if transport.get("hostPublishedPorts") is not False:
            errors.append("production deployment must not publish service ports to the host")

    signing = manifest.get("signingKeyRotation")
    if not isinstance(signing, dict):
        errors.append("signingKeyRotation must be an object")
    else:
        if signing.get("algorithm") != "Ed25519" or signing.get("overlapRotationSupported") is not True:
            errors.append("Ed25519 overlap key rotation must be declared")
        if signing.get("kmsOrHsmIntegrated") is not False:
            errors.append("manifest must disclose that KMS/HSM integration is not implemented")

    migration = manifest.get("migration")
    if not isinstance(migration, dict):
        errors.append("migration must be an object")
    else:
        if migration.get("mode") != "offline_write_frozen":
            errors.append("SQLite-to-PostgreSQL migration must remain offline with writers frozen")
        if migration.get("transfersActiveLease") is not False:
            errors.append("migration must never transfer active leases")
        if migration.get("transfersLeaseTokensOrHashes") is not False:
            errors.append("migration must never transfer lease tokens or token hashes")
        if migration.get("dryRunSupported") is not True:
            errors.append("migration must provide a dry-run mode")

    backup = manifest.get("backupAndRecovery")
    if not isinstance(backup, dict):
        errors.append("backupAndRecovery must be an object")
    else:
        if backup.get("backupOrchestratorImplemented") is not False:
            errors.append("manifest must disclose backup scheduler is not implemented")
        if backup.get("restoreDrillCompleted") is not False:
            errors.append("manifest must disclose no restore drill has been completed")

    tests = manifest.get("tests")
    if not isinstance(tests, dict):
        errors.append("tests must be an object")
    else:
        if tests.get("realStagingDeploymentPerformed") is not False:
            errors.append("manifest must disclose that staging deployment has not been performed")
        if tests.get("productionDeploymentPerformed") is not False:
            errors.append("manifest must disclose that production deployment has not been performed")
        if tests.get("unrealEngineBuildPerformed") is not False:
            errors.append("manifest must disclose that Unreal has not been built in this stage")

    followups = manifest.get("mandatoryFollowUp")
    if not isinstance(followups, list) or len(followups) < 5:
        errors.append("manifest must retain explicit staging, operational and SLO follow-up requirements")
    return errors


def validate_project(root: Path) -> list[str]:
    errors: list[str] = []
    for relative in REQUIRED_PATHS:
        if not (root / relative).is_file():
            errors.append(f"required file missing: {relative}")
    if errors:
        return errors

    manifest = _load_json(
        root / "Data/Encounter/EncounterCheckpointProductionDeploymentManifest.json",
        errors,
        "Stage 41 manifest",
    )
    if manifest is not None:
        errors.extend(validate_manifest(manifest))

    keyring = _load_json(
        root / "Data/Encounter/EncounterCheckpointVerificationKeyringTemplate.json",
        errors,
        "verification keyring template",
    )
    if keyring is not None and (set(keyring) != {"keys"} or keyring.get("keys") != []):
        errors.append("repository keyring template must be empty and contain no deployment public keys")

    service = (root / "Backend/EncounterCheckpointService/service.py").read_text(encoding="utf-8")
    for marker in REQUIRED_PROVIDER_MARKERS:
        if marker not in service:
            errors.append(f"checkpoint service missing Stage 41 marker: {marker}")

    compose = (root / "Backend/EncounterCheckpointService/compose.production.yml").read_text(encoding="utf-8")
    for marker in REQUIRED_COMPOSE_ENV:
        if marker not in compose:
            errors.append(f"production compose missing required setting: {marker}")
    if re.search(r"(?m)^\s+ports\s*:", compose):
        errors.append("production compose must not publish a host ports mapping")
    if "read_only: true" not in compose or "cap_drop:" not in compose or "no-new-privileges:true" not in compose:
        errors.append("production compose is missing container hardening")
    if "external: true" not in compose or "CHECKPOINT_INGRESS_NETWORK" not in compose:
        errors.append("production compose must attach only to the explicitly configured external ingress network")
    for secret_name in (
        "checkpoint_server_token",
        "checkpoint_signing_private_key",
        "checkpoint_database_url",
        "checkpoint_postgres_ca",
        "checkpoint_server_certificate",
        "checkpoint_server_private_key",
        "checkpoint_trusted_client_ca",
    ):
        if compose.count(f"  {secret_name}:") != 1:
            errors.append(f"production compose must declare secret {secret_name} exactly once")

    dockerfile = (root / "Backend/EncounterCheckpointService/Dockerfile").read_text(encoding="utf-8")
    if "FROM python:3.12-slim" not in dockerfile or "USER 10001:10001" not in dockerfile:
        errors.append("Docker image must use Python 3.12 slim and non-root UID/GID 10001")
    if re.search(r"(?im)^\s*COPY\s+.*(?:\.pem|\.key|\.env|\.secrets)", dockerfile):
        errors.append("Dockerfile must never copy deployment secrets or private keys into the image")

    requirements = (root / "Backend/EncounterCheckpointService/requirements.txt").read_text(encoding="utf-8")
    if "psycopg[binary]" not in requirements:
        errors.append("PostgreSQL psycopg3 dependency is missing")

    migration = (root / "Scripts/migrate_encounter_checkpoint_store.py").read_text(encoding="utf-8")
    for marker in REQUIRED_MIGRATION_MARKERS:
        if marker not in migration:
            errors.append(f"migration utility missing required safety marker: {marker}")
    if "lease_token_hash" in migration or "leaseToken" in migration:
        errors.append("migration utility must not transfer lease tokens or token hashes")

    sqlite_tests = (root / "Scripts/Tests/test_encounter_checkpoint_service.py").read_text(encoding="utf-8")
    for marker in REQUIRED_TEST_MARKERS:
        if marker not in sqlite_tests:
            errors.append(f"SQLite regression suite missing: {marker}")
    pg_tests = (root / "Scripts/Tests/test_encounter_checkpoint_postgres.py").read_text(encoding="utf-8")
    for marker in (
        "JUMANJI_TEST_POSTGRES_URL",
        "test_postgres_persists_signed_checkpoint_and_cas",
        "test_postgres_fences_old_lease_after_reacquire",
        "test_postgres_concurrent_acquire_has_one_owner",
        "test_sqlite_migration_carries_signed_checkpoint_but_not_lease",
    ):
        if marker not in pg_tests:
            errors.append(f"PostgreSQL integration suite missing: {marker}")

    backend_validator = (root / "Scripts/validate_encounter_checkpoint_backend.py").read_text(encoding="utf-8")
    if '"verification_key.verify("' not in backend_validator:
        errors.append("Stage 33 validator has not been updated for Stage 41 verification keyring")
    doc = (root / "Docs/STAGE_41_PRODUCTION_CHECKPOINT_STORE_DEPLOYMENT_HARDENING.md").read_text(encoding="utf-8")
    for marker in (
        "UNBASELINED",
        "offline import",
        "KMS/HSM",
        "restore drill",
        "mTLS",
    ):
        if marker not in doc:
            errors.append(f"Stage 41 runbook missing limitation or safety term: {marker}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())
    if errors:
        print(f"Encounter checkpoint production hardening validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Encounter checkpoint production hardening validation: PASS")
    print("PostgreSQL provider, key rotation, TLS/mTLS, migration and deployment boundaries are consistent.")
    print("This is static validation only; it does not assert production readiness or HA.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
