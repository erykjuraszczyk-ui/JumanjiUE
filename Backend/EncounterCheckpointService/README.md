# Encounter Checkpoint Service — Stage 33 prototype

This is a **trusted server-to-server prototype**, not a public game API and not a production multi-region service.

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
