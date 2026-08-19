#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
"$root/scripts/build_host.sh"
"$root/build/host/rr_servo_tests"
