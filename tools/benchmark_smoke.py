#!/usr/bin/env python3
"""Validate the benchmark's machine-readable evidence contract."""

import json
import math
import re
import statistics
import subprocess
import sys


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_number(report: dict, field: str) -> float:
    value = report.get(field)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        fail(f"{field} is not numeric")
    number = float(value)
    if not math.isfinite(number):
        fail(f"{field} is not finite")
    return number


def median_absolute_deviation(samples: list[float]) -> float:
    center = statistics.median(samples)
    return statistics.median(abs(sample - center) for sample in samples)


def validate_report(report: dict, lifecycle: str) -> None:
    expected = {
        "width": 128,
        "height": 128,
        "kernel": "gaussian",
        "threads": 4,
        "activeThreads": 4,
        "workerLifecycle": lifecycle,
        "iterations": 3,
        "exactMatch": True,
        "model": "row-partitioned shared-memory CPU threads",
    }
    for field, value in expected.items():
        if report.get(field) != value:
            fail(f"{field} was {report.get(field)!r}, expected {value!r}")

    for prefix in ("scalar", "parallel"):
        samples = report.get(f"{prefix}SamplesMs")
        if not isinstance(samples, list) or len(samples) != expected["iterations"]:
            fail(f"{prefix}SamplesMs does not match the iteration count")
        numeric_samples = [require_number({"sample": sample}, "sample") for sample in samples]
        if any(sample <= 0 for sample in numeric_samples):
            fail(f"{prefix}SamplesMs contains a non-positive duration")

        reported_median = require_number(report, f"{prefix}MedianMs")
        reported_mad = require_number(report, f"{prefix}MadMs")
        if not math.isclose(reported_median, statistics.median(numeric_samples), abs_tol=1e-6):
            fail(f"{prefix}MedianMs does not match the raw samples")
        if not math.isclose(
            reported_mad, median_absolute_deviation(numeric_samples), abs_tol=1e-6
        ):
            fail(f"{prefix}MadMs does not match the raw samples")

        reported_throughput = require_number(report, f"{prefix}MegapixelsPerSecond")
        expected_throughput = (
            expected["width"] * expected["height"] / (reported_median * 1_000.0)
        )
        if reported_throughput <= 0 or not math.isclose(
            reported_throughput, expected_throughput, rel_tol=2e-5, abs_tol=1e-6
        ):
            fail(f"{prefix}MegapixelsPerSecond does not match the reported median")

    speedup = require_number(report, "speedup")
    if speedup <= 0:
        fail("speedup is not positive")
    throughput_speedup = require_number(report, "parallelMegapixelsPerSecond") / require_number(
        report, "scalarMegapixelsPerSecond"
    )
    if not math.isclose(speedup, throughput_speedup, rel_tol=2e-5, abs_tol=1e-6):
        fail("speedup does not match the throughput ratio")
    checksum = report.get("outputChecksum")
    if not isinstance(checksum, str) or re.fullmatch(r"[0-9a-f]+", checksum) is None:
        fail("outputChecksum is not a lowercase hexadecimal string")


def run(binary: str, reuse_workers: bool) -> None:
    command = [
        binary,
        "benchmark",
        "--width",
        "128",
        "--height",
        "128",
        "--threads",
        "4",
        "--iterations",
        "3",
        "--json",
    ]
    lifecycle = "per-invocation"
    if reuse_workers:
        command.append("--reuse-workers")
        lifecycle = "persistent"
    completed = subprocess.run(command, check=True, capture_output=True, text=True)
    validate_report(json.loads(completed.stdout), lifecycle)
    print(f"PASS benchmark JSON contract ({lifecycle})")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <parallel-pixels-binary>", file=sys.stderr)
        return 2
    try:
        run(sys.argv[1], reuse_workers=False)
        run(sys.argv[1], reuse_workers=True)
    except (json.JSONDecodeError, OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"FAIL benchmark JSON contract: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
