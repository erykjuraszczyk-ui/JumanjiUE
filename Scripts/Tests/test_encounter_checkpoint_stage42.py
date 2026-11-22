from __future__ import annotations

import base64
import json
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from uuid import uuid4

import rfc8785
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from Scripts.validate_encounter_checkpoint_contract import validate_payload
from Scripts.validate_encounter_checkpoint_staging_report import EXPECTED_CHECKS, validate_report
from Scripts.run_encounter_checkpoint_staging_drill import (
    build_payload,
    load_verification_keys,
    verify_signed_envelope,
)
from Scripts.validate_encounter_checkpoint_stage42 import validate_project


def make_envelope(payload: dict, key_id: str, private_key: Ed25519PrivateKey) -> dict:
    unsigned = {
        "envelopeVersion": 1,
        "payload": payload,
        "integrity": {"algorithm": "Ed25519", "keyId": key_id},
    }
    signature = private_key.sign(rfc8785.dumps(unsigned))
    return {
        **unsigned,
        "integrity": {
            **unsigned["integrity"],
            "signature": base64.urlsafe_b64encode(signature).decode("ascii").rstrip("="),
        },
    }


def valid_report() -> dict:
    now = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    return {
        "schema": "JUMANJI.CheckpointStagingDrillReport.v1",
        "stage": 42,
        "runId": str(uuid4()),
        "startedAtUtc": now,
        "completedAtUtc": now,
        "result": "PASS",
        "environment": {
            "class": "ephemeral-ci",
            "id": "gha-stage42-run-12345",
            "storageProvider": "postgresql",
            "serviceHost": "localhost",
            "transport": "https-mtls",
        },
        "checks": [
            {"id": check_id, "status": "PASS", "durationMs": 0.5, "detailCode": "OK"}
            for check_id in EXPECTED_CHECKS
        ],
        "evidence": {
            "encounterId": "DRILL_STAGE42_20261122T100000_A1B2C3D4",
            "priorSequence": 0,
            "writtenSequence": 1,
            "firstAuthorityEpoch": 1,
            "reacquiredAuthorityEpoch": 2,
            "payloadSha256": "a" * 64,
            "envelopeSha256": "b" * 64,
            "signingKeyId": "STAGE42-CI-KEY",
        },
        "failureCode": None,
    }


class Stage42StagingDrillTests(unittest.TestCase):
    def test_synthetic_payload_satisfies_stage32_contract(self) -> None:
        payload = build_payload(
            "DRILL_STAGE42_20261122T100000_A1B2C3D4",
            "srv-stage42-ci",
            "stage42-ci-shard",
            7,
            1,
            "JUMANJI-STAGE42-CI",
            "ruleset-stage42-v1",
            "world-stage42-v1",
        )
        errors = validate_payload(payload, now=datetime.now(timezone.utc))
        self.assertEqual(errors, [])

    def test_public_keyring_loads_ed25519_key(self) -> None:
        key = Ed25519PrivateKey.generate()
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            public_path = root / "stage42-public.pem"
            public_path.write_bytes(
                key.public_key().public_bytes(
                    encoding=serialization.Encoding.PEM,
                    format=serialization.PublicFormat.SubjectPublicKeyInfo,
                )
            )
            keyring_path = root / "keyring.json"
            keyring_path.write_text(
                json.dumps({"keys": [{"keyId": "STAGE42-TEST-KEY", "publicKeyFile": str(public_path)}]}),
                encoding="utf-8",
            )
            keys = load_verification_keys(keyring_path)
            self.assertIn("STAGE42-TEST-KEY", keys)

    def test_signed_readback_signature_is_verified(self) -> None:
        payload = build_payload(
            "DRILL_STAGE42_20261122T100000_A1B2C3D4",
            "srv-stage42-ci",
            "stage42-ci-shard",
            3,
            1,
            "JUMANJI-STAGE42-CI",
            "ruleset-stage42-v1",
            "world-stage42-v1",
        )
        key = Ed25519PrivateKey.generate()
        envelope = make_envelope(payload, "STAGE42-TEST-KEY", key)
        keyring = {"STAGE42-TEST-KEY": key.public_key()}
        key_id, digest = verify_signed_envelope(
            envelope,
            keyring,
            payload["encounterId"],
            1,
        )
        self.assertEqual(key_id, "STAGE42-TEST-KEY")
        self.assertEqual(len(digest), 64)

    def test_tampered_signed_payload_is_rejected(self) -> None:
        payload = build_payload(
            "DRILL_STAGE42_20261122T100000_A1B2C3D4",
            "srv-stage42-ci",
            "stage42-ci-shard",
            3,
            1,
            "JUMANJI-STAGE42-CI",
            "ruleset-stage42-v1",
            "world-stage42-v1",
        )
        key = Ed25519PrivateKey.generate()
        envelope = make_envelope(payload, "STAGE42-TEST-KEY", key)
        envelope["payload"]["checkpointSequence"] = 2
        with self.assertRaisesRegex(Exception, "SIGNED_ENVELOPE_SEQUENCE_MISMATCH"):
            verify_signed_envelope(
                envelope,
                {"STAGE42-TEST-KEY": key.public_key()},
                payload["encounterId"],
                1,
            )

    def test_unknown_key_id_is_rejected(self) -> None:
        payload = build_payload(
            "DRILL_STAGE42_20261122T100000_A1B2C3D4",
            "srv-stage42-ci",
            "stage42-ci-shard",
            3,
            1,
            "JUMANJI-STAGE42-CI",
            "ruleset-stage42-v1",
            "world-stage42-v1",
        )
        key = Ed25519PrivateKey.generate()
        envelope = make_envelope(payload, "OLD-STAGE42-KEY", key)
        with self.assertRaisesRegex(Exception, "SIGNED_ENVELOPE_KEY_ID_UNKNOWN"):
            verify_signed_envelope(
                envelope,
                {"STAGE42-CURRENT-KEY": Ed25519PrivateKey.generate().public_key()},
                payload["encounterId"],
                1,
            )

    def test_complete_sanitized_report_passes(self) -> None:
        self.assertEqual(validate_report(valid_report()), [])

    def test_report_with_lease_token_is_rejected(self) -> None:
        report = valid_report()
        report["evidence"]["leaseToken"] = "should-never-be-logged"
        errors = validate_report(report)
        self.assertTrue(any("leaseToken" in error for error in errors))

    def test_report_with_missing_required_check_is_rejected(self) -> None:
        report = valid_report()
        report["checks"] = [item for item in report["checks"] if item["id"] != "signed_readback"]
        errors = validate_report(report)
        self.assertTrue(any("signed_readback" in error for error in errors))

    def test_report_cannot_claim_production_environment(self) -> None:
        report = valid_report()
        report["environment"]["class"] = "production"
        errors = validate_report(report)
        self.assertTrue(any("environment class" in error for error in errors))

    def test_stage42_project_contract_passes(self) -> None:
        self.assertEqual(validate_project(ROOT), [])


if __name__ == "__main__":
    unittest.main()
