#!/usr/bin/env bash

set -euo pipefail

binary=${BINARY:-./build/parallel-pixels}
sizes=${SIZES:-"128 512 2048"}
worker_counts=${THREADS:-"1 2 4 8"}
kernels=${KERNELS:-"gaussian box"}
worker_lifecycles=${WORKER_LIFECYCLES:-"per-invocation persistent"}
iterations=${ITERATIONS:-7}

if [[ ! -x "$binary" ]]; then
  printf 'error: benchmark binary is not executable: %s\n' "$binary" >&2
  exit 1
fi

for lifecycle in $worker_lifecycles; do
  case "$lifecycle" in
    per-invocation|persistent) ;;
    *)
      printf 'error: unsupported worker lifecycle: %s\n' "$lifecycle" >&2
      exit 1
      ;;
  esac
  for kernel in $kernels; do
    for size in $sizes; do
      for workers in $worker_counts; do
        benchmark_command=(
          "$binary" benchmark
          --width "$size"
          --height "$size"
          --threads "$workers"
          --iterations "$iterations"
          --kernel "$kernel"
        )
        if [[ "$lifecycle" == persistent ]]; then
          benchmark_command+=(--reuse-workers)
        fi
        benchmark_command+=(--json)
        result=$("${benchmark_command[@]}")
        printf '%s' "$result" | tr -d '\n'
        printf '\n'
      done
    done
  done
done
