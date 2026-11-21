from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from Scripts.plan_encounter_recovery_capacity import CapacityPlanningError, build_capacity_plan
from Scripts.plan_encounter_recovery_capacity import _load_profile


def synthetic_report(workers: int = 4) -> dict:
    """Synthetic test data only; never a measured performance claim."""
    return {
        "benchmark": "JUMANJI Encounter Checkpoint Recovery",
        "stage": 39,
        "scope": "single-host local HTTP + SQLite WAL + Ed25519 service prototype",
        "measurement_window_seconds": 90.0,
        "workers": workers,
        "completed_write_read_pairs": 900,
        "failed_http_requests": 0,
        "worker_setup_failures": 0,
        "environment": {
            "python": "3.12.test",
            "platform": "synthetic-test-platform",
            "processor": "synthetic-test-cpu",
            "cpu_count": 8,
            "sqlite_version": "3.test",
            "database_journal_mode": "wal",
        },
        "operations": {
            "checkpoint_write": {
                "successes": 900, "failures": 0,
                "p50_ms": 2.0, "p95_ms": 4.0, "p99_ms": 8.0,
                "latency_sample_count": 900,
            },
            "checkpoint_read": {
                "successes": 900, "failures": 0,
                "p50_ms": 1.0, "p95_ms": 3.0, "p99_ms": 6.0,
                "latency_sample_count": 900,
            },
        },
        "throughput": {
            "successful_checkpoint_pairs_per_second": 10.0,
        },
    }


class EncounterRecoveryCapacityPlanningTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.profile = _load_profile(
            ROOT / "Data/Encounter/EncounterRecoverySLOProfiles.csv",
            "LOCAL-BASELINE",
        )

    def test_clean_report_produces_bounded_capacity_estimate(self) -> None:
        report = synthetic_report(workers=4)
        plan = build_capacity_plan(report, self.profile, "a" * 64)
        self.assertLessEqual(
            plan["capacityEnvelope"]["projectedSafeConcurrentEncounters"],
            report["workers"],
        )
        self.assertEqual(plan["sloAssessment"], "NOT_ASSESSED")
        self.assertFalse(plan["approvalGate"]["approved"])
        self.assertEqual(plan["evidenceClass"], "STAGE39_SINGLE_HOST_SQLITE_WAL_LOOPBACK")
        self.assertTrue(plan["planningAssumptions"]["noExtrapolationBeyondTestedWorkers"])

    def test_failed_benchmark_is_not_eligible(self) -> None:
        report = synthetic_report()
        report["failed_http_requests"] = 1
        with self.assertRaises(CapacityPlanningError):
            build_capacity_plan(report, self.profile, "b" * 64)

    def test_capacity_never_exceeds_tested_worker_envelope(self) -> None:
        report = synthetic_report(workers=7)
        report["throughput"]["successful_checkpoint_pairs_per_second"] = 100000.0
        # Keep p95 <= p99; make measured latency non-limiting without fabricating an invalid percentile order.
        plan = build_capacity_plan(report, self.profile, "c" * 64)
        self.assertLessEqual(
            plan["capacityEnvelope"]["projectedSafeConcurrentEncounters"],
            report["workers"],
        )

    def test_unbaselined_profile_never_claims_slo_pass(self) -> None:
        plan = build_capacity_plan(synthetic_report(), self.profile, "d" * 64)
        self.assertEqual(self.profile["Status"], "UNBASELINED")
        self.assertNotEqual(plan["sloAssessment"], "PASS")
        self.assertFalse(plan["sloTargetsAreApproved"])

    def test_low_sample_count_is_rejected(self) -> None:
        report = synthetic_report()
        report["operations"]["checkpoint_read"]["latency_sample_count"] = 2
        with self.assertRaises(CapacityPlanningError):
            build_capacity_plan(report, self.profile, "e" * 64)

    def test_inventory_digest_must_match_the_benchmark_report(self) -> None:
        profile = dict(self.profile)
        inventory = {
            "schemaVersion": 1,
            "hostId": "qa-host-01",
            "hostRole": "single-host-checkpoint-prototype",
            "measuredAtUtc": "2026-11-21T12:00:00Z",
            "osName": "test-os",
            "osVersion": "test-version",
            "cpuModel": "test-cpu",
            "physicalCoreCount": 4,
            "logicalProcessorCount": 8,
            "memoryGiB": 16,
            "storageModel": "test-nvme",
            "storageType": "NVMe",
            "filesystem": "test-fs",
            "pythonVersion": "3.12.test",
            "sqliteVersion": "3.test",
            "benchmarkCommitSha": "abcdef0123456789",
            "benchmarkReportSha256": "1" * 64,
        }
        with self.assertRaises(CapacityPlanningError):
            build_capacity_plan(synthetic_report(), profile, "2" * 64, inventory, "3" * 64)

    def test_capacity_can_be_zero_when_checkpoint_cadence_cannot_be_met(self) -> None:
        report = synthetic_report(workers=4)
        report["throughput"]["successful_checkpoint_pairs_per_second"] = 0.001
        plan = build_capacity_plan(report, self.profile, "0" * 64)
        self.assertEqual(plan["capacityEnvelope"]["projectedSafeConcurrentEncounters"], 0)

    def test_profile_minimums_are_enforced(self) -> None:
        report = synthetic_report(workers=2)
        with self.assertRaises(CapacityPlanningError):
            build_capacity_plan(report, self.profile, "f" * 64)

    def test_approved_slo_is_assessed_but_requires_host_inventory_for_approval(self) -> None:
        profile = dict(self.profile)
        profile.update({
            "Status": "APPROVED",
            "TargetWriteP95Ms": "5.0",
            "TargetWriteP99Ms": "10.0",
            "TargetReadP95Ms": "4.0",
            "TargetReadP99Ms": "8.0",
            "TargetPairsPerSecond": "5.0",
            "EvidenceId": "BENCH-EVIDENCE-001",
            "Approver": "QA-REVIEW",
            "ReviewedAtUtc": "2026-11-21T12:00:00Z",
        })
        inventory = {
            "schemaVersion": 1,
            "hostId": "qa-host-01",
            "hostRole": "single-host-checkpoint-prototype",
            "measuredAtUtc": "2026-11-21T12:00:00Z",
            "osName": "test-os",
            "osVersion": "test-version",
            "cpuModel": "test-cpu",
            "physicalCoreCount": 4,
            "logicalProcessorCount": 8,
            "memoryGiB": 16,
            "storageModel": "test-nvme",
            "storageType": "NVMe",
            "filesystem": "test-fs",
            "pythonVersion": "3.12.test",
            "sqliteVersion": "3.test",
            "benchmarkCommitSha": "abcdef0123456789",
            "benchmarkReportSha256": "2" * 64,
        }
        with_inventory = build_capacity_plan(
            synthetic_report(), profile, "2" * 64, inventory, "3" * 64
        )
        self.assertEqual(with_inventory["sloAssessment"], "PASS")
        self.assertTrue(with_inventory["approvalGate"]["approved"])

        without_inventory = build_capacity_plan(synthetic_report(), profile, "4" * 64)
        self.assertEqual(without_inventory["sloAssessment"], "PASS")
        self.assertFalse(without_inventory["approvalGate"]["approved"])


if __name__ == "__main__":
    unittest.main()
