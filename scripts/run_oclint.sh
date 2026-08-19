#!/usr/bin/env bash
set -euo pipefail
if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == CYGWIN* || "$(uname -s)" == MSYS* ]]; then
  echo "OCLint skipped on Windows"
  exit 0
fi
if ! command -v oclint >/dev/null 2>&1; then
  echo "oclint not installed (Linux only); skip"
  exit 0
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
oclint "$root/lib/rr_servo/src/"*.h "$root/tests/"*.cpp "$root/host/"*.cpp \
  -- -std=c++11 -I "$root/lib/rr_servo/src"
