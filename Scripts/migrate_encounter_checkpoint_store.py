#!/usr/bin/env python3
"""Import verified, unexpired SQLite checkpoint envelopes into the PostgreSQL provider.

The tool copies logical checkpoint rows and monotonic counters only. It never
copies active lease tokens, token hashes, lease owners or live server authority.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from Backend.EncounterCheckpointService.service import EncounterCheckpointBackend  # noqa: E402


def migrate_sqlite_store(
    source_path: Path,
    destination: EncounterCheckpointBackend,
    *,
    dry_run: bool = False,
) -> dict[str, Any]:
    if destination.storage_kind != "postgresql":
        raise ValueError("Destination provider must be PostgreSQL")
    source_path = source_path.resolve()
    if not source_path.is_file():
        raise FileNotFoundError("Source SQLite database file does not exist")

    digest_builder = hashlib.sha256()
    with source_path.open("rb") as source_stream:
        for chunk in iter(lambda: source_stream.read(1024 * 1024), b""):
            digest_builder.update(chunk)
    digest = digest_builder.hexdigest()
    uri = f"file:{source_path.as_posix()}?mode=ro"
    connection = sqlite3.connect(uri, uri=True, timeout=10.0)
    connection.row_factory = sqlite3.Row
    imported = 0
    validated_in_dry_run = 0
    already_present = 0
    skipped_empty = 0
    try:
        columns = {
            row["name"]
            for row in connection.execute("PRAGMA table_info(encounter_checkpoint_state)").fetchall()
        }
        required = {
            "encounter_id", "checkpoint_sequence", "authority_epoch",
            "checkpoint_envelope_json", "checkpoint_expires_at",
        }
        if not required.issubset(columns):
            raise RuntimeError("Source SQLite database does not match the Stage 33 checkpoint schema")

        rows = connection.execute(
            """
            SELECT encounter_id, checkpoint_sequence, authority_epoch, checkpoint_envelope_json
            FROM encounter_checkpoint_state
            ORDER BY encounter_id
            """
        ).fetchall()

        for row in rows:
            envelope_json = row["checkpoint_envelope_json"]
            if not envelope_json:
                skipped_empty += 1
                continue
            if dry_run:
                # Perform the same integrity/contract checks without writing.
                envelope = json.loads(envelope_json)
                payload = envelope["payload"]
                if (
                    payload.get("encounterId") != row["encounter_id"]
                    or payload.get("checkpointSequence") != row["checkpoint_sequence"]
                    or payload.get("authority", {}).get("authorityEpoch") != row["authority_epoch"]
                ):
                    raise RuntimeError("SQLite row metadata does not match its signed checkpoint payload")
                errors = destination.validate_import_envelope(envelope)
                if errors:
                    raise RuntimeError(f"Checkpoint {row['encounter_id']} failed verification: {errors[0]}")
                validated_in_dry_run += 1
                continue

            did_import = destination.import_checkpoint_record(
                row["encounter_id"],
                int(row["checkpoint_sequence"]),
                int(row["authority_epoch"]),
                envelope_json,
            )
            if did_import:
                imported += 1
            else:
                already_present += 1
    finally:
        connection.close()

    return {
        "sourceDatabaseSha256": digest,
        "sourceRows": len(rows),
        "imported": imported,
        "validatedInDryRun": validated_in_dry_run,
        "alreadyPresent": already_present,
        "skippedWithoutCheckpoint": skipped_empty,
        "dryRun": dry_run,
        "activeLeasesTransferred": 0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-sqlite", type=Path, required=True)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    try:
        destination, _server_token = EncounterCheckpointBackend.from_environment()
        if destination.storage_kind != "postgresql":
            raise RuntimeError("Configure JUMANJI_CHECKPOINT_DATABASE_URL for the PostgreSQL destination")
        report = migrate_sqlite_store(args.source_sqlite, destination, dry_run=args.dry_run)
    except (OSError, ValueError, RuntimeError, sqlite3.Error) as error:
        print(f"Checkpoint store migration failed: {error}", file=sys.stderr)
        return 2
    except Exception as error:
        if error.__class__.__module__.startswith("psycopg"):
            print("Checkpoint store migration failed: PostgreSQL destination is unavailable", file=sys.stderr)
        else:
            print("Checkpoint store migration failed: source rows could not be safely imported", file=sys.stderr)
        return 2

    rendered = json.dumps(report, indent=2, sort_keys=True)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
