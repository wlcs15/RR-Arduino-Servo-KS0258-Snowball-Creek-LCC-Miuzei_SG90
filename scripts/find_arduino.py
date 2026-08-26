#!/usr/bin/env python3
"""Locate Arduino cores and Library Manager trees already on this PC.

Does not install or uninstall anything. Does not touch a Wi-Fi password.

Arduino IDE and arduino-cli often use different user dirs. On Windows 11,
Documents is frequently redirected to OneDrive, so `arduino-cli lib list`
can be empty while the libraries are present. This script searches the
filesystem first, then CLI output.

  python -u scripts/find_arduino.py
  python -u scripts/find_arduino.py --json

Used by check_tools.sh and check_tools.ps1.
"""
from __future__ import print_function

import json
import os
import subprocess
import sys

REQUIRED_LIBS = (
    "LibLCC",
    "ACAN2517",
    "ACAN2515",
    "M95_EEPROM",
    "OpenMRNLite",
    "ESP32Servo",
)
REQUIRED_CORES = (
    ("arduino:avr", ("arduino", "avr")),
    ("esp32:esp32", ("esp32", "esp32")),
)


def _norm(name):
    return "".join(ch.lower() for ch in name if ch.isalnum())


def _home():
    return os.path.expanduser("~")


def find_cli():
    names = ["arduino-cli"]
    if os.name == "nt":
        names = ["arduino-cli.exe", "arduino-cli"]
    for n in names:
        path = _which(n)
        if path:
            return path
    home = _home()
    cands = [
        os.path.join(home, "ex-installer", "arduino-cli", "arduino-cli.exe"),
        os.path.join(home, "ex-installer", "arduino-cli", "arduino-cli"),
        os.path.join(
            os.environ.get("ProgramFiles", r"C:\Program Files"),
            "Arduino IDE",
            "resources",
            "app",
            "lib",
            "backend",
            "resources",
            "arduino-cli.exe",
        ),
        os.path.join(home, "bin", "arduino-cli"),
        "/usr/local/bin/arduino-cli",
        "/usr/bin/arduino-cli",
    ]
    for p in cands:
        if p and os.path.isfile(p):
            return p
    return None


def _which(name):
    exts = [""]
    if os.name == "nt":
        pathext = os.environ.get("PATHEXT", ".EXE").split(os.pathsep)
        exts = [""] + pathext
    for folder in os.environ.get("PATH", "").split(os.pathsep):
        if not folder:
            continue
        for ext in exts:
            cand = os.path.join(folder, name + ext)
            if os.path.isfile(cand):
                return cand
    return None


def _run(cmd):
    try:
        out = subprocess.check_output(
            cmd, stderr=subprocess.STDOUT, universal_newlines=True
        )
    except (OSError, subprocess.CalledProcessError):
        return ""
    return out


def cli_dirs(cli):
    data = None
    user = None
    if not cli:
        return data, user
    text = _run([cli, "config", "dump"])
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("data:"):
            data = line.split(":", 1)[1].strip().strip("\"'")
        elif line.startswith("user:"):
            user = line.split(":", 1)[1].strip().strip("\"'")
    return data, user


def user_dir_candidates(cli_user):
    home = _home()
    out = []

    def add(p):
        if p and os.path.isdir(p) and p not in out:
            out.append(p)

    add(cli_user)
    add(os.path.join(home, "Arduino"))
    add(os.path.join(home, "Documents", "Arduino"))
    # Windows 11 Known Folder redirect (Dell XPS / OneDrive).
    add(os.path.join(home, "OneDrive", "Documents", "Arduino"))
    add(os.path.join(home, "OneDrive", "Arduino"))
    try:
        for ent in os.listdir(home):
            if not ent.lower().startswith("onedrive"):
                continue
            base = os.path.join(home, ent)
            add(os.path.join(base, "Documents", "Arduino"))
            add(os.path.join(base, "Arduino"))
    except OSError:
        pass
    return out


def data_dir_candidates(cli_data):
    home = _home()
    local = os.environ.get("LOCALAPPDATA") or os.path.join(
        home, "AppData", "Local"
    )
    out = []

    def add(p):
        if p and os.path.isdir(p) and p not in out:
            out.append(p)

    add(cli_data)
    add(os.path.join(home, ".arduino15"))
    add(os.path.join(local, "Arduino15"))
    add(os.path.join(home, "AppData", "Local", "Arduino15"))
    return out


def _lib_name_from_properties(path):
    try:
        with open(path, "r") as fh:
            for line in fh:
                if line.lower().startswith("name="):
                    return line.split("=", 1)[1].strip()
    except OSError:
        return None
    return None


def find_library(name, user_dirs):
    want = _norm(name)
    for user in user_dirs:
        libroot = os.path.join(user, "libraries")
        if not os.path.isdir(libroot):
            continue
        direct = os.path.join(libroot, name)
        if os.path.isdir(direct):
            return direct
        try:
            entries = os.listdir(libroot)
        except OSError:
            continue
        for ent in entries:
            folder = os.path.join(libroot, ent)
            if not os.path.isdir(folder):
                continue
            if _norm(ent) == want:
                return folder
            prop = _lib_name_from_properties(
                os.path.join(folder, "library.properties")
            )
            if prop and _norm(prop) == want:
                return folder
    return None


def find_core(vendor, arch, data_dirs):
    for data in data_dirs:
        root = os.path.join(data, "packages", vendor, "hardware", arch)
        if os.path.isdir(root):
            try:
                vers = [
                    n
                    for n in os.listdir(root)
                    if os.path.isdir(os.path.join(root, n))
                ]
            except OSError:
                vers = []
            if vers:
                vers.sort()
                return os.path.join(root, vers[-1])
            return root
    return None


def inspect():
    cli = find_cli()
    cli_data, cli_user = cli_dirs(cli)
    users = user_dir_candidates(cli_user)
    datas = data_dir_candidates(cli_data)
    libs = {}
    for name in REQUIRED_LIBS:
        libs[name] = find_library(name, users)
    cores = {}
    for fqbn, pair in REQUIRED_CORES:
        cores[fqbn] = find_core(pair[0], pair[1], datas)
    return {
        "cli": cli,
        "user_dirs": users,
        "data_dirs": datas,
        "libs": libs,
        "cores": cores,
    }


def main():
    json_mode = "--json" in sys.argv
    info = inspect()
    if json_mode:
        print(json.dumps(info, indent=2))
        missing = any(v is None for v in info["libs"].values()) or any(
            v is None for v in info["cores"].values()
        )
        return 1 if missing else 0
    print("arduino-cli: %s" % (info["cli"] or "NOT FOUND"))
    print("user dirs:")
    if info["user_dirs"]:
        for d in info["user_dirs"]:
            print("  %s" % d)
    else:
        print("  (none exist yet)")
    print("data dirs:")
    if info["data_dirs"]:
        for d in info["data_dirs"]:
            print("  %s" % d)
    else:
        print("  (none exist yet)")
    missing = 0
    print("cores (filesystem, already installed):")
    for fqbn, _pair in REQUIRED_CORES:
        path = info["cores"].get(fqbn)
        if path:
            print("  OK       %s  %s" % (fqbn, path))
        else:
            print("  MISSING  %s  not under Arduino15/packages (no uninstall)" % fqbn)
            missing = 1
    print("libraries (filesystem, already installed):")
    for name in REQUIRED_LIBS:
        path = info["libs"].get(name)
        if path:
            print("  OK       lib %-14s %s" % (name, path))
        else:
            print(
                "  MISSING  lib %-14s not in Arduino/libraries (no uninstall)"
                % name
            )
            missing = 1
    return missing


if __name__ == "__main__":
    sys.exit(main())
