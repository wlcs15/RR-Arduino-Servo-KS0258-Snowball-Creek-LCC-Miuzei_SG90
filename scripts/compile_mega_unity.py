#!/usr/bin/env python3
"""Compile (and optionally upload) on-target Unity for Mega 2560.

--coverage instruments only tests/test_rr_servo.cpp (where lib/rr_servo
headers are compiled), links Arduino avr-gcc libgcov.a, and provides
__gcov_exit (missing from that archive).
"""
from __future__ import print_function

import argparse
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FQBN = "arduino:avr:mega:cpu=atmega2560"
SKETCH = os.path.join(ROOT, "sketches", "RrServoUnity")
WRAP = os.path.join(ROOT, "scripts", "avr_cc_wrap.py")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import find_arduino  # pylint: disable=wrong-import-position


def arduino_cli():
    found = find_arduino.find_cli()
    if found:
        return found
    return "arduino-cli"


def extra_flags(coverage):
    root = ROOT.replace("\\", "/")
    flags = [
        "-DUNITY_INCLUDE_CONFIG_H",
        "-DUNITY_EXCLUDE_FLOAT",
        "-I%s/third_party/Unity/src" % root,
        "-I%s/sketches/RrServoUnity" % root,
    ]
    if coverage:
        flags.append("-DRR_GCOV")
    return " ".join(flags)


def wrap_recipe(lang):
    py = sys.executable.replace("\\", "/")
    wrap = WRAP.replace("\\", "/")
    if lang == "cpp":
        cmd = "{compiler.path}{compiler.cpp.cmd}"
        flags = "{compiler.cpp.flags}"
        extra = "{compiler.cpp.extra_flags}"
    else:
        cmd = "{compiler.path}{compiler.c.cmd}"
        flags = "{compiler.c.flags}"
        extra = "{compiler.c.extra_flags}"
    return (
        '"%s" "%s" "%s" %s -mmcu={build.mcu} -DF_CPU={build.f_cpu} '
        "-DARDUINO={runtime.ide.version} -DARDUINO_{build.board} "
        "-DARDUINO_ARCH_{build.arch} %s {build.extra_flags} {includes} "
        '"{source_file}" -o "{object_file}"' % (py, wrap, cmd, flags, extra)
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--upload", action="store_true")
    parser.add_argument("--port", default="COM5")
    args = parser.parse_args()

    extra = extra_flags(args.coverage)
    cli = arduino_cli()
    config = os.path.join(
        os.environ.get("LOCALAPPDATA", ""), "Arduino15", "arduino-cli.yaml"
    )
    cmd = [
        cli,
        "compile",
        "--clean",
        "--fqbn",
        FQBN,
        "--library",
        os.path.join(ROOT, "lib", "rr_servo"),
    ]
    if os.path.isfile(config):
        cmd.extend(["--config-file", config])
    cmd.extend(
        [
            "--build-property",
            "compiler.cpp.extra_flags=" + extra,
            "--build-property",
            "compiler.c.extra_flags=" + extra,
        ]
    )
    if args.coverage:
        cmd.extend(
            [
                "--build-property",
                "recipe.cpp.o.pattern=" + wrap_recipe("cpp"),
                "--build-property",
                "recipe.c.o.pattern=" + wrap_recipe("c"),
                "--build-property",
                "compiler.libraries.ldflags=-lgcov",
                "--build-property",
                "compiler.c.elf.extra_flags=--coverage -Wl,-u,__gcov_exit -Wl,-u,__gcov_init",
            ]
        )
    if args.upload:
        cmd.extend(["--upload", "-p", args.port])
    cmd.append(SKETCH)
    print(" ".join(cmd))
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main())
