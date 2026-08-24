#!/usr/bin/env python3
"""Interactive Wi-Fi PSK provisioner. Run in YOUR terminal, never from Grok.

Copied from the D1 R32 OpenMRN display project's provision_wifi_build.sh.

  1. python scripts/build_lcc_wifi.py  (Grok can do this; no password)
  2. python scripts/collect_hw_ids.py --port COM7
  3. python scripts/provision_wifi_build.py          # hidden prompt
     python scripts/provision_wifi_build.py --flash --port COM7

Refuses a non-TTY, a pre-set WIFI_PASSWORD, and --password / --psk.
PSK is piped to wifi_wrap.py on stdin (not argv, not a project file).
"""

from __future__ import print_function

import getpass
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
WRAP_OUT = os.path.join(ROOT, "sketches", "LccWifiTurnoutNode", "wifi_psk_wrap.inc")
IDS_DEFAULT = os.path.join(ROOT, "local", "hw_ids.env")
WRAP_PY = os.path.join(ROOT, "scripts", "wifi_wrap.py")
BUILD_PY = os.path.join(ROOT, "scripts", "build_lcc_wifi.py")


def _die(msg):
    sys.stderr.write(msg + "\n")
    raise SystemExit(1)


def main():
    if not (sys.stdin.isatty() and sys.stdout.isatty() and sys.stderr.isatty()):
        _die(
            "Must be run in an interactive terminal (keyboard + screen).\n"
            "Do not run this from Grok, a pipe, or CI."
        )
    if os.environ.get("WIFI_PASSWORD"):
        _die(
            "WIFI_PASSWORD is already set in the environment.\n"
            "Unset it and type the password at the hidden prompt.\n"
            "    unset WIFI_PASSWORD   (or  Remove-Item Env:WIFI_PASSWORD)"
        )
    for arg in sys.argv[1:]:
        low = arg.lower()
        if low.startswith("--password") or low.startswith("--psk") or low.startswith(
            "--wifi-password"
        ):
            _die("Do not pass the Wi-Fi password as a command-line argument.")

    flash = False
    port = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] in ("--flash", "flash"):
            flash = True
            i += 1
        elif args[i] in ("-p", "--port"):
            if i + 1 >= len(args):
                _die("Missing port after %s" % args[i])
            port = args[i + 1]
            i += 2
        else:
            _die("Unknown argument: %s" % args[i])

    ids = os.environ.get("WIFI_IDS_FILE", IDS_DEFAULT)
    wrap_out = os.environ.get("WIFI_WRAP_OUT", WRAP_OUT)
    if not os.path.isfile(ids):
        _die(
            "Missing %s\nFlash the wrap-free image, then:\n"
            "  python scripts/collect_hw_ids.py --port COM7" % ids
        )

    default_ssid = os.environ.get("WIFI_SSID", "SRIF2333")
    try:
        ssid_in = input("Wi-Fi SSID [%s]: " % default_ssid).strip()
    except EOFError:
        _die("SSID prompt failed")
    ssid = ssid_in if ssid_in else default_ssid
    if not ssid:
        _die("SSID must not be empty.")

    psk = getpass.getpass("Wi-Fi password (hidden, not written as plaintext): ")
    psk2 = getpass.getpass("Again to confirm: ")
    if psk != psk2:
        _die("Passwords did not match.")
    if not psk:
        _die("Password must not be empty.")

    print("SSID in wrap blob: %s" % ssid)
    print("Encrypting on the host. Only ciphertext will be compiled in.")
    env = os.environ.copy()
    env.pop("WIFI_PASSWORD", None)
    proc = subprocess.Popen(
        [sys.executable, WRAP_PY, "encrypt", "--ids", ids, "--ssid", ssid, "--out", wrap_out],
        stdin=subprocess.PIPE,
        cwd=ROOT,
        env=env,
    )
    proc.communicate(psk.encode("utf-8"))
    psk = psk2 = "x"
    if proc.returncode != 0:
        raise SystemExit(proc.returncode)

    with open(wrap_out, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()
    if "WIFI_PASSWORD" in text.upper() or "PSK=" in text.upper() or "password=" in text.lower():
        _die("Refusing to build: wrap file looks like it contains a password field.")

    print("Host wrap written.")
    if flash:
        cmd = [sys.executable, "-u", BUILD_PY, "--flash"]
        if port:
            cmd.extend(["--port", port])
        print("Building firmware (no PSK in the environment). Verbose compile/flash.")
        raise SystemExit(subprocess.call(cmd, cwd=ROOT, env=env))
    print("Run in cmd:")
    print("  python -u scripts\\build_lcc_wifi.py --flash --port COM7")


if __name__ == "__main__":
    main()
