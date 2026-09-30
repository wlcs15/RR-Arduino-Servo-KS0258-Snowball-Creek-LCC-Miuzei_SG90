#!/usr/bin/env bash
# On-target Unity under simavr (ATmega2560 @ 16 MHz). Never flashes hardware.
# Builds sketches/RrServoUnity (same suite as scripts/run_tests.sh) and runs:
#   simavr -m atmega2560 -f 16000000 <elf>
# Sources /workspace/env/emulators.sh and arduino.sh when present.
# Windows: scripts/run_on_target_tests.ps1 (and this .py on nt) print SKIP
# and exit 0. simavr on-target is Linux CI only.
#   ./scripts/run_on_target_tests.sh
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f /workspace/env/emulators.sh ]]; then
  # shellcheck disable=SC1091
  source /workspace/env/emulators.sh
fi
if [[ -d /workspace/tools/bin ]]; then
  export PATH="/workspace/tools/bin:${PATH}"
fi
if [[ -f /workspace/env/arduino.sh ]]; then
  # shellcheck disable=SC1091
  source /workspace/env/arduino.sh
fi
# Host C++ include paths poison avr-g++.
unset CPLUS_INCLUDE_PATH C_INCLUDE_PATH CPATH GCC_ROOT || true
if command -v python3 >/dev/null 2>&1; then
  py=python3
elif command -v python >/dev/null 2>&1; then
  py=python
else
  echo "python3 not found"
  exit 1
fi
exec "$py" -u "$root/scripts/run_on_target_tests.py" "$@"
