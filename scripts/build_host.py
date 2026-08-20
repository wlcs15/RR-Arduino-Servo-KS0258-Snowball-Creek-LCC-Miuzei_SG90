#!/usr/bin/env python3
"""Configure and build host Unity tests + DEBUG CLI with Clang + CMake."""
from __future__ import print_function

import os
import sys

from host_cmake import HOST_BUILD, cmake_build, cmake_configure, tests_exe


def main():
    env = cmake_configure(HOST_BUILD, coverage=False)
    cmake_build(HOST_BUILD, env)
    print("host binaries in %s" % HOST_BUILD)
    print("tests: %s" % tests_exe(HOST_BUILD))
    cli = os.path.join(HOST_BUILD, "rr_servo_debug_cli.exe")
    if not os.path.isfile(cli):
        cli = os.path.join(HOST_BUILD, "rr_servo_debug_cli")
    if os.path.isfile(cli):
        print("debug CLI: %s" % cli)
    return 0


if __name__ == "__main__":
    sys.exit(main())
