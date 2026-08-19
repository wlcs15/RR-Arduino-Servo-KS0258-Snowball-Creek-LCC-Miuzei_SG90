#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if ! command -v cppcheck >/dev/null 2>&1; then
  echo "cppcheck not installed"
  exit 1
fi
src=("$root/lib" "$root/sketches" "$root/tests" "$root/host")
# Errors are always enabled and fail this target.
cppcheck --std=c++11 --error-exitcode=1 --inline-suppr \
  --suppress=missingIncludeSystem "${src[@]}"
# Warnings/style are reports only.
cppcheck --std=c++11 --enable=warning,style,performance --inline-suppr \
  --suppress=missingIncludeSystem "${src[@]}" || true
