#!/usr/bin/env python3
"""Cyclomatic complexity for our sources, reported by project module.

Fail if any function CCN exceeds 10. Never scan third_party/ or build/.
Arduino .ino files are analyzed as C++.
"""
from __future__ import print_function

import os
import shutil
import sys


def _venv_site_packages(venv_root):
    sites = []
    if not venv_root or not os.path.isdir(venv_root):
        return sites
    for dirpath, dirnames, _filenames in os.walk(venv_root):
        if os.path.basename(dirpath).lower() == "site-packages":
            sites.append(dirpath)
            dirnames[:] = []
    return sites


def _pipx_venv_roots():
    home = os.path.expanduser("~")
    local = os.environ.get("LOCALAPPDATA") or os.path.join(home, "AppData", "Local")
    roots = []
    pipx_home = os.environ.get("PIPX_HOME")
    if pipx_home:
        roots.append(os.path.join(pipx_home, "venvs", "lizard"))
    roots.extend(
        [
            os.path.join(home, ".local", "share", "pipx", "venvs", "lizard"),
            os.path.join(home, ".local", "pipx", "venvs", "lizard"),
            os.path.join(home, "pipx", "venvs", "lizard"),
            os.path.join(local, "pipx", "venvs", "lizard"),
            os.path.join(home, "scoop", "persist", "pipx", "venvs", "lizard"),
        ]
    )
    return roots


def load_lizard():
    """Import lizard from this Python, then pipx venvs (Windows + Linux).

    Do not require pip --user (blocked on Ubuntu 24.04 and Store Python).
    """
    try:
        import lizard as mod

        return mod
    except ImportError:
        pass

    sites = []
    exe = shutil.which("lizard")
    if exe:
        bindir = os.path.dirname(os.path.abspath(exe))
        sites.extend(_venv_site_packages(os.path.dirname(bindir)))
    for root in _pipx_venv_roots():
        sites.extend(_venv_site_packages(root))

    seen = set()
    for site in sites:
        key = os.path.normcase(os.path.abspath(site))
        if key in seen:
            continue
        seen.add(key)
        if site not in sys.path:
            sys.path.insert(0, site)
        try:
            import lizard as mod

            return mod
        except ImportError:
            continue
    return None


def _in_venv():
    if os.environ.get("VIRTUAL_ENV"):
        return True
    base = getattr(sys, "base_prefix", None) or getattr(sys, "real_prefix", None)
    if base:
        return os.path.normcase(sys.prefix) != os.path.normcase(base)
    return False


def _die_missing_lizard():
    sys.stderr.write(
        "lizard is not importable in this Python:\n  %s\n"
        "Do NOT run:  pip install --user lizard\n"
        "  (fails in a venv: User site-packages are not visible in this virtualenv)\n"
        % sys.executable
    )
    if _in_venv():
        sys.stderr.write(
            "This is a virtualenv. Install into it (no --user):\n"
            "  python -m pip install lizard\n"
        )
    else:
        sys.stderr.write(
            "Install with pipx (recommended) or into this interpreter:\n"
            "  pipx install lizard\n"
            "  python -m pip install lizard\n"
        )
    raise SystemExit(1)

CCN_LIMIT = 10
ROOT_DIRS = ("lib", "sketches", "tests", "host")
SKIP_DIRS = {"third_party", "build", ".git"}
SOURCE_EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ino"}


def repo_root():
    return os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def rel_posix(root, path):
    return os.path.relpath(path, root).replace("\\", "/")


def module_name(relpath):
    parts = relpath.split("/")
    if parts[0] == "lib" and len(parts) > 1:
        return "lib/" + parts[1]
    if parts[0] == "sketches" and len(parts) > 1:
        return "sketches/" + parts[1]
    return parts[0]


def iter_sources(root):
    for top in ROOT_DIRS:
        start = os.path.join(root, top)
        if not os.path.isdir(start):
            continue
        for dirpath, dirnames, filenames in os.walk(start):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            for name in filenames:
                ext = os.path.splitext(name)[1].lower()
                if ext in SOURCE_EXTS:
                    yield os.path.join(dirpath, name)


def analyze_file(path, lizard):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        code = handle.read()
    as_name = path + ".cpp" if path.lower().endswith(".ino") else path
    analyzer = lizard.FileAnalyzer(lizard.get_extensions([]))
    info = analyzer.analyze_source_code(as_name, code)
    info.filename = path
    return info


def main():
    if "--check" in sys.argv:
        mod = load_lizard()
        if not mod:
            print("MISSING lizard")
            return 1
        print("OK %s" % getattr(mod, "__file__", "lizard"))
        return 0
    try:
        import session_log
        session_log.attach("run_lizard")
    except Exception:
        pass
    lizard = load_lizard()
    if not lizard:
        _die_missing_lizard()
    root = repo_root()
    modules = {}
    over = []

    for path in sorted(iter_sources(root)):
        rel = rel_posix(root, path)
        info = analyze_file(path, lizard)
        key = module_name(rel)
        bucket = modules.setdefault(
            key,
            {
                "nloc": 0,
                "funcs": 0,
                "ccn_sum": 0,
                "max_ccn": 0,
                "over": 0,
                "files": 0,
            },
        )
        bucket["files"] += 1
        bucket["nloc"] += info.nloc
        for fn in info.function_list:
            ccn = fn.cyclomatic_complexity
            bucket["funcs"] += 1
            bucket["ccn_sum"] += ccn
            if ccn > bucket["max_ccn"]:
                bucket["max_ccn"] = ccn
            if ccn > CCN_LIMIT:
                bucket["over"] += 1
                over.append((key, rel, fn.name, fn.start_line, ccn, fn.nloc))

    print("Cyclomatic complexity by module (lizard, CCN limit %d)" % CCN_LIMIT)
    print(
        "%-28s %6s %6s %7s %7s %6s"
        % ("Module", "NLOC", "Funcs", "AvgCCN", "MaxCCN", "CCN>10")
    )
    print("-" * 70)
    total_nloc = 0
    total_funcs = 0
    total_ccn = 0
    total_over = 0
    max_ccn = 0
    for key in sorted(modules):
        b = modules[key]
        avg = (float(b["ccn_sum"]) / b["funcs"]) if b["funcs"] else 0.0
        print(
            "%-28s %6d %6d %7.1f %7d %6d"
            % (key, b["nloc"], b["funcs"], avg, b["max_ccn"], b["over"])
        )
        total_nloc += b["nloc"]
        total_funcs += b["funcs"]
        total_ccn += b["ccn_sum"]
        total_over += b["over"]
        if b["max_ccn"] > max_ccn:
            max_ccn = b["max_ccn"]
    avg = (float(total_ccn) / total_funcs) if total_funcs else 0.0
    print("-" * 70)
    print(
        "%-28s %6d %6d %7.1f %7d %6d"
        % ("TOTAL (our code)", total_nloc, total_funcs, avg, max_ccn, total_over)
    )
    print("third_party/ and build/ are not scanned.")
    print(
        "Fail rule: any function CCN > %d (McCabe is per-function, not per-file)."
        % CCN_LIMIT
    )

    if over:
        print()
        print("Functions with CCN > %d:" % CCN_LIMIT)
        for key, rel, name, line, ccn, nloc in over:
            print(
                "  %s  %s:%d  %s  CCN=%d NLOC=%d" % (key, rel, line, name, ccn, nloc)
            )
        return 1

    print()
    print("OK: no function exceeds CCN %d" % CCN_LIMIT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
