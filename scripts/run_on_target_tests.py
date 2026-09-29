#!/usr/bin/env python3
"""On-target Unity under simavr (ATmega2560 @ 16 MHz). Never flashes hardware.

Builds sketches/RrServoUnity the same way as scripts/compile_mega_unity.py
(FQBN arduino:avr:mega:cpu=atmega2560, lib/rr_servo, Unity include flags)
and runs:

    simavr -m atmega2560 -f 16000000 <build/avr-unity/*.ino.elf>

No arduino-cli --upload and no avrdude. Host scripts/run_tests.sh stays the
native Unity suite; this is that suite on a simulated Mega.

Linux / CI:
    ./scripts/run_on_target_tests.sh
    python3 scripts/run_on_target_tests.py

simavr discovery: command -v simavr after sourcing /workspace/env/emulators.sh
when that file exists, and after prepending /workspace/tools/bin when present.
Missing simavr on Linux is a fatal error (PATH, that env file, apt or tools).

Windows (os.name == nt): simavr on-target is Linux CI only. Prints SKIP and
exits 0 so a local Windows run is not a hard tool failure (same spirit as
check_tools.ps1). Does not flash.
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\\run_on_target_tests.ps1
"""
from __future__ import print_function

import glob
import os
import re
import select
import shutil
import signal
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
COMPILE = os.path.join(ROOT, "scripts", "compile_mega_unity.py")
OUT_DIR = os.path.join(ROOT, "build", "avr-unity")
TIMEOUT_SEC = 75
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")

SKIP_WIN = (
    "SKIP on-target-via-simavr is not supported on Windows "
    "(Linux CI only; does not flash hardware)."
)
FATAL_SIMAVR = (
    "FATAL simavr not found on PATH. Source /workspace/env/emulators.sh "
    "(prepends /workspace/tools/bin) or install it (apt install simavr, "
    "or the workspace tools tree). This runner never flashes hardware."
)


def _is_windows():
    return os.name == "nt" or sys.platform.startswith("win")


def apply_workspace_env():
    """Prefer workspace simavr and arduino-cli when those env files exist."""
    tools_bin = "/workspace/tools/bin"
    if os.path.isdir(tools_bin):
        os.environ["PATH"] = tools_bin + os.pathsep + os.environ.get("PATH", "")
    scripts = [
        path
        for path in (
            "/workspace/env/emulators.sh",
            "/workspace/env/arduino.sh",
        )
        if os.path.isfile(path)
    ]
    if scripts and not _is_windows():
        inner = (
            "set -a; "
            + "; ".join('source "%s"' % path for path in scripts)
            + "; set +a; env -0"
        )
        try:
            proc = subprocess.run(
                ["bash", "-c", inner],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
        except OSError:
            proc = None
        if proc is not None and proc.returncode == 0:
            keep_prefix = ("ARDUINO_",)
            keep_exact = {
                "PATH",
                "LD_LIBRARY_PATH",
                "SIMAVR",
                "SIMAVR_ROOT",
            }
            for item in proc.stdout.split(b"\0"):
                if not item or b"=" not in item:
                    continue
                key, val = item.split(b"=", 1)
                try:
                    name = key.decode("utf-8")
                    value = val.decode("utf-8", "surrogateescape")
                except UnicodeError:
                    continue
                if name in keep_exact or name.startswith(keep_prefix):
                    os.environ[name] = value
    # Host C++ include paths poison avr-g++ (same guard as CI).
    for poison in ("CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "CPATH", "GCC_ROOT"):
        os.environ.pop(poison, None)


def find_simavr():
    explicit = os.environ.get("SIMAVR") or ""
    if explicit and os.path.isfile(explicit) and os.access(explicit, os.X_OK):
        return explicit
    return shutil.which("simavr")


def classify(text):
    """Return (True, why), (False, why), or (None, '') if still inconclusive."""
    clean = ANSI_RE.sub("", text or "")
    match = re.search(r"unity_failures=(\d+)", clean)
    if match:
        count = int(match.group(1))
        if count == 0:
            return True, "unity_failures=0"
        return False, "unity_failures=%d" % count
    if "FAIL:" in clean:
        return False, "FAIL:"
    summary = re.search(r"(\d+)\s+Tests\s+(\d+)\s+Failures\b", clean)
    if summary:
        fails = int(summary.group(2))
        if fails == 0:
            return True, "unity summary 0 failures"
        return False, "unity summary %s failures" % summary.group(2)
    return None, ""


def early_stop(text):
    """Stop simavr once the sketch's counter or a hard failure is visible.

    The Unity summary is printed inside the test runner, before the sketch
    emits unity_failures=. A zero-failure summary is not decisive yet.
    """
    ok, why = classify(text)
    if why.startswith("unity_failures"):
        return ok, why
    if why == "FAIL:" or ok is False:
        return False, why
    return None, ""


def find_elf(out_dir):
    for pattern in ("*.ino.elf", "*.elf"):
        hits = sorted(glob.glob(os.path.join(out_dir, pattern)))
        if hits:
            return hits[0]
    for dirpath, _dirnames, filenames in os.walk(out_dir):
        for name in filenames:
            if name.endswith(".ino.elf"):
                return os.path.join(dirpath, name)
    return None


def _emit(data):
    if not data:
        return
    sys.stdout.write(data.decode("utf-8", "replace"))
    sys.stdout.flush()


def _killpg(proc, sig=None):
    if sig is None:
        sig = signal.SIGINT
    if proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, sig)
    except OSError:
        try:
            proc.send_signal(sig)
        except OSError:
            pass


def _drain(fd, chunks, seconds):
    end = time.time() + seconds
    while time.time() < end:
        ready, _, _ = select.select([fd], [], [], 0.1)
        if not ready:
            continue
        try:
            data = os.read(fd, 4096)
        except OSError:
            return
        if not data:
            return
        chunks.append(data)
        _emit(data)


def run_simavr(simavr, elf, timeout_sec):
    cmd = [simavr, "-m", "atmega2560", "-f", "16000000", elf]
    timeout_bin = shutil.which("timeout")
    if timeout_bin:
        cmd = [
            timeout_bin,
            "--signal=INT",
            "--kill-after=5",
            str(int(timeout_sec)),
        ] + cmd
    print("RUN " + " ".join(cmd))
    sys.stdout.flush()
    try:
        import pty
    except ImportError:
        pty = None
    if pty is None:
        return _run_pipe(cmd, timeout_sec)
    master, slave = pty.openpty()
    proc = subprocess.Popen(
        cmd,
        stdin=slave,
        stdout=slave,
        stderr=slave,
        start_new_session=True,
        close_fds=True,
    )
    os.close(slave)
    chunks = []
    deadline = time.time() + timeout_sec + 10
    try:
        while True:
            if time.time() > deadline:
                _killpg(proc)
                _drain(master, chunks, 1.0)
                break
            ready, _, _ = select.select([master], [], [], 0.4)
            if ready:
                try:
                    data = os.read(master, 4096)
                except OSError:
                    break
                if not data:
                    break
                chunks.append(data)
                _emit(data)
                ok, _why = early_stop(b"".join(chunks).decode("utf-8", "replace"))
                if ok is not None:
                    _killpg(proc)
                    _drain(master, chunks, 1.0)
                    break
            if proc.poll() is not None:
                _drain(master, chunks, 0.5)
                break
    finally:
        try:
            os.close(master)
        except OSError:
            pass
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            _killpg(proc, signal.SIGKILL)
            proc.wait()
    text = b"".join(chunks).decode("utf-8", "replace")
    return proc.returncode, text


def _run_pipe(cmd, timeout_sec):
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    chunks = []
    deadline = time.time() + timeout_sec + 10
    assert proc.stdout is not None
    try:
        while True:
            if time.time() > deadline:
                _killpg(proc)
                break
            ready, _, _ = select.select([proc.stdout], [], [], 0.4)
            if ready:
                data = proc.stdout.read(4096)
                if not data:
                    break
                chunks.append(data)
                _emit(data)
                ok, _why = early_stop(b"".join(chunks).decode("utf-8", "replace"))
                if ok is not None:
                    _killpg(proc)
                    break
            if proc.poll() is not None:
                rest = proc.stdout.read()
                if rest:
                    chunks.append(rest)
                    _emit(rest)
                break
    finally:
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            _killpg(proc, signal.SIGKILL)
            proc.wait()
    text = b"".join(chunks).decode("utf-8", "replace")
    return proc.returncode, text


def build_unity():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    cmd = [
        sys.executable,
        "-u",
        COMPILE,
        "--output-dir",
        OUT_DIR,
    ]
    print("BUILD " + " ".join(cmd))
    print("NOTE no --upload; simavr only, hardware is not flashed")
    sys.stdout.flush()
    return subprocess.call(cmd)


def verdict_line(ok, why, rc):
    if ok is True:
        return "PASS on-target simavr ATmega2560 (%s)" % why
    if ok is False:
        return "FAIL on-target simavr ATmega2560 (%s)" % why
    if rc == 124:
        return (
            "FAIL on-target simavr ATmega2560 "
            "(timeout with no success marker, simavr_exit=%s)" % rc
        )
    return (
        "FAIL on-target simavr ATmega2560 "
        "(no success marker, simavr_exit=%s)" % rc
    )


def main():
    if _is_windows():
        print(SKIP_WIN)
        return 0
    apply_workspace_env()
    simavr = find_simavr()
    if not simavr:
        print(FATAL_SIMAVR)
        return 1
    print("simavr: %s" % simavr)
    rc = build_unity()
    if rc != 0:
        print("FAIL on-target simavr ATmega2560 (compile rc=%s)" % rc)
        return rc if rc else 1
    elf = find_elf(OUT_DIR)
    if not elf:
        print("FAIL on-target simavr ATmega2560 (no .ino.elf under %s)" % OUT_DIR)
        return 1
    print("ELF %s" % elf)
    sim_rc, text = run_simavr(simavr, elf, TIMEOUT_SEC)
    ok, why = classify(text)
    line = verdict_line(ok, why, sim_rc)
    print(line)
    return 0 if ok is True else 1


if __name__ == "__main__":
    sys.exit(main())
