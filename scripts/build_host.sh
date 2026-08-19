#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build/host"
mkdir -p "$build"
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build"
echo "host binaries in $build"
