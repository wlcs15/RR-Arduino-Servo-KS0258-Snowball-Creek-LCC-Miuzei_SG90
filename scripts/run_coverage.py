#!/usr/bin/env python3
"""Host coverage: Clang -fprofile-instr-generate -fcoverage-mapping + llvm-cov.

Usual entry: python scripts/run_coverage.py
CMake equivalent (after this script has configured the tree):
  cmake --build build/host-coverage --target coverage

No Ceedling, no gcovr, no MinGW, no AVR-GCC.
"""
from __future__ import print_function

import os
import sys

from host_cmake import COVERAGE_BUILD, cmake_build, cmake_configure, require


def main():
    require("llvm-profdata", "LLVM bin on PATH")
    require("llvm-cov", "LLVM bin on PATH")
    env = cmake_configure(COVERAGE_BUILD, coverage=True)
    cmake_build(COVERAGE_BUILD, env, target="coverage")
    html = os.path.join(COVERAGE_BUILD, "coverage", "index.html")
    if os.path.isfile(html):
        print("HTML report: %s" % html)
    return 0


if __name__ == "__main__":
    sys.exit(main())
