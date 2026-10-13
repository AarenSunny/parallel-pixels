#!/usr/bin/env bash

set -euo pipefail

binary=${BINARY:-./build/parallel-pixels}
sizes=${SIZES:-"128 512 2048"}
worker_counts=${THREADS:-"1 2 4 8"}
iterations=${ITERATIONS:-7}

if [[ ! -x "$binary" ]]; then
  printf 'error: benchmark binary is not executable: %s\n' "$binary" >&2
  exit 1
fi

for size in $sizes; do
  for workers in $worker_counts; do
    result=$(
      "$binary" benchmark \
        --width "$size" \
        --height "$size" \
        --threads "$workers" \
        --iterations "$iterations" \
        --json
    )
    printf '%s' "$result" | tr -d '\n'
    printf '\n'
  done
done
