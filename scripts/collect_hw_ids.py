#!/usr/bin/env python3
"""Collect MAC, OpenLCB node ID, and SPI flash UID from DEBUG serial.

Does not handle the Wi-Fi password. Writes local/hw_ids.env only.

  python scripts/collect_hw_ids.py --from-log path/to/serial.log
  python scripts/collect_hw_ids.py --port COM7
"""

from __future__ import print_function

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wifi_wrap import parse_debug_log, write_hw_ids  # noqa: E402

DEFAULT_OUT = "local/hw_ids.env"


def collect_from_log(log_path, out_path):
    with open(log_path, "r", encoding="utf-8", errors="replace") as handle:
        ids = parse_debug_log(handle.read())
    write_hw_ids(out_path, ids)
    return ids


def _reset_esp32(ser):
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.1)
    ser.setRTS(False)
    time.sleep(0.1)


def collect_from_port(port, baud, timeout_s, out_path):
    try:
        import serial
    except ImportError:
        sys.stderr.write("pyserial is required for --port\n")
        raise SystemExit(1)

    chunks = []
    deadline = time.time() + timeout_s
    with serial.Serial(port, baudrate=baud, timeout=0.2) as ser:
        _reset_esp32(ser)
        ser.reset_input_buffer()
        while time.time() < deadline:
            raw = ser.read(1024)
            if raw:
                chunks.append(raw.decode("utf-8", errors="replace"))
                text = "".join(chunks)
                if (
                    "MAC Address:" in text
                    and "OpenLCB Node ID:" in text
                    and "SPI flash unique ID:" in text
                ):
                    break
    text = "".join(chunks)
    if not text.strip():
        sys.stderr.write("No serial data from %s. Is the board connected?\n" % port)
        raise SystemExit(1)
    ids = parse_debug_log(text)
    write_hw_ids(out_path, ids)
    return ids


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device, e.g. COM7 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--from-log", dest="from_log")
    parser.add_argument("--out", default=DEFAULT_OUT)
    args = parser.parse_args()

    if args.from_log:
        ids = collect_from_log(args.from_log, args.out)
    elif args.port:
        ids = collect_from_port(args.port, args.baud, args.timeout, args.out)
    else:
        parser.error("provide --port or --from-log")

    print("Wrote %s" % args.out)
    print("MAC Address: %s" % ids["WIFI_MAC"])
    print("OpenLCB Node ID: %s" % ids["WIFI_NODE_ID"])
    print("SPI flash unique ID: %s" % ids["WIFI_FLASH_UID"])


if __name__ == "__main__":
    main()
