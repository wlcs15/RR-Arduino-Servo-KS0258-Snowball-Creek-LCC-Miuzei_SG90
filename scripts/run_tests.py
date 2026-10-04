#!/usr/bin/env python3
"""Host tests for the arm-arduino and uno-r3 tip.

This tip has tests/test_rr_servo.cpp and no CMake host build. The AVR CI
host step runs scripts/run_tests.sh and skips the branch when scripts/ is
absent.
"""
from __future__ import print_function

import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_DIR = os.path.join(ROOT, "build", "host")
OUT = os.path.join(OUT_DIR, "rr_servo_tests")


def compiler():
    import shutil
    for name in (os.environ.get("CXX", ""), "g++", "c++"):
        if name and shutil.which(name):
            return name
    raise SystemExit("g++ not found")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    src = os.path.join(ROOT, "tests", "test_rr_servo.cpp")
    inc = os.path.join(ROOT, "lib", "rr_servo", "src")
    rc = subprocess.call([compiler(), "-std=c++17", "-Wall", "-Wextra", "-I", inc, src, "-o", OUT])
    if rc:
        return rc
    return subprocess.call([OUT])


if __name__ == "__main__":
    sys.exit(main())
