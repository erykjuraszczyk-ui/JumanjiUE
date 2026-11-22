from __future__ import annotations

import copy
import hashlib
import json
import sys
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import validate_encounter_checkpoint_stage43_evidence as MODULE
import validate_encounter_checkpoint_staging_report as STAGE42_VALIDATOR


STAMP = "2026-11-22T12:00:00Z"
STAGING_ID = "STAGING-EVIDENCE-01"


def _stage42_report(environment_id: str, environment_class: str = "staging") -> dict:
    return {
        "schema": "JUMANJI.CheckpointStagingDrillReport.v1",
        "stage": 42,
        "runId": str(uuid4()),
        "startedAtUtc": STAMP,
        "completedAtUtc": STAMP,
        "result": "PASS",
        "environment": {
            "class": environment_class,
            "id": environment_id,
            "storageProvider": "postgresql",
            "serviceHost": "staging-checkpoint.internal",
            "transport": "https-mtls",
        },
        "checks": [
            {"id": check_id, "status": "PASS", "durationMs": 1, "detailCode": "OK"}
            for check_id in STAGE42_VALIDATOR.EXPECTED_CHECKS
        ],
        "evidence": {
            "encounterId": "DRILL_STAGE42_UNITTEST_20261122",
            "payloadSha256": "a" * 64,
            "envelopeSha256": "b" * 64,
        },
        "failureCode": None,
    }


def _fencing_report(environment_id: str, mode: str) -> dict:
    return {
        "schema": "JUMANJI.CheckpointRestoreFencingReport.v1",
        "drillId": str(uuid4()),
        "mode": mode,
        "schemaVersion": 1,
        "rowCount": 3,
        "maxAuthorityEpochBefore": 9,
        "epochOverflowRows": 0,
        "leaseOwnerRowsBefore": 2,
        "rowsFenced": 0 if mode == "dry-run" else 3,
        "leaseOwnerRowsInvalidated": 0 if mode == "dry-run" else 2,
        "maxAuthorityEpochAfter": 9 if mode == "dry-run" else 10,
        "checkpointRowsPreserved": 3,
        "completedAtUtc": STAMP,
        "environmentClass": "dr-rehearsal",
        "environmentId": environment_id,
        "result": "PASS",
    }


def _claims_for(kind: str) -> dict:
    true = MODULE.TRUE_CLAIMS.get(kind, ())
    claims = {key: True for key in true}

    if kind == "postgres_primary_failover":
        claims.update({
            "rtoSeconds": 24.5, "rpoSeconds": 0,
            "syntheticEncounterId": "DRILL_STAGE42_FAILOVER_20261122",
            "checkpointSequence": 7,
        })
    elif kind == "postgres_pitr_restore":
        claims.update({
            "rtoSeconds": 140, "rpoSeconds": 4,
            "syntheticEncounterId": "DRILL_STAGE42_PITR_20261122",
            "checkpointSequence": 8,
        })
    elif kind == "signing_key_overlap_rotation":
        claims.update({
            "oldKeyId": "STAGE42-OLD-KEY",
            "newKeyId": "STAGE43-NEW-KEY",
            "syntheticEncounterId": "DRILL_STAGE42_ROTATION_20261122",
        })
    elif kind == "ue58_dedicated_server_recovery":
        claims.update({
            "engineVersion": "5.8",
            "recoveryDurationMs": 2450,
            "syntheticEncounterId": "DRILL_STAGE42_UE_SERVER_20261122",
            "checkpointSequence": 9,
        })
    elif kind == "ue58_pie_multiplayer":
        claims.update({"engineVersion": "5.8", "clientCount": 2})
    elif kind == "android_device_package":
        claims.update({
            "packageFormat": "APK",
            "packageSha256": "c" * 64,
            "deviceModel": "Test Device Model",
            "androidApiLevel": 34,
            "packageBuildId": "JUMANJI-ANDROID-QA-01",
        })
    elif kind == "stage40_slo_approval":
        claims.update({
            "profileName": "STAGING-APPROVED",
            "profileStatus": "APPROVED",
            "profileSha256": "d" * 64,
            "benchmarkReportSha256": "e" * 64,
            "hostInventorySha256": "f" * 64,
            "capacityPlanSha256": "1" * 64,
            "baselineRuns": 3,
            "soakMinutes": 30,
            "approverRoles": ["SERVICE_OWNER", "DATABASE_REVIEWER"],
        })
    return claims


class Stage43EvidenceGateTests(unittest.TestCase):
    def _build_packet(self, evidence_root: Path) -> dict:
        entries = []
        for index, kind in enumerate(MODULE.REQUIRED_KINDS):
            if kind == "staging_smoke":
                environment_id = STAGING_ID
                report = _stage42_report(environment_id)
            elif kind.startswith("post_restore_fencing_"):
                environment_id = STAGING_ID
                mode = "dry-run" if kind.endswith("dry_run") else "apply"
                report = _fencing_report(environment_id, mode)
            else:
                environment_id = {
                    "postgres_primary_failover": STAGING_ID,
                    "postgres_pitr_restore": STAGING_ID,
                    "signing_key_overlap_rotation": STAGING_ID,
                    "ue58_dedicated_server_recovery": "UE-DEDICATED-SERVER-01",
                    "ue58_pie_multiplayer": "UE-PIE-MULTIPLAYER-01",
                    "android_device_package": "ANDROID-QA-DEVICE-01",
                    "stage40_slo_approval": "CAPACITY-GOVERNANCE-01",
                }[kind]
                report = {
                    "schema": MODULE.SUMMARY_SCHEMAS[kind],
                    "result": "PASS",
                    "environmentId": environment_id,
                    "capturedAtUtc": STAMP,
                    "claims": _claims_for(kind),
                }

            relative = f"{kind}.json"
            encoded = json.dumps(report, sort_keys=True).encode("utf-8")
            (evidence_root / relative).write_bytes(encoded)
            source_class = MODULE.SOURCE_CLASS_BY_KIND[kind]
            operator_role = {
                "staging_smoke": "SERVICE_OPERATOR",
                "postgres_primary_failover": "DATABASE_OPERATOR",
                "postgres_pitr_restore": "DATABASE_OPERATOR",
                "post_restore_fencing_dry_run": "DATABASE_OPERATOR",
                "post_restore_fencing_apply": "DATABASE_OPERATOR",
                "signing_key_overlap_rotation": "SECURITY_OPERATOR",
                "ue58_dedicated_server_recovery": "UNREAL_QA_OPERATOR",
                "ue58_pie_multiplayer": "UNREAL_QA_OPERATOR",
                "android_device_package": "ANDROID_QA_OPERATOR",
                "stage40_slo_approval": "SLO_REVIEWER",
            }[kind]
            reviewer_role = (
                "DATABASE_REVIEWER"
                if kind in MODULE.CRITICAL_EVIDENCE_KINDS
                else "QA_REVIEWER"
            )
            entries.append({
                "evidenceId": str(uuid4()),
                "kind": kind,
                "sourceClass": source_class,
                "environmentId": environment_id,
                "capturedAtUtc": STAMP,
                "operatorRole": operator_role,
                "reviewerRole": reviewer_role,
                "artifact": {
                    "path": relative,
                    "sha256": hashlib.sha256(encoded).hexdigest(),
                    "mediaType": "application/json",
                },
            })

        return {
            "schema": MODULE.PACKET_SCHEMA,
            "stage": 43,
            "packetId": str(uuid4()),
            "createdAtUtc": STAMP,
            "environment": {
                "id": STAGING_ID,
                "class": "staging",
                "storageProvider": "postgresql",
            },
            "submissionStatus": "REVIEWED",
            "entries": entries,
            "review": {
                "decision": "APPROVED",
                "reviewerRoles": ["SERVICE_OWNER", "SECURITY_REVIEWER"],
                "reviewedAtUtc": STAMP,
            },
        }

    def test_repository_template_is_valid_but_blocked(self) -> None:
        packet_path = ROOT / "Data/Encounter/ProductionReadinessEvidencePacket.template.json"
        packet = json.loads(packet_path.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as folder:
            result = MODULE.evaluate_packet(packet, Path(folder), ROOT)
        self.assertEqual(result["errors"], [])
        self.assertEqual(result["outcome"], "BLOCKED_MISSING_EVIDENCE")
        self.assertEqual(set(result["missingEvidenceKinds"]), set(MODULE.REQUIRED_KINDS))
        self.assertFalse(result["productionReady"])
        self.assertFalse(result["promotionAllowed"])

    def test_complete_two_role_review_is_eligible_but_never_auto_promoted(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet = self._build_packet(root)
            result = MODULE.evaluate_packet(packet, root, ROOT)
        self.assertEqual(result["errors"], [])
        self.assertEqual(result["outcome"], "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW")
        self.assertEqual(result["missingEvidenceKinds"], [])
        self.assertFalse(result["productionReady"])
        self.assertFalse(result["highAvailabilityApproved"])
        self.assertEqual(result["sloStatus"], "UNBASELINED")
        self.assertFalse(result["promotionAllowed"])

    def test_ephemeral_ci_does_not_count_as_real_staging(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet = self._build_packet(root)
            stage_entry = next(item for item in packet["entries"] if item["kind"] == "staging_smoke")
            report_path = root / stage_entry["artifact"]["path"]
            report = json.loads(report_path.read_text(encoding="utf-8"))
            report["environment"]["class"] = "ephemeral-ci"
            encoded = json.dumps(report, sort_keys=True).encode("utf-8")
            report_path.write_bytes(encoded)
            stage_entry["artifact"]["sha256"] = hashlib.sha256(encoded).hexdigest()
            result = MODULE.evaluate_packet(packet, root, ROOT)
        self.assertIn("EPHEMERAL_CI_IS_NOT_OPERATOR_STAGING", result["errors"])

    def test_artifact_digest_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet = self._build_packet(root)
            entry = packet["entries"][0]
            (root / entry["artifact"]["path"]).write_text("{}", encoding="utf-8")
            result = MODULE.evaluate_packet(packet, root, ROOT)
        self.assertIn("ARTIFACT_SHA256_MISMATCH", [item["code"] for item in result["rejectedEvidence"]])

    def test_path_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet = self._build_packet(root)
            packet["entries"][0]["artifact"]["path"] = "../outside.json"
            result = MODULE.evaluate_packet(packet, root, ROOT)
        self.assertIn("ARTIFACT_PATH_TRAVERSAL_REJECTED", [item["code"] for item in result["rejectedEvidence"]])

    def test_sensitive_artifact_fields_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet = self._build_packet(root)
            entry = packet["entries"][1]
            report_path = root / entry["artifact"]["path"]
            report = json.loads(report_path.read_text(encoding="utf-8"))
            report["claims"]["rawProfileId"] = "must-not-be-present"
            encoded = json.dumps(report, sort_keys=True).encode("utf-8")
            report_path.write_bytes(encoded)
            entry["artifact"]["sha256"] = hashlib.sha256(encoded).hexdigest()
            result = MODULE.evaluate_packet(packet, root, ROOT)
        self.assertIn("ARTIFACT_SENSITIVE_FIELDS_REJECTED", [item["code"] for item in result["rejectedEvidence"]])

    def test_fencing_apply_must_increment_every_row(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet = self._build_packet(root)
            entry = next(item for item in packet["entries"] if item["kind"] == "post_restore_fencing_apply")
            report_path = root / entry["artifact"]["path"]
            report = json.loads(report_path.read_text(encoding="utf-8"))
            report["rowsFenced"] = 2
            encoded = json.dumps(report, sort_keys=True).encode("utf-8")
            report_path.write_bytes(encoded)
            entry["artifact"]["sha256"] = hashlib.sha256(encoded).hexdigest()
            result = MODULE.evaluate_packet(packet, root, ROOT)
        self.assertIn("FENCING_APPLY_DID_NOT_FENCE_ALL_ROWS", result["errors"])

    def test_final_approval_requires_two_independent_review_roles(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet = self._build_packet(root)
            packet["review"]["reviewerRoles"] = ["SERVICE_OWNER"]
            result = MODULE.evaluate_packet(packet, root, ROOT)
        self.assertIn(
            "APPROVAL_REQUIRES_TWO_REVIEWERS_INCLUDING_SECURITY_OR_DATABASE",
            result["errors"],
        )

    def test_stage41_and_stage42_manifests_must_remain_fail_closed(self) -> None:
        errors = MODULE.validate_project_boundaries(ROOT)
        self.assertEqual(errors, [])


if __name__ == "__main__":
    unittest.main()
