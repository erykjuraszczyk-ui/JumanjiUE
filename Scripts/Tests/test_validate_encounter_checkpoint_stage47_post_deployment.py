from __future__ import annotations

import base64
import hashlib
import json
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from uuid import uuid4

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

import encounter_checkpoint_deployment_evidence as MODULE


def _stamp(value: datetime) -> str:
    return value.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def _json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def _refresh_journal_signatures(context: dict) -> None:
    context["journal_raw"] = _json_bytes(context["journal"])
    journal_sha = hashlib.sha256(context["journal_raw"]).hexdigest()
    plan_sha = hashlib.sha256(context["plan_raw"]).hexdigest()
    context["attestation"]["journalSha256"] = journal_sha
    context["attestation"]["planSha256"] = plan_sha
    signatures = []
    for role, private_key in context["private_keys"].items():
        key_id = f"TEST-{role}"
        signed_at = MODULE.utc_now()
        raw_signature = private_key.sign(MODULE.signature_message(
            journal_sha, plan_sha, role, key_id, signed_at
        ))
        signatures.append({
            "role": role,
            "keyId": key_id,
            "signedAtUtc": signed_at,
            "signatureBase64": base64.b64encode(raw_signature).decode("ascii"),
        })
    context["attestation"]["signatures"] = signatures
    context["attestation_raw"] = _json_bytes(context["attestation"])


def _build_context(root: Path) -> dict:
    evidence_root = root / "evidence"
    evidence_root.mkdir()
    now = datetime.now(timezone.utc).replace(microsecond=0)
    started = now - timedelta(minutes=3)
    phases = []
    for index, (phase_name, categories) in enumerate(MODULE.PHASE_REQUIREMENTS):
        phase_start = started + timedelta(seconds=index * 15)
        phase_end = phase_start + timedelta(seconds=10)
        records = []
        for category in categories:
            evidence_id = str(uuid4())
            relative = f"{phase_name.lower()}/{category}.txt"
            path = evidence_root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            raw = f"synthetic test evidence: {phase_name}:{category}\n".encode("utf-8")
            path.write_bytes(raw)
            records.append({
                "evidenceId": evidence_id,
                "category": category,
                "path": relative,
                "sha256": hashlib.sha256(raw).hexdigest(),
                "sizeBytes": len(raw),
                "mediaType": "text/plain",
            })
        phases.append({
            "phase": phase_name,
            "status": "PASSED",
            "startedAtUtc": _stamp(phase_start),
            "completedAtUtc": _stamp(phase_end),
            "evidence": records,
        })

    plan = {
        "schema": "JUMANJI.ProductionDeploymentPlan.v1",
        "stage": 46,
        "planId": str(uuid4()),
        "targetEnvironment": "production",
        "executionMode": "MANUAL_OUT_OF_BAND",
        "state": "READY_FOR_OPERATOR_REVIEW",
        "candidate": {"candidateSha256": "a" * 64},
        "rollbackBaseline": {"baselineSha256": "b" * 64},
        "sideEffectsPerformed": False,
        "deploymentExecuted": False,
        "postDeploymentVerified": False,
        "rollbackExecuted": False,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
        "promotionAllowed": False,
    }
    plan_raw = _json_bytes(plan)
    journal = {
        "schema": MODULE.JOURNAL_SCHEMA,
        "journalId": str(uuid4()),
        "planId": plan["planId"],
        "planSha256": hashlib.sha256(plan_raw).hexdigest(),
        "candidateSha256": plan["candidate"]["candidateSha256"],
        "rollbackBaselineSha256": plan["rollbackBaseline"]["baselineSha256"],
        "environmentId": "production",
        "executionMode": "MANUAL_OUT_OF_BAND",
        "outcome": "SUCCEEDED",
        "rollbackOutcome": "NOT_REQUIRED",
        "startedAtUtc": _stamp(started),
        "completedAtUtc": _stamp(started + timedelta(seconds=55)),
        "phases": phases,
    }
    private_keys = {
        role: Ed25519PrivateKey.generate()
        for role in ("SERVICE_OPERATOR", "INDEPENDENT_REVIEWER")
    }
    ring_keys = []
    for role, private_key in private_keys.items():
        public_raw = private_key.public_key().public_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PublicFormat.Raw,
        )
        ring_keys.append({
            "keyId": f"TEST-{role}",
            "publicKeyBase64": base64.b64encode(public_raw).decode("ascii"),
            "roles": [role],
            "status": "ACTIVE",
            "validFromUtc": _stamp(now - timedelta(days=1)),
            "validUntilUtc": None,
        })
    keyring = {"schema": MODULE.KEYRING_SCHEMA, "keys": ring_keys}
    keyring_raw = _json_bytes(keyring)
    context = {
        "plan": plan, "plan_raw": plan_raw,
        "journal": journal, "journal_raw": _json_bytes(journal),
        "evidence_root": evidence_root,
        "attestation": {
            "schema": MODULE.ATTESTATION_SCHEMA,
            "journalId": journal["journalId"],
            "journalSha256": "",
            "planSha256": "",
            "canonicalization": MODULE.CANONICALIZATION,
            "signatures": [],
        },
        "keyring": keyring, "keyring_raw": keyring_raw,
        "trusted_sha": hashlib.sha256(keyring_raw).hexdigest(),
        "private_keys": private_keys,
    }
    _refresh_journal_signatures(context)
    return context


def _verify(context: dict) -> dict:
    return MODULE.verify_deployment(
        context["plan"], context["plan_raw"],
        context["journal"], context["journal_raw"],
        context["evidence_root"],
        context["attestation"], context["attestation_raw"],
        context["keyring"], context["keyring_raw"], context["trusted_sha"],
    )


class Stage47DeploymentEvidenceTests(unittest.TestCase):
    def test_signed_complete_journal_verifies_without_promoting_production(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            report = _verify(_build_context(Path(folder)))
        self.assertEqual(report["outcome"], "POST_DEPLOYMENT_EVIDENCE_VERIFIED")
        self.assertEqual(report["verifiedEvidenceCount"], 18)
        self.assertTrue(report["deploymentExecuted"])
        self.assertTrue(report["postDeploymentVerified"])
        self.assertFalse(report["rollbackExecuted"])
        self.assertFalse(report["productionReady"])
        self.assertFalse(report["highAvailabilityApproved"])
        self.assertEqual(report["sloStatus"], "UNBASELINED")
        self.assertFalse(report["promotionAllowed"])
        self.assertFalse(report["sideEffectsPerformedByVerifier"])

    def test_missing_phase_evidence_blocks_verification(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["journal"]["phases"][1]["evidence"].pop()
            _refresh_journal_signatures(context)
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "DEPLOYMENT_PHASE_EVIDENCE_COUNT_INVALID"):
                _verify(context)

    def test_evidence_file_tampering_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            record = context["journal"]["phases"][0]["evidence"][0]
            (context["evidence_root"] / record["path"]).write_text("tampered", encoding="utf-8")
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "DEPLOYMENT_EVIDENCE_(SIZE|HASH)_MISMATCH"):
                _verify(context)

    def test_plan_byte_change_breaks_journal_binding(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["plan"]["state"] = "MUTATED"
            context["plan_raw"] = _json_bytes(context["plan"])
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "DEPLOYMENT_PLAN_NOT_READY_FOR_REVIEW|DEPLOYMENT_PLAN_BINDING_MISMATCH"):
                _verify(context)

    def test_signer_key_cannot_be_reused_for_both_roles(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            first_key = context["attestation"]["signatures"][0]["keyId"]
            context["attestation"]["signatures"][1]["keyId"] = first_key
            context["attestation_raw"] = _json_bytes(context["attestation"])
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "DEPLOYMENT_SIGNER_KEY_REUSED"):
                _verify(context)

    def test_revoked_operator_key_blocks_verification(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["keyring"]["keys"][0]["status"] = "REVOKED"
            context["keyring_raw"] = _json_bytes(context["keyring"])
            context["trusted_sha"] = hashlib.sha256(context["keyring_raw"]).hexdigest()
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "DEPLOYMENT_SIGNER_NOT_TRUSTED_FOR_ROLE"):
                _verify(context)

    def test_keyring_fingerprint_mismatch_blocks_verification(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["trusted_sha"] = "0" * 64
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "TRUST_KEYRING_FINGERPRINT_MISMATCH"):
                _verify(context)

    def test_path_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["journal"]["phases"][0]["evidence"][0]["path"] = "../outside.txt"
            _refresh_journal_signatures(context)
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "EVIDENCE_PATH_TRAVERSAL"):
                _verify(context)

    def test_unsuccessful_rollout_or_executed_rollback_is_not_closable(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["journal"]["rollbackOutcome"] = "MANUAL_ROLLBACK_COMPLETED"
            _refresh_journal_signatures(context)
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "DEPLOYMENT_JOURNAL_OUTCOME_NOT_CLOSABLE"):
                _verify(context)

    def test_invalid_ed25519_signature_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["attestation"]["signatures"][0]["signatureBase64"] = base64.b64encode(b"x" * 64).decode("ascii")
            context["attestation_raw"] = _json_bytes(context["attestation"])
            with self.assertRaisesRegex(MODULE.DeploymentEvidenceError, "DEPLOYMENT_SIGNATURE_INVALID"):
                _verify(context)

    def test_empty_repository_templates_remain_blocked(self) -> None:
        root = Path(__file__).resolve().parents[2]
        journal = json.loads((root / "Data/Encounter/ProductionDeploymentEvidenceJournal.template.json").read_text(encoding="utf-8"))
        attestation = json.loads((root / "Data/Encounter/ProductionDeploymentEvidenceAttestation.template.json").read_text(encoding="utf-8"))
        self.assertIsNone(journal["journalId"])
        self.assertEqual(journal["outcome"], "NOT_RECORDED")
        self.assertEqual(journal["phases"], [])
        self.assertEqual(attestation["signatures"], [])


if __name__ == "__main__":
    unittest.main()
