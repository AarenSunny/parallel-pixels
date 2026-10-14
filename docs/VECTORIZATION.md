# Compiler vectorization review

Run the report with the same compiler family used for the project build:

```bash
make vectorization-report
```

The script compiles `src/blur.cpp` at `-O3` in a temporary directory and asks
Clang or GCC for loop-vectorization diagnostics. It filters out standard-library
messages so the output stays focused on project code. No build artifact or
generated report is retained.

## Verified baseline — 2026-10-14

Apple Clang 21.0.0 on arm64 macOS reported that the horizontal pixel loop in
`blur_rows` was not vectorized. The diagnostics cited early-exit control flow
and a value that could not be identified as a reduction. This is useful negative
evidence: the current speedups come from row-level threading, not a claimed SIMD
path.

The narrow next experiment would separate border handling from the interior
loop and expose contiguous spans to the compiler, then compare correctness,
diagnostics, and measured timings against this baseline. An explicit SIMD
implementation is not justified until that simpler layout experiment produces
repeatable evidence.

## Boundaries

A compiler remark is static optimization evidence, not a runtime profile or a
guarantee about emitted instructions. Results depend on compiler version,
target architecture, flags, and source shape. Re-run the report and benchmark
on the target machine before making any vectorization or speedup claim.
