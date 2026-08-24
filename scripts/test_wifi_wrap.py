#!/usr/bin/env python3
"""Fake-data tests for collect + host wrap. Never uses a real house PSK."""

from __future__ import print_function

import os
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PY = sys.executable


def run(cmd, **kwargs):
    print("==", " ".join(cmd))
    rc = subprocess.call(cmd, cwd=ROOT, **kwargs)
    if rc != 0:
        raise SystemExit(rc)


def main():
    run([PY, os.path.join(ROOT, "scripts", "wifi_wrap.py"), "selftest"])

    testdata = os.path.join(ROOT, "scripts", "testdata", "fake_debug_ids.txt")
    tmp = tempfile.mkdtemp()
    ids_path = os.path.join(tmp, "hw_ids.env")
    wrap_path = os.path.join(tmp, "wifi_psk_wrap.inc")
    run(
        [
            PY,
            os.path.join(ROOT, "scripts", "collect_hw_ids.py"),
            "--from-log",
            testdata,
            "--out",
            ids_path,
        ]
    )
    with open(ids_path, "r", encoding="utf-8") as handle:
        text = handle.read()
    if "WIFI_MAC=DE:AD:BE:EF:00:01" not in text:
        raise SystemExit("collect MAC mismatch")
    if "WIFI_NODE_ID=05.01.01.01.A5.03" not in text:
        raise SystemExit("collect node mismatch")
    if "WIFI_FLASH_UID=0123456789ABCDEF" not in text:
        raise SystemExit("collect UID mismatch")

    fake = "fake-psk-not-a-house-password"
    proc = subprocess.Popen(
        [
            PY,
            os.path.join(ROOT, "scripts", "wifi_wrap.py"),
            "encrypt",
            "--ids",
            ids_path,
            "--ssid",
            "SRIF2333",
            "--out",
            wrap_path,
        ],
        stdin=subprocess.PIPE,
        cwd=ROOT,
    )
    proc.communicate(fake.encode("utf-8"))
    if proc.returncode != 0:
        raise SystemExit(proc.returncode)
    with open(wrap_path, "r", encoding="utf-8") as handle:
        wrap = handle.read()
    if fake in wrap:
        raise SystemExit("FAIL: plaintext PSK leaked into wrap include")
    if "kWifiWrapBlob[94]" not in wrap:
        raise SystemExit("wrap blob size mismatch")

    print("== provision script refuses non-TTY ==")
    env = os.environ.copy()
    env.pop("WIFI_PASSWORD", None)
    proc = subprocess.Popen(
        [PY, os.path.join(ROOT, "scripts", "provision_wifi_build.py")],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=ROOT,
        env=env,
    )
    _out, err = proc.communicate(b"")
    if proc.returncode == 0:
        raise SystemExit("FAIL: provision_wifi_build.py should refuse a pipe")
    if b"interactive terminal" not in err:
        raise SystemExit("FAIL: expected interactive-terminal error, got %r" % err)

    print("All fake-data script tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
