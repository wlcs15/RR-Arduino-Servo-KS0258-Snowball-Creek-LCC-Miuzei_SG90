#!/usr/bin/env bash
# Ubuntu wrapper. Same rules as scripts/provision_wifi_build.py.
# Do not run from Grok. Do not pass a password on the command line.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if command -v python3 >/dev/null 2>&1; then
  py=python3
else
  py=python
fi
exec "$py" "$root/scripts/provision_wifi_build.py" "$@"
