#!/usr/bin/env python3
"""Grok-safe compile/flash of sketches/LccWifiTurnoutNode.

Never prompts. Never reads a Wi-Fi password. If wifi_psk_wrap.inc already
exists (written by provision_wifi_build.py in YOUR terminal), bakes the
ciphertext with -DWIFI_WRAP_BLOB_PRESENT=1.

Always runs arduino-cli -v so each source file and each flash block prints.
Use: python -u scripts/build_lcc_wifi.py --flash --port COM7
"""

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import find_arduino  # pylint: disable=wrong-import-position
import git_version  # pylint: disable=wrong-import-position

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SKETCH = os.path.join(ROOT, "sketches", "LccWifiTurnoutNode")
WRAP = os.path.join(SKETCH, "wifi_psk_wrap.inc")
FQBN = "esp32:esp32:d1_uno32"
def find_cli():
    found = find_arduino.find_cli()
    if found:
        return found
    return "arduino-cli"


def extra_flags():
    inc = os.path.join(SKETCH, "git_version.inc")
    ver = git_version.write_inc(inc)
    # Token plus a quoted LITERAL so v2.06+ is a real SNIP string.
    flags = "-DRR_WIFI_LCC=1 -DRR_GIT_VERSION=%s" % ver
    print("SNIP softwareVersion %s" % ver)
    print("wrote %s" % inc)
    if os.path.isfile(WRAP):
        flags += " -DWIFI_WRAP_BLOB_PRESENT=1"
        print("baking existing wrap ciphertext (file is gitignored; not a password)")
    else:
        print("wrap-free image (WIFI PASSWORD: NOT SET until you provision)")
    return flags


def run_logged(cmd, title):
    """Run cmd with inherited stdout/stderr so gcc and esptool stream live."""
    print("")
    print("=== %s ===" % title)
    print(" ".join(cmd))
    print("OpenMRN first compile can take several minutes; -v prints each file.")
    sys.stdout.flush()
    sys.stderr.flush()
    t0 = time.time()
    rc = subprocess.call(cmd, cwd=ROOT)
    dt = time.time() - t0
    print("=== %s finished in %.1f s (exit %d) ===" % (title, dt, rc))
    sys.stdout.flush()
    return rc


def main():
    if os.environ.get("WIFI_PASSWORD"):
        sys.stderr.write(
            "WIFI_PASSWORD is set. Unset it. This script never reads a PSK.\n"
        )
        return 1
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--flash", action="store_true")
    parser.add_argument("--port", default="COM7")
    args = parser.parse_args()
    cli = find_cli()
    flags = extra_flags()
    compile_cmd = [
        cli,
        "-v",
        "compile",
        "--fqbn",
        FQBN,
        "--library",
        os.path.join(ROOT, "lib", "rr_servo"),
        "--build-property",
        "compiler.cpp.extra_flags=" + flags,
        "--build-property",
        "compiler.c.extra_flags=" + flags,
        SKETCH,
    ]
    rc = run_logged(compile_cmd, "compile LccWifiTurnoutNode")
    if rc != 0:
        return rc
    if args.flash:
        up = [
            cli,
            "-v",
            "upload",
            "--fqbn",
            FQBN,
            "-p",
            args.port,
            SKETCH,
        ]
        return run_logged(up, "upload %s" % args.port)
    return 0


if __name__ == "__main__":
    sys.exit(main())
