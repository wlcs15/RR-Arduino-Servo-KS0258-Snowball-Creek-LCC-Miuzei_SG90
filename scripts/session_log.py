#!/usr/bin/env python3
"""Tee stdout/stderr to gitignored local/<name>-*.log for sharing without Grok CLI.

Do not use this from provision_wifi_build.py (that script handles a PSK).

  import session_log
  session_log.attach("build_host")
"""
from __future__ import print_function

import atexit
import os
import socket
import sys
from datetime import datetime

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
_attached = []


class _Tee(object):
    def __init__(self, stream, handle):
        self.stream = stream
        self.handle = handle

    def write(self, data):
        self.stream.write(data)
        try:
            self.handle.write(data)
            self.handle.flush()
        except Exception:
            pass
        return len(data) if data is not None else 0

    def flush(self):
        self.stream.flush()
        try:
            self.handle.flush()
        except Exception:
            pass

    def fileno(self):
        return self.stream.fileno()

    def isatty(self):
        return False


def attach(name):
    """Start teeing this process. Safe to call once per script."""
    if os.environ.get("RR_SESSION_LOG_INNER") == name:
        return None
    if name in _attached:
        return None
    local_dir = os.path.join(ROOT, "local")
    try:
        os.makedirs(local_dir)
    except OSError:
        pass
    host = socket.gethostname() or "host"
    host = "".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in host)
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    log_path = os.path.join(local_dir, "%s-%s-%s.log" % (name, ts, host))
    last_path = os.path.join(local_dir, "%s-last.log" % name)
    try:
        handle = open(log_path, "w", encoding="utf-8", errors="replace")
    except OSError:
        return None
    header = [
        "=== %s log (share this file with Grok; Grok CLI not required) ===" % name,
        "file: %s" % log_path,
        "time: %s" % datetime.now().isoformat(),
        "host: %s" % (socket.gethostname() or ""),
        "user: %s" % (os.environ.get("USERNAME") or os.environ.get("USER") or ""),
        "repo: %s" % ROOT,
        "python: %s" % sys.executable,
        "argv: %s" % " ".join(sys.argv),
        "",
    ]
    handle.write("\n".join(header) + "\n")
    handle.flush()
    sys.stdout = _Tee(sys.stdout, handle)
    sys.stderr = _Tee(sys.stderr, handle)
    _attached.append(name)
    os.environ["RR_SESSION_LOG_INNER"] = name

    def _finish():
        try:
            handle.flush()
            handle.close()
        except Exception:
            pass
        try:
            import shutil

            shutil.copyfile(log_path, last_path)
        except Exception:
            pass
        try:
            real_out = sys.stdout.stream if isinstance(sys.stdout, _Tee) else sys.stdout
            real_out.write("\nShare this file with Grok (no Grok CLI needed):\n  %s\n  %s\n" % (log_path, last_path))
            real_out.flush()
        except Exception:
            pass

    atexit.register(_finish)
    print("logging to %s" % log_path)
    return log_path
