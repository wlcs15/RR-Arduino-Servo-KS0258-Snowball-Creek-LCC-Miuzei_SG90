#!/usr/bin/env bash
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
exec "$py" "$root/scripts/run_lizard.py"
