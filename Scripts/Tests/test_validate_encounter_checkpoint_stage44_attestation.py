from __future__ import annotations

import base64
import copy
import hashlib
import json
import sys
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "Scripts"
TESTS = ROOT / "Scripts" / "Tests"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(TESTS))

import encounter_checkpoint_evidence_attestation as MODULE
import test_validate_encounter_checkpoint_stage43_evidence as STAGE43_FIXTURES


STAMP = "2026-11-22T12:00:00Z"


def _roles() -> list[str]:
    return sorted(MODULE.ALL_ROLES)


def _key_material() -> tuple[dict, dict[str, Ed25519PrivateKey]]:
    private_keys: dict[str, Ed25519PrivateKey] = {}
    records = []
    for index, role in enumerate(_roles()):
        private = Ed25519PrivateKey.generate()
        private_keys[role] = private
        public = private.public_key().public_bytes(
            serialization.Encoding.Raw, serialization.PublicFormat.Raw
        )
        records.append({
            "keyId": f"TEST-KEY-{index:02d}",
            "publicKeyBase64": base64.b64encode(public).decode("ascii"),
            "roles": [role],
            "status": "ACTIVE",
            "validFromUtc": STAMP,
            "validUntilUtc": None,
        })
    return {"schema": MODULE.KEYRING_SCHEMA, "keys": records}, private_keys


def _make_complete(
    root: Path,
    *,
    keyring: dict | None = None,
    private_keys: dict[str, Ed25519PrivateKey] | None = None,
) -> tuple[dict, dict, dict, dict[str, Ed25519PrivateKey], bytes, str]:
    root.mkdir(parents=True, exist_ok=True)
    packet = STAGE43_FIXTURES.Stage43EvidenceGateTests()._build_packet(root)
    if keyring is None or private_keys is None:
        keyring, private_keys = _key_material()
    raw_keyring = (json.dumps(keyring, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    trusted_hash = hashlib.sha256(raw_keyring).hexdigest()
    envelope = MODULE._empty_envelope(packet)

    key_by_role = {
        role: next(record["keyId"] for record in keyring["keys"] if role in record["roles"])
        for role in _roles()
    }
    for entry in packet["entries"]:
        MODULE.sign_entry(
            packet, envelope, keyring, entry["evidenceId"], "operator",
            key_by_role[entry["operatorRole"]],
            private_keys[entry["operatorRole"]], STAMP,
        )
        MODULE.sign_entry(
            packet, envelope, keyring, entry["evidenceId"], "reviewer",
            key_by_role[entry["reviewerRole"]],
            private_keys[entry["reviewerRole"]], STAMP,
        )
    for role in packet["review"]["reviewerRoles"]:
        MODULE.sign_final_review(
            packet, envelope, keyring, key_by_role[role], role,
            private_keys[role], STAMP, root,
        )
    return packet, envelope, keyring, private_keys, raw_keyring, trusted_hash


class Stage44EvidenceAttestationTests(unittest.TestCase):
    def test_empty_repository_templates_remain_blocked(self) -> None:
        packet = json.loads((ROOT / "Data/Encounter/ProductionReadinessEvidencePacket.template.json").read_text())
        envelope = json.loads((ROOT / "Data/Encounter/ProductionEvidenceAttestation.template.json").read_text())
        keyring = json.loads((ROOT / "Data/Encounter/ProductionEvidenceTrustKeyring.template.json").read_text())
        raw_keyring = (json.dumps(keyring, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
        with tempfile.TemporaryDirectory() as folder:
            report = MODULE.evaluate(
                packet, Path(folder), envelope, keyring,
                hashlib.sha256(raw_keyring).hexdigest(), raw_keyring, ROOT,
            )
        self.assertEqual(report["outcome"], "BLOCKED_MISSING_EVIDENCE")
        self.assertFalse(report["productionReady"])
        self.assertFalse(report["highAvailabilityApproved"])
        self.assertEqual(report["sloStatus"], "UNBASELINED")
        self.assertFalse(report["promotionAllowed"])

    def test_complete_packet_with_all_signatures_is_only_release_board_eligible(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, raw, digest = _make_complete(root)
            report = MODULE.evaluate(packet, root, envelope, keyring, digest, raw, ROOT)
        self.assertEqual(report["outcome"], "ELIGIBLE_FOR_FINAL_RELEASE_BOARD_REVIEW")
        self.assertEqual(report["verifiedOperatorSignatures"], 10)
        self.assertEqual(report["verifiedEvidenceReviewerSignatures"], 10)
        self.assertEqual(report["verifiedFinalReviewSignatures"], 2)
        self.assertTrue(report["trustAnchorVerified"])
        self.assertFalse(report["productionReady"])
        self.assertFalse(report["highAvailabilityApproved"])
        self.assertEqual(report["sloStatus"], "UNBASELINED")
        self.assertFalse(report["promotionAllowed"])

    def test_missing_entry_or_final_signatures_block(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, raw, digest = _make_complete(root)
            envelope["entryAttestations"] = []
            envelope["finalReviewSignatures"] = []
            report = MODULE.evaluate(packet, root, envelope, keyring, digest, raw, ROOT)
        self.assertEqual(report["outcome"], "BLOCKED_MISSING_ATTESTATIONS")
        self.assertGreater(report["missingEntrySignatures"], 0)
        self.assertGreater(report["missingFinalReviewSignatures"], 0)

    def test_missing_out_of_band_keyring_hash_blocks_trust(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, raw, _ = _make_complete(root)
            report = MODULE.evaluate(packet, root, envelope, keyring, None, raw, ROOT)
        self.assertEqual(report["outcome"], "BLOCKED_TRUST_ANCHOR_UNPINNED")
        self.assertFalse(report["trustAnchorVerified"])

    def test_keyring_hash_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, raw, _ = _make_complete(root)
            report = MODULE.evaluate(packet, root, envelope, keyring, "0" * 64, raw, ROOT)
        self.assertEqual(report["outcome"], "INVALID_ATTESTATION")
        self.assertIn("TRUST_ANCHOR_HASH_MISMATCH", report["errors"])

    def test_packet_mutation_invalidates_every_packet_bound_signature(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, raw, digest = _make_complete(root)
            packet["environment"]["id"] = "STAGING-CHANGED"
            report = MODULE.evaluate(packet, root, envelope, keyring, digest, raw, ROOT)
        self.assertIn("ATTESTATION_PACKET_BINDING_MISMATCH", report["errors"])

    def test_signature_tampering_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, raw, digest = _make_complete(root)
            signature = envelope["entryAttestations"][0]["operator"]["signatureBase64"]
            damaged = bytearray(base64.b64decode(signature))
            damaged[0] ^= 1
            envelope["entryAttestations"][0]["operator"]["signatureBase64"] = base64.b64encode(damaged).decode("ascii")
            report = MODULE.evaluate(packet, root, envelope, keyring, digest, raw, ROOT)
        self.assertEqual(report["outcome"], "INVALID_ATTESTATION")
        self.assertIn("SIGNATURE_VERIFICATION_FAILED", report["errors"])

    def test_revoked_key_invalidates_existing_signature(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, _, _ = _make_complete(root)
            keyring["keys"][0]["status"] = "REVOKED"
            raw = (json.dumps(keyring, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
            digest = hashlib.sha256(raw).hexdigest()
            report = MODULE.evaluate(packet, root, envelope, keyring, digest, raw, ROOT)
        self.assertEqual(report["outcome"], "INVALID_ATTESTATION")
        self.assertIn("TRUSTED_KEY_REVOKED", report["errors"])

    def test_duplicate_public_key_in_trust_keyring_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, _, _ = _make_complete(root)
            keyring["keys"][1]["publicKeyBase64"] = keyring["keys"][0]["publicKeyBase64"]
            raw = (json.dumps(keyring, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
            digest = hashlib.sha256(raw).hexdigest()
            report = MODULE.evaluate(packet, root, envelope, keyring, digest, raw, ROOT)
        self.assertEqual(report["outcome"], "INVALID_ATTESTATION")
        self.assertIn("DUPLICATE_PUBLIC_KEY", report["errors"])

    def test_final_reviewer_cannot_be_an_evidence_operator(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet = STAGE43_FIXTURES.Stage43EvidenceGateTests()._build_packet(root)
            keyring, private_keys = _key_material()
            operator_record = next(item for item in keyring["keys"] if "SERVICE_OPERATOR" in item["roles"])
            service_owner_record = next(item for item in keyring["keys"] if "SERVICE_OWNER" in item["roles"])
            keyring["keys"].remove(service_owner_record)
            operator_record["roles"].append("SERVICE_OWNER")
            private_keys["SERVICE_OWNER"] = private_keys["SERVICE_OPERATOR"]
            raw = (json.dumps(keyring, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
            key_by_role = {
                role: next(item["keyId"] for item in keyring["keys"] if role in item["roles"])
                for role in _roles()
            }
            envelope = MODULE._empty_envelope(packet)
            entry = packet["entries"][0]
            MODULE.sign_entry(packet, envelope, keyring, entry["evidenceId"], "operator",
                              key_by_role[entry["operatorRole"]], private_keys[entry["operatorRole"]], STAMP)
            with self.assertRaisesRegex(MODULE.AttestationError, "FINAL_REVIEWER_WAS_EVIDENCE_OPERATOR"):
                MODULE.sign_final_review(
                    packet, envelope, keyring, key_by_role["SERVICE_OWNER"], "SERVICE_OWNER",
                    private_keys["SERVICE_OWNER"], STAMP, root,
                )

    def test_signature_before_evidence_capture_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, private_keys, raw, digest = _make_complete(root)
            record = envelope["entryAttestations"][0]["operator"]
            entry = next(item for item in packet["entries"] if item["evidenceId"] == envelope["entryAttestations"][0]["evidenceId"])
            payload = MODULE._entry_payload(packet, digest, entry, "operator", entry["operatorRole"], "2026-11-22T11:00:00Z")
            record["signedAtUtc"] = "2026-11-22T11:00:00Z"
            signature = private_keys[entry["operatorRole"]].sign(MODULE._canonical(payload))
            record["signatureBase64"] = base64.b64encode(signature).decode("ascii")
            report = MODULE.evaluate(packet, root, envelope, keyring, digest, raw, ROOT)
        self.assertEqual(report["outcome"], "INVALID_ATTESTATION")
        self.assertIn("SIGNATURE_PREDATES_EVIDENCE_OR_REVIEW", report["errors"])

    def test_key_role_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            packet, envelope, keyring, _, raw, digest = _make_complete(root)
            first = envelope["entryAttestations"][0]
            first["reviewer"]["keyId"] = first["operator"]["keyId"]
            report = MODULE.evaluate(packet, root, envelope, keyring, digest, raw, ROOT)
        self.assertEqual(report["outcome"], "INVALID_ATTESTATION")
        self.assertTrue(any(code in report["errors"] for code in (
            "TRUSTED_KEY_ROLE_MISMATCH", "ENTRY_SIGNERS_NOT_INDEPENDENT"
        )))


if __name__ == "__main__":
    unittest.main()
