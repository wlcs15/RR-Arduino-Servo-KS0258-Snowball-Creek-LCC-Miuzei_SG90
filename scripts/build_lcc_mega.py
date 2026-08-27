#!/usr/bin/env python3
"""Compile sketches/LccTurnoutNode with SNIP softwareVersion from git tag.

Does not flash. Never opens the Mega serial port (JMRI / DTR).
  python -u scripts/build_lcc_mega.py
"""

from __future__ import print_function

import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import find_arduino  # pylint: disable=wrong-import-position
import git_version  # pylint: disable=wrong-import-position
import session_log  # pylint: disable=wrong-import-position

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SKETCH = os.path.join(ROOT, "sketches", "LccTurnoutNode")
FQBN = "arduino:avr:mega:cpu=atmega2560"


def find_cli():
    found = find_arduino.find_cli()
    if found:
        return found
    return "arduino-cli"


def main():
    session_log.attach("build_lcc_mega")
    inc = os.path.join(SKETCH, "git_version.inc")
    ver = git_version.write_inc(inc)
    print("SNIP softwareVersion %s" % ver)
    print("wrote %s" % inc)
    cmd = [
        find_cli(),
        "-v",
        "compile",
        "--fqbn",
        FQBN,
        "--library",
        os.path.join(ROOT, "lib", "rr_servo"),
        "--library",
        os.path.join(ROOT, "third_party", "Adafruit-PWM-Servo-Driver-Library"),
        "--library",
        os.path.join(ROOT, "third_party", "Adafruit_BusIO"),
        SKETCH,
    ]
    print("=== compile LccTurnoutNode ===")
    t0 = time.time()
    rc = subprocess.call(cmd, cwd=ROOT)
    print("=== compile finished in %.1f s (exit %d) ===" % (time.time() - t0, rc))
    return rc


if __name__ == "__main__":
    sys.exit(main())
