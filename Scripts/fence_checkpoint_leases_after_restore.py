#!/usr/bin/env python3
"""Invalidate every restored PostgreSQL checkpoint lease before service restart.

This is an offline recovery operation. It never edits signed checkpoint
envelopes or checkpoint sequence. Apply mode is restricted to an explicitly
identified staging/DR-rehearsal target and requires two independent approvals.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from uuid import uuid4

import psycopg

MAX_BIGINT = 9_223_372_036_854_775_807


class FenceError(RuntimeError):
    pass


def read_database_url() -> str:
    file_path = os.environ.get("JUMANJI_CHECKPOINT_DATABASE_URL_FILE", "").strip()
    if file_path:
        value = Path(file_path).read_text(encoding="utf-8").strip()
    else:
        value = os.environ.get("JUMANJI_CHECKPOINT_DATABASE_URL", "").strip()
    if not value:
        raise FenceError("POSTGRES_DATABASE_URL_REQUIRED")
    parsed = urlsplit(value)
    if parsed.scheme not in {"postgres", "postgresql"} or not parsed.hostname:
        raise FenceError("POSTGRES_DATABASE_URL_INVALID")
    insecure_override = os.environ.get("JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE", "").lower() == "true"
    ssl_mode = parse_qs(parsed.query).get("sslmode", [""])[0]
    if ssl_mode != "verify-full" and not insecure_override:
        raise FenceError("POSTGRES_SSLMODE_VERIFY_FULL_REQUIRED")
    return value


def _inspect_rows(connection) -> dict:
    migration = connection.execute(
        "SELECT COALESCE(MAX(version), 0) AS schema_version FROM checkpoint_schema_migrations"
    ).fetchone()
    if not migration or int(migration["schema_version"]) < 1:
        raise FenceError("CHECKPOINT_SCHEMA_VERSION_UNAVAILABLE")

    row = connection.execute(
        """
        SELECT
            COUNT(*) AS row_count,
            COALESCE(MAX(authority_epoch), 0) AS max_epoch,
            COUNT(*) FILTER (WHERE authority_epoch >= %s) AS epoch_overflow_rows,
            COUNT(*) FILTER (WHERE lease_token_hash IS NOT NULL) AS lease_owner_rows
        FROM encounter_checkpoint_state
        """,
        (MAX_BIGINT,),
    ).fetchone()
    if not row:
        raise FenceError("CHECKPOINT_STATE_TABLE_UNAVAILABLE")

    summary = {
        "schemaVersion": int(migration["schema_version"]),
        "rowCount": int(row["row_count"]),
        "maxAuthorityEpochBefore": int(row["max_epoch"]),
        "epochOverflowRows": int(row["epoch_overflow_rows"]),
        "leaseOwnerRowsBefore": int(row["lease_owner_rows"]),
    }
    if summary["epochOverflowRows"] > 0:
        raise FenceError("AUTHORITY_EPOCH_OVERFLOW")
    return summary


def fence_restored_target(connection, apply: bool = False) -> dict:
    """Dry-run or fence all restored rows within one transaction.

    Caller must already have stopped every checkpoint service writer and
    blocked staging ingress. ACCESS EXCLUSIVE prevents concurrent SQL writes
    while the fencing update runs; it does not stop old app servers from
    reconnecting after commit, so ingress must remain blocked until completion.
    """
    with connection.transaction():
        connection.execute("LOCK TABLE encounter_checkpoint_state IN ACCESS EXCLUSIVE MODE")
        summary = _inspect_rows(connection)
        report = {
            "schema": "JUMANJI.CheckpointRestoreFencingReport.v1",
            "drillId": str(uuid4()),
            "mode": "apply" if apply else "dry-run",
            **summary,
            "rowsFenced": 0,
            "leaseOwnerRowsInvalidated": 0,
            "maxAuthorityEpochAfter": summary["maxAuthorityEpochBefore"],
            "checkpointRowsPreserved": summary["rowCount"],
            "completedAtUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        }

        if apply:
            changed = connection.execute(
                """
                UPDATE encounter_checkpoint_state
                SET authority_epoch = authority_epoch + 1,
                    lease_server_instance_id = NULL,
                    lease_shard_id = NULL,
                    lease_token_hash = NULL,
                    lease_expires_at = 0
                WHERE authority_epoch < %s
                RETURNING authority_epoch
                """,
                (MAX_BIGINT,),
            ).fetchall()
            report["rowsFenced"] = len(changed)
            report["leaseOwnerRowsInvalidated"] = summary["leaseOwnerRowsBefore"]
            report["maxAuthorityEpochAfter"] = max(
                (int(row["authority_epoch"]) for row in changed),
                default=summary["maxAuthorityEpochBefore"],
            )
            if report["rowsFenced"] != summary["rowCount"]:
                raise FenceError("FENCING_ROWCOUNT_MISMATCH")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Increment all fencing epochs and clear restored lease owners.")
    parser.add_argument("--confirm-restored-target", action="store_true", help="Confirm the database is an isolated restored staging/DR target.")
    parser.add_argument("--report", type=Path, default=Path("checkpoint-restore-fencing-report.json"))
    args = parser.parse_args()

    report = None
    try:
        environment_class = os.environ.get("JUMANJI_CHECKPOINT_RESTORE_ENVIRONMENT_CLASS", "").strip()
        environment_id = os.environ.get("JUMANJI_CHECKPOINT_RESTORE_ENVIRONMENT_ID", "").strip()
        if environment_class not in {"staging", "dr-rehearsal"} or not environment_id:
            raise FenceError("RESTORE_TARGET_ENVIRONMENT_ID_REQUIRED")
        if environment_class.casefold() in {"production", "prod", "live"}:
            raise FenceError("PRODUCTION_TARGET_NOT_ALLOWED")
        if args.apply and not args.confirm_restored_target:
            raise FenceError("APPLY_REQUIRES_EXPLICIT_RESTORED_TARGET_CONFIRMATION")

        database_url = read_database_url()
        with psycopg.connect(database_url, autocommit=True, row_factory=psycopg.rows.dict_row) as connection:
            report = fence_restored_target(connection, apply=args.apply)
        report["environmentClass"] = environment_class
        report["environmentId"] = environment_id
        report["result"] = "PASS"
    except (FenceError, OSError, UnicodeError, psycopg.Error) as error:
        report = {
            "schema": "JUMANJI.CheckpointRestoreFencingReport.v1",
            "mode": "apply" if args.apply else "dry-run",
            "result": "FAIL",
            "failureCode": error.args[0] if isinstance(error, FenceError) else "DATABASE_OPERATION_FAILED",
            "completedAtUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        }

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Checkpoint restore fencing: {report['result']} ({report['mode']})")
    print(f"Report: {args.report}")
    if report["result"] != "PASS":
        print(f"Failure code: {report.get('failureCode', 'UNKNOWN')}", file=sys.stderr)
        return 1
    if not args.apply:
        print("DRY RUN ONLY: no lease rows were changed.")
    else:
        print(
            f"Fenced {report['rowsFenced']} restored rows; "
            f"invalidated {report['leaseOwnerRowsInvalidated']} lease owners."
        )
        print("Checkpoint sequence and signed envelope columns were not modified.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
