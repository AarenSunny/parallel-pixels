# Benchmark method

Run a release-optimized comparison with explicit dimensions, thread count, and
iteration count:

```bash
make
./build/parallel-pixels benchmark --width 2048 --height 2048 --threads 8 --iterations 9 --json
```

The tool creates the same deterministic grayscale input on every run, computes
an untimed scalar and parallel result first, and refuses to benchmark if they
differ. Timed scalar and parallel calls alternate order. The report uses the
median rather than the fastest sample and includes an exact-match flag and
output checksum.

Always record CPU model, operating system, compiler, optimization flags, image
dimensions, requested thread count, and iteration count beside a result. A
speedup from one machine is not a general throughput promise. The implementation
creates threads per invocation, so it intentionally exposes startup overhead
and may lose on small inputs.

The JSON `threads` field records the requested count, while `activeThreads`
records the number actually launched. Active threads are capped at the image
height so that every worker receives at least one row. This is an implementation
count, not a measurement of simultaneous CPU utilization or operating-system
scheduling.

## Verified development run — 2026-10-04

Environment: Apple M4 (10 physical/logical cores), arm64, macOS 27.0, Apple
Clang 21.0.0, `-O3`, 2048×2048 deterministic input, nine iterations per
configuration. Each configuration ran in isolation.

| Requested threads | Scalar median | Parallel median | Speedup |
|---:|---:|---:|---:|
| 1 | 15.018 ms | 15.065 ms | 0.997× |
| 2 | 15.052 ms | 7.612 ms | 1.977× |
| 4 | 15.108 ms | 3.887 ms | 3.887× |
| 8 | 15.105 ms | 2.625 ms | 5.755× |

Every output matched the scalar reference exactly with checksum
`9d2346a9be271ae0`. A 128×128 smoke input measured 0.624× at four threads,
demonstrating that startup overhead makes parallel execution slower for small
workloads. These figures are local diagnostic evidence, not portable promises.
