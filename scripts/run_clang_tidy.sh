#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "clang-tidy not installed"
  exit 1
fi
"$root/scripts/build_host.sh"
db="$root/build/host/compile_commands.json"
if [[ ! -f "$db" ]]; then
  echo "missing compile_commands.json"
  exit 1
fi
clang-tidy -p "$root/build/host" \
  "$root/tests/test_rr_servo.cpp" "$root/host/debug_cli.cpp"
