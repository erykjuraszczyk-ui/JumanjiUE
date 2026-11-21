# Encounter Checkpoint Service — Stage 33 backend + Stage 41 deployment hardening

This is a **trusted server-to-server service foundation**, not a public game API. Stage 33 SQLite remains a single-host prototype; Stage 41 adds a PostgreSQL provider and hardened deployment template but does not claim production HA or approved SLO.

## What it implements

- Server-authenticated HTTP routes for acquire, renew, release, write and read.
- Monotonic fencing epoch on every successful lease acquisition.
- Bounded lease TTL (15–120 seconds) and opaque lease tokens stored only as SHA-256 hashes.
- SQLite BEGIN IMMEDIATE transactions for single-host lease exclusion and checkpoint compare-and-swap.
- Strict Stage 32 payload validation before storage.
- Ed25519 signing of the envelope after RFC 8785 JSON canonicalization.
- Signature verification, current-key check and expiry validation on read.
- No client endpoint, profile secret, wallet balance or inventory data.

The private signing key is loaded from a file path supplied to the **server process environment**. It is never stored in the game client, DefaultGame.ini or this repository.

## Local development

Use Python 3.12 or newer.

~~~bash
python -m venv .venv
# Linux/macOS:
source .venv/bin/activate
# Windows PowerShell:
# .venv\Scripts\Activate.ps1

python -m pip install -r Backend/EncounterCheckpointService/requirements.txt
python Backend/EncounterCheckpointService/generate_dev_key.py
~~~

Configure the local terminal environment. Do not put actual values in a checked-in file:

~~~bash
export JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE="$PWD/.secrets/checkpoint-ed25519-dev.pem"
export JUMANJI_CHECKPOINT_KEY_ID="LOCAL-DEV-KEY-01"
export JUMANJI_CHECKPOINT_SERVER_TOKEN="replace-with-a-random-secret-of-at-least-32-characters"
export JUMANJI_CHECKPOINT_DB="$PWD/.secrets/checkpoints.sqlite3"
export JUMANJI_CHECKPOINT_BIND_HOST="127.0.0.1"
export JUMANJI_CHECKPOINT_BIND_PORT="8787"

python Backend/EncounterCheckpointService/service.py
~~~

In PowerShell, use $env:NAME = "value" for each environment variable instead of export.

The default bind address is loopback. Keep the service private. A remote deployment must sit behind a private network and an mTLS-terminating reverse proxy; the bearer token here is a prototype application credential, not a replacement for mTLS, service identity, rate limiting or secret rotation.

Health probe:

~~~bash
curl http://127.0.0.1:8787/healthz
~~~

## API

All /v1 routes require Authorization: Bearer <server-token>. Every request uses JSON. The client app must never call these routes.

| Method | Route | Purpose |
|---|---|---|
| POST | /v1/encounters/{EncounterId}/leases | Acquire a lease and receive a new fencing epoch |
| POST | /v1/encounters/{EncounterId}/leases/renew | Renew the active lease without changing its epoch |
| POST | /v1/encounters/{EncounterId}/leases/release | Release the active lease |
| PUT | /v1/encounters/{EncounterId}/checkpoint | Store the next checkpoint sequence using CAS |
| POST | /v1/encounters/{EncounterId}/checkpoint/read | Read the latest checkpoint while holding the lease |

EncounterId must be a stable uppercase content ID. serverInstanceId and shardId are server-owned values, not client input.

A successful acquire returns an opaque leaseToken and authorityEpoch. The service stores only a hash of the token. The active lease excludes another server process. When a lease expires or is released, the next acquire increments authorityEpoch; every old process is fenced out even if it still has a stale network connection.

Checkpoint writes require expectedSequence and a payload with checkpointSequence = expectedSequence + 1. The payload's encounter, server, shard and epoch must match the route and active lease. Any stale sequence is rejected with HTTP 409.

The response envelope follows the Stage 32 v1 contract. The service signs the canonical envelope with integrity.signature omitted and integrity.algorithm/integrity.keyId retained, then includes the Base64URL Ed25519 signature. Reads verify that signature before returning the payload.

## Unreal server adapter

UJumanjiEncounterCheckpointBackendSubsystem is a native-only GameInstanceSubsystem. It rejects calls from NM_Client, reads its bearer token and server identity from process environment variables, and exposes asynchronous native C++ methods. It is not Blueprint-callable and must be used only by trusted server code.

Set the service URL in the server's DefaultGame.ini:

~~~ini
[/Script/JumanjiUE.JumanjiEncounterCheckpointBackendSubsystem]
ServiceBaseUrl=http://127.0.0.1:8787
RequestTimeoutSeconds=8.0
~~~

For a remote endpoint use HTTPS. Plain HTTP is allowed only for loopback development addresses.

Required server environment:

- JUMANJI_CHECKPOINT_SERVER_TOKEN
- JUMANJI_SERVER_INSTANCE_ID
- JUMANJI_SHARD_ID

The signing key belongs to the checkpoint service process, not the Unreal server adapter. The adapter never receives the private key.

The current Stage 33 adapter sends and receives payload JSON but does not create a safe-point snapshot from live Unreal actors. Snapshot construction, restore staging, profile-service verification and publication barriers remain later work.

## Storage and production boundary

SQLite WAL and BEGIN IMMEDIATE are suitable for a single-host prototype and concurrency tests. They do not provide multi-host or cross-region consensus. Production requires a shared transactional store with a true conditional write, strongly consistent lease ownership, atomic fencing checks, durable backups, KMS/HSM key management, key rotation, server mTLS identity, audit events and operational monitoring.

No endpoint performs a reward grant. RewardPending must still be reconciled against the authoritative profile transaction ledger before a recovered encounter is published.


## Stage 41 — shared PostgreSQL provider

SQLite WAL remains the default when only `JUMANJI_CHECKPOINT_DB` is configured. It is intended for local development, unit tests and a single-host prototype.

To select PostgreSQL, configure `JUMANJI_CHECKPOINT_DATABASE_URL` or `JUMANJI_CHECKPOINT_DATABASE_URL_FILE`. The Psycopg 3 provider:
- uses per-operation database connections;
- wraps lease/CAS operations in explicit transactions;
- locks the encounter row with `SELECT ... FOR UPDATE`;
- preserves the Stage 33 fencing epoch and monotonic checkpoint sequence;
- stores sequence/epoch as BIGINT and timestamps as DOUBLE PRECISION;
- initializes the `checkpoint_schema_migrations` version table;
- requires `sslmode=verify-full` unless `JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE=true` is set in an isolated development/test environment.

The DSN supplied through the secret file must include the PostgreSQL CA path, for example:

```text
postgresql://checkpoint_user:...@postgres.example.internal:5432/jumanji_checkpoint?sslmode=verify-full&sslrootcert=/run/secrets/checkpoint-postgres-ca.pem
```

Do not put the database password in a checked-in file or Compose environment literal. Use the deployment platform's secret manager and mount the DSN as a read-only file.

## Signing-key overlap rotation

The active signing key is configured by:
- `JUMANJI_CHECKPOINT_SIGNING_PRIVATE_KEY_FILE`;
- `JUMANJI_CHECKPOINT_KEY_ID`.

The optional `JUMANJI_CHECKPOINT_VERIFY_KEYRING_FILE` contains old public Ed25519 keys:

```json
{
  "keys": [
    {
      "keyId": "PREVIOUS-KEY-ID",
      "publicKeyFile": "/run/config/previous-public-key.pem"
    }
  ]
}
```

The active public key is added automatically. Do not repeat its key ID in the keyring. During a rotation, deploy the previous public key to all service instances before activating the new signing key. Keep the old public key until all checkpoints signed with it have expired and all required restore/backup windows have passed. The private signing key is never distributed to the Unreal adapter or game client. KMS/HSM integration and automated key management are not implemented in Stage 41.

Template: `Data/Encounter/EncounterCheckpointVerificationKeyringTemplate.json`. It is deliberately empty; do not commit deployment keys.

## TLS and mutual TLS

The local loopback development mode may run without TLS. A non-loopback bind is rejected unless direct TLS and mandatory client-certificate verification are configured.

Server process variables:
- `JUMANJI_CHECKPOINT_TLS_CERT_FILE`;
- `JUMANJI_CHECKPOINT_TLS_KEY_FILE`;
- `JUMANJI_CHECKPOINT_MTLS_CA_FILE`;
- `JUMANJI_CHECKPOINT_MTLS_REQUIRED=true`.

TLS minimum is 1.2. The /v1 routes additionally require the existing bearer token; mTLS and bearer authentication are separate controls. `/healthz` reports process liveness and `/readyz` checks actual storage connectivity. Both probes disclose only a generic status.

This prototype validates the client certificate against a configured CA, but does not yet map certificate SAN/subject to per-server or per-shard authorization policies. The bearer token is still a service credential and requires external secret rotation.

## Bounded HTTP concurrency

Configure `JUMANJI_CHECKPOINT_MAX_CONCURRENCY` to a value from 1 to 1024. The production Compose file requires an explicit value; it does not select a production capacity target. When request slots are exhausted, the listener rejects new connections with HTTP 503. This is a process safety ceiling, not a claim about supported concurrent encounters. Choose it only after Stage 39-style measurements on representative PostgreSQL, network and Unreal traffic.

The listener also sets a 10-second socket timeout and retains the 1 MiB JSON request limit.

## Docker image and private Compose template

Files:
- `Backend/EncounterCheckpointService/Dockerfile`;
- `Backend/EncounterCheckpointService/compose.production.yml`;
- `.dockerignore`.

The image runs as UID/GID 10001. Compose enables a read-only root filesystem, drops Linux capabilities, enables no-new-privileges, limits PIDs and mounts secrets read-only. It does not publish a host `ports:` mapping. The service is attached only to an explicitly named external ingress network.

Before running Compose, configure these operator variables with real file paths and reviewed limits:
- `CHECKPOINT_MAX_CONCURRENCY`;
- `CHECKPOINT_ACTIVE_KEY_ID`;
- `CHECKPOINT_VERIFY_KEYRING_DIRECTORY`;
- `CHECKPOINT_SERVER_TOKEN_FILE`;
- `CHECKPOINT_SIGNING_PRIVATE_KEY_FILE`;
- `CHECKPOINT_DATABASE_URL_FILE`;
- `CHECKPOINT_POSTGRES_CA_FILE`;
- `CHECKPOINT_SERVER_CERTIFICATE_FILE`;
- `CHECKPOINT_SERVER_PRIVATE_KEY_FILE`;
- `CHECKPOINT_TRUSTED_CLIENT_CA_FILE`;
- `CHECKPOINT_PROBE_CLIENT_CERTIFICATE_FILE`;
- `CHECKPOINT_PROBE_CLIENT_PRIVATE_KEY_FILE`;
- `CHECKPOINT_SERVICE_CA_FILE`;
- `CHECKPOINT_INGRESS_NETWORK`.

The keyring directory must contain `checkpoint-verification-keyring.json` and the referenced public PEM files. The database DSN file must use `sslmode=verify-full` and point `sslrootcert` at `/run/secrets/checkpoint-postgres-ca.pem`.

The Compose template assumes an existing private ingress network and external PostgreSQL service. It does not provision a database cluster, load balancer, firewall, DNS, certificate authority, secret manager, backups or HA/failover.

## Offline SQLite-to-PostgreSQL import

Tool: `Scripts/migrate_encounter_checkpoint_store.py`.

This is an offline migration. Stop every source writer, checkpoint SQLite WAL, run an integrity check and create an immutable backup before proceeding. Configure the PostgreSQL destination plus the new active signing key and a verification keyring that contains each old key used by imported checkpoint envelopes.

Dry-run first:

```bash
python Scripts/migrate_encounter_checkpoint_store.py \
  --source-sqlite /secure/backup/checkpoints.sqlite3 \
  --dry-run \
  --report /secure/reports/checkpoint-migration-dry-run.json
```

Then import:

```bash
python Scripts/migrate_encounter_checkpoint_store.py \
  --source-sqlite /secure/backup/checkpoints.sqlite3 \
  --report /secure/reports/checkpoint-migration.json
```

The importer verifies Stage 32 schema, TTL, key ID and Ed25519 signature; confirms row metadata matches the signed payload; and transfers only the signed envelope, checkpoint sequence, authority epoch and expiry. Active lease owners, lease token hashes and raw lease tokens are never copied. The first new PostgreSQL lease therefore advances the preserved epoch. Identical existing destination rows are idempotent; different rows cause a conflict.

Do not dual-write to SQLite and PostgreSQL during cutover.

## PostgreSQL backup and restore

Stage 41 supplies a runbook, not a backup scheduler. Configure managed backups, WAL archiving and PITR in the chosen PostgreSQL platform. Use a least-privilege backup role and a restricted `PGPASSFILE`; do not put DB passwords in process arguments.

Example after configuring PGHOST/PGPORT/PGUSER/PGDATABASE, TLS and PGPASSFILE:

```bash
pg_dump --no-password --format=custom --no-owner \
  --file=/secure/backup/jumanji-checkpoint.dump

pg_restore --no-password --no-owner \
  --dbname="$PGDATABASE" \
  /secure/backup/jumanji-checkpoint.dump
```

Restore first into an isolated staging database. Verify readiness, signed checkpoint readback, sequence and fencing. Never restore active lease ownership or tokens. Perform and record regular restore drills before treating backups as an operational control.

For SQLite development backups, use the SQLite online backup API or stop writers and checkpoint WAL before copying the database. Do not copy only the main file while WAL writes are active.

## Stage 41 tests and limitations

Run:

```bash
python Scripts/validate_encounter_checkpoint_backend.py
python Scripts/validate_encounter_checkpoint_production.py
python -m unittest discover -s Scripts/Tests -p "test_encounter_checkpoint_service.py" -v
python -m unittest discover -s Scripts/Tests -p "test_encounter_checkpoint_postgres.py" -v
python -m unittest discover -s Scripts/Tests -p "test_validate_encounter_checkpoint_production.py" -v
```

PostgreSQL integration tests require `JUMANJI_TEST_POSTGRES_URL` pointing to a disposable database. They are skipped if the variable is absent. Insecure TLS settings may be used only for the isolated test container and must never be copied into production deployment settings.

Stage 41 does not provide multi-region consensus, managed PostgreSQL HA/failover, connection pooling, KMS/HSM, automated key/token rotation, external audit storage, completed backup restore drills, approved SLO or production capacity numbers. No real staging deployment, Unreal Engine 5.8 build, Dedicated Server, PIE multiplayer or Android package is claimed by this repository update.
