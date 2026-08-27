#!/usr/bin/env python3
"""Shared CMake + Clang host-build helpers for Ubuntu and Windows 11.

No MinGW, no Ceedling, no gcovr. Coverage uses llvm-cov.
"""
from __future__ import print_function

import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
HOST_BUILD = os.path.join(ROOT, "build", "host")
COVERAGE_BUILD = os.path.join(ROOT, "build", "host-coverage")


def _which(name):
    found = shutil.which(name)
    if found:
        return found
    extras = []
    if os.name == "nt":
        extras = [
            os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "CMake", "bin"),
            os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "LLVM", "bin"),
        ]
        pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        extras.append(os.path.join(pf86, "Microsoft Visual Studio", "Installer"))
    for folder in extras:
        candidate = os.path.join(folder, name)
        if os.path.isfile(candidate):
            return candidate
        if os.name == "nt":
            candidate_exe = candidate + ".exe"
            if os.path.isfile(candidate_exe):
                return candidate_exe
    return None


def require(name, extra=""):
    path = _which(name)
    if not path:
        msg = "missing %s" % name
        if extra:
            msg += " (%s)" % extra
        raise SystemExit(msg)
    return path


def find_rc():
    """Windows resource compiler. CMake+Clang needs CMAKE_RC_COMPILER.

    Prefer Windows SDK rc.exe (not MinGW windres). llvm-rc is the fallback.
    """
    if os.name != "nt":
        return None
    found = _which("rc")
    if found:
        return found
    pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    pf = os.environ.get("ProgramFiles", r"C:\Program Files")
    kit_roots = [
        os.path.join(pf86, "Windows Kits", "10", "bin"),
        os.path.join(pf, "Windows Kits", "10", "bin"),
    ]
    versions = []
    for kits in kit_roots:
        if not os.path.isdir(kits):
            continue
        try:
            names = os.listdir(kits)
        except OSError:
            names = []
        for name in names:
            cand = os.path.join(kits, name, "x64", "rc.exe")
            if os.path.isfile(cand):
                versions.append(cand)
    if versions:
        versions.sort(reverse=True)
        return versions[0]
    vswhere = _which("vswhere")
    if not vswhere:
        vswhere = os.path.join(pf86, "Microsoft Visual Studio", "Installer", "vswhere.exe")
        if not os.path.isfile(vswhere):
            vswhere = None
    if vswhere:
        try:
            out = subprocess.check_output(
                [vswhere, "-latest", "-products", "*", "-find", r"**\rc.exe"],
                universal_newlines=True,
            )
        except (OSError, subprocess.CalledProcessError):
            out = ""
        lines = [ln.strip() for ln in out.splitlines() if ln.strip()]
        x64 = [ln for ln in lines if ln.lower().endswith(os.path.join("x64", "rc.exe").lower())]
        if x64:
            return x64[0]
        if lines:
            return lines[0]
    llvm_rc = _which("llvm-rc")
    if llvm_rc:
        return llvm_rc
    llvm_bin = os.path.join(pf, "LLVM", "bin", "llvm-rc.exe")
    if os.path.isfile(llvm_bin):
        return llvm_bin
    return None


def find_nmake():
    found = _which("nmake")
    if found:
        return found
    vswhere = _which("vswhere")
    if not vswhere:
        pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        vswhere = os.path.join(
            pf86, "Microsoft Visual Studio", "Installer", "vswhere.exe"
        )
        if not os.path.isfile(vswhere):
            return None
    try:
        out = subprocess.check_output(
            [vswhere, "-latest", "-products", "*", "-find", r"**\nmake.exe"],
            universal_newlines=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    lines = [ln.strip() for ln in out.splitlines() if ln.strip()]
    preferred = [ln for ln in lines if "Hostx64" in ln and "x64" in ln]
    if preferred:
        return preferred[0]
    return lines[0] if lines else None


def generator_and_env():
    """Return (generator, extra_env) for a single-config Clang host build."""
    env = os.environ.copy()
    if os.name == "nt":
        rc = find_rc()
        if rc:
            env["PATH"] = os.path.dirname(rc) + os.pathsep + env.get("PATH", "")
        ninja = _which("ninja")
        if ninja:
            return "Ninja", env
        nmake = find_nmake()
        if not nmake:
            raise SystemExit(
                "Windows host build needs nmake (VS Build Tools) or ninja. "
                "Not using MinGW."
            )
        nmake_dir = os.path.dirname(nmake)
        env["PATH"] = nmake_dir + os.pathsep + env.get("PATH", "")
        return "NMake Makefiles", env
    ninja = _which("ninja")
    if ninja:
        return "Ninja", env
    return "Unix Makefiles", env


def _read_cache_kv(cache_text, key):
    prefix = key + "="
    for line in cache_text.splitlines():
        if line.startswith(prefix):
            return line.split("=", 1)[1]
    return None


def reset_stale_cache(build_dir, gen):
    """Drop CMakeCache when this tree was last configured on the other OS."""
    cache_path = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache_path):
        return
    with open(cache_path, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()
    cached_gen = _read_cache_kv(text, "CMAKE_GENERATOR:INTERNAL")
    cached_home = _read_cache_kv(text, "CMAKE_HOME_DIRECTORY:INTERNAL")
    # Dual-boot NTFS: /home/chucks/Git -> /NTFS/Git. Compare real paths.
    root_norm = os.path.normcase(os.path.realpath(ROOT))
    if cached_home:
        home_norm = os.path.normcase(os.path.realpath(os.path.normpath(cached_home)))
    else:
        home_norm = ""
    stale = False
    if cached_gen and cached_gen != gen:
        stale = True
    if cached_home and home_norm != root_norm:
        stale = True
    if not stale:
        return
    print("resetting stale CMake cache in %s (was %s / %s)" % (build_dir, cached_gen, cached_home))
    os.remove(cache_path)
    cmake_files = os.path.join(build_dir, "CMakeFiles")
    if os.path.isdir(cmake_files):
        shutil.rmtree(cmake_files)


def cmake_configure(build_dir, coverage=False):
    cmake = require("cmake", "add C:\\Program Files\\CMake\\bin to PATH")
    clang = require("clang", "LLVM Clang")
    clangxx = require("clang++", "LLVM Clang")
    gen, env = generator_and_env()
    os.makedirs(build_dir, exist_ok=True)
    reset_stale_cache(build_dir, gen)
    cmd = [
        cmake,
        "-S",
        ROOT,
        "-B",
        build_dir,
        "-G",
        gen,
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_C_COMPILER=" + clang,
        "-DCMAKE_CXX_COMPILER=" + clangxx,
        "-DRR_ENABLE_COVERAGE=" + ("ON" if coverage else "OFF"),
    ]
    if os.name == "nt":
        rc = find_rc()
        if not rc:
            raise SystemExit(
                "CMAKE_RC_COMPILER: need Windows SDK rc.exe or LLVM llvm-rc. "
                "Install Windows 10/11 SDK (Visual Studio Build Tools) or LLVM. "
                "Not MinGW windres."
            )
        cmd.append("-DCMAKE_RC_COMPILER=" + rc)
        print("CMAKE_RC_COMPILER=%s" % rc)
    print(" ".join(cmd))
    subprocess.check_call(cmd, env=env)
    return env


def cmake_build(build_dir, env, target=None):
    cmake = require("cmake")
    cmd = [cmake, "--build", build_dir]
    if target:
        cmd.extend(["--target", target])
    print(" ".join(cmd))
    subprocess.check_call(cmd, env=env)


def host_exe(build_dir, stem):
    """Pick the native host binary. Dual-boot NTFS may leave both ELF and .exe."""
    unix = os.path.join(build_dir, stem)
    windows = os.path.join(build_dir, stem + ".exe")
    if os.name == "nt":
        candidates = (windows, unix)
    else:
        candidates = (unix, windows)
    for path in candidates:
        if os.path.isfile(path):
            return path
    raise SystemExit("%s not built in %s" % (stem, build_dir))


def tests_exe(build_dir):
    return host_exe(build_dir, "rr_servo_tests")


if __name__ == "__main__":
    sys.stderr.write("import host_cmake from build_host.py / run_coverage.py\n")
    sys.exit(2)
