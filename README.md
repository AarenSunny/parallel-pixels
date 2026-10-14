# Parallel Pixels

[![CI](https://github.com/AarenSunny/parallel-pixels/actions/workflows/ci.yml/badge.svg)](https://github.com/AarenSunny/parallel-pixels/actions/workflows/ci.yml)

Parallel Pixels is a dependency-free C++20 image-processing lab built to make
parallel-software decisions measurable. It reads binary grayscale PGM images,
runs exact integer 3×3 Gaussian or box blur through either a scalar reference
or a row-partitioned multithreaded implementation, and verifies byte-for-byte
equivalence before reporting speedup.

This is deliberately a software project. It demonstrates algorithms, memory
layout, validation, concurrency, profiling discipline, and technical honesty
without claiming CUDA or hardware-design experience.

## Build and test

Requirements: a C++20 compiler, POSIX threads, and Make.

```bash
make test
make benchmark-smoke
make crossover-smoke
```

The eight test groups cover both fixed-point kernels, border behavior, constant
images, exact parallel/scalar equality across shapes and thread counts, PGM
round trips, effective worker limits, benchmark statistics, and invalid input
handling. The smoke targets verify both a single benchmark and the crossover
sweep.

## Process an image

```bash
make
./build/parallel-pixels blur input.pgm output.pgm --threads 8 --kernel box
```

Only binary P5 PGM input with an 8-bit maximum value is accepted. Dimensions,
raster length, and output failures are validated rather than silently repaired.

## Measure parallel speedup

```bash
./build/parallel-pixels benchmark --width 2048 --height 2048 --threads 8 --iterations 9 --kernel gaussian --json
```

The benchmark uses deterministic input, alternates scalar/parallel timing order,
reports medians and median absolute deviations, validates exact equality, and
includes a stable checksum. Read the [benchmark method](docs/BENCHMARKING.md)
before quoting a result.

To compare both kernels across workload sizes and thread counts with the same
benchmark method:

```bash
./tools/crossover_sweep.sh > crossover.ndjson
```

The sweep emits one self-contained JSON object per kernel, size, and thread
configuration, which keeps partial results usable if a long experiment is
interrupted.

## Architecture

The image buffer is contiguous and row-major. The parallel path shares it
read-only, balances rows across at most `min(requested_threads, image_height)`
workers, and assigns exclusive output rows so synchronization is unnecessary.
The [architecture review](docs/ARCHITECTURE.md) documents the decision gate,
data flow, and limitations.

## Current boundaries

- CPU shared-memory threads only; no CUDA or GPU claim
- New threads per blur call rather than a reusable pool
- Grayscale PGM only; no production image-codec dependency
- Two fixed 3×3 kernels; no arbitrary kernels, SIMD, or cache tiling yet
- Local benchmark, not a cross-machine performance guarantee

## Roadmap

- Reusable worker pool and tiled scheduling experiment
- Additional exact kernels beyond the current Gaussian and box pair
- Compiler-vectorization report and optional explicit SIMD path
- Optional CUDA implementation only with a reproducible GPU test environment

## License

MIT
