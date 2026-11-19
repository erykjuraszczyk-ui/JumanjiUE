#!/usr/bin/env python3
"""Validate the Stage 32 encounter checkpoint contract and recovery matrix.

Dependency-free structural and semantic checks only. This script does not verify
Ed25519 signatures, contact a backend, compile Unreal C++, or resume a server.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sys
from datetime import datetime
from pathlib import Path
from uuid import UUID

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_PATH = Path("Data/Encounter/EncounterCheckpoint.schema.json")
EXAMPLE_PATH = Path("Data/Encounter/EncounterCheckpointExample.json")
MATRIX_PATH = Path("Data/Encounter/EncounterCheckpointRecoveryMatrix.csv")

MATRIX_COLUMNS = (
    "CaseId",
    "Mutation",
    "ExpectedDecision",
    "ExpectedReason",
    "ValidationLayer",
)
REQUIRED_CASES = {
    "ACCEPT_TRIAL_BOUNDARY": ("ACCEPT", "ValidSafePoint"),
    "ACCEPT_WAVE_BOUNDARY": ("ACCEPT", "ValidSafePoint"),
    "ACCEPT_BOSS_PHASE": ("ACCEPT", "ValidSafePoint"),
    "ACCEPT_REWARD_PENDING": ("ACCEPT", "LedgerLookupRequired"),
    "REJECT_SCHEMA_VERSION": ("REJECT", "UnsupportedSchemaVersion"),
    "REJECT_EXPIRED": ("REJECT", "CheckpointExpired"),
    "REJECT_SEQUENCE_ROLLBACK": ("REJECT", "SequenceNotMonotonic"),
    "REJECT_SIGNATURE": ("REJECT", "InvalidSignature"),
    "REJECT_AUTHORITY_EPOCH": ("REJECT", "StaleAuthorityEpoch"),
    "REJECT_RULESET": ("REJECT", "IncompatibleRuleset"),
    "REJECT_UNSAFE_POINT": ("REJECT", "NotARecoveryBarrier"),
    "REJECT_ACTOR_REFERENCE": ("REJECT", "ForbiddenRuntimeReference"),
    "REJECT_RAW_PROFILE_ID": ("REJECT", "SensitiveIdentityData"),
    "REJECT_REWARD_IDEMPOTENCY": ("REJECT", "MissingIdempotencyKey"),
    "REJECT_DUPLICATE_OWNER": ("REJECT", "RecoveryLeaseAlreadyHeld"),
}
ALLOWED_PHASES = {
    "Trials": "TrialBoundary",
    "Combat": "WaveBoundary",
    "Boss": "BossPhaseBoundary",
    "Reward": "RewardPending",
}
FORBIDDEN_KEYS = {
    "actor", "actorref", "netguid", "uobject", "objectpath",
    "transform", "location", "velocity", "blackboard", "timer",
    "latentaction", "accesstoken", "refreshtoken", "apikey",
    "secret", "password", "profileid", "rawprofileid",
    "walletbalance", "inventory", "clientpayload", "privatekey",
}
ID_PATTERN = re.compile(r"^[A-Z][A-Z0-9_]{2,95}$")
PARTICIPANT_PATTERN = re.compile(r"^pk_[A-Za-z0-9_-]{20,96}$")
SIGNATURE_PATTERN = re.compile(r"^[A-Za-z0-9_-]{64,256}$")
MAX_TTL_SECONDS = 24 * 60 * 60


def _exact_keys(value: object, required: set[str], optional: set[str] | None = None) -> bool:
    if not isinstance(value, dict):
        return False
    optional = optional or set()
    return required.issubset(value) and set(value).issubset(required | optional)


def _is_int(value: object) -> bool:
    if type(value) is int:
        return True
    # RFC 8259 permits JSON number formatting such as 1.0; Draft 2020-12
    # treats it as an integer when its mathematical value is integral.
    # Bound float inputs to IEEE-754's exact integer range to avoid rounded IDs.
    return (
        type(value) is float
        and math.isfinite(value)
        and value.is_integer()
        and abs(value) <= 9007199254740991
    )


def _is_finite_number(value: object) -> bool:
    return type(value) in (int, float) and math.isfinite(float(value))


def _parse_utc(value: object) -> datetime | None:
    if not isinstance(value, str):
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
    return parsed if parsed.tzinfo is not None else None


def _find_forbidden_keys(value: object, path: str = "$") -> list[str]:
    errors: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            normalized = re.sub(r"[^a-z0-9]", "", str(key).casefold())
            sensitive_suffixes = (
                "actorref", "netguid", "objectpath", "profileid",
                "accesstoken", "refreshtoken", "privatekey",
            )
            if normalized in FORBIDDEN_KEYS or normalized.endswith(sensitive_suffixes):
                errors.append(f"{path}.{key}: forbidden field")
            errors.extend(_find_forbidden_keys(child, f"{path}.{key}"))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(_find_forbidden_keys(child, f"{path}[{index}]"))
    return errors


def validate_payload(payload: object, now: datetime | None = None) -> list[str]:
    errors: list[str] = []
    required = {
        "schemaVersion", "checkpointId", "encounterId", "sessionId",
        "checkpointSequence", "createdAtUtc", "expiresAtUtc", "authority",
        "safePoint", "deterministic", "participants",
    }
    if not isinstance(payload, dict):
        return ["payload must be an object"]
    if not _exact_keys(payload, required, {"reward"}):
        errors.append("payload has missing or unsupported fields")
    if payload.get("schemaVersion") != 1 or not _is_int(payload.get("schemaVersion")):
        errors.append("schemaVersion must be 1")
    for field in ("checkpointId", "sessionId"):
        try:
            UUID(str(payload.get(field, "")))
        except (ValueError, TypeError, AttributeError):
            errors.append(f"{field} must be a UUID")
    if not isinstance(payload.get("encounterId"), str) or not ID_PATTERN.fullmatch(payload["encounterId"]):
        errors.append("encounterId must be a stable uppercase content ID")
    if not _is_int(payload.get("checkpointSequence")) or payload["checkpointSequence"] < 1:
        errors.append("checkpointSequence must be a positive integer")

    created = _parse_utc(payload.get("createdAtUtc"))
    expires = _parse_utc(payload.get("expiresAtUtc"))
    if created is None or expires is None:
        errors.append("createdAtUtc and expiresAtUtc must be timezone-aware ISO-8601 timestamps")
    elif expires <= created or (expires - created).total_seconds() > MAX_TTL_SECONDS:
        errors.append("checkpoint TTL must be positive and no longer than 24 hours")
    elif now is not None and expires <= now:
        errors.append("checkpoint has expired according to trusted validation time")

    authority = payload.get("authority")
    authority_required = {
        "serverInstanceId", "shardId", "authorityEpoch",
        "serverBuildId", "rulesetVersion", "worldContentVersion",
    }
    if not _exact_keys(authority, authority_required):
        errors.append("authority has missing or unsupported fields")
    else:
        for field in ("serverInstanceId", "shardId", "serverBuildId", "rulesetVersion", "worldContentVersion"):
            if not isinstance(authority[field], str) or not authority[field].strip():
                errors.append(f"authority.{field} must be a non-empty string")
        if not _is_int(authority["authorityEpoch"]) or authority["authorityEpoch"] < 1:
            errors.append("authority.authorityEpoch must be a positive integer")

    safe = payload.get("safePoint")
    safe_required = {"phase", "kind", "encounterRevision", "chapter", "combat"}
    phase = safe.get("phase") if isinstance(safe, dict) else None
    if not _exact_keys(safe, safe_required):
        errors.append("safePoint has missing or unsupported fields")
    else:
        kind = safe.get("kind")
        if phase not in ALLOWED_PHASES:
            errors.append("safePoint.phase is not resumable in schema v1")
        elif kind != ALLOWED_PHASES[phase]:
            errors.append(f"safePoint.kind does not match phase {phase}")
        if not _is_int(safe.get("encounterRevision")) or safe["encounterRevision"] < 1:
            errors.append("safePoint.encounterRevision must be a positive integer")

        chapter = safe.get("chapter")
        chapter_required = {
            "chapterId", "boardEventId", "chapterRevision",
            "completedObjectiveIds", "completedTrialIds",
        }
        if not _exact_keys(chapter, chapter_required):
            errors.append("safePoint.chapter has missing or unsupported fields")
        else:
            for field in ("chapterId", "boardEventId"):
                if not isinstance(chapter[field], str) or not ID_PATTERN.fullmatch(chapter[field]):
                    errors.append(f"safePoint.chapter.{field} must be a stable content ID")
            if not _is_int(chapter["chapterRevision"]) or chapter["chapterRevision"] < 1:
                errors.append("chapterRevision must be a positive integer")
            for field, max_items in (("completedObjectiveIds", 64), ("completedTrialIds", 9)):
                values = chapter[field]
                if not isinstance(values, list) or len(values) > max_items:
                    errors.append(f"safePoint.chapter.{field} must be an array with at most {max_items} items")
                elif any(not isinstance(v, str) or not ID_PATTERN.fullmatch(v) for v in values):
                    errors.append(f"safePoint.chapter.{field} contains an invalid content ID")
                elif len(values) != len(set(values)):
                    errors.append(f"safePoint.chapter.{field} contains duplicate IDs")

        combat = safe.get("combat")
        if not isinstance(combat, dict) or not _exact_keys(combat, {"currentWave", "totalWaves"}, {"boss"}):
            errors.append("safePoint.combat has missing or unsupported fields")
        else:
            current = combat["currentWave"]
            total = combat["totalWaves"]
            if not _is_int(current) or not _is_int(total) or total < 0 or total > 100 or current < 0 or current > total:
                errors.append("combat wave values must be integers with 0 <= currentWave <= totalWaves")
            if phase == "Combat" and _is_int(current) and current < 1:
                errors.append("Combat checkpoint requires a one-based currentWave")
            if phase in {"Trials", "Boss", "Reward"} and current != 0:
                errors.append(f"{phase} checkpoint requires currentWave=0")
            boss = combat.get("boss")
            if phase == "Boss":
                if not isinstance(boss, dict) or not _exact_keys(
                    boss, {"definitionId", "phaseId", "healthFraction", "phaseRevision"}
                ):
                    errors.append("Boss checkpoint requires a bounded logical boss snapshot")
                else:
                    for field in ("definitionId", "phaseId"):
                        if not isinstance(boss[field], str) or not ID_PATTERN.fullmatch(boss[field]):
                            errors.append(f"boss.{field} must be a stable content ID")
                    if not _is_finite_number(boss["healthFraction"]) or not 0 <= boss["healthFraction"] <= 1:
                        errors.append("boss.healthFraction must be finite and between 0 and 1")
                    if not _is_int(boss["phaseRevision"]) or boss["phaseRevision"] < 1:
                        errors.append("boss.phaseRevision must be a positive integer")
            elif boss is not None:
                errors.append("boss snapshot is allowed only at BossPhaseBoundary")

    deterministic = payload.get("deterministic")
    if not _exact_keys(deterministic, {"encounterSeed", "spawnSetId"}):
        errors.append("deterministic has missing or unsupported fields")
    else:
        if not _is_int(deterministic["encounterSeed"]) or not 0 <= deterministic["encounterSeed"] <= 4294967295:
            errors.append("encounterSeed must be an unsigned 32-bit integer")
        if not isinstance(deterministic["spawnSetId"], str) or not ID_PATTERN.fullmatch(deterministic["spawnSetId"]):
            errors.append("spawnSetId must be a stable allowlisted content ID")

    participants = payload.get("participants")
    if not isinstance(participants, list) or not 1 <= len(participants) <= 64:
        errors.append("participants must contain between 1 and 64 entries")
    else:
        keys: list[str] = []
        for index, participant in enumerate(participants):
            if not _exact_keys(participant, {"participantKey", "profileRevision"}):
                errors.append(f"participants[{index}] has missing or unsupported fields")
                continue
            key = participant["participantKey"]
            if not isinstance(key, str) or not PARTICIPANT_PATTERN.fullmatch(key):
                errors.append(f"participants[{index}].participantKey must be an opaque backend-issued key")
            else:
                keys.append(key)
            if not _is_int(participant["profileRevision"]) or participant["profileRevision"] < 0:
                errors.append(f"participants[{index}].profileRevision must be a non-negative integer")
        if len(keys) != len(set(keys)):
            errors.append("participants contains duplicate participantKey values")

    reward = payload.get("reward")
    is_reward_phase = isinstance(safe, dict) and safe.get("phase") == "Reward"
    if is_reward_phase:
        if not _exact_keys(reward, {"rewardDefinitionId", "transactionId", "status"}):
            errors.append("Reward checkpoint requires rewardDefinitionId and idempotent transactionId")
        else:
            if not isinstance(reward["rewardDefinitionId"], str) or not ID_PATTERN.fullmatch(reward["rewardDefinitionId"]):
                errors.append("reward.rewardDefinitionId must be a stable content ID")
            if not isinstance(reward["transactionId"], str) or not ID_PATTERN.fullmatch(reward["transactionId"]):
                errors.append("reward.transactionId must be a stable idempotency key")
            if reward["status"] != "PendingLedgerCheck":
                errors.append("reward.status must be PendingLedgerCheck")
    elif reward is not None:
        errors.append("reward object is permitted only in Reward phase")

    return errors


def validate_envelope(envelope: object, now: datetime | None = None) -> list[str]:
    if not _exact_keys(envelope, {"envelopeVersion", "payload", "integrity"}):
        return ["envelope has missing or unsupported fields"]
    errors: list[str] = []
    if envelope["envelopeVersion"] != 1 or not _is_int(envelope["envelopeVersion"]):
        errors.append("envelopeVersion must be 1")
    errors.extend(validate_payload(envelope["payload"], now=now))
    integrity = envelope["integrity"]
    if not _exact_keys(integrity, {"algorithm", "keyId", "signature"}):
        errors.append("integrity has missing or unsupported fields")
    else:
        if integrity["algorithm"] != "Ed25519":
            errors.append("integrity.algorithm must be Ed25519")
        if not isinstance(integrity["keyId"], str) or not integrity["keyId"].strip():
            errors.append("integrity.keyId must be non-empty")
        if not isinstance(integrity["signature"], str) or not SIGNATURE_PATTERN.fullmatch(integrity["signature"]):
            errors.append("integrity.signature must be base64url-shaped text of 64-256 characters")
    errors.extend(_find_forbidden_keys(envelope))
    return errors


def validate_schema(schema: object) -> list[str]:
    if not isinstance(schema, dict):
        return ["JSON Schema root must be an object"]
    errors: list[str] = []
    if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        errors.append("schema must declare JSON Schema Draft 2020-12")
    properties = schema.get("properties", {})
    if schema.get("additionalProperties") is not False:
        errors.append("envelope schema must close additional properties")
    if not {"envelopeVersion", "payload", "integrity"}.issubset(properties):
        errors.append("schema is missing envelopeVersion, payload or integrity")
    payload = properties.get("payload", {})
    if not isinstance(payload, dict):
        errors.append("schema payload definition must be an object")
        return errors
    payload_properties = payload.get("properties", {})
    if payload.get("additionalProperties") is not False or not {
        "schemaVersion", "checkpointId", "encounterId", "checkpointSequence",
        "authority", "safePoint", "deterministic", "participants",
    }.issubset(payload_properties):
        errors.append("payload schema does not define the strict v1 contract")
    return errors


def validate_matrix(path: Path) -> list[str]:
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            columns = reader.fieldnames or []
            missing = [column for column in MATRIX_COLUMNS if column not in columns]
            if missing:
                return [f"matrix missing columns: {', '.join(missing)}"]
            rows = list(reader)
    except (OSError, UnicodeError, csv.Error) as exc:
        return [f"matrix cannot be read: {exc}"]

    errors: list[str] = []
    by_id: dict[str, dict[str, str]] = {}
    for line_number, row in enumerate(rows, start=2):
        case_id = row.get("CaseId", "").strip()
        if not case_id:
            errors.append(f"row {line_number}: CaseId is empty")
            continue
        if case_id in by_id:
            errors.append(f"row {line_number}: duplicate CaseId {case_id}")
            continue
        by_id[case_id] = row
        if row.get("ExpectedDecision", "").strip() not in {"ACCEPT", "REJECT"}:
            errors.append(f"{case_id}: ExpectedDecision must be ACCEPT or REJECT")
        for field in ("Mutation", "ExpectedReason", "ValidationLayer"):
            if not row.get(field, "").strip():
                errors.append(f"{case_id}: {field} is required")

    for case_id, (decision, reason) in REQUIRED_CASES.items():
        row = by_id.get(case_id)
        if row is None:
            errors.append(f"matrix missing required case {case_id}")
        elif (
            row.get("ExpectedDecision", "").strip() != decision
            or row.get("ExpectedReason", "").strip() != reason
        ):
            errors.append(f"{case_id}: expected {decision}/{reason}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    args = parser.parse_args()
    root = args.project_root.resolve()

    errors: list[str] = []
    try:
        schema = json.loads((root / SCHEMA_PATH).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        schema = None
        errors.append(f"schema cannot be read: {exc}")
    if schema is not None:
        errors.extend(validate_schema(schema))

    try:
        example = json.loads((root / EXAMPLE_PATH).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        example = None
        errors.append(f"example cannot be read: {exc}")
    if example is not None:
        errors.extend(validate_envelope(example))

    errors.extend(validate_matrix(root / MATRIX_PATH))
    if errors:
        print(f"Encounter checkpoint contract validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Encounter checkpoint contract validation: PASS")
    print(f"Validated schema v1, one fixture and {len(REQUIRED_CASES)} recovery matrix cases.")
    print("Structural/semantic validation only; cryptographic verification and Unreal server recovery are not run.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
