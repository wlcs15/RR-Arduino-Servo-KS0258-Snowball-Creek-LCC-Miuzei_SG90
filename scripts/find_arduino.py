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
import re
import subprocess
import sys

try:
    from urllib.request import Request, urlopen
except ImportError:
    Request = None
    urlopen = None

# Arduino CLI 1.0 (June 2024) is the current major. 0.35.x is the old line.
MIN_CLI = (1, 0, 0)
LATEST_URL = "https://api.github.com/repos/arduino/arduino-cli/releases/latest"
VERSION_RE = re.compile(r"Version:\s*v?([0-9]+(?:\.[0-9]+)*)", re.I)

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


def _ver_tuple(text):
    parts = []
    for bit in (text or "0").split("."):
        try:
            parts.append(int(bit))
        except ValueError:
            parts.append(0)
    while len(parts) < 3:
        parts.append(0)
    return tuple(parts[:3])


def parse_cli_version_text(text):
    match = VERSION_RE.search(text or "")
    if not match:
        return None
    return match.group(1)


def cli_version_of(path):
    if not path:
        return None
    text = _run([path, "version"])
    return parse_cli_version_text(text)


def _cli_candidate_paths():
    home = _home()
    pf = os.environ.get("ProgramFiles", r"C:\Program Files")
    names = ["arduino-cli.exe", "arduino-cli"] if os.name == "nt" else ["arduino-cli"]
    found = []
    seen = set()

    def add(path):
        if not path:
            return
        path = os.path.normpath(path)
        if not os.path.isfile(path):
            return
        key = os.path.normcase(os.path.realpath(path))
        if key in seen:
            return
        seen.add(key)
        found.append(path)

    for n in names:
        add(_which(n))
    add(os.path.join(home, "ex-installer", "arduino-cli", "arduino-cli.exe"))
    add(os.path.join(home, "ex-installer", "arduino-cli", "arduino-cli"))
    def add_ide_cli(root):
        add(
            os.path.join(
                root,
                "Arduino IDE",
                "resources",
                "app",
                "lib",
                "backend",
                "resources",
                "arduino-cli.exe",
            )
        )

    add_ide_cli(pf)
    pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    add_ide_cli(pf86)
    local = os.environ.get("LOCALAPPDATA") or os.path.join(home, "AppData", "Local")
    add_ide_cli(os.path.join(local, "Programs"))
    add_ide_cli(local)
    try:
        programs = os.path.join(local, "Programs")
        if os.path.isdir(programs):
            for ent in os.listdir(programs):
                if "arduino" in ent.lower():
                    add_ide_cli(os.path.join(programs, ent))
                    add(
                        os.path.join(
                            programs,
                            ent,
                            "resources",
                            "app",
                            "lib",
                            "backend",
                            "resources",
                            "arduino-cli.exe",
                        )
                    )
    except OSError:
        pass
    add(os.path.join(home, "scoop", "shims", "arduino-cli.exe"))
    add(os.path.join(home, "scoop", "apps", "arduino-cli", "current", "arduino-cli.exe"))
    add(os.path.join(home, "bin", "arduino-cli"))
    add("/usr/local/bin/arduino-cli")
    add("/usr/bin/arduino-cli")
    add(os.path.join(local, "Arduino15", "arduino-cli.exe"))
    return found


def list_cli_copies():
    copies = []
    for path in _cli_candidate_paths():
        ver = cli_version_of(path) or "0.0.0"
        copies.append({"path": path, "version": ver, "tuple": _ver_tuple(ver)})
    copies.sort(key=lambda c: c["tuple"], reverse=True)
    return copies


def find_cli():
    copies = list_cli_copies()
    if not copies:
        return None
    return copies[0]["path"]


def fetch_latest_cli():
    """Latest stable GitHub tag (no -rc). None if offline or urllib missing."""
    if Request is None or urlopen is None:
        return None
    try:
        req = Request(
            LATEST_URL,
            headers={
                "User-Agent": "rr-servo-check-tools",
                "Accept": "application/vnd.github+json",
            },
        )
        resp = urlopen(req, timeout=8)
        raw = resp.read()
        if hasattr(raw, "decode"):
            raw = raw.decode("utf-8", "replace")
        data = json.loads(raw)
    except Exception:
        return None
    tag = data.get("tag_name") or ""
    if tag.startswith("v") or tag.startswith("V"):
        tag = tag[1:]
    if not tag or "-" in tag:
        return None
    return tag


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


def _yaml_dir_user(path):
    """directories.user from an arduino-cli.yaml (no PyYAML)."""
    try:
        with open(path, "r") as fh:
            text = fh.read()
    except OSError:
        return None
    in_dirs = False
    for raw in text.splitlines():
        line = raw.rstrip()
        stripped = line.strip()
        if stripped.startswith("directories:"):
            in_dirs = True
            continue
        if in_dirs:
            if line and not line[:1].isspace() and not stripped.startswith("#"):
                in_dirs = False
            elif stripped.startswith("user:"):
                val = stripped.split(":", 1)[1].strip().strip("\"'")
                if val:
                    return val
    return None


def _preferences_sketchbook(path):
    try:
        with open(path, "r") as fh:
            for line in fh:
                if line.lower().startswith("sketchbook.path="):
                    return line.split("=", 1)[1].strip()
    except OSError:
        return None
    return None


def extra_sketchbook_dirs():
    """IDE 2 ~/.arduinoIDE yaml and Arduino15/preferences.txt sketchbook."""
    home = _home()
    local = os.environ.get("LOCALAPPDATA") or os.path.join(home, "AppData", "Local")
    files = [
        os.path.join(home, ".arduinoIDE", "arduino-cli.yaml"),
        os.path.join(local, "Arduino15", "arduino-cli.yaml"),
        os.path.join(home, ".arduino15", "arduino-cli.yaml"),
        os.path.join(local, "Arduino15", "preferences.txt"),
        os.path.join(home, ".arduino15", "preferences.txt"),
    ]
    found = []
    for path in files:
        if path.endswith(".yaml"):
            user = _yaml_dir_user(path)
        else:
            user = _preferences_sketchbook(path)
        if user and os.path.isdir(user) and user not in found:
            found.append(user)
    return found


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
    for extra in extra_sketchbook_dirs():
        add(extra)
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


def _names_match(want, got):
    """LibLCC vs liblcc-arduino, M95_EEPROM vs M95-EEPROM, etc."""
    w = _norm(want)
    g = _norm(got or "")
    if not w or not g:
        return False
    if w == g:
        return True
    if w in g or g in w:
        return True
    return False


def find_library(name, user_dirs):
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
            if _names_match(name, ent):
                return folder
            prop = _lib_name_from_properties(
                os.path.join(folder, "library.properties")
            )
            if prop and _names_match(name, prop):
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
    copies = list_cli_copies()
    cli = copies[0]["path"] if copies else None
    latest = fetch_latest_cli()
    cli_data, cli_user = cli_dirs(cli)
    users = user_dir_candidates(cli_user)
    datas = data_dir_candidates(cli_data)
    libs = {}
    for name in REQUIRED_LIBS:
        libs[name] = find_library(name, users)
    cores = {}
    for fqbn, pair in REQUIRED_CORES:
        cores[fqbn] = find_core(pair[0], pair[1], datas)
    lib_roots = []
    for user in users:
        root = os.path.join(user, "libraries")
        if os.path.isdir(root) and root not in lib_roots:
            lib_roots.append(root)
    return {
        "cli": cli,
        "cli_version": copies[0]["version"] if copies else None,
        "cli_copies": copies,
        "cli_latest": latest,
        "user_dirs": users,
        "data_dirs": datas,
        "lib_roots": lib_roots,
        "libs": libs,
        "cores": cores,
    }


def cli_status_lines(info):
    """OK/MISSING/WARN lines for check_tools.ps1 / check_tools.sh."""
    lines = []
    copies = info.get("cli_copies") or []
    latest = info.get("cli_latest")
    hint = "https://github.com/arduino/arduino-cli/releases/latest"
    if not copies:
        msg = "not found. Need Arduino CLI 1.0+ (current major)."
        if latest:
            msg += " Latest stable %s. %s" % (latest, hint)
        else:
            msg += " %s" % hint
        lines.append("  MISSING  arduino-cli        %s" % msg)
        return lines

    best = copies[0]
    ver = best["version"]
    path = best["path"]
    if best["tuple"] < MIN_CLI:
        lines.append(
            "  MISSING  arduino-cli        %s (%s); need 1.0+ (latest stable %s). %s"
            % (ver, path, latest or "unknown", hint)
        )
    elif latest and _ver_tuple(ver) == _ver_tuple(latest):
        lines.append(
            "  OK       arduino-cli        %s  %s  (matches latest stable)"
            % (ver, path)
        )
    elif latest and _ver_tuple(ver) < _ver_tuple(latest):
        lines.append("  OK       arduino-cli        %s  %s  (1.0+ required)" % (ver, path))
        lines.append(
            "  WARN     arduino-cli-latest installed %s; latest stable is %s. %s"
            % (ver, latest, hint)
        )
    elif latest and _ver_tuple(ver) > _ver_tuple(latest):
        lines.append(
            "  OK       arduino-cli        %s  %s  (newer than latest stable %s)"
            % (ver, path, latest)
        )
    else:
        lines.append(
            "  OK       arduino-cli        %s  %s  (1.0+ required; latest not queried)"
            % (ver, path)
        )
        lines.append(
            "  WARN     arduino-cli-latest could not query GitHub for latest stable"
        )

    if not _which("arduino-cli") and not _which("arduino-cli.exe"):
        lines.append(
            "  WARN     arduino-cli-path   not on PATH; clones still find %s. "
            "Add that folder to PATH for a plain arduino-cli command."
            % path
        )

    for copy in copies[1:]:
        if copy["tuple"] < best["tuple"]:
            lines.append(
                "  WARN     arduino-cli-old    %s  %s  (older copy; using %s)"
                % (copy["version"], copy["path"], ver)
            )
    return lines


def main():
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import session_log
        session_log.attach("find_arduino")
    except Exception:
        pass
    json_mode = "--json" in sys.argv
    info = inspect()
    if json_mode:
        dump = dict(info)
        copies = []
        for item in dump.get("cli_copies") or []:
            copies.append({"path": item["path"], "version": item["version"]})
        dump["cli_copies"] = copies
        print(json.dumps(dump, indent=2))
        missing = any(v is None for v in info["libs"].values()) or any(
            v is None for v in info["cores"].values()
        )
        copies = info.get("cli_copies") or []
        if not copies or copies[0]["tuple"] < MIN_CLI:
            missing = 1
        return 1 if missing else 0
    for line in cli_status_lines(info):
        print(line)
    print("user dirs:")
    if info["user_dirs"]:
        for d in info["user_dirs"]:
            print("  %s" % d)
    else:
        print("  (none exist yet)")
    print("library folders searched:")
    if info.get("lib_roots"):
        for d in info["lib_roots"]:
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
            roots = info.get("lib_roots") or []
            where = "; ".join(roots) if roots else "(no Arduino/libraries folders yet)"
            print(
                "  MISSING  lib %-14s not under: %s  "
                "(arduino-cli lib search %s  then  lib install). Do not uninstall."
                % (name, where, name)
            )
            missing = 1
    copies = info.get("cli_copies") or []
    if not copies or copies[0]["tuple"] < MIN_CLI:
        missing = 1
    return missing


if __name__ == "__main__":
    sys.exit(main())
