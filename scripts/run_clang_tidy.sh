#!/usr/bin/env bash
# Launcher for scripts/run_clang_tidy.py (Ubuntu / Git Bash).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if command -v python3 >/dev/null 2>&1; then
  py=python3
elif command -v python >/dev/null 2>&1; then
  py=python
else
  echo "python3 not found"
  exit 1
fi
exec "$py" -u "$root/scripts/run_clang_tidy.py" "$@"
