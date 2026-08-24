#!/usr/bin/env python3
"""Grok-safe compile/flash of sketches/LccWifiTurnoutNode.

Never prompts. Never reads a Wi-Fi password. If wifi_psk_wrap.inc already
exists (written by provision_wifi_build.py in YOUR terminal), bakes the
ciphertext with -DWIFI_WRAP_BLOB_PRESENT=1.
"""

from __future__ import print_function

import argparse
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SKETCH = os.path.join(ROOT, "sketches", "LccWifiTurnoutNode")
WRAP = os.path.join(SKETCH, "wifi_psk_wrap.inc")
FQBN = "esp32:esp32:d1_uno32"
CLI_CANDIDATES = [
    os.path.expandvars(r"%USERPROFILE%\ex-installer\arduino-cli\arduino-cli.exe"),
    os.path.join(os.path.expanduser("~"), "ex-installer", "arduino-cli", "arduino-cli.exe"),
    "arduino-cli",
]


def find_cli():
    for cand in CLI_CANDIDATES:
        if cand == "arduino-cli":
            return cand
        if os.path.isfile(cand):
            return cand
    return "arduino-cli"


def extra_flags():
    flags = "-DRR_WIFI_LCC=1"
    if os.path.isfile(WRAP):
        flags += " -DWIFI_WRAP_BLOB_PRESENT=1"
        print("baking existing wrap ciphertext (file is gitignored; not a password)")
    else:
        print("wrap-free image (WIFI PASSWORD: NOT SET until you provision)")
    return flags


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
    cmd = [
        cli,
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
    print(" ".join(cmd))
    rc = subprocess.call(cmd, cwd=ROOT)
    if rc != 0:
        return rc
    if args.flash:
        up = [cli, "upload", "--fqbn", FQBN, "-p", args.port, SKETCH]
        print(" ".join(up))
        return subprocess.call(up, cwd=ROOT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
