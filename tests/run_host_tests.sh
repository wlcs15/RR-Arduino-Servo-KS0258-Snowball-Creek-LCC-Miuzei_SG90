#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
g++ -std=c++17 -Wall -Wextra -I "$root/lib/rr_servo/src" \
  "$root/tests/test_rr_servo.cpp" -o /tmp/rr_servo_tests
/tmp/rr_servo_tests
