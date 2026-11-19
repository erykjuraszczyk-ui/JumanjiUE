#!/usr/bin/env python3
"""Validate Stage 31 reconnect UX contracts and the network QA matrix.

Static source checks only. This does not compile Unreal C++, run PIE,
simulate packet loss, or prove reconnect behavior on a live server/device.
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

MATRIX_COLUMNS = (
    "ScenarioId",
    "ClientState",
    "DisconnectPhase",
    "ElapsedSeconds",
    "ExpectedOutcome",
    "ExpectedFeedbackVisible",
    "ExpectedEncounterMutation",
    "ValidationLayer",
    "Notes",
)
ALLOWED_OUTCOMES = {"None", "Reconnected", "GraceWindowExpired", "EncounterEnded"}
REQUIRED_SCENARIOS = {
    "REJOIN_TRIALS_001": "Reconnected",
    "REJOIN_COMBAT_001": "Reconnected",
    "REJOIN_BOSS_001": "Reconnected",
    "REJOIN_REWARD_001": "Reconnected",
    "GRACE_EXPIRED_001": "GraceWindowExpired",
    "END_COMPLETED_001": "EncounterEnded",
    "END_FAILED_001": "EncounterEnded",
    "FRESH_LOGIN_ACTIVE_001": "None",
    "DUPLICATE_PROFILE_001": "None",
    "PROFILE_ID_MISSING_001": "None",
    "NETWORK_FLAP_3X_001": "Reconnected",
    "ANDROID_BACKGROUND_EXPIRED_001": "GraceWindowExpired",
    "TERMINAL_NOTICE_ONCE_001": "EncounterEnded",
}
SOURCE_REQUIREMENTS = {
    "Source/JumanjiUE/Public/JumanjiWorldEncounter.h": (
        "enum class EJumanjiEncounterReconnectOutcome",
        "GraceWindowExpired",
        "EncounterEnded",
        "FJumanjiEncounterReconnectFeedbackState",
        "DisplayDurationSeconds",
    ),
    "Source/JumanjiUE/Public/JumanjiWorldEncounterSessionSubsystem.h": (
        "FJumanjiEncounterReconnectFeedbackState& OutFeedbackState",
        "RecentlyEndedEncounterIds",
        "RecentlyEndedAtUtc",
        "MaxTerminalNoticeEntries = 256",
        "TryRejoinPlayer",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp": (
        "OutFeedbackState.Outcome = EJumanjiEncounterReconnectOutcome::GraceWindowExpired",
        "OutFeedbackState.Outcome = EJumanjiEncounterReconnectOutcome::EncounterEnded",
        "OutFeedbackState.Outcome = EJumanjiEncounterReconnectOutcome::Reconnected",
        "PruneRecentlyEndedParticipants",
        "RecentlyEndedEncounterIds.Remove(ProfileId)",
        "ElapsedSeconds > SafeGraceWindow",
        "EndEncounterSession(EndedEncounterId)",
    ),
    "Source/JumanjiUE/Public/JumanjiPlayerState.h": (
        "SetEncounterReconnectFeedback",
        "GetEncounterReconnectFeedbackState",
        "OnEncounterReconnectFeedbackChanged",
        "OnRep_EncounterReconnectFeedbackState",
        "EncounterReconnectFeedbackState",
    ),
    "Source/JumanjiUE/Private/JumanjiPlayerState.cpp": (
        "bool AJumanjiPlayerState::SetEncounterReconnectFeedback(",
        "OnEncounterReconnectFeedbackChanged.Broadcast(EncounterReconnectFeedbackState)",
        "DOREPLIFETIME(AJumanjiPlayerState, EncounterReconnectFeedbackState)",
    ),
    "Source/JumanjiUE/Private/JumanjiGameMode.cpp": (
        "SessionSubsystem->TryRejoinPlayer(JumanjiPlayerState, ReconnectState, FeedbackState)",
        "JumanjiPlayerState->SetEncounterReconnectFeedback(FeedbackState)",
        "ProfileSubsystem->LoadProfile",
    ),
    "Source/JumanjiUE/Public/JumanjiWorldEncounterHUDWidget.h": (
        "GetReconnectFeedbackState() const",
        "GetReconnectFeedbackText() const",
        "IsReconnectFeedbackSuccess() const",
        "OnEncounterReconnectFeedbackPresentationChanged",
        "HandleEncounterReconnectFeedbackChanged",
    ),
    "Source/JumanjiUE/Private/JumanjiWorldEncounterHUDWidget.cpp": (
        "OnEncounterReconnectFeedbackChanged.AddUniqueDynamic",
        "OnEncounterReconnectFeedbackChanged.RemoveDynamic",
        "ReconnectGraceExpired",
        "ReconnectEncounterEnded",
        "PresentReconnectFeedback()",
    ),
}

def read_text(root: Path, relative: str, errors: list[str]) -> str:
    path = root / relative
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"{relative}: cannot read: {exc}")
        return ""

def validate_matrix(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            columns = reader.fieldnames or []
            missing = [name for name in MATRIX_COLUMNS if name not in columns]
            if missing:
                return [f"QA matrix missing columns: {', '.join(missing)}"]
            rows = list(reader)
    except (OSError, UnicodeError, csv.Error) as exc:
        return [f"QA matrix cannot be read: {exc}"]

    by_id: dict[str, dict[str, str]] = {}
    for row_number, row in enumerate(rows, start=2):
        scenario_id = row.get("ScenarioId", "").strip()
        if not scenario_id:
            errors.append(f"row {row_number}: ScenarioId is empty")
            continue
        if scenario_id in by_id:
            errors.append(f"row {row_number}: duplicate ScenarioId {scenario_id}")
            continue
        by_id[scenario_id] = row

        outcome = row.get("ExpectedOutcome", "").strip()
        if outcome not in ALLOWED_OUTCOMES:
            errors.append(f"{scenario_id}: unsupported ExpectedOutcome {outcome!r}")
            continue

        visible_text = row.get("ExpectedFeedbackVisible", "").strip().lower()
        if visible_text not in {"true", "false"}:
            errors.append(f"{scenario_id}: ExpectedFeedbackVisible must be true/false")
        elif (outcome == "None") == (visible_text == "true"):
            errors.append(f"{scenario_id}: banner visibility does not match ExpectedOutcome")

        elapsed_text = row.get("ElapsedSeconds", "").strip()
        if elapsed_text:
            try:
                elapsed = float(elapsed_text)
                if elapsed < 0 or elapsed != elapsed or elapsed in (float("inf"), float("-inf")):
                    raise ValueError
            except ValueError:
                errors.append(f"{scenario_id}: ElapsedSeconds must be a finite non-negative number")

        if not row.get("ExpectedEncounterMutation", "").strip():
            errors.append(f"{scenario_id}: ExpectedEncounterMutation is required")
        if not row.get("ValidationLayer", "").strip():
            errors.append(f"{scenario_id}: ValidationLayer is required")

    for scenario_id, expected_outcome in REQUIRED_SCENARIOS.items():
        row = by_id.get(scenario_id)
        if not row:
            errors.append(f"QA matrix missing required scenario {scenario_id}")
        elif row.get("ExpectedOutcome", "").strip() != expected_outcome:
            errors.append(
                f"{scenario_id}: expected {expected_outcome}, "
                f"found {row.get('ExpectedOutcome', '').strip()}"
            )

    return errors

def validate_sources(project_root: Path) -> list[str]:
    errors: list[str] = []
    loaded: dict[str, str] = {}

    for relative, required_tokens in SOURCE_REQUIREMENTS.items():
        content = read_text(project_root, relative, errors)
        loaded[relative] = content
        for token in required_tokens:
            if token not in content:
                errors.append(f"{relative}: missing Stage 31 reconnect UX contract: {token}")

    player_state_cpp = loaded.get("Source/JumanjiUE/Private/JumanjiPlayerState.cpp", "")
    start = player_state_cpp.find(
        "FJumanjiPlayerProfileSnapshot AJumanjiPlayerState::BuildProfileSnapshot() const"
    )
    end = player_state_cpp.find("void AJumanjiPlayerState::PersistProfileSnapshot()", start + 1)
    profile_snapshot = player_state_cpp[start:end] if start >= 0 and end > start else ""
    if not profile_snapshot:
        errors.append("PlayerState: cannot isolate BuildProfileSnapshot")
    elif "EncounterReconnect" in profile_snapshot:
        errors.append("Reconnect feedback must never be serialized into the persistent profile")

    session_cpp = loaded.get(
        "Source/JumanjiUE/Private/JumanjiWorldEncounterSessionSubsystem.cpp", ""
    )
    for forbidden in ("AJumanjiBossCharacter", "TObjectPtr<AActor>", "USaveGame", "SaveProfile("):
        if forbidden in session_cpp:
            errors.append(f"Session subsystem must remain actor/profile agnostic: {forbidden}")

    game_mode = loaded.get("Source/JumanjiUE/Private/JumanjiGameMode.cpp", "")
    load_position = game_mode.find("ProfileSubsystem->LoadProfile")
    reconnect_position = game_mode.find(
        "SessionSubsystem->TryRejoinPlayer(JumanjiPlayerState, ReconnectState, FeedbackState)"
    )
    if load_position < 0 or reconnect_position < 0 or reconnect_position < load_position:
        errors.append("GameMode must restore the persistent profile before evaluating reconnect")

    hud_cpp = loaded.get("Source/JumanjiUE/Private/JumanjiWorldEncounterHUDWidget.cpp", "")
    bind_state = hud_cpp.find("void UJumanjiWorldEncounterHUDWidget::BindPlayerState()")
    native_destruct = hud_cpp.find("void UJumanjiWorldEncounterHUDWidget::NativeDestruct()")
    if bind_state < 0 or native_destruct < 0:
        errors.append("Encounter HUD must bind and release PlayerState delegates")

    return errors

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--matrix",
        type=Path,
        default=Path("Data/Encounter/EncounterReconnectSessionQAMatrix.csv"),
        help="QA matrix path relative to project root unless absolute.",
    )
    args = parser.parse_args()

    root = args.project_root.resolve()
    matrix_path = args.matrix if args.matrix.is_absolute() else root / args.matrix
    errors = validate_sources(root) + validate_matrix(matrix_path)

    if errors:
        print(f"Encounter reconnect UX validation: FAIL ({len(errors)} error(s))")
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Encounter reconnect UX validation: PASS")
    print(f"Validated {len(REQUIRED_SCENARIOS)} reconnect UX/session QA scenarios.")
    print("This validates source contracts and the test matrix only; it does not run Unreal PIE or mobile network tests.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
