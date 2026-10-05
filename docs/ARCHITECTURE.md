# Architecture and design review

## Data flow

```mermaid
flowchart LR
    A[P5 grayscale image] --> B[Validated GrayImage]
    B --> C{Execution policy}
    C -->|reference| D[Scalar row traversal]
    C -->|parallel| E[Balanced row partitions]
    E --> F[Independent jthread workers]
    D --> G[Exact output comparison]
    F --> G
    G --> H[PGM output or benchmark report]
```

`GrayImage` owns one contiguous row-major byte buffer and validates positive,
non-overflowing dimensions. Both implementations apply the same integer 3×3
Gaussian kernel with clamped borders, so equality is byte-for-byte rather than
an image-quality tolerance.

The parallel path rejects requests above a 256-thread safety cap, limits active
workers to image height, divides rows as evenly as possible, and gives each
worker an exclusive output range. Workers share the read-only input but never
write the same output byte, so no mutex is required. `std::jthread` joins
automatically on exceptional paths, and the normal path explicitly destroys
the worker group before the output can be moved into the return value.

## Design decisions checked before implementation

- **Role signal:** modern C++, memory layout, algorithms, thread partitioning,
  benchmarking, and correctness target NVIDIA software roles without claiming
  electrical or accelerator experience.
- **Smallest credible scope:** one real file format and one exact kernel expose
  I/O validation, boundaries, parallel work decomposition, and measurement.
- **Reference before optimization:** the scalar function is the correctness
  oracle and is timed independently from the multithreaded path.
- **Reproducibility:** the benchmark uses a fixed xorshift input, alternating
  execution order, medians, exact comparison, and an output checksum.

## Boundaries

This is shared-memory CPU parallelism. Thread startup occurs for every call, so
small images can be slower than scalar execution. It does not use SIMD,
thread-pool reuse, tiled caches, color formats, JPEG/PNG codecs, GPUs, or CUDA.
Those are possible experiments only after preserving the scalar equivalence
tests and recording hardware-specific measurements.
