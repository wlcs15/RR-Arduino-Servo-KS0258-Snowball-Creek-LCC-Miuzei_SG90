#!/usr/bin/env python3
"""Insert --coverage only for our test TU, not Arduino core.

Usage: avr_cc_wrap.py <avr-gcc> [args...]
"""
from __future__ import print_function

import os
import subprocess
import sys

OURS = ("test_rr_servo.cpp",)


def source_file(args):
    for a in args:
        al = a.replace("\\", "/").lower()
        if al.endswith(".c") or al.endswith(".cpp") or al.endswith(".ino") or al.endswith(".s"):
            if "-o" in args and args[args.index(a) - 1] == "-o":
                continue
            return os.path.basename(a.replace("\\", "/"))
    return ""


def main():
    if len(sys.argv) < 3:
        return 2
    cc = sys.argv[1]
    args = sys.argv[2:]
    extra = []
    if source_file(args) in OURS:
        extra = ["-DRR_GCOV", "-fprofile-arcs", "-ftest-coverage"]
    return subprocess.call([cc] + extra + args)


if __name__ == "__main__":
    sys.exit(main())
