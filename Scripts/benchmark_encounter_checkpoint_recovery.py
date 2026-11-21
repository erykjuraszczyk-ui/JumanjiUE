#!/usr/bin/env python3
"""Load/soak benchmark for the real Stage 33 Python/SQLite/HTTP checkpoint service.

The harness starts an ephemeral Ed25519-backed service on loopback and exercises
its real HTTP routes. It never uses production credentials or a production DB.
This measures a single-host prototype, not distributed or multi-region capacity.
"""
from __future__ import annotations

import argparse
import copy
import json
import platform
import random
import sqlite3
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey  # noqa: E402

from Backend.EncounterCheckpointService.service import (  # noqa: E402
    EncounterCheckpointBackend,
    make_handler,
)

FIXTURE_PATH = ROOT / "Data/Encounter/EncounterCheckpointExample.json"
DEFAULT_SAMPLE_CAP = 50_000
MAX_DURATION_SECONDS = 3_600
MAX_WORKERS = 64


class BenchmarkRequestError(RuntimeError):
    def __init__(self, operation: str, status: int, code: str):
        super().__init__(f"{operation} failed with HTTP {status}: {code}")
        self.operation = operation
        self.status = status
        self.code = code


class Metrics:
    """Thread-safe counters plus bounded reservoir samples for long soak runs."""

    def __init__(self, sample_cap: int = DEFAULT_SAMPLE_CAP) -> None:
        self.sample_cap = sample_cap
        self._lock = threading.Lock()
        self._operations: dict[str, dict[str, Any]] = {}
        self._errors: dict[str, int] = {}
        self._completed_pairs = 0
        self._worker_setup_failures = 0

    def record(self, operation: str, latency_ms: float, success: bool, code: str = "") -> None:
        with self._lock:
            entry = self._operations.setdefault(
                operation,
                {"count": 0, "success": 0, "failures": 0, "total_ms": 0.0,
                 "min_ms": None, "max_ms": None, "samples_ms": []},
            )
            entry["count"] += 1
            entry["success" if success else "failures"] += 1
            entry["total_ms"] += latency_ms
            entry["min_ms"] = latency_ms if entry["min_ms"] is None else min(entry["min_ms"], latency_ms)
            entry["max_ms"] = latency_ms if entry["max_ms"] is None else max(entry["max_ms"], latency_ms)
            samples = entry["samples_ms"]
            if len(samples) < self.sample_cap:
                samples.append(latency_ms)
            else:
                # Reservoir sampling avoids unbounded RAM growth during multi-hour runs.
                selected = random.randrange(entry["count"])
                if selected < self.sample_cap:
                    samples[selected] = latency_ms
            if not success:
                key = f"{operation}:{code or 'UNKNOWN'}"
                self._errors[key] = self._errors.get(key, 0) + 1

    def note_pair(self) -> None:
        with self._lock:
            self._completed_pairs += 1

    def note_worker_setup_failure(self) -> None:
        with self._lock:
            self._worker_setup_failures += 1

    @staticmethod
    def _percentile(samples: list[float], quantile: float) -> float | None:
        if not samples:
            return None
        ordered = sorted(samples)
        index = max(0, min(len(ordered) - 1, int((len(ordered) * quantile + 0.999999) - 1)))
        return ordered[index]

    def snapshot(self) -> dict[str, Any]:
        with self._lock:
            operation_snapshot: dict[str, Any] = {}
            total_requests = 0
            total_successes = 0
            total_failures = 0
            for name, entry in self._operations.items():
                samples = list(entry["samples_ms"])
                count = int(entry["count"])
                total_requests += count
                total_successes += int(entry["success"])
                total_failures += int(entry["failures"])
                operation_snapshot[name] = {
                    "requests": count,
                    "successes": int(entry["success"]),
                    "failures": int(entry["failures"]),
                    "mean_ms": round(entry["total_ms"] / count, 3) if count else None,
                    "min_ms": round(entry["min_ms"], 3) if entry["min_ms"] is not None else None,
                    "max_ms": round(entry["max_ms"], 3) if entry["max_ms"] is not None else None,
                    "p50_ms": round(self._percentile(samples, 0.50), 3) if samples else None,
                    "p95_ms": round(self._percentile(samples, 0.95), 3) if samples else None,
                    "p99_ms": round(self._percentile(samples, 0.99), 3) if samples else None,
                    "latency_sample_count": len(samples),
                    "latency_sample_cap": self.sample_cap,
                }
            return {
                "operations": operation_snapshot,
                "total_http_requests": total_requests,
                "successful_http_requests": total_successes,
                "failed_http_requests": total_failures,
                "completed_write_read_pairs": self._completed_pairs,
                "worker_setup_failures": self._worker_setup_failures,
                "errors_by_operation_and_code": dict(self._errors),
            }


def _utc_text(value: datetime) -> str:
    return value.astimezone(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def _request_json(
    base_url: str,
    server_token: str,
    method: str,
    path: str,
    body: dict[str, Any] | None,
    operation: str,
    metrics: Metrics,
    record: bool = True,
    timeout_seconds: float = 10.0,
) -> dict[str, Any]:
    data = json.dumps(body, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode("utf-8") if body is not None else None
    headers = {"Authorization": f"Bearer {server_token}", "Accept": "application/json"}
    if data is not None:
        headers["Content-Type"] = "application/json"
    request = urllib.request.Request(base_url + path, data=data, headers=headers, method=method)
    started = time.perf_counter()
    status = 0
    result: dict[str, Any] = {}
    error_code = ""
    try:
        with urllib.request.urlopen(request, timeout=timeout_seconds) as response:
            status = response.status
            raw = response.read()
        parsed = json.loads(raw.decode("utf-8")) if raw else {}
        if not isinstance(parsed, dict):
            raise ValueError("service returned a non-object JSON response")
        result = parsed
        if status < 200 or status >= 300:
            error_code = str(parsed.get("error", "HTTP_ERROR"))
    except urllib.error.HTTPError as error:
        status = error.code
        try:
            parsed = json.loads(error.read().decode("utf-8"))
            error_code = str(parsed.get("error", "HTTP_ERROR")) if isinstance(parsed, dict) else "HTTP_ERROR"
        except (UnicodeDecodeError, json.JSONDecodeError):
            error_code = "HTTP_ERROR"
    except (urllib.error.URLError, TimeoutError, OSError):
        status = 0
        error_code = "TRANSPORT_ERROR"
    except (UnicodeDecodeError, json.JSONDecodeError, ValueError):
        status = 0
        error_code = "INVALID_RESPONSE"
    latency_ms = (time.perf_counter() - started) * 1000.0
    success = 200 <= status < 300
    if record:
        metrics.record(operation, latency_ms, success, error_code)
    if not success:
        raise BenchmarkRequestError(operation, status, error_code or "HTTP_ERROR")
    return result


def _make_payload(template: dict[str, Any], encounter_id: str, session_id: str, server_id: str, shard_id: str, epoch: int, sequence: int) -> dict[str, Any]:
    payload = copy.deepcopy(template)
    now = datetime.now(timezone.utc).replace(microsecond=0)
    payload["checkpointId"] = str(uuid4())
    payload["sessionId"] = session_id
    payload["encounterId"] = encounter_id
    payload["checkpointSequence"] = sequence
    payload["createdAtUtc"] = _utc_text(now)
    payload["expiresAtUtc"] = _utc_text(now + timedelta(minutes=30))
    payload["authority"]["serverInstanceId"] = server_id
    payload["authority"]["shardId"] = shard_id
    payload["authority"]["authorityEpoch"] = epoch
    payload["safePoint"]["encounterRevision"] = sequence
    payload["safePoint"]["chapter"]["chapterRevision"] = max(1, sequence)
    return payload


def _cycle(
    base_url: str,
    server_token: str,
    identity: dict[str, str],
    encounter_id: str,
    session_id: str,
    lease: dict[str, Any],
    template: dict[str, Any],
    sequence: int,
    metrics: Metrics,
    record: bool,
) -> int:
    payload = _make_payload(
        template,
        encounter_id,
        session_id,
        identity["serverInstanceId"],
        identity["shardId"],
        int(lease["authorityEpoch"]),
        sequence + 1,
    )
    auth = {
        **identity,
        "leaseToken": lease["leaseToken"],
        "authorityEpoch": lease["authorityEpoch"],
    }
    path = f"/v1/encounters/{encounter_id}"
    written = _request_json(
        base_url, server_token, "PUT", path + "/checkpoint",
        {**auth, "expectedSequence": sequence, "payload": payload},
        "checkpoint_write", metrics, record=record,
    )
    if written.get("checkpointSequence") != sequence + 1:
        raise BenchmarkRequestError("checkpoint_write", 0, "WRITE_SEQUENCE_MISMATCH")

    read = _request_json(
        base_url, server_token, "POST", path + "/checkpoint/read",
        auth, "checkpoint_read", metrics, record=record,
    )
    envelope = read.get("envelope", {})
    read_payload = envelope.get("payload", {}) if isinstance(envelope, dict) else {}
    if (
        read.get("checkpointSequence") != sequence + 1
        or read_payload.get("checkpointId") != payload["checkpointId"]
        or envelope.get("integrity", {}).get("algorithm") != "Ed25519"
    ):
        raise BenchmarkRequestError("checkpoint_read", 0, "READBACK_MISMATCH")
    if record:
        metrics.note_pair()
    return sequence + 1


def _worker(
    index: int,
    base_url: str,
    server_token: str,
    template: dict[str, Any],
    lease_ttl_seconds: int,
    warmup_iterations: int,
    barrier: threading.Barrier,
    start_event: threading.Event,
    start_holder: list[float],
    metrics: Metrics,
) -> None:
    encounter_id = f"ENCOUNTER_BENCHMARK_{index:04d}"
    identity = {
        "serverInstanceId": f"srv-bench-{index:04d}",
        "shardId": "shard-benchmark-local",
    }
    path = f"/v1/encounters/{encounter_id}"
    lease: dict[str, Any] | None = None
    session_id = str(uuid4())
    sequence = 0
    setup_ok = True

    try:
        lease = _request_json(
            base_url, server_token, "POST", path + "/leases",
            {**identity, "ttlSeconds": lease_ttl_seconds},
            "lease_acquire", metrics, record=True,
        )
        # Warm up JSON parsing, SQLite and signing before the measured interval.
        for _ in range(warmup_iterations):
            sequence = _cycle(
                base_url, server_token, identity, encounter_id, session_id, lease,
                template, sequence, metrics, record=False,
            )
    except Exception as error:
        setup_ok = False
        metrics.note_worker_setup_failure()
        if isinstance(error, BenchmarkRequestError):
            metrics.record("worker_setup", 0.0, False, error.code)
        else:
            metrics.record("worker_setup", 0.0, False, "SETUP_ERROR")
    finally:
        # All workers start the measured window together, including failed setups.
        try:
            barrier.wait(timeout=60.0)
        except threading.BrokenBarrierError:
            setup_ok = False
        start_event.wait(timeout=60.0)

    if not setup_ok or lease is None or not start_holder:
        if lease is not None:
            try:
                _request_json(
                    base_url, server_token, "POST", path + "/leases/release",
                    {**identity, "leaseToken": lease["leaseToken"], "authorityEpoch": lease["authorityEpoch"]},
                    "lease_release", metrics, record=True,
                )
            except Exception:
                pass
        return

    deadline = start_holder[0]
    last_renewal = time.monotonic()
    renew_after = max(5.0, lease_ttl_seconds * 0.40)
    try:
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now - last_renewal >= renew_after:
                _request_json(
                    base_url, server_token, "POST", path + "/leases/renew",
                    {
                        **identity,
                        "leaseToken": lease["leaseToken"],
                        "authorityEpoch": lease["authorityEpoch"],
                        "ttlSeconds": lease_ttl_seconds,
                    },
                    "lease_renew", metrics, record=True,
                )
                last_renewal = now
            sequence = _cycle(
                base_url, server_token, identity, encounter_id, session_id, lease,
                template, sequence, metrics, record=True,
            )
    except BenchmarkRequestError:
        # Record the failure and stop this worker; do not retry an ambiguous write.
        pass
    finally:
        try:
            _request_json(
                base_url, server_token, "POST", path + "/leases/release",
                {**identity, "leaseToken": lease["leaseToken"], "authorityEpoch": lease["authorityEpoch"]},
                "lease_release", metrics, record=True,
            )
        except Exception:
            pass


def run_benchmark(
    duration_seconds: float = 20.0,
    workers: int = 4,
    warmup_iterations: int = 2,
    lease_ttl_seconds: int = 120,
    sample_cap: int = DEFAULT_SAMPLE_CAP,
) -> dict[str, Any]:
    if not 0.25 <= duration_seconds <= MAX_DURATION_SECONDS:
        raise ValueError(f"duration_seconds must be between 0.25 and {MAX_DURATION_SECONDS}")
    if not 1 <= workers <= MAX_WORKERS:
        raise ValueError(f"workers must be between 1 and {MAX_WORKERS}")
    if not 0 <= warmup_iterations <= 10:
        raise ValueError("warmup_iterations must be between 0 and 10")
    if not 15 <= lease_ttl_seconds <= 120:
        raise ValueError("lease_ttl_seconds must be between 15 and 120")
    if not 100 <= sample_cap <= 250_000:
        raise ValueError("sample_cap must be between 100 and 250000")

    fixture_doc = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
    template = fixture_doc["payload"]
    metrics = Metrics(sample_cap=sample_cap)
    server_token = "benchmark-ephemeral-token-" + str(uuid4())
    signing_key = Ed25519PrivateKey.generate()

    with tempfile.TemporaryDirectory(prefix="jumanji-checkpoint-bench-") as temp_dir:
        database_path = Path(temp_dir) / "benchmark.sqlite3"
        backend = EncounterCheckpointBackend(database_path, signing_key, "BENCHMARK-EPHEMERAL-KEY")
        handler = make_handler(backend, server_token)
        handler.log_message = lambda self, format_string, *args: None  # type: ignore[method-assign]
        server = __import__("http.server", fromlist=["ThreadingHTTPServer"]).ThreadingHTTPServer(
            ("127.0.0.1", 0), handler
        )
        server.daemon_threads = True
        server_thread = threading.Thread(target=server.serve_forever, name="checkpoint-benchmark-http", daemon=True)
        server_thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"
        barrier = threading.Barrier(workers + 1)
        start_event = threading.Event()
        start_holder: list[float] = []

        try:
            _request_json(base_url, server_token, "GET", "/healthz", None, "health", metrics, record=False)
            with ThreadPoolExecutor(max_workers=workers) as pool:
                futures = [
                    pool.submit(
                        _worker, index, base_url, server_token, template,
                        lease_ttl_seconds, warmup_iterations,
                        barrier, start_event, start_holder, metrics,
                    )
                    for index in range(workers)
                ]
                try:
                    barrier.wait(timeout=60.0)
                except threading.BrokenBarrierError as error:
                    start_event.set()
                    raise RuntimeError("workers did not reach the benchmark start barrier") from error
                started_monotonic = time.monotonic()
                start_holder.append(started_monotonic + duration_seconds)
                started_utc = datetime.now(timezone.utc).isoformat()
                start_event.set()
                for future in futures:
                    future.result()
                wall_seconds = time.monotonic() - started_monotonic
        finally:
            server.shutdown()
            server.server_close()
            server_thread.join(timeout=5.0)

        try:
            connection = sqlite3.connect(database_path)
            journal_mode = connection.execute("PRAGMA journal_mode").fetchone()[0]
            connection.close()
        except sqlite3.Error:
            journal_mode = "unknown"
        database_bytes = database_path.stat().st_size if database_path.exists() else 0

    snapshot = metrics.snapshot()
    measured_requests = (
        snapshot["operations"].get("checkpoint_write", {}).get("successes", 0)
        + snapshot["operations"].get("checkpoint_read", {}).get("successes", 0)
    )
    report = {
        "benchmark": "JUMANJI Encounter Checkpoint Recovery",
        "stage": 39,
        "scope": "single-host local HTTP + SQLite WAL + Ed25519 service prototype",
        "started_at_utc": started_utc,
        "measurement_window_seconds": duration_seconds,
        "wall_seconds_including_worker_cleanup": round(wall_seconds, 3),
        "workers": workers,
        "warmup_iterations_per_worker": warmup_iterations,
        "lease_ttl_seconds": lease_ttl_seconds,
        "sample_cap_per_operation": sample_cap,
        "environment": {
            "python": platform.python_version(),
            "platform": platform.platform(),
            "processor": platform.processor() or "unknown",
            "cpu_count": __import__("os").cpu_count(),
            "sqlite_version": sqlite3.sqlite_version,
            "database_journal_mode": journal_mode,
            "database_file_bytes": database_bytes,
            "transport": "HTTP loopback 127.0.0.1; ephemeral server token and ephemeral Ed25519 key",
        },
        **snapshot,
        "throughput": {
            "successful_checkpoint_pairs_per_second": round(
                snapshot["completed_write_read_pairs"] / duration_seconds, 3
            ),
            "successful_checkpoint_http_operations_per_second": round(
                measured_requests / duration_seconds, 3
            ),
        },
        "interpretation": [
            "This report measures the Python service and SQLite database on one host over loopback HTTP.",
            "It is not a multi-host, multi-region, Unreal Dedicated Server, Android or production capacity result.",
            "Latency percentiles are calculated from a bounded reservoir sample during long runs.",
            "Do not compare absolute latency between different hardware, runner load, Python versions or storage devices.",
            "A benchmark with any failed operation is not a clean performance sample.",
        ],
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration-seconds", type=float, default=20.0)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--warmup-iterations", type=int, default=2)
    parser.add_argument("--lease-ttl-seconds", type=int, default=120)
    parser.add_argument("--sample-cap", type=int, default=DEFAULT_SAMPLE_CAP)
    parser.add_argument("--report", type=Path, help="Optional path for a JSON report.")
    args = parser.parse_args()

    try:
        report = run_benchmark(
            duration_seconds=args.duration_seconds,
            workers=args.workers,
            warmup_iterations=args.warmup_iterations,
            lease_ttl_seconds=args.lease_ttl_seconds,
            sample_cap=args.sample_cap,
        )
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print(f"Encounter recovery benchmark: FAIL ({error})", file=sys.stderr)
        return 1

    encoded = json.dumps(report, ensure_ascii=False, indent=2)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(encoded + "\n", encoding="utf-8")
    print(encoded)
    failures = report["failed_http_requests"] + report["worker_setup_failures"]
    if failures:
        print(f"Encounter recovery benchmark completed with {failures} failure(s).", file=sys.stderr)
        return 2
    print(
        "Encounter recovery benchmark: PASS "
        f"({report['completed_write_read_pairs']} write/read pairs, "
        f"{report['throughput']['successful_checkpoint_pairs_per_second']} pairs/s).",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
