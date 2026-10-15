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

## Rejected interior-loop experiment — 2026-10-14

A trial separated clamped border pixels from a contiguous interior loop using
raw row-major spans. Apple Clang then reported that the interior loop was
vectorized at width 16, and all nine correctness groups remained exact.

An A/B run compiled the committed and experimental `blur.cpp` with the same
Apple Clang 21.0.0 flags and linked both against the same remaining objects. On
a 2048×2048 Gaussian workload with nine iterations, the committed scalar path
measured 20.185 ms versus 20.188 ms for the experiment. At eight workers, the
parallel medians were 3.722 ms and 3.735 ms respectively. Both produced checksum
`9d2346a9be271ae0`.

Those sub-percent differences do not establish an improvement, so the more
complex layout was not retained. A future explicit SIMD path needs a different
hypothesis and repeatable runtime evidence, not only a positive compiler remark.

## Boundaries

A compiler remark is static optimization evidence, not a runtime profile or a
guarantee about emitted instructions. Results depend on compiler version,
target architecture, flags, and source shape. Re-run the report and benchmark
on the target machine before making any vectorization or speedup claim.
