#!/usr/bin/env python3
"""Validate same-server encounter reconnect contracts.

This is a static source guard only. It does not compile Unreal C++, run PIE,
or prove that an online session survives server/process/shard migration.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REQUIRED_TOKENS = {
    "Source/JumanjiUE/Public/JumanjiWorldEncounterSessionSubsystem.h": (
        "class JUMANJIUE_API UJumanjiWorldEncounterSessionSubsystem",
        "BeginEncounterSession",
        "MarkPlayerDisconnected",
        "TryRejoinPlayer",
        "RegisterConnectedParticipant",
        "EndEncounterSession",
        "ReconnectGraceWindowSeconds = 300.0f",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp": (
        "ParticipantProfileIds.Contains(ProfileId)",
        "DisconnectedAtUtc.Add(ProfileId, FDateTime::UtcNow())",
        "ElapsedSeconds > SafeGraceWindow",
        "EncounterState.EncounterId == ActiveEncounterId",
        "EJumanjiWorldEncounterPhase::Trials",
        "EJumanjiWorldEncounterPhase::Combat",
        "EJumanjiWorldEncounterPhase::Boss",
        "EJumanjiWorldEncounterPhase::Reward",
        "OutReconnectState.EncounterRevision = EncounterState.Revision",
        "ConnectedProfileIds.Add(ProfileId)",
        "bool UJumanjiWorldEncounterSessionSubsystem::RegisterConnectedParticipant(",
    ),
    "Source/JumanjiUE/Private/JumanjiGameMode.cpp": (
        "SessionSubsystem->MarkPlayerDisconnected(JumanjiPlayerState)",
        "SessionSubsystem->TryRejoinPlayer(JumanjiPlayerState, ReconnectState)",
        "JumanjiPlayerState->SetEncounterReconnectState(ReconnectState)",
        "ReconnectState.Phase == EJumanjiWorldEncounterPhase::Reward",
        "RewardSubsystem->GrantWorldEncounterCompletionReward(",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp": (
        "SessionSubsystem->BeginEncounterSession(EncounterId, InitiatingPlayer)",
        "SessionSubsystem->EndEncounterSession(EncounterId)",
        "void AJumanjiWorldEncounterDirector::CompleteEncounter()",
        "void AJumanjiWorldEncounterDirector::FailEncounter(FName ReasonId)",
        "void AJumanjiWorldEncounterDirector::EndPlay(",
    ),
    "Source/JumanjiUE/Public/JumanjiPlayerState.h": (
        "FJumanjiEncounterReconnectState EncounterReconnectState",
        "OnEncounterReconnectChanged",
        "SetEncounterReconnectState",
        "OnRep_EncounterReconnectState",
    ),
    "Source/JumanjiUE/Private/JumanjiPlayerState.cpp": (
        "bool AJumanjiPlayerState::SetEncounterReconnectState(",
        "OnEncounterReconnectChanged.Broadcast(EncounterReconnectState)",
        "DOREPLIFETIME(AJumanjiPlayerState, EncounterReconnectState)",
        "FJumanjiPlayerProfileSnapshot AJumanjiPlayerState::BuildProfileSnapshot() const",
        "Snapshot.ProcessedTransactionIds = ProcessedTransactionIds",
    ),
    "Source/JumanjiUE/Public/JumanjiWorldEncounterHUDWidget.h": (
        "GetReconnectState() const",
        "OnEncounterReconnectNotice",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterHUDWidget.cpp": (
        "BindPlayerState()",
        "OnEncounterReconnectChanged.AddUniqueDynamic",
        "OnEncounterReconnectChanged.RemoveDynamic",
        "OnEncounterReconnectNotice(ReconnectState)",
    ),
    "Source/JumanjiUE/Private/JumanjiPlayerProfileSubsystem.cpp": (
        "LoadProfile",
        "SaveProfile",
    ),

    "Docs/STAGE_30_ENCOUNTER_SESSION_RECONNECT_RECOVERY.md": (
        "tym samym procesie serwera",
        "ReconnectGraceWindowSeconds",
        "Completed",
        "Failed",
        "cross-shard handoff",
    ),
}


def _read(root: Path, relative: str, errors: list[str]) -> str:
    path = root / relative
    if not path.is_file():
        errors.append(f"Missing required file: {relative}")
        return ""
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"Cannot read {relative}: {exc}")
        return ""


def _function_slice(content: str, start_token: str, next_token: str) -> str:
    start = content.find(start_token)
    if start < 0:
        return ""
    if not next_token:
        return content[start:]
    end = content.find(next_token, start + len(start_token))
    return content[start:] if end < 0 else content[start:end]


def validate_sources(project_root: Path) -> list[str]:
    errors: list[str] = []
    loaded: dict[str, str] = {}

    for relative, required in REQUIRED_TOKENS.items():
        content = _read(project_root, relative, errors)
        loaded[relative] = content
        for token in required:
            if token not in content:
                errors.append(f"{relative}: missing reconnect contract: {token}")

    session_header = loaded.get(
        "Source/JumanjiUE/Public/JumanjiWorldEncounterSessionSubsystem.h", ""
    )
    session_cpp = loaded.get(
        "Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp", ""
    )
    forbidden_session_tokens = (
        "AJumanjiBossCharacter",
        "SpawnedEncounterActors",
        "TObjectPtr<AActor>",
        "USaveGame",
        "SaveGameToSlot",
        "SaveProfile(",
        "PersistProfileSnapshot",
    )
    for token in forbidden_session_tokens:
        if token in session_header or token in session_cpp:
            errors.append(
                f"Encounter session registry must not serialize/store live actors or profile data: {token}"
            )

    player_state_cpp = loaded.get(
        "Source/JumanjiUE/Private/JumanjiPlayerState.cpp", ""
    )
    profile_snapshot = _function_slice(
        player_state_cpp,
        "FJumanjiPlayerProfileSnapshot AJumanjiPlayerState::BuildProfileSnapshot() const",
        "void AJumanjiPlayerState::PersistProfileSnapshot()",
    )
    if not profile_snapshot:
        errors.append("PlayerState: cannot isolate BuildProfileSnapshot")
    elif "EncounterReconnectState" in profile_snapshot:
        errors.append("Transient reconnect presentation state must not be written to the profile snapshot")

    game_mode = loaded.get("Source/JumanjiUE/Private/JumanjiGameMode.cpp", "")
    post_login = _function_slice(
        game_mode,
        "void AJumanjiGameMode::PostLogin(APlayerController* NewPlayer)",
        "void AJumanjiGameMode::Logout(AController* Exiting)",
    )
    if post_login:
        load_position = post_login.find("ProfileSubsystem->LoadProfile")
        reconnect_position = post_login.find("TryRejoinPlayer")
        if load_position < 0 or reconnect_position < 0 or reconnect_position < load_position:
            errors.append("GameMode PostLogin must restore the persistent profile before attempting encounter rejoin")
    else:
        errors.append("GameMode: cannot isolate PostLogin")

    logout = _function_slice(
        game_mode,
        "void AJumanjiGameMode::Logout(AController* Exiting)",
        "Super::Logout(Exiting);",
    )
    mark_position = logout.find("MarkPlayerDisconnected")
    save_position = logout.find("RequestProfileSave")
    if mark_position < 0 or save_position < 0 or mark_position > save_position:
        errors.append("GameMode Logout must mark the encounter participant disconnected before saving/removing PlayerState")

    director = loaded.get("Source/JumanjiUE/Private/JumanjiWorldEncounterDirector.cpp", "")
    if director.count("SessionSubsystem->EndEncounterSession(EncounterId)") < 3:
        errors.append("Encounter director must clear session registry on completion, failure and EndPlay")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    args = parser.parse_args()
    errors = validate_sources(args.project_root.resolve())

    if errors:
        print(f"Encounter session reconnect validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Encounter session reconnect validation: PASS")
    print("Same-server identity, grace window, active-phase, cleanup and profile-separation guards are present.")
    print("Note: PASS does not prove Unreal compilation, live reconnect, multiplayer PIE or server migration.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
