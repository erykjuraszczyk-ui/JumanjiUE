from __future__ import annotations

import hashlib
import json
import os
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))

import validate_encounter_checkpoint_stage49_reentry as MODULE


def _stamp(value: datetime) -> str:
    return value.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def _raw(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def _attachment(root: Path, name: str, content: bytes) -> dict:
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return {
        "path": name,
        "sha256": hashlib.sha256(content).hexdigest(),
        "sizeBytes": len(content),
        "mediaType": "text/plain",
    }


def _context(root: Path) -> dict:
    now = datetime.now(timezone.utc).replace(microsecond=0)
    gate_created = now - timedelta(minutes=2)
    incident_id = str(uuid4())
    incident = {
        "schema": MODULE.INCIDENT_SCHEMA,
        "stage": 48,
        "incidentId": incident_id,
        "status": "RESOLVED_FOR_REVIEW",
        "serviceState": "RESTORED",
        "dataIntegrityState": "VERIFIED",
        "releaseDisposition": "HOLD",
    }
    incident_raw = _raw(incident)
    attestation = {
        "schema": MODULE.ATTESTATION_SCHEMA,
        "incidentId": incident_id,
        "incidentSha256": hashlib.sha256(incident_raw).hexdigest(),
        "canonicalization": "RFC8785",
        "signatures": [
            {"role": "SERVICE_OPERATOR", "keyId": "service-key-01"},
            {"role": "INDEPENDENT_REVIEWER", "keyId": "review-key-01"},
        ],
    }
    attestation_raw = _raw(attestation)
    report = {
        "schema": MODULE.REPORT_SCHEMA,
        "outcome": "INCIDENT_EVIDENCE_VERIFIED",
        "incidentId": incident_id,
        "incidentSha256": hashlib.sha256(incident_raw).hexdigest(),
        "incidentClosedForAudit": True,
        "releaseDisposition": "HOLD",
        "promotionAllowed": False,
        "productionReady": False,
        "highAvailabilityApproved": False,
        "sloStatus": "UNBASELINED",
    }
    report_raw = _raw(report)

    evidence_root = root / "evidence"
    evidence_root.mkdir()
    checks = []
    for check_id in MODULE.REQUIRED_CHECKS:
        evidence = _attachment(
            evidence_root, f"checks/{check_id}.txt",
            f"Synthetic operator evidence for {check_id}.\n".encode(),
        )
        checks.append({
            "id": check_id,
            "status": "PASS",
            "observedAtUtc": _stamp(now - timedelta(seconds=50)),
            "summary": f"Synthetic verified observation for {check_id}.",
            "evidence": evidence,
        })
    approvals = []
    for role, reviewer in (
        ("SERVICE_OWNER", "service-owner-ref-01"),
        ("INDEPENDENT_OPERATIONS_REVIEWER", "independent-reviewer-01"),
    ):
        evidence = _attachment(
            evidence_root, f"approvals/{role.lower()}.txt",
            f"Synthetic approval record for {role}.\n".encode(),
        )
        approvals.append({
            "role": role,
            "reviewerRef": reviewer,
            "decision": "APPROVED",
            "approvedAtUtc": _stamp(now - timedelta(seconds=20)),
            "evidence": evidence,
        })
    gate = {
        "schema": MODULE.SCHEMA,
        "stage": 49,
        "gateId": str(uuid4()),
        "createdAtUtc": _stamp(gate_created),
        "environment": {"class": "production", "id": "prod-eu-01"},
        "status": "SUBMITTED_FOR_HUMAN_REVIEW",
        "incidentBinding": {
            "incidentId": incident_id,
            "incidentRecordSha256": hashlib.sha256(incident_raw).hexdigest(),
            "incidentAttestationSha256": hashlib.sha256(attestation_raw).hexdigest(),
            "incidentReportSha256": hashlib.sha256(report_raw).hexdigest(),
        },
        "checks": checks,
        "approvals": approvals,
        "releaseControls": {
            "releaseDisposition": "HOLD",
            "serviceReentryState": "HUMAN_REVIEW_REQUIRED",
            "trafficMutationAllowed": False,
            "promotionAllowed": False,
        },
    }
    return {
        "gate": gate,
        "report": report,
        "report_raw": report_raw,
        "incident": incident,
        "incident_raw": incident_raw,
        "attestation": attestation,
        "attestation_raw": attestation_raw,
        "evidence_root": evidence_root,
    }


def _validate(context: dict) -> dict:
    return MODULE.validate_gate(
        context["gate"], context["report"], context["report_raw"],
        context["incident"], context["incident_raw"],
        context["attestation"], context["attestation_raw"],
        context["evidence_root"],
    )


class Stage49ServiceReentryTests(unittest.TestCase):
    def test_complete_packet_is_only_ready_for_human_review(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            result = _validate(_context(Path(folder)))
        self.assertEqual(result["outcome"], "REENTRY_EVIDENCE_VERIFIED_FOR_HUMAN_REVIEW")
        self.assertEqual(result["verifiedCheckCount"], 11)
        self.assertEqual(result["verifiedApprovalCount"], 2)
        self.assertEqual(result["serviceReentryDecision"], "HUMAN_REVIEW_REQUIRED")
        self.assertFalse(result["trafficMutationAllowed"])
        self.assertFalse(result["promotionAllowed"])
        self.assertFalse(result["productionReady"])
        self.assertFalse(result["highAvailabilityApproved"])
        self.assertEqual(result["sloStatus"], "UNBASELINED")

    def test_missing_check_blocks_gate(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            context["gate"]["checks"].pop()
            with self.assertRaisesRegex(MODULE.GateError, "REENTRY_CHECK_COUNT_INVALID"):
                _validate(context)

    def test_failed_check_blocks_gate(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            context["gate"]["checks"][0]["status"] = "FAIL"
            with self.assertRaisesRegex(MODULE.GateError, "REENTRY_CHECK_NOT_PASS"):
                _validate(context)

    def test_attachment_tampering_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            evidence = context["gate"]["checks"][0]["evidence"]
            (context["evidence_root"] / evidence["path"]).write_text("tampered", encoding="utf-8")
            with self.assertRaisesRegex(MODULE.GateError, "EVIDENCE_(SIZE|HASH)_MISMATCH"):
                _validate(context)

    def test_path_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            context["gate"]["checks"][0]["evidence"]["path"] = "../outside.txt"
            with self.assertRaisesRegex(MODULE.GateError, "EVIDENCE_PATH_INVALID|EVIDENCE_PATH_TRAVERSAL"):
                _validate(context)

    def test_incident_record_binding_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            context["gate"]["incidentBinding"]["incidentRecordSha256"] = "0" * 64
            with self.assertRaisesRegex(MODULE.GateError, "INCIDENT_RECORD_HASH_MISMATCH"):
                _validate(context)

    def test_unverified_stage48_report_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            context["report"]["outcome"] = "INCIDENT_EVIDENCE_PENDING"
            context["report_raw"] = _raw(context["report"])
            context["gate"]["incidentBinding"]["incidentReportSha256"] = hashlib.sha256(context["report_raw"]).hexdigest()
            with self.assertRaisesRegex(MODULE.GateError, "STAGE48_REPORT_NOT_VERIFIED"):
                _validate(context)

    def test_stage48_release_hold_must_be_preserved(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            context["report"]["releaseDisposition"] = "APPROVED"
            context["report_raw"] = _raw(context["report"])
            context["gate"]["incidentBinding"]["incidentReportSha256"] = hashlib.sha256(context["report_raw"]).hexdigest()
            with self.assertRaisesRegex(MODULE.GateError, "STAGE48_RELEASE_BOUNDARY_INVALID"):
                _validate(context)

    def test_two_distinct_reviewer_references_are_required(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            context["gate"]["approvals"][1]["reviewerRef"] = context["gate"]["approvals"][0]["reviewerRef"]
            with self.assertRaisesRegex(MODULE.GateError, "REENTRY_REVIEWER_NOT_INDEPENDENT"):
                _validate(context)

    def test_template_remains_blocked_and_no_traffic_mutation(self) -> None:
        template_path = ROOT / "Data/Encounter/ProductionServiceReentryGate.template.json"
        template = json.loads(template_path.read_text(encoding="utf-8"))
        self.assertEqual(template["status"], "BLOCKED_MISSING_EVIDENCE")
        self.assertEqual(template["checks"], [])
        self.assertEqual(template["approvals"], [])
        self.assertFalse(template["releaseControls"]["trafficMutationAllowed"])
        self.assertFalse(template["releaseControls"]["promotionAllowed"])

    def test_symlink_evidence_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            context = _context(Path(folder))
            evidence = context["gate"]["checks"][0]["evidence"]
            source = context["evidence_root"] / evidence["path"]
            link = context["evidence_root"] / "checks" / "linked-evidence.txt"
            try:
                link.symlink_to(source)
            except (OSError, NotImplementedError):
                self.skipTest("symlinks are unavailable")
            evidence["path"] = "checks/linked-evidence.txt"
            with self.assertRaisesRegex(MODULE.GateError, "EVIDENCE_SYMLINK_REJECTED"):
                _validate(context)


if __name__ == "__main__":
    unittest.main()
