from __future__ import annotations

import unittest

from Scripts.benchmark_encounter_checkpoint_recovery import Metrics, run_benchmark


class EncounterRecoveryBenchmarkTests(unittest.TestCase):
    def test_percentile_calculation_uses_nearest_rank(self) -> None:
        values = [float(value) for value in range(1, 101)]
        self.assertEqual(Metrics._percentile(values, 0.50), 50.0)
        self.assertEqual(Metrics._percentile(values, 0.95), 95.0)
        self.assertEqual(Metrics._percentile(values, 0.99), 99.0)
        self.assertIsNone(Metrics._percentile([], 0.95))

    def test_bounded_latency_reservoir_tracks_all_requests(self) -> None:
        metrics = Metrics(sample_cap=100)
        for index in range(250):
            metrics.record("checkpoint_read", float(index), True)
        snapshot = metrics.snapshot()
        operation = snapshot["operations"]["checkpoint_read"]
        self.assertEqual(operation["requests"], 250)
        self.assertEqual(operation["successes"], 250)
        self.assertEqual(operation["latency_sample_count"], 100)
        self.assertEqual(operation["latency_sample_cap"], 100)

    def test_real_http_sqlite_checkpoint_smoke(self) -> None:
        report = run_benchmark(
            duration_seconds=0.75,
            workers=2,
            warmup_iterations=1,
            lease_ttl_seconds=30,
            sample_cap=100,
        )
        self.assertEqual(report["stage"], 39)
        self.assertEqual(report["environment"]["database_journal_mode"].lower(), "wal")
        self.assertEqual(report["worker_setup_failures"], 0)
        self.assertEqual(report["failed_http_requests"], 0)
        self.assertGreater(report["completed_write_read_pairs"], 0)
        self.assertGreater(report["throughput"]["successful_checkpoint_pairs_per_second"], 0)
        self.assertIn("p95_ms", report["operations"]["checkpoint_write"])
        self.assertIn("p99_ms", report["operations"]["checkpoint_read"])
        # The report must never carry bearer credentials, lease tokens or private keys.
        self.assertNotIn("leaseToken", str(report))
        self.assertNotIn("server_token", str(report))
        self.assertNotIn("private_key", str(report))

    def test_invalid_benchmark_limits_are_rejected(self) -> None:
        with self.assertRaises(ValueError):
            run_benchmark(duration_seconds=0, workers=1)
        with self.assertRaises(ValueError):
            run_benchmark(duration_seconds=0.5, workers=0)
        with self.assertRaises(ValueError):
            run_benchmark(duration_seconds=0.5, workers=1, lease_ttl_seconds=10)


if __name__ == "__main__":
    unittest.main()
