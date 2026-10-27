# Benchmark method

Run a release-optimized comparison with explicit dimensions, thread count, and
iteration count:

```bash
make
./build/parallel-pixels benchmark --width 2048 --height 2048 --threads 8 --iterations 9 --kernel gaussian --json
```

The tool creates the same deterministic grayscale input on every run, computes
an untimed scalar and parallel result first, and refuses to benchmark if they
differ. Timed scalar and parallel calls alternate order. The report uses the
median rather than the fastest sample, reports median absolute deviation (MAD)
for timing variability, and includes an exact-match flag and output checksum.
Machine-readable reports also retain both raw timing arrays at six decimal
places so the summary statistics can be recomputed. A workload that produces a
non-positive clock sample is rejected instead of emitting a meaningless zero
duration or speedup; increase the image dimensions and repeat the run.

MAD is the median distance from the sample median. It is resistant to a single
timing outlier and makes unstable runs visible, but it is not a confidence
interval and does not make results portable across machines.

Capture a machine-readable environment sidecar before a benchmark run:

```bash
mkdir -p benchmark-artifacts
make benchmark-environment > benchmark-artifacts/environment.json
```

The report records a best-effort CPU model (falling back to the processor or
architecture name), operating system, architecture, logical CPU count,
compiler, build flags, Git revision, and whether the working tree is clean.
Benchmark JSON separately records the dimensions, kernel, requested thread
count, and iteration count. Review the sidecar before sharing because build
flags can contain local paths. A speedup from one machine is not a general
throughput promise. The implementation creates threads per invocation, so it
intentionally exposes startup overhead and may lose on small inputs.
The output directory is ignored by Git, preventing the redirected sidecar from
changing the `workingTreeClean` value it is trying to record.

The JSON `threads` field records the requested count, while `activeThreads`
records the number actually launched. Active threads are capped at the image
height so that every worker receives at least one row. This is an implementation
count, not a measurement of simultaneous CPU utilization or operating-system
scheduling.

Pass `--reuse-workers` to construct a fixed worker pool before the untimed
reference run and reuse it for every timed parallel iteration. Pool construction
is intentionally outside the timed region; output allocation, dispatch,
synchronization, and blur work remain inside it. The `workerLifecycle` field
distinguishes `persistent` results from the default `per-invocation` model.
This comparison measures repeated single-call dispatch only—the pool has no
queue, overlapping submissions, work stealing, or production scheduler claim.

### Verified worker-reuse comparison — 2026-10-14

On the Apple M4 environment documented below, a nine-iteration 128×128 Gaussian
run with four requested workers measured a 0.074 ms parallel median when threads
were created per invocation and 0.038 ms with persistent workers. The reported
speedup relative to each run's scalar median changed from 1.080× to 2.126×. Both
paths matched exactly with checksum `ba5b426411b4759f`.

This is a local sequential comparison showing that thread startup materially
affects this small workload. It is not a throughput, concurrent-request, or
cross-machine claim; repeat the experiment before quoting the values elsewhere.

## Workload crossover sweep

Run the bounded default matrix and capture newline-delimited JSON:

```bash
./tools/crossover_sweep.sh > benchmark-artifacts/crossover.ndjson
```

The sweep delegates every measurement to the same benchmark command rather
than implementing a second timer. Its defaults cover per-invocation and
persistent workers, both kernels, square images of 128, 512, and 2048 pixels,
and one, two, four, and eight requested threads. Override the matrix without
editing the script when a machine has different limits:

```bash
WORKER_LIFECYCLES="persistent" KERNELS="box" SIZES="64 256 1024" THREADS="1 4" ITERATIONS=9 ./tools/crossover_sweep.sh
```

Each line remains valid JSON and includes the worker lifecycle, kernel,
dimensions, requested and active threads, exact-match evidence, and a checksum.
The raw sample arrays make each median and MAD auditable. Runs are sequential,
so the output supports crossover analysis but does not isolate thermal
throttling, background load, or CPU-frequency changes. Record the environment
and repeat the full sweep before making a performance claim.

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
