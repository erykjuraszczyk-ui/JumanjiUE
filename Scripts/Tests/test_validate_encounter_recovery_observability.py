from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
import validate_encounter_recovery_observability as MODULE


class RecoveryObservabilityTests(unittest.TestCase):
    def _copy_fixture(self, temp_dir: str) -> Path:
        source_root = Path(__file__).resolve().parents[2]
        target_root = Path(temp_dir)
        for relative in MODULE.REQUIRED:
            source = source_root / relative
            destination = target_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        return target_root

    def test_repository_contract_passes(self) -> None:
        root = Path(__file__).resolve().parents[2]
        self.assertEqual(MODULE.validate_project(root), [])

    def test_diagnostics_must_not_expose_lease_token(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._copy_fixture(tmp)
            path = root / MODULE.RUNTIME_HEADER
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "FGuid CorrelationId;",
                "FGuid CorrelationId;\n    FString LeaseToken;",
                1,
            )
            path.write_text(source, encoding="utf-8")
            self.assertTrue(any("LeaseToken" in e for e in MODULE.validate_project(root)))

    def test_operator_retry_must_be_server_authoritative(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._copy_fixture(tmp)
            path = root / MODULE.RUNTIME_CPP
            source = path.read_text(encoding="utf-8")
            source = source.replace("if (!IsServerRuntime() || !IsValid(EncounterDirector) || !EncounterDirector->HasAuthority())",
                                    "if (!IsValid(EncounterDirector))", 1)
            path.write_text(source, encoding="utf-8")
            self.assertTrue(any("!IsServerRuntime()" in e for e in MODULE.validate_project(root)))

    def test_operator_retry_must_not_end_live_gameplay_lease(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._copy_fixture(tmp)
            path = root / MODULE.RUNTIME_CPP
            source = path.read_text(encoding="utf-8")
            source = source.replace("OPERATOR_RETRY_NOT_A_STAGED_OR_IDLE_RECOVERY",
                                    "OPERATOR_RETRY_ALWAYS_ALLOWED", 1)
            path.write_text(source, encoding="utf-8")
            self.assertTrue(any("OPERATOR_RETRY_NOT_A_STAGED_OR_IDLE_RECOVERY" in e
                                for e in MODULE.validate_project(root)))

    def test_operator_abort_is_limited_to_staged_participant_wait(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._copy_fixture(tmp)
            path = root / MODULE.RUNTIME_CPP
            source = path.read_text(encoding="utf-8")
            source = source.replace("!bHasStagedRestoreCandidate || !bWaitingForRestoreParticipants",
                                    "!bHasStagedRestoreCandidate", 1)
            path.write_text(source, encoding="utf-8")
            self.assertTrue(any("bHasStagedRestoreCandidate || !bWaitingForParticipants" in e
                                for e in MODULE.validate_project(root)))

    def test_runtime_header_cannot_add_blueprint_exposure(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = self._copy_fixture(tmp)
            path = root / MODULE.RUNTIME_HEADER
            source = path.read_text(encoding="utf-8")
            source = source.replace(
                "FJumanjiEncounterRecoveryDiagnostics GetRecoveryDiagnostics() const;",
                "UFUNCTION(BlueprintCallable)\n    FJumanjiEncounterRecoveryDiagnostics GetRecoveryDiagnostics() const;",
                1,
            )
            path.write_text(source, encoding="utf-8")
            self.assertTrue(any("no UFUNCTION is allowed" in e for e in MODULE.validate_project(root)))


if __name__ == "__main__":
    unittest.main()
