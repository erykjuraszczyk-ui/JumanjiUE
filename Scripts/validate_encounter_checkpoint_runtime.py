#!/usr/bin/env python3
"""Validate the Stage 34 encounter checkpoint runtime contract and C++ hooks.

This is a source/manifest validator. It is not a C++ compiler, UHT run,
cryptographic signature verifier, backend integration test or world simulation.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

EXPECTED_BARRIERS = {
    "Trials": "TrialBoundary",
    "Combat": "WaveBoundary",
    "Boss": "BossPhaseBoundary",
    "Reward": "RewardPending",
}
REQUIRED_ENV = {
    "JUMANJI_CHECKPOINT_SERVER_TOKEN",
    "JUMANJI_SERVER_INSTANCE_ID",
    "JUMANJI_SHARD_ID",
    "JUMANJI_SERVER_BUILD_ID",
    "JUMANJI_RULESET_VERSION",
    "JUMANJI_WORLD_CONTENT_VERSION",
    "JUMANJI_CHECKPOINT_EXPECTED_KEY_ID",
}
REQUIRED_EXCLUSIONS = {
    "AActor/UObject references",
    "raw ProfileId/account IDs/authentication material",
    "wallet balances/inventory stacks/item contents",
    "timers/latent actions/animation montages",
}
FORBIDDEN_CPP_TYPE_PATTERNS = (
    re.compile(r"\b(?:TObjectPtr|TWeakObjectPtr|TSoftObjectPtr|TSubclassOf)\s*<"),
    re.compile(r"\b(?:AActor|UObject)\s*\*"),
    re.compile(r"\b(?:FTransform|FVector|FRotator|FTimerHandle)\b"),
)
REQUIRED_FILES = (
    "Data/Encounter/EncounterCheckpoint.schema.json",
    "Data/Encounter/EncounterCheckpointStage34Manifest.json",
    "Docs/STAGE_32_ENCOUNTER_SESSION_CHECKPOINT_CONTRACT.md",
    "Docs/STAGE_33_ENCOUNTER_CHECKPOINT_BACKEND_LEASE_SERVICE.md",
    "Docs/STAGE_34_SAFE_POINT_SNAPSHOT_BUILDER_RESTORE_STAGING.md",
    "Source/JumanjiUE/Public/JumanjiEncounterCheckpointTypes.h",
    "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h",
    "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp",
    "Source/JumanjiUE/Public/JumanjiPlayerState.h",
    "Source/JumanjiUE/Private/JumanjiPlayerState.cpp",
    "Source/JumanjiUE/Public/JumanjiAdventureChapterDirector.h",
    "Source/JumanjiUE/Private/JumanjiAdventureChapterDirector.cpp",
    "Source/JumanjiUE/Public/JumanjiBossCharacter.h",
    "Source/JumanjiUE/Private/JumanjiBossCharacter.cpp",
    "Source/JumanjiUE/Public/JumanjiWorldEncounterDirector.h",
    "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp",
)


def strip_cpp_comments(source: str) -> str:
    """Remove // and /* */ comments while retaining quoted string contents."""
    token_pattern = re.compile(
        r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|(/\*.*?\*/|//[^\n]*)',
        re.DOTALL,
    )
    return token_pattern.sub(lambda match: match.group(1) or "", source)


def validate_manifest_object(manifest: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if manifest.get("schema") != "JUMANJI.EncounterCheckpointRuntime.v1":
        errors.append("manifest schema must be JUMANJI.EncounterCheckpointRuntime.v1")
    if manifest.get("stage") != 34:
        errors.append("manifest stage must be 34")
    if manifest.get("status") != "NATIVE_SAFE_POINT_BUILDER_AND_PRIVATE_RESTORE_STAGING":
        errors.append("manifest status is not the Stage 34 runtime status")

    contract = manifest.get("contract")
    if not isinstance(contract, dict):
        errors.append("contract must be an object")
        contract = {}
    if contract.get("payloadSchemaVersion") != 1 or contract.get("envelopeVersion") != 1:
        errors.append("Stage 34 must continue using checkpoint payload/envelope v1")
    integrity = contract.get("integrity")
    if not isinstance(integrity, dict) or integrity.get("algorithm") != "Ed25519":
        errors.append("integrity algorithm must remain Ed25519")
    elif integrity.get("canonicalization") != "RFC8785":
        errors.append("Ed25519 signature canonicalization must remain RFC8785")
    elif integrity.get("unrealLocalSignatureVerification") is not False:
        errors.append("manifest must state that Unreal does not locally verify Ed25519")
    elif integrity.get("expectedKeyIdEnvironment") != "JUMANJI_CHECKPOINT_EXPECTED_KEY_ID":
        errors.append("approved key ID environment variable is missing")

    environment = manifest.get("requiredServerEnvironment")
    if not isinstance(environment, list) or not REQUIRED_ENV.issubset(set(environment)):
        errors.append("requiredServerEnvironment is missing one or more server-only variables")

    barriers = manifest.get("safePointBarriers")
    if not isinstance(barriers, list):
        errors.append("safePointBarriers must be an array")
        barriers = []
    found: dict[str, str] = {}
    for barrier in barriers:
        if not isinstance(barrier, dict):
            errors.append("each safe-point barrier must be an object")
            continue
        phase = barrier.get("phase")
        kind = barrier.get("kind")
        if phase in found:
            errors.append(f"duplicate safe-point phase: {phase}")
        found[phase] = kind
        for required in ("trigger", "requirements", "resumeMeaning"):
            if not barrier.get(required):
                errors.append(f"{phase}: missing {required}")
        if not isinstance(barrier.get("requirements"), list) or not barrier["requirements"]:
            errors.append(f"{phase}: requirements must be a non-empty array")
    if found != EXPECTED_BARRIERS:
        errors.append(f"safe-point phase/kind map must be exactly {EXPECTED_BARRIERS!r}; found {found!r}")

    identity = manifest.get("participantIdentity")
    if not isinstance(identity, dict):
        errors.append("participantIdentity must be an object")
    else:
        if identity.get("pattern") != r"^pk_[A-Za-z0-9_-]{20,96}$":
            errors.append("participant key pattern does not match the trusted opaque-key contract")
        if identity.get("source") != "trusted profile/account service only":
            errors.append("participant key must originate from a trusted profile/account service")
        if identity.get("rawProfileIdIncluded") is not False:
            errors.append("raw ProfileId must never be included")
        if identity.get("providerBindingImplemented") is not False:
            errors.append("manifest must accurately disclose that provider binding is not implemented")
        if identity.get("captureBehaviorWhenUnbound") != "reject":
            errors.append("capture must reject unbound participant identity")

    staging = manifest.get("restoreStaging")
    if not isinstance(staging, dict):
        errors.append("restoreStaging must be an object")
    else:
        if staging.get("privateValueOnlyPlan") is not True:
            errors.append("restore plan must remain private and value-only")
        for key in ("spawnActors", "mutateGameState", "publishReplicatedState", "applyRewards",
                    "createTimersOrAI", "automaticServerMigration"):
            if staging.get(key) is not False:
                errors.append(f"restore staging must not enable {key}")

    exclusions = manifest.get("serializationExclusions")
    if not isinstance(exclusions, list) or not REQUIRED_EXCLUSIONS.issubset(set(exclusions)):
        errors.append("serializationExclusions does not cover all mandatory forbidden data classes")

    limitations = manifest.get("limitations")
    if not isinstance(limitations, list) or len(limitations) < 3:
        errors.append("manifest must explicitly disclose runtime/identity limitations")

    return errors


def validate_checkpoint_type_source(source: str) -> list[str]:
    errors: list[str] = []
    code = strip_cpp_comments(source)
    for pattern in FORBIDDEN_CPP_TYPE_PATTERNS:
        match = pattern.search(code)
        if match:
            errors.append(f"checkpoint logical structs contain forbidden runtime type: {match.group(0)}")
    for forbidden in ("PersistentProfileId", "GetPersistentProfileId", "WalletBalances",
                      "InventoryStacks", "TransactionLedger", "ActiveGameplayEffects"):
        if forbidden in code:
            errors.append(f"checkpoint logical structs contain forbidden field/reference: {forbidden}")
    required_structs = (
        "FJumanjiCheckpointParticipant",
        "FJumanjiEncounterCheckpointCaptureInput",
        "FJumanjiEncounterCheckpointRestoreExpectations",
        "FJumanjiEncounterCheckpointRestorePlan",
    )
    for name in required_structs:
        if name not in code:
            errors.append(f"checkpoint types missing {name}")
    return errors


def validate_project(project_root: Path) -> list[str]:
    errors: list[str] = []
    for relative in REQUIRED_FILES:
        if not (project_root / relative).is_file():
            errors.append(f"required file missing: {relative}")
    if errors:
        return errors

    manifest_path = project_root / "Data/Encounter/EncounterCheckpointStage34Manifest.json"
    schema_path = project_root / "Data/Encounter/EncounterCheckpoint.schema.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        return [f"manifest/schema JSON is invalid: {exc}"]

    errors.extend(validate_manifest_object(manifest))
    if schema.get("properties", {}).get("envelopeVersion", {}).get("const") != 1:
        errors.append("Stage 32 envelope schema must remain version 1")
    if schema.get("properties", {}).get("payload", {}).get("properties", {}).get("schemaVersion", {}).get("const") != 1:
        errors.append("Stage 32 payload schema must remain version 1")

    runtime_types = (project_root / "Source/JumanjiUE/Public/JumanjiEncounterCheckpointTypes.h").read_text(encoding="utf-8")
    errors.extend(validate_checkpoint_type_source(runtime_types))

    runtime_header = strip_cpp_comments(
        (project_root / "Source/JumanjiUE/Public/JumanjiEncounterCheckpointRuntimeSubsystem.h").read_text(encoding="utf-8")
    )
    if "UFUNCTION(" in runtime_header or "Server," in runtime_header or "Client," in runtime_header:
        errors.append("checkpoint runtime API must remain native-only and must not expose RPC/UFUNCTION entry points")

    runtime_cpp = strip_cpp_comments(
        (project_root / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointRuntimeSubsystem.cpp").read_text(encoding="utf-8")
    )
    runtime_requirements = (
        "AcquireLease(", "RenewLease(", "WriteCheckpoint(", "ReadCheckpoint(", "ReleaseLease(",
        "ParticipantRevisionValidator", "SourceEpoch >= AuthorityEpoch",
        "bCaptureRequestPending", "CHECKPOINT_COMMIT_RESPONSE_INVALID",
        "ReadAndStageRestorePlan", "RESTORE_PLAN_STAGED",
    )
    for item in runtime_requirements:
        if item not in runtime_cpp:
            errors.append(f"runtime implementation missing required contract: {item}")

    chapter_cpp = strip_cpp_comments(
        (project_root / "Source/JumanjiUE/Private/JumanjiAdventureChapterDirector.cpp").read_text(encoding="utf-8")
    )
    for item in ("OnCheckpointBoundaryReady.Broadcast()", "EJumanjiTrialState::InProgress",
                 "EJumanjiTrialState::Completed", "RequiredProgress"):
        if item not in chapter_cpp:
            errors.append(f"chapter/trial barrier hook missing: {item}")

    world_cpp = strip_cpp_comments(
        (project_root / "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp").read_text(encoding="utf-8")
    )
    for item in ("RequestCheckpointCapture(EJumanjiWorldEncounterPhase::Trials",
                 "RequestCheckpointCapture(EJumanjiWorldEncounterPhase::Combat",
                 "RequestCheckpointCapture(EJumanjiWorldEncounterPhase::Boss",
                 "RequestCheckpointCapture(EJumanjiWorldEncounterPhase::Reward",
                 "OnCheckpointBoundaryReady.AddUObject",
                 "CaptureSafePoint("):
        if item not in world_cpp:
            errors.append(f"world encounter integration missing barrier: {item}")

    boss_cpp = strip_cpp_comments(
        (project_root / "Source/JumanjiUE/Private/JumanjiBossCharacter.cpp").read_text(encoding="utf-8")
    )
    if "ConfirmBossPhaseCheckpointBoundary" not in boss_cpp or "BossPhaseRevision" not in boss_cpp:
        errors.append("boss phase checkpoint quiescence hook is missing")

    player_cpp = strip_cpp_comments(
        (project_root / "Source/JumanjiUE/Private/JumanjiPlayerState.cpp").read_text(encoding="utf-8")
    )
    if "SetCheckpointParticipantIdentity" not in player_cpp or "CheckpointParticipantKey" not in player_cpp:
        errors.append("opaque PlayerState identity binding is missing")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_project(args.project_root.resolve())
    if errors:
        print(f"Encounter checkpoint runtime validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Encounter checkpoint runtime validation: PASS")
    print("Stage 34 manifest, logical-only structs, safe-point hooks and private restore boundary are consistent.")
    print("Note: this is static source validation, not UHT/C++ compilation, cryptographic verification or live restore testing.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
