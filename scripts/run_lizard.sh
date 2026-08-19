#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
lizard_cmd=()
if command -v lizard >/dev/null 2>&1; then
  lizard_cmd=(lizard)
elif python3 -c "import lizard" >/dev/null 2>&1; then
  lizard_cmd=(python3 -m lizard)
else
  echo "lizard not installed (pip install --user lizard)"
  exit 1
fi
# Fail at cyclomatic complexity 10 on our sources only.
"${lizard_cmd[@]}" --CCN 10 -w -l cpp -l c \
  "$root/lib" "$root/sketches" "$root/tests" "$root/host"
