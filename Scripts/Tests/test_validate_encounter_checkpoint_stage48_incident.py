from __future__ import annotations

import base64
import hashlib
import json
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

import encounter_checkpoint_incident_evidence as MODULE


def _stamp(value: datetime) -> str:
    return value.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def _json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def _build_context(root: Path, rollback: str = "SUCCEEDED") -> dict:
    evidence_root = root / "evidence"
    evidence_root.mkdir()
    now = datetime.now(timezone.utc).replace(microsecond=0)
    start = now - timedelta(minutes=8)
    event_types = list(MODULE.BASE_EVENT_ORDER)
    if rollback != "NOT_REQUIRED":
        event_types.append("ROLLBACK_EXECUTED")
    event_types.extend(MODULE.TERMINAL_EVENT_ORDER)
    actor_roles = {
        "DETECTED": "SERVICE_OPERATOR",
        "DECLARED": "SERVICE_OPERATOR",
        "IMPACT_ASSESSED": "SECURITY_OPERATOR",
        "CONTAINED": "SERVICE_OPERATOR",
        "RECOVERY_DECISION": "SERVICE_OWNER",
        "ROLLBACK_EXECUTED": "DATABASE_OPERATOR",
        "SERVICE_RESTORED": "SERVICE_OPERATOR",
        "DATA_INTEGRITY_VERIFIED": "DATABASE_REVIEWER",
        "POST_INCIDENT_REVIEW": "INDEPENDENT_REVIEWER",
    }
    events = []
    for index, event_type in enumerate(event_types):
        occurred = start + timedelta(seconds=index * 35)
        relative = f"incident/{index:02d}-{event_type.lower()}.txt"
        raw = f"Synthetic sanitized evidence for {event_type}\n".encode("utf-8")
        attachment = evidence_root / relative
        attachment.parent.mkdir(parents=True, exist_ok=True)
        attachment.write_bytes(raw)
        events.append({
            "eventId": str(uuid4()),
            "eventType": event_type,
            "occurredAtUtc": _stamp(occurred),
            "actorRole": actor_roles[event_type],
            "summary": f"Synthetic test record for {event_type.lower()} with verified evidence.",
            "evidence": {
                "evidenceId": str(uuid4()),
                "path": relative,
                "sha256": hashlib.sha256(raw).hexdigest(),
                "sizeBytes": len(raw),
                "mediaType": "text/plain",
            },
        })

    plan = {
        "schema": MODULE.PLAN_SCHEMA,
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
    incident = {
        "schema": MODULE.INCIDENT_SCHEMA,
        "stage": 48,
        "incidentId": str(uuid4()),
        "planId": plan["planId"],
        "planSha256": hashlib.sha256(plan_raw).hexdigest(),
        "candidateSha256": plan["candidate"]["candidateSha256"],
        "rollbackBaselineSha256": plan["rollbackBaseline"]["baselineSha256"],
        "environmentId": "production",
        "executionMode": "MANUAL_OUT_OF_BAND",
        "status": "RESOLVED_FOR_REVIEW",
        "severity": "SEV2",
        "deploymentOutcome": "PARTIALLY_APPLIED",
        "rollbackOutcome": rollback,
        "resolutionAction": (
            "ROLLBACK" if rollback == "SUCCEEDED"
            else "FORWARD_FIX" if rollback == "FAILED"
            else "FORWARD_FIX"
        ),
        "serviceState": "RESTORED",
        "dataIntegrityState": "VERIFIED",
        "releaseDisposition": "HOLD",
        "startedAtUtc": events[0]["occurredAtUtc"],
        "resolvedAtUtc": events[-1]["occurredAtUtc"],
        "events": events,
    }
    incident_raw = _json_bytes(incident)
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
    attestation = {
        "schema": MODULE.ATTESTATION_SCHEMA,
        "incidentId": incident["incidentId"],
        "incidentSha256": hashlib.sha256(incident_raw).hexdigest(),
        "planSha256": hashlib.sha256(plan_raw).hexdigest(),
        "canonicalization": MODULE.CANONICALIZATION,
        "signatures": [],
    }
    for role, private_key in private_keys.items():
        key_id = f"TEST-{role}"
        signed_at = MODULE.utc_now()
        signature = private_key.sign(MODULE.signature_message(
            attestation["incidentSha256"], attestation["planSha256"],
            role, key_id, signed_at,
        ))
        attestation["signatures"].append({
            "role": role,
            "keyId": key_id,
            "signedAtUtc": signed_at,
            "signatureBase64": base64.b64encode(signature).decode("ascii"),
        })
    return {
        "plan": plan, "plan_raw": plan_raw,
        "incident": incident, "incident_raw": incident_raw,
        "evidence_root": evidence_root,
        "attestation": attestation, "attestation_raw": _json_bytes(attestation),
        "keyring": keyring, "keyring_raw": keyring_raw,
        "trusted_sha": hashlib.sha256(keyring_raw).hexdigest(),
        "private_keys": private_keys,
    }


def _verify(context: dict) -> dict:
    return MODULE.verify_incident(
        context["plan"], context["plan_raw"],
        context["incident"], context["incident_raw"],
        context["evidence_root"],
        context["attestation"], context["attestation_raw"],
        context["keyring"], context["keyring_raw"], context["trusted_sha"],
    )


class Stage48IncidentEvidenceTests(unittest.TestCase):
    def test_signed_rollback_incident_verifies_and_keeps_release_on_hold(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            report = _verify(_build_context(Path(folder), "SUCCEEDED"))
        self.assertEqual(report["outcome"], "INCIDENT_EVIDENCE_VERIFIED")
        self.assertEqual(report["verifiedEvidenceCount"], 9)
        self.assertTrue(report["rollbackExecuted"])
        self.assertTrue(report["incidentClosedForAudit"])
        self.assertEqual(report["releaseDisposition"], "HOLD")
        self.assertFalse(report["productionReady"])
        self.assertFalse(report["highAvailabilityApproved"])
        self.assertEqual(report["sloStatus"], "UNBASELINED")
        self.assertFalse(report["promotionAllowed"])
        self.assertFalse(report["sideEffectsPerformedByVerifier"])

    def test_no_rollback_incident_uses_eight_ordered_evidence_records(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder), "NOT_REQUIRED")
            context["incident"]["deploymentOutcome"] = "ABORTED"
            context["incident_raw"] = _json_bytes(context["incident"])
            context["attestation"]["incidentSha256"] = hashlib.sha256(context["incident_raw"]).hexdigest()
            context["attestation_raw"] = _json_bytes(context["attestation"])
            material = MODULE.validate_incident(
                context["plan"], context["plan_raw"], context["incident"],
                context["incident_raw"], context["evidence_root"],
            )
        self.assertEqual(material["verifiedEvidenceCount"], 8)

    def test_missing_rollback_event_blocks_rollback_incident(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder), "SUCCEEDED")
            context["incident"]["events"].pop(5)
            context["incident_raw"] = _json_bytes(context["incident"])
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_EVENT_COUNT_INVALID"):
                MODULE.validate_incident(
                    context["plan"], context["plan_raw"], context["incident"],
                    context["incident_raw"], context["evidence_root"],
                )

    def test_attachment_tampering_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            item = context["incident"]["events"][0]["evidence"]
            (context["evidence_root"] / item["path"]).write_text("tampered", encoding="utf-8")
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_EVIDENCE_(SIZE|HASH)_MISMATCH"):
                MODULE.validate_incident(
                    context["plan"], context["plan_raw"], context["incident"],
                    context["incident_raw"], context["evidence_root"],
                )

    def test_plan_byte_change_breaks_incident_binding(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["plan"]["state"] = "MUTATED"
            context["plan_raw"] = _json_bytes(context["plan"])
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "DEPLOYMENT_PLAN_NOT_OPERATOR_READY"):
                MODULE.validate_incident(
                    context["plan"], context["plan_raw"], context["incident"],
                    context["incident_raw"], context["evidence_root"],
                )

    def test_path_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["incident"]["events"][0]["evidence"]["path"] = "../outside.txt"
            context["incident_raw"] = _json_bytes(context["incident"])
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_EVIDENCE_PATH_TRAVERSAL"):
                MODULE.validate_incident(
                    context["plan"], context["plan_raw"], context["incident"],
                    context["incident_raw"], context["evidence_root"],
                )

    def test_failed_rollback_requires_alternate_recovery(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder), "FAILED")
            context["incident"]["resolutionAction"] = "ROLLBACK"
            context["incident_raw"] = _json_bytes(context["incident"])
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_FAILED_ROLLBACK_REQUIRES_ALTERNATE_RECOVERY"):
                MODULE.validate_incident(
                    context["plan"], context["plan_raw"], context["incident"],
                    context["incident_raw"], context["evidence_root"],
                )

    def test_service_and_data_must_be_restored_before_closure(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["incident"]["serviceState"] = "DEGRADED"
            context["incident_raw"] = _json_bytes(context["incident"])
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_SERVICE_OR_DATA_NOT_RESTORED"):
                MODULE.validate_incident(
                    context["plan"], context["plan_raw"], context["incident"],
                    context["incident_raw"], context["evidence_root"],
                )

    def test_sensitive_json_fields_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            item = context["incident"]["events"][0]["evidence"]
            raw = b'{"status":"ok","access_token":"should-not-be-here"}'
            path = context["evidence_root"] / item["path"]
            path.write_bytes(raw)
            item["sha256"] = hashlib.sha256(raw).hexdigest()
            item["sizeBytes"] = len(raw)
            item["mediaType"] = "application/json"
            context["incident_raw"] = _json_bytes(context["incident"])
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_EVIDENCE_SENSITIVE_FIELD_REJECTED"):
                MODULE.validate_incident(
                    context["plan"], context["plan_raw"], context["incident"],
                    context["incident_raw"], context["evidence_root"],
                )

    def test_revoked_signer_key_blocks_attestation(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["keyring"]["keys"][0]["status"] = "REVOKED"
            context["keyring_raw"] = _json_bytes(context["keyring"])
            context["trusted_sha"] = hashlib.sha256(context["keyring_raw"]).hexdigest()
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_SIGNER_NOT_TRUSTED_FOR_ROLE"):
                _verify(context)

    def test_same_key_cannot_sign_both_roles(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["attestation"]["signatures"][1]["keyId"] = context["attestation"]["signatures"][0]["keyId"]
            context["attestation_raw"] = _json_bytes(context["attestation"])
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_SIGNER_KEY_REUSED"):
                _verify(context)

    def test_tampered_signature_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _build_context(Path(folder))
            context["attestation"]["signatures"][0]["signatureBase64"] = base64.b64encode(b"x" * 64).decode("ascii")
            context["attestation_raw"] = _json_bytes(context["attestation"])
            with self.assertRaisesRegex(MODULE.IncidentEvidenceError, "INCIDENT_SIGNATURE_INVALID"):
                _verify(context)

    def test_empty_templates_remain_blocked(self) -> None:
        root = Path(__file__).resolve().parents[2]
        record = json.loads((root / "Data/Encounter/ProductionDeploymentIncidentRecord.template.json").read_text(encoding="utf-8"))
        attestation = json.loads((root / "Data/Encounter/ProductionDeploymentIncidentAttestation.template.json").read_text(encoding="utf-8"))
        self.assertEqual(record["status"], "BLOCKED_MISSING_EVIDENCE")
        self.assertEqual(record["events"], [])
        self.assertEqual(record["releaseDisposition"], "HOLD")
        self.assertEqual(attestation["signatures"], [])


if __name__ == "__main__":
    unittest.main()