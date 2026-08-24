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
"$py" "$root/scripts/git_version.py" --selftest
"$py" "$root/scripts/build_host.py"
# Dual-boot NTFS may leave a Windows .exe beside the Linux ELF. Prefer native.
if [[ -x "$root/build/host/rr_servo_tests" ]]; then
  exec "$root/build/host/rr_servo_tests"
fi
if [[ -x "$root/build/host/rr_servo_tests.exe" ]]; then
  exec "$root/build/host/rr_servo_tests.exe"
fi
echo "rr_servo_tests not built in $root/build/host"
exit 1
