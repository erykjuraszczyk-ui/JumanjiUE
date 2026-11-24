from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "Scripts"
TESTS = ROOT / "Scripts" / "Tests"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(TESTS))

import encounter_checkpoint_deployment as MODULE
import test_validate_encounter_checkpoint_stage45_release_board as STAGE45_FIXTURES

BASELINE_STAMP = "2026-11-21T12:00:00Z"


def _json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def _build_context(root: Path) -> dict:
    context = STAGE45_FIXTURES._build_context(root)
    context["unsigned_decision"] = context["decision"]
    context["decision"] = STAGE45_FIXTURES._sign_all(context)
    context["decision_raw"] = _json_bytes(context["decision"])

    rollback_root = root / "rollback-artifacts"
    rollback_root.mkdir()
    server_dir = rollback_root / "server"
    android_dir = rollback_root / "android"
    server_dir.mkdir()
    android_dir.mkdir()
    server_bytes = b"synthetic-last-known-good-ue58-server"
    android_bytes = b"synthetic-last-known-good-android-apk"
    (server_dir / "previous-server.zip").write_bytes(server_bytes)
    (android_dir / "previous.apk").write_bytes(android_bytes)

    baseline = {
        "schema": MODULE.BASELINE_SCHEMA,
        "baselineId": "JUMANJI-PRODUCTION-GOOD-0001",
        "environmentId": "production",
        "capturedAtUtc": BASELINE_STAMP,
        "sourceCommitSha": "b" * 40,
        "targets": [
            {
                "target": "ue58-dedicated-server",
                "path": "server/previous-server.zip",
                "sha256": hashlib.sha256(server_bytes).hexdigest(),
                "sizeBytes": len(server_bytes),
                "mediaType": "application/zip",
            },
            {
                "target": "android-package",
                "path": "android/previous.apk",
                "sha256": hashlib.sha256(android_bytes).hexdigest(),
                "sizeBytes": len(android_bytes),
                "mediaType": "application/vnd.android.package-archive",
            },
        ],
    }
    context["rollback_root"] = rollback_root
    context["baseline"] = baseline
    context["baseline_raw"] = _json_bytes(baseline)
    return context


def _prepare(context: dict) -> dict:
    return MODULE.prepare_plan(
        context["packet"],
        context["evidence_root"],
        context["envelope"],
        context["attestation_raw"],
        context["keyring"],
        context["keyring_raw"],
        context["trusted_hash"],
        context["candidate"],
        context["candidate_raw"],
        context["artifact_root"],
        context["decision"],
        context["decision_raw"],
        context["baseline"],
        context["baseline_raw"],
        context["rollback_root"],
        ROOT,
    )


class Stage46ControlledDeploymentTests(unittest.TestCase):
    def test_approved_release_creates_plan_bound_to_candidate_and_rollback(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            plan = _prepare(context)

        self.assertEqual(plan["state"], "READY_FOR_OPERATOR_REVIEW")
        self.assertEqual(plan["authorization"]["stage45Outcome"], "APPROVED_FOR_DEPLOYMENT")
        self.assertEqual(
            set(plan["authorization"]["verifiedBoardRoles"]),
            set(("SERVICE_OWNER", "SECURITY_REVIEWER", "PLATFORM_REVIEWER")),
        )
        self.assertEqual(plan["candidate"]["candidateSha256"], hashlib.sha256(context["candidate_raw"]).hexdigest())
        self.assertEqual(
            plan["authorization"]["decisionSha256"],
            hashlib.sha256(context["decision_raw"]).hexdigest(),
        )
        self.assertEqual(plan["rollbackBaseline"]["baselineId"], context["baseline"]["baselineId"])
        self.assertEqual(len(plan["rollbackBaseline"]["targets"]), 2)
        self.assertEqual([item["phase"] for item in plan["rollout"]["phases"]], [
            "PREFLIGHT", "CANARY", "PROGRESSIVE_ROLLOUT", "POST_DEPLOYMENT_VERIFICATION"
        ])
        self.assertFalse(plan["sideEffectsPerformed"])
        self.assertFalse(plan["deploymentExecuted"])
        self.assertFalse(plan["postDeploymentVerified"])
        self.assertFalse(plan["rollbackExecuted"])
        self.assertFalse(plan["productionReady"])
        self.assertFalse(plan["promotionAllowed"])
        self.assertEqual(plan["sloStatus"], "UNBASELINED")

    def test_unsigned_release_board_decision_cannot_create_a_plan(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["decision"] = context["unsigned_decision"]
            context["decision_raw"] = _json_bytes(context["decision"])
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "STAGE45_NOT_APPROVED"):
                _prepare(context)

    def test_rejected_release_board_decision_cannot_create_a_plan(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            rejected = dict(context["decision"])
            rejected["decision"] = "REJECT"
            rejected["boardSignatures"] = []
            context["decision"] = rejected
            context["decision_raw"] = _json_bytes(rejected)
            context["decision"] = STAGE45_FIXTURES._sign_all(context)
            context["decision_raw"] = _json_bytes(context["decision"])
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "STAGE45_NOT_APPROVED:REJECTED_BY_RELEASE_BOARD"):
                _prepare(context)

    def test_candidate_artifact_mutation_blocks_plan(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            (context["artifact_root"] / "android" / "jumanji.apk").write_bytes(b"tampered")
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "STAGE45_NOT_APPROVED:INVALID_RELEASE_CANDIDATE"):
                _prepare(context)

    def test_rollback_artifact_hash_mismatch_blocks_plan(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            (context["rollback_root"] / "android" / "previous.apk").write_bytes(b"tampered")
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "ROLLBACK_BASELINE_HASH_MISMATCH:android-package"):
                _prepare(context)

    def test_rollback_path_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["baseline"]["targets"][0]["path"] = "../outside.zip"
            context["baseline_raw"] = _json_bytes(context["baseline"])
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "ROLLBACK_BASELINE_PATH_TRAVERSAL"):
                _prepare(context)

    def test_rollback_target_set_must_match_release_targets(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["baseline"]["targets"][0]["target"] = "windows-client"
            context["baseline_raw"] = _json_bytes(context["baseline"])
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "ROLLBACK_BASELINE_TARGET_SET_MISMATCH"):
                _prepare(context)

    def test_decision_bytes_must_match_verified_decision_object(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["decision_raw"] += b" "
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "RELEASE_BOARD_DECISION_BYTES_MISMATCH"):
                _prepare(context)

    def test_rollback_baseline_bytes_must_match_parsed_object(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["baseline_raw"] += b" "
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "ROLLBACK_BASELINE_BYTES_MISMATCH"):
                _prepare(context)

    def test_candidate_bytes_must_match_parsed_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["candidate_raw"] += b" "
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "CANDIDATE_BYTES_MISMATCH"):
                _prepare(context)

    def test_plan_verification_detects_mutation_and_recomputes_bindings(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            plan = _prepare(context)
            valid = MODULE.verify_plan(
                plan,
                context["packet"], context["evidence_root"],
                context["envelope"], context["attestation_raw"],
                context["keyring"], context["keyring_raw"], context["trusted_hash"],
                context["candidate"], context["candidate_raw"], context["artifact_root"],
                context["decision"], context["decision_raw"],
                context["baseline"], context["baseline_raw"], context["rollback_root"], ROOT,
            )
            self.assertEqual(valid["outcome"], "PLAN_VERIFIED_NO_SIDE_EFFECTS")
            plan["candidate"]["buildId"] = "JUMANJI-TAMPERED"
            with self.assertRaisesRegex(MODULE.DeploymentPlanError, "DEPLOYMENT_PLAN_BINDING_MISMATCH"):
                MODULE.verify_plan(
                    plan,
                    context["packet"], context["evidence_root"],
                    context["envelope"], context["attestation_raw"],
                    context["keyring"], context["keyring_raw"], context["trusted_hash"],
                    context["candidate"], context["candidate_raw"], context["artifact_root"],
                    context["decision"], context["decision_raw"],
                    context["baseline"], context["baseline_raw"], context["rollback_root"], ROOT,
                )

    def test_empty_repository_templates_remain_blocked(self) -> None:
        plan_template = json.loads(
            (ROOT / "Data/Encounter/ProductionDeploymentPlan.template.json").read_text(encoding="utf-8")
        )
        baseline_template = json.loads(
            (ROOT / "Data/Encounter/ProductionRollbackBaseline.template.json").read_text(encoding="utf-8")
        )
        self.assertEqual(plan_template["state"], "BLOCKED_MISSING_EVIDENCE")
        self.assertIsNone(plan_template["planId"])
        self.assertFalse(plan_template["deploymentExecuted"])
        self.assertFalse(plan_template["sideEffectsPerformed"])
        self.assertEqual(baseline_template["targets"], [])
        self.assertIsNone(baseline_template["baselineId"])


if __name__ == "__main__":
    unittest.main()
