#!/usr/bin/env bash

set -euo pipefail

compiler=${CXX:-c++}
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temporary_directory=$(mktemp -d)
trap 'rm -rf "$temporary_directory"' EXIT

cd "$repo_root"
compiler_version=$("$compiler" --version)
compiler_heading=${compiler_version%%$'\n'*}
compiler_macros=$("$compiler" -dM -E -x c++ /dev/null)

if [[ "$compiler_macros" == *"#define __clang__ 1"* ]]; then
    diagnostics=$(
      "$compiler" -Iinclude -std=c++20 -O3 -pthread \
        -Rpass=loop-vectorize \
        -Rpass-missed=loop-vectorize \
        -Rpass-analysis=loop-vectorize \
        -c src/blur.cpp \
        -o "$temporary_directory/blur.o" 2>&1
    )
elif [[ "$compiler_macros" == *"#define __GNUC__"* ]]; then
    diagnostics=$(
      "$compiler" -Iinclude -std=c++20 -O3 -pthread \
        -fopt-info-vec-all \
        -c src/blur.cpp \
        -o "$temporary_directory/blur.o" 2>&1
    )
else
    printf 'error: unsupported compiler for vectorization diagnostics: %s\n' "$compiler_heading" >&2
    exit 1
fi

source_diagnostics=$(printf '%s\n' "$diagnostics" | awk '/^src\/blur\.cpp:/')
printf 'Compiler: %s\n' "$compiler_heading"
if [[ -n "$source_diagnostics" ]]; then
  printf '%s\n' "$source_diagnostics"
else
  printf 'No loop-vectorization diagnostics were emitted for src/blur.cpp.\n'
fi
