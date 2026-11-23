from __future__ import annotations

import base64
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "Scripts"
TESTS = ROOT / "Scripts" / "Tests"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(TESTS))

import encounter_checkpoint_evidence_attestation as STAGE44
import encounter_checkpoint_release_board as MODULE
import test_validate_encounter_checkpoint_stage43_evidence as STAGE43_FIXTURES
import test_validate_encounter_checkpoint_stage44_attestation as STAGE44_FIXTURES

STAMP = "2026-11-22T12:00:00Z"


def _json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def _build_context(root: Path) -> dict:
    evidence_root = root / "evidence"
    evidence_root.mkdir(parents=True)
    artifact_root = root / "artifacts"
    artifact_root.mkdir(parents=True)

    packet = STAGE43_FIXTURES.Stage43EvidenceGateTests()._build_packet(evidence_root)
    build_id = "JUMANJI-ANDROID-QA-01"

    android_entry = next(item for item in packet["entries"] if item["kind"] == "android_device_package")
    android_report_path = evidence_root / android_entry["artifact"]["path"]
    android_report = json.loads(android_report_path.read_text(encoding="utf-8"))

    android_dir = artifact_root / "android"
    server_dir = artifact_root / "server"
    android_dir.mkdir()
    server_dir.mkdir()
    android_bytes = b"synthetic-stage45-android-apk"
    server_bytes = b"synthetic-stage45-ue58-dedicated-server-package"
    android_path = android_dir / "jumanji.apk"
    server_path = server_dir / "jumanji-server.zip"
    android_path.write_bytes(android_bytes)
    server_path.write_bytes(server_bytes)

    android_sha = hashlib.sha256(android_bytes).hexdigest()
    server_sha = hashlib.sha256(server_bytes).hexdigest()
    android_report["claims"]["packageSha256"] = android_sha
    android_report["claims"]["packageBuildId"] = build_id
    encoded_report = json.dumps(android_report, sort_keys=True).encode("utf-8")
    android_report_path.write_bytes(encoded_report)
    android_entry["artifact"]["sha256"] = hashlib.sha256(encoded_report).hexdigest()

    keyring, evidence_private = STAGE44_FIXTURES._key_material()
    envelope = STAGE44._empty_envelope(packet)
    key_by_role = {
        role: next(record["keyId"] for record in keyring["keys"] if role in record["roles"])
        for role in STAGE44.ALL_ROLES
    }
    for entry in packet["entries"]:
        STAGE44.sign_entry(
            packet, envelope, keyring, entry["evidenceId"], "operator",
            key_by_role[entry["operatorRole"]],
            evidence_private[entry["operatorRole"]], STAMP,
        )
        STAGE44.sign_entry(
            packet, envelope, keyring, entry["evidenceId"], "reviewer",
            key_by_role[entry["reviewerRole"]],
            evidence_private[entry["reviewerRole"]], STAMP,
        )
    for role in packet["review"]["reviewerRoles"]:
        STAGE44.sign_final_review(
            packet, envelope, keyring, key_by_role[role],
            role, evidence_private[role], STAMP, evidence_root,
        )

    board_private: dict[str, Ed25519PrivateKey] = {}
    for index, role in enumerate(MODULE.BOARD_ROLES):
        private = Ed25519PrivateKey.generate()
        board_private[role] = private
        public = private.public_key().public_bytes(
            serialization.Encoding.Raw, serialization.PublicFormat.Raw
        )
        keyring["keys"].append({
            "keyId": f"BOARD-TEST-{index:02d}",
            "publicKeyBase64": base64.b64encode(public).decode("ascii"),
            "roles": [role],
            "status": "ACTIVE",
            "validFromUtc": STAMP,
            "validUntilUtc": None,
        })

    keyring_raw = _json_bytes(keyring)
    trusted_hash = hashlib.sha256(keyring_raw).hexdigest()
    attestation_raw = _json_bytes(envelope)
    candidate = {
        "schema": MODULE.CANDIDATE_SCHEMA,
        "candidateId": "JUMANJI-STAGE45-CANDIDATE-01",
        "buildId": build_id,
        "packetId": packet["packetId"],
        "packetSha256": STAGE44.packet_digest(packet),
        "environmentId": packet["environment"]["id"],
        "sourceCommitSha": "a" * 40,
        "engineVersion": "5.8",
        "createdAtUtc": STAMP,
        "targets": [
            {
                "target": "ue58-dedicated-server",
                "path": "server/jumanji-server.zip",
                "sha256": server_sha,
                "sizeBytes": len(server_bytes),
                "mediaType": "application/zip",
            },
            {
                "target": "android-package",
                "path": "android/jumanji.apk",
                "sha256": android_sha,
                "sizeBytes": len(android_bytes),
                "mediaType": "application/vnd.android.package-archive",
            },
        ],
    }
    candidate_raw = _json_bytes(candidate)
    decision = MODULE.prepare_decision(
        packet, candidate, candidate_raw, envelope, attestation_raw,
        keyring, keyring_raw, trusted_hash, evidence_root, artifact_root,
        "APPROVE", project_root=ROOT,
    )
    return {
        "packet": packet,
        "evidence_root": evidence_root,
        "artifact_root": artifact_root,
        "candidate": candidate,
        "candidate_raw": candidate_raw,
        "envelope": envelope,
        "attestation_raw": attestation_raw,
        "keyring": keyring,
        "keyring_raw": keyring_raw,
        "trusted_hash": trusted_hash,
        "decision": decision,
        "board_private": board_private,
        "evidence_private": evidence_private,
    }


def _sign_all(context: dict) -> dict:
    decision = context["decision"]
    for role in MODULE.BOARD_ROLES:
        key_record = next(
            item for item in context["keyring"]["keys"]
            if item["keyId"].startswith("BOARD-TEST-") and role in item["roles"]
        )
        decision = MODULE.sign_decision(
            decision,
            context["packet"],
            context["candidate"],
            context["candidate_raw"],
            context["envelope"],
            context["attestation_raw"],
            context["envelope"],
            context["keyring"],
            context["keyring_raw"],
            context["trusted_hash"],
            context["evidence_root"],
            context["artifact_root"],
            key_record["keyId"],
            role,
            context["board_private"][role],
            project_root=ROOT,
        )
    return decision


class Stage45ReleaseBoardTests(unittest.TestCase):
    def _evaluate(self, context: dict, decision: dict | None = None) -> dict:
        return MODULE.evaluate(
            context["packet"],
            context["evidence_root"],
            context["envelope"],
            context["attestation_raw"],
            context["envelope"],
            context["keyring"],
            context["keyring_raw"],
            context["trusted_hash"],
            context["candidate"],
            context["candidate_raw"],
            context["artifact_root"],
            decision if decision is not None else context["decision"],
            ROOT,
        )

    def test_unsigned_decision_waits_for_all_three_board_roles(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            report = self._evaluate(context)
        self.assertEqual(report["outcome"], "WAITING_FOR_RELEASE_BOARD_SIGNATURES")
        self.assertEqual(set(report["missingBoardRoles"]), set(MODULE.BOARD_ROLES))
        self.assertFalse(report["deploymentAuthorizationGranted"])
        self.assertFalse(report["productionReady"])
        self.assertFalse(report["promotionAllowed"])

    def test_three_independent_signatures_authorize_exact_candidate_only(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            decision = _sign_all(context)
            report = self._evaluate(context, decision)
        self.assertEqual(report["outcome"], "APPROVED_FOR_DEPLOYMENT")
        self.assertEqual(set(report["verifiedBoardRoles"]), set(MODULE.BOARD_ROLES))
        self.assertTrue(report["deploymentAuthorizationGranted"])
        self.assertFalse(report["deploymentExecuted"])
        self.assertFalse(report["productionReady"])
        self.assertFalse(report["highAvailabilityApproved"])
        self.assertEqual(report["sloStatus"], "UNBASELINED")
        self.assertFalse(report["promotionAllowed"])

    def test_stage44_signer_cannot_be_reused_as_release_board_member(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            record = context["envelope"]["entryAttestations"][0]["operator"]
            signer_id = record["keyId"]
            signer_role = next(
                entry["operatorRole"] for entry in context["packet"]["entries"]
                if entry["evidenceId"] == context["envelope"]["entryAttestations"][0]["evidenceId"]
            )
            with self.assertRaisesRegex(MODULE.ReleaseBoardError, "BOARD_KEY_REUSED_UPSTREAM"):
                MODULE.sign_decision(
                    context["decision"], context["packet"], context["candidate"],
                    context["candidate_raw"], context["envelope"], context["attestation_raw"],
                    context["envelope"], context["keyring"], context["keyring_raw"],
                    context["trusted_hash"], context["evidence_root"], context["artifact_root"],
                    signer_id, signer_role, context["evidence_private"][signer_role],
                    project_root=ROOT,
                )

    def test_mutating_android_artifact_invalidates_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            (context["artifact_root"] / "android" / "jumanji.apk").write_bytes(b"tampered")
            report = self._evaluate(context)
        self.assertEqual(report["outcome"], "INVALID_RELEASE_CANDIDATE")
        self.assertIn("CANDIDATE_ARTIFACT_SIZE_MISMATCH:android-package", report["errors"])
        self.assertFalse(report["deploymentAuthorizationGranted"])

    def test_candidate_manifest_mutation_invalidates_decision_binding(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["candidate"]["candidateId"] = "JUMANJI-MUTATED-CANDIDATE"
            report = self._evaluate(context)
        self.assertEqual(report["outcome"], "INVALID_RELEASE_CANDIDATE")
        self.assertIn("CANDIDATE_ANDROID_BUILD_ID_MISMATCH", report["errors"])

    def test_rejected_board_decision_never_authorizes_deployment(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["decision"]["decision"] = "REJECT"
            context["decision"]["candidateSha256"] = hashlib.sha256(context["candidate_raw"]).hexdigest()
            context["decision"]["packetSha256"] = STAGE44.packet_digest(context["packet"])
            context["decision"]["attestationSha256"] = hashlib.sha256(context["attestation_raw"]).hexdigest()
            context["decision"]["keyringSha256"] = hashlib.sha256(context["keyring_raw"]).hexdigest()
            decision = _sign_all(context)
            report = self._evaluate(context, decision)
        self.assertEqual(report["outcome"], "REJECTED_BY_RELEASE_BOARD")
        self.assertFalse(report["deploymentAuthorizationGranted"])

    def test_changed_trust_keyring_breaks_decision_binding(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            decision = _sign_all(context)
            context["keyring"]["keys"][-1]["status"] = "REVOKED"
            context["keyring_raw"] = _json_bytes(context["keyring"])
            context["trusted_hash"] = hashlib.sha256(context["keyring_raw"]).hexdigest()
            report = self._evaluate(context, decision)
        self.assertEqual(report["outcome"], "INVALID_RELEASE_BOARD_DECISION")
        self.assertTrue(any("keyringSha256" in error for error in report["errors"]))
        self.assertFalse(report["deploymentAuthorizationGranted"])

    def test_empty_repository_templates_remain_blocked(self) -> None:
        candidate = json.loads(
            (ROOT / "Data/Encounter/ProductionReleaseCandidate.template.json").read_text(encoding="utf-8")
        )
        decision = json.loads(
            (ROOT / "Data/Encounter/ProductionReleaseBoardDecision.template.json").read_text(encoding="utf-8")
        )
        self.assertEqual(candidate["targets"], [])
        self.assertIsNone(candidate["candidateId"])
        self.assertEqual(decision["decision"], "NOT_DECIDED")
        self.assertEqual(decision["boardSignatures"], [])


if __name__ == "__main__":
    unittest.main()
