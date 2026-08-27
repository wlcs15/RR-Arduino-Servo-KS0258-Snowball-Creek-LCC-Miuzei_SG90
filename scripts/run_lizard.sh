#!/usr/bin/env bash
# Use the first Python that can import lizard (pipx venv counts).
# Windows Git Bash: python3 is often the Store stub without packages.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cands=()
if command -v python >/dev/null 2>&1; then
  cands+=("python")
fi
if command -v python3 >/dev/null 2>&1; then
  cands+=("python3")
fi
if [[ ${#cands[@]} -eq 0 ]]; then
  echo "python not found"
  exit 1
fi
py=""
for c in "${cands[@]}"; do
  if "$c" -u "$root/scripts/run_lizard.py" --check >/dev/null 2>&1; then
    py="$c"
    break
  fi
done
if [[ -z "$py" ]]; then
  py="${cands[0]}"
fi
exec "$py" -u "$root/scripts/run_lizard.py" "$@"
